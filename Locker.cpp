class Locker
{
	struct Node
	{
		std::shared_ptr<std::mutex> mtx;

		const int id_node;
		std::shared_ptr<Node> next = nullptr;
		std::shared_ptr<Node> prev = nullptr;

		Node(const int id, std::shared_ptr<std::mutex> init_mtx) : id_node(id), mtx(init_mtx) {}
		Node(const int id, std::shared_ptr<Node> &node_copy) : id_node(id), mtx(node_copy->mtx) {}
	};
	
	std::shared_ptr<Node> node = nullptr;
	const int count_nodes;

public:
	Locker(int count_node) : count_nodes(count_node), node(std::make_shared<Node>(count_node, std::make_shared<std::mutex>()))
	{	
		std::shared_ptr<Node> tmp1 = node;
		if (count_node > 1)
			while (--count_node)
			{
				std::shared_ptr<Node> tmp2 = std::make_shared<Node>(count_node, std::make_shared<std::mutex>());
				tmp1->next = tmp2;
				tmp2->prev = tmp1;
				tmp1 = tmp2;
			}
		else
			throw std::runtime_error(" error argument in Locker constructor.");
		tmp1->next = node;
		node->prev = tmp1;
		node->mtx->lock();
		node->prev->mtx->lock();
	}

	Locker(const Locker &copy_locker) : count_nodes(copy_locker.count_nodes), node(std::make_shared<Node>(copy_locker.node->id_node, copy_locker.node->mtx))
	{
		std::shared_ptr<Node> tmp1 = node;
		std::shared_ptr<Node> var = copy_locker.node;
		while (var->next != copy_locker.node)
		{
			var = var->next;
			std::shared_ptr<Node> tmp2 = std::make_shared<Node>(var->id_node, var->mtx);
			tmp1->next = tmp2;
			tmp2->prev = tmp1;
			tmp1 = tmp2;			
		}

		tmp1->next = node;
		node->prev = tmp1;
		for (int i = 1; !node->mtx->try_lock(); ++i)
		{
			if (i == count_nodes)
				throw std::runtime_error(" exceded Locker instance.");
			node = node->next;
		}
		node = node->next;
		node->mtx->lock();
	}

	void next_iter()
	{
		node->prev->mtx->unlock();
		node->next->mtx->lock();
		node = node->next;
	}
};