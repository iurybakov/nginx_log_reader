#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <string.h>
#include <stdio.h>
#include <thread>
#include <mutex>
#include <unordered_map>
#include <stdexcept>
#include <unistd.h>
#include <sstream>
#include <chrono>
#include <sstream>
#include <iostream>
#include <string>
#include <atomic>

#define BUFFER_SIZE 1024000
#define LINE_SIZE 12800
#define INTERVAL_SEND_INFLUX_DATA 10
const char *const path_named_pipe = "/home/rybakov/test_named_pipe/.file_pipe";
using map_type = std::unordered_map<std::string, long long int>;



inline void swap(map_type *&a, map_type *&b)
{
    map_type *c = a;
    a = b;
    b = c;
}

void send_influx(map_type **ptrptr_map_reader1, map_type **ptrptr_map_reader2, std::atomic_bool *ptr_flag_switch1, std::atomic_bool *ptr_flag_switch2)
{
    map_type *&ptr_map_reader1 = *ptrptr_map_reader1;
    map_type *&ptr_map_reader2 = *ptrptr_map_reader2;
    std::atomic_bool &flag_switch1 = *ptr_flag_switch1;
    std::atomic_bool &flag_switch2 = *ptr_flag_switch2;
    std::string buffer = "";

    while (true)
    {
        if (flag_switch1 || flag_switch2)
        {
            std::this_thread::sleep_for(std::chrono::seconds(5));
            continue;
        }
        else if (ptr_map_reader1->empty() && ptr_map_reader2->empty())
        {
            flag_switch1 = true;
            flag_switch2 = true;
            std::this_thread::sleep_for(std::chrono::seconds(INTERVAL_SEND_INFLUX_DATA));
            continue;
        }

        for (auto &member_map1 : *ptr_map_reader1)
        {
            (*ptr_map_reader2)[member_map1.first] += member_map1.second;
            member_map1.second = 0;
        }
        
        for (auto &member_map2 : *ptr_map_reader2)
            if (member_map2.second)
            {
                buffer.append(member_map2.first);
                buffer.append(std::to_string(member_map2.second));
                buffer.append("i\n");
                member_map2.second = 0;
            }

        std::cout << "'send_influx' send data, both flag true" << std::endl;

        std::cout << buffer << "\n\n\n\n" << std::endl;
        // send data

        // to do clear map once in day

        flag_switch1 = true;
        flag_switch2 = true;
        buffer.clear();
        std::this_thread::sleep_for(std::chrono::seconds(INTERVAL_SEND_INFLUX_DATA));
    }
}

void agregation(long &file_desc, std::mutex *mtx_ptr, map_type **ptrptr_map_reader, map_type **ptrptr_map_worker, std::atomic_bool *ptr_flag_switch)
{
    char buffer[BUFFER_SIZE];
    char line[LINE_SIZE];
    long lenght_read_data = 0;
    std::istringstream stream;

    std::mutex &mtx = *mtx_ptr;
    map_type *&ptr_map_reader = *ptrptr_map_reader;
    map_type *&ptr_map_worker = *ptrptr_map_worker;
    std::atomic_bool &flag_switch = *ptr_flag_switch;

    while (true)
    {
        mtx.lock();
        memset(buffer, '\0', BUFFER_SIZE);
        if ((lenght_read_data = read(file_desc, buffer, BUFFER_SIZE)) <= 0)
        {
            int count_trying_recreate_named_pipe = 0;
            do {
                close(file_desc);
                remove(path_named_pipe);
                if (++count_trying_recreate_named_pipe > 10)     
                    throw std::runtime_error(" exceded count trying reopend named pipe...");

                else if (mkfifo(path_named_pipe, S_IRWXO | S_IRWXG | S_IRWXU))
                {
                    remove(path_named_pipe);
                    throw std::runtime_error(" unable create named pipe, mkfifo error...");
                }                
                else if ((file_desc = open(path_named_pipe, O_RDONLY)) <= 0)
                {
                    remove(path_named_pipe);
                    throw std::runtime_error(" error recreate named pipe...");
                }                
            } while ((lenght_read_data = read(file_desc, buffer, BUFFER_SIZE)) <= 0);
        }
        mtx.unlock();

        stream.str(buffer);

        while (stream.getline(line, LINE_SIZE))
            ++(*ptr_map_worker)[line];

        if (flag_switch)
        {
            swap(ptr_map_reader, ptr_map_worker);
            flag_switch = false;
        }
        stream.clear();
    }
}

int main()
{
    std::atomic_bool flag_switch1(false);
    std::atomic_bool flag_switch2(false);

    map_type    mp1, mp2, mp3, mp4,
                *ptr_map_reader1 = &mp1, *ptr_map_reader2 = &mp2,
                *ptr_map_worker1 = &mp3, *ptr_map_worker2 = &mp4;
    
    std::mutex mtx;

    remove(path_named_pipe);

    if (mkfifo(path_named_pipe, S_IRWXO | S_IRWXG | S_IRWXU))
    {
        remove(path_named_pipe);
        throw std::runtime_error("Unable create named pipe, mkfifo error...");
    }

    long file_desc = open(path_named_pipe, O_RDONLY);

    if (file_desc <= 0)
    {
        remove(path_named_pipe);
        throw std::runtime_error("Unable open named pipe, open error...");
    }

    std::thread agr_thr(agregation, std::ref(file_desc), &mtx, &ptr_map_reader1, &ptr_map_worker1, &flag_switch1);
    agr_thr.detach();

    std::thread snd_thr(send_influx, &ptr_map_reader1, &ptr_map_reader2, &flag_switch1, &flag_switch2);    
    snd_thr.detach();

    agregation(file_desc, &mtx, &ptr_map_reader2, &ptr_map_worker2, &flag_switch2);

    return 0;
}