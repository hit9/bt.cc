// Copyright (c) 2024 Chao Wang <hit9@icloud.com>.
// License: BSD, Version: 0.4.3.  https://github.com/hit9/bt.cc
// A lightweight behavior tree library that separates data and behavior.

#include "bt.h"

#include <algorithm> // for max
#include <cstdio>	 // for printf
#include <random>	 // for mt19937
#include <thread>	 // for this_thread::sleep_for

namespace bt
{

	/////////////////
	/// TreeBlob
	/////////////////

	std::pair<void*, bool> ITreeBlob::make(const NodeId id, const size_t size, const std::size_t cap)
	{
		if (cap)
			reserve(cap);
		std::size_t idx = id - 1;
		if (exist(idx))
			return { get(idx), false };
		return { allocate(idx, size), true };
	}

	void* DynamicTreeBlob::allocate(const std::size_t idx, const std::size_t size)
	{
		if (m.size() <= idx)
		{
			m.resize(idx + 1);
			e.resize(idx + 1, false);
		}
		auto p = std::make_unique_for_overwrite<unsigned char[]>(size);
		auto rp = p.get();
		std::fill_n(rp, size, 0);
		m[idx] = std::move(p);
		e[idx] = true;
		return rp;
	}

	void DynamicTreeBlob::reserve(const std::size_t cap)
	{
		if (m.capacity() < cap)
		{
			m.reserve(cap);
			e.reserve(cap);
		}
	}

	////////////////////////////
	/// Node
	////////////////////////////

	// Returns char representation of given status.
	static const char statusRepr(Status s)
	{
		switch (s)
		{
			case Status::UNDEFINED:
				return 'U';
			case Status::RUNNING:
				return 'R';
			case Status::SUCCESS:
				return 'S';
			case Status::FAILURE:
				return 'F';
		}
		return 'U';
	}

	void Node::makeVisualizeString(std::string& s, int depth, ull seq)
	{
		const auto* b = GetNodeBlob();
		if (depth > 0)
			s += " |";
		for (int i = 1; i < depth; i++)
			s += "---|";
		if (depth > 0)
			s += "- ";
		if (b->lastSeq == seq)
			s += "\033[32m"; // color if catches up with seq.
		s += Name();
		s.push_back('(');
		s.push_back(statusRepr(b->lastStatus));
		s.push_back(')');
		if (b->lastSeq == seq)
			s += "\033[0m";
	}

	unsigned int Node::GetPriorityCurrentTick(const Context& ctx)
	{
		if (ctx.seq != priorityCurrentTickSeq)
			priorityCurrentTick = 0;
		// try cache in this tick firstly.
		if (!priorityCurrentTick)
		{
			priorityCurrentTick = Priority(ctx);
			priorityCurrentTickSeq = ctx.seq;
		}
		return priorityCurrentTick;
	}

	void Node::Traverse(TraversalCallback& pre, TraversalCallback& post, Ptr<Node>& ptr)
	{
		pre(*this, ptr);
		post(*this, ptr);
	}

	Status Node::Tick(const Context& ctx)
	{
		auto b = GetNodeBlob();
		// First run of current round.
		if (!b->running)
			OnEnter(ctx);
		b->running = true;

		auto status = Update(ctx);
		b->lastStatus = status;
		b->lastSeq = ctx.seq;

		// Last run of current round.
		if (status == Status::FAILURE || status == Status::SUCCESS)
		{
			OnTerminate(ctx, status);
			b->running = false; // reset
		}
		return status;
	}

	////////////////////////////////////////////////
	/// Node > InternalNode > SingleNode
	////////////////////////////////////////////////

	void SingleNode::makeVisualizeString(std::string& s, int depth, ull seq)
	{
		Node::makeVisualizeString(s, depth, seq);
		if (child != nullptr)
		{
			s.push_back('\n');
			child->makeVisualizeString(s, depth + 1, seq);
		}
	}

	void SingleNode::Traverse(TraversalCallback& pre, TraversalCallback& post, Ptr<Node>& ptr)
	{
		pre(*this, ptr);
		if (child != nullptr)
			child->Traverse(pre, post, child);
		post(*this, ptr);
	}

	////////////////////////////////////////////////
	/// Node > InternalNode > CompositeNode
	////////////////////////////////////////////////

	void CompositeNode::makeVisualizeString(std::string& s, int depth, ull seq)
	{
		Node::makeVisualizeString(s, depth, seq);
		for (auto& child : children)
		{
			if (child != nullptr)
			{
				s.push_back('\n');
				child->makeVisualizeString(s, depth + 1, seq);
			}
		}
	}

	void CompositeNode::Traverse(TraversalCallback& pre, TraversalCallback& post, Ptr<Node>& ptr)
	{
		pre(*this, ptr);
		for (auto& child : children)
			if (child != nullptr)
				child->Traverse(pre, post, child);
		post(*this, ptr);
	}

	unsigned int CompositeNode::Priority(const Context& ctx) const
	{
		unsigned int ans = 0;
		for (int i = 0; i < children.size(); i++)
			if (considerable(i))
				ans = std::max(ans, children[i]->GetPriorityCurrentTick(ctx));
		return ans;
	}

	//////////////////////////////////////////////////////////////
	/// Node > InternalNode > CompositeNode > _Internal Impls
	///////////////////////////////////////////////////////////////

	void _InternalStatefulCompositeNode::OnTerminate(const Context& ctx, Status status)
	{
		auto& t = getNodeBlob<Blob>()->st;
		std::fill(t.begin(), t.end(), false);
	}

	void _InternalStatefulCompositeNode::OnBlobAllocated(NodeBlob* blob) const
	{
		auto ptr = static_cast<Blob*>(blob);
		ptr->st.resize(children.size(), false);
	}

	_MixedQueueHelper::_MixedQueueHelper(Cmp cmp, const std::size_t n)
		: use1(false)
	{
		// reserve capacity for q1.
		q1Container.reserve(n);
		q1 = &q1Container;
		// reserve capacity for q2.
		decltype(q2) _q2(cmp);
		q2.swap(_q2);
		q2.reserve(n);
	}

	int _MixedQueueHelper::pop()
	{
		if (use1)
			return (*q1)[q1Front++];
		int v = q2.top();
		q2.pop();
		return v;
	}

	void _MixedQueueHelper::push(int v)
	{
		if (use1)
		{
			if (q1 != &q1Container)
				throw std::runtime_error("bt: cant push on outside q1 container");
			return q1->push_back(v);
		}
		q2.push(v);
	}

	bool _MixedQueueHelper::empty() const
	{
		if (use1)
			return q1Front == q1->size();
		return q2.empty();
	}

	void _MixedQueueHelper::clear()
	{
		if (use1)
		{
			if (q1 != &q1Container)
				throw std::runtime_error("bt: cant clear on outside q1 container");
			q1->resize(0);
			q1Front = 0;
			return;
		}
		q2.clear();
	}

	void _MixedQueueHelper::setQ1Container(std::vector<int>* c)
	{
		q1 = c;
		q1Front = 0;
	}

	void _InternalPriorityCompositeNode::refresh(const Context& ctx)
	{
		areAllEqual = true;
		// v is the first valid priority value.
		unsigned int v = 0;

		for (int i = 0; i < children.size(); i++)
		{
			if (!considerable(i))
				continue;
			p[i] = children[i]->GetPriorityCurrentTick(ctx);
			if (!v)
				v = p[i];
			if (v != p[i])
				areAllEqual = false;
		}
	}

	void _InternalPriorityCompositeNode::enqueue()
	{
		// if all priorities are equal, use q1 O(N)
		// otherwise, use q2 O(n*logn)
		q.setflag(areAllEqual);

		// We have to consider all children, and all priorities are equal,
		// then, we should just use a pre-exist vector to avoid a O(n) copy to q1.
		if ((!isParatialConsidered()) && areAllEqual)
		{
			q.setQ1Container(&simpleQ1Container);
			return; // no need to perform enqueue
		}

		q.resetQ1Container();

		// Clear and enqueue.
		q.clear();
		for (int i = 0; i < children.size(); i++)
			if (considerable(i))
				q.push(i);
	}

	void _InternalPriorityCompositeNode::internalOnBuild()
	{
		CompositeNode::internalOnBuild();
		// pre-allocate capacity for p.
		p.resize(children.size());
		// initialize simpleQ1Container;
		for (int i = 0; i < children.size(); i++)
			simpleQ1Container.push_back(i);
		// Compare priorities between children, where a and b are indexes.
		// priority from large to smaller, so use `less`: pa < pb
		// order: from small to larger, so use `greater`: a > b
		auto cmp = [&](const int a, const int b) { return p[a] < p[b] || a > b; };
		q = _MixedQueueHelper(cmp, children.size());
	}

	Status _InternalPriorityCompositeNode::Update(const Context& ctx)
	{
		refresh(ctx);
		enqueue();
		// propagates ticks
		return update(ctx);
	}

	//////////////////////////////////////////////////////////////
	/// Node > InternalNode > CompositeNode > SequenceNode
	///////////////////////////////////////////////////////////////

	Status _InternalSequenceNodeBase::update(const Context& ctx)
	{
		// propagates ticks, one by one sequentially.
		while (!q.empty())
		{
			auto i = q.pop();
			auto status = children[i]->Tick(ctx);
			if (status == Status::RUNNING)
				return Status::RUNNING;
			// F if any child F.
			if (status == Status::FAILURE)
			{
				onChildFailure(i);
				return Status::FAILURE;
			}
			// S
			onChildSuccess(i);
		}
		// S if all children S.
		return Status::SUCCESS;
	}

	//////////////////////////////////////////////////////////////
	/// Node > InternalNode > CompositeNode > SelectorNode
	///////////////////////////////////////////////////////////////

	Status _InternalSelectorNodeBase::update(const Context& ctx)
	{
		// select a success children.
		while (!q.empty())
		{
			auto i = q.pop();
			auto status = children[i]->Tick(ctx);
			if (status == Status::RUNNING)
				return Status::RUNNING;
			// S if any child S.
			if (status == Status::SUCCESS)
			{
				onChildSuccess(i);
				return Status::SUCCESS;
			}
			// F
			onChildFailure(i);
		}
		// F if all children F.
		return Status::FAILURE;
	}

	//////////////////////////////////////////////////////////////
	/// Node > InternalNode > CompositeNode > RandomSelectorNode
	///////////////////////////////////////////////////////////////

	static std::mt19937 rng(std::random_device{}()); // seed random

	Status _InternalRandomSelectorNodeBase::Update(const Context& ctx)
	{
		refresh(ctx);
		// Sum of weights/priorities.
		unsigned int total = 0;
		for (int i = 0; i < children.size(); i++)
			if (considerable(i))
				total += p[i];

		// random select one, in range [1, total]
		std::uniform_int_distribution<unsigned int> distribution(1, total);

		auto select = [&]() -> int {
			unsigned int v = distribution(rng); // gen random unsigned int between [0, sum]
			unsigned int s = 0;					// sum of iterated children.
			for (int i = 0; i < children.size(); i++)
			{
				if (!considerable(i))
					continue;
				s += p[i];
				if (v <= s)
					return i;
			}
			return 0; // won't reach here.
		};

		// While still have children considerable.
		// total reaches 0 only if no children left,
		// notes that Priority() always returns a positive value.
		while (total)
		{
			int	 i = select();
			auto status = children[i]->Tick(ctx);
			if (status == Status::RUNNING)
				return Status::RUNNING;
			// S if any child S.
			if (status == Status::SUCCESS)
			{
				onChildSuccess(i);
				return Status::SUCCESS;
			}
			// Failure, it shouldn't be considered any more in this tick.
			onChildFailure(i);
			// remove its weight from total, won't be consider again.
			total -= p[i];
			// updates the upper bound of distribution.
			distribution.param(std::uniform_int_distribution<unsigned int>::param_type(1, total));
		}
		// F if all children F.
		return Status::FAILURE;
	}

	//////////////////////////////////////////////////////////////
	/// Node > InternalNode > CompositeNode > ParallelNode
	///////////////////////////////////////////////////////////////

	Status _InternalParallelNodeBase::update(const Context& ctx)
	{
		// Propagates tick to all considerable children.
		int cntFailure = 0, cntSuccess = 0, total = 0;
		while (!q.empty())
		{
			auto i = q.pop();
			auto status = children[i]->Tick(ctx);
			total++;
			if (status == Status::FAILURE)
			{
				cntFailure++;
				onChildFailure(i);
			}
			if (status == Status::SUCCESS)
			{
				cntSuccess++;
				onChildSuccess(i);
			}
		}

		// S if all children S.
		if (cntSuccess == total)
			return Status::SUCCESS;
		// F if any child F.
		if (cntFailure > 0)
			return Status::FAILURE;
		return Status::RUNNING;
	}

	//////////////////////////////////////////////////////////////
	/// Node > InternalNode > CompositeNode > Decorator
	///////////////////////////////////////////////////////////////

	Status InvertNode::Update(const Context& ctx)
	{
		auto status = child->Tick(ctx);
		switch (status)
		{
			case Status::RUNNING:
				return Status::RUNNING;
			case Status::FAILURE:
				return Status::SUCCESS;
			default:
				return Status::FAILURE;
		}
	}

	void ConditionalRunNode::Traverse(TraversalCallback& pre, TraversalCallback& post, Ptr<Node>& ptr)
	{
		pre(*this, ptr);
		if (condition != nullptr)
			condition->Traverse(pre, post, condition);
		if (child != nullptr)
			child->Traverse(pre, post, child);
		post(*this, ptr);
	}

	Status ConditionalRunNode::Update(const Context& ctx)
	{
		if (condition->Tick(ctx) == Status::SUCCESS)
			return child->Tick(ctx);
		return Status::FAILURE;
	}

	Status RepeatNode::Update(const Context& ctx)
	{
		if (n == 0)
			return Status::SUCCESS;
		auto status = child->Tick(ctx);
		if (status == Status::RUNNING)
			return Status::RUNNING;
		if (status == Status::FAILURE)
			return Status::FAILURE;
		// Count success until n times, -1 will never stop.
		if (++(getNodeBlob<Blob>()->cnt) == n)
			return Status::SUCCESS;
		// Otherwise, it's still running.
		return Status::RUNNING;
	}

	void TimeoutNode::OnEnter(const Context& ctx)
	{
		getNodeBlob<Blob>()->startAt = std::chrono::steady_clock::now();
	}

	Status TimeoutNode::Update(const Context& ctx)
	{
		// Check if timeout at first.
		auto now = std::chrono::steady_clock::now();
		if (now > getNodeBlob<Blob>()->startAt + duration)
			return Status::FAILURE;
		return child->Tick(ctx);
	}

	void DelayNode::OnEnter(const Context& ctx)
	{
		getNodeBlob<Blob>()->firstRunAt = std::chrono::steady_clock::now();
	}
	void DelayNode::OnTerminate(const Context& ctx, Status status)
	{
		getNodeBlob<Blob>()->firstRunAt = Timepoint::min();
	}

	Status DelayNode::Update(const Context& ctx)
	{
		auto now = std::chrono::steady_clock::now();
		if (now < getNodeBlob<Blob>()->firstRunAt + duration)
			return Status::RUNNING;
		return child->Tick(ctx);
	}

	void RetryNode::OnEnter(const Context& ctx)
	{
		auto b = getNodeBlob<Blob>();
		b->cnt = 0;
		b->lastRetryAt = Timepoint::min();
	}

	void RetryNode::OnTerminate(const Context& ctx, Status status)
	{
		auto b = getNodeBlob<Blob>();
		b->cnt = 0;
		b->lastRetryAt = status == Status::FAILURE ? std::chrono::steady_clock::now() : Timepoint::min();
	}

	Status RetryNode::Update(const Context& ctx)
	{
		auto b = getNodeBlob<Blob>();

		if (maxRetries != -1 && b->cnt > maxRetries)
			return Status::FAILURE;

		// If has failures before, and retry timepoint isn't arriving.
		auto now = std::chrono::steady_clock::now();
		if (b->cnt > 0 && now < b->lastRetryAt + interval)
			return Status::RUNNING;

		// Time to run/retry.
		auto status = child->Tick(ctx);
		switch (status)
		{
			case Status::RUNNING:
				[[fallthrough]];
			case Status::SUCCESS:
				return status;
			default:
				// Failure
				if (++b->cnt > maxRetries && maxRetries != -1)
					return Status::FAILURE; // exeeds max retries.
				return Status::RUNNING;		// continues retry
		}
	}

	Status ForceSuccessNode::Update(const Context& ctx)
	{
		return (child->Update(ctx) == Status::RUNNING) ? Status::RUNNING : Status::SUCCESS;
	}

	Status ForceFailureNode::Update(const Context& ctx)
	{
		return (child->Update(ctx) == Status::RUNNING) ? Status::RUNNING : Status::FAILURE;
	}

	//////////////////////////////////////////////////////////////
	/// Node > SingleNode > RootNode
	///////////////////////////////////////////////////////////////

	void RootNode::Visualize(ull seq)
	{
		// CSI[2J clears screen.
		// CSI[H moves the cursor to top-left corner
		printf("\x1B[2J\x1B[H");
		// Make a string.
		std::string s;
		makeVisualizeString(s, 0, seq);
		printf("%s", s.c_str());
	}

	void RootNode::TickForever(Context& ctx, std::chrono::nanoseconds interval, bool visualize,
		std::function<void(const Context&)> post)
	{
		auto lastTickAt = std::chrono::steady_clock::now();

		while (true)
		{
			auto nextTickAt = lastTickAt + interval;

			// Time delta between last tick and current tick.
			ctx.delta = std::chrono::steady_clock::now() - lastTickAt;
			++ctx.seq;
			Tick(ctx);
			if (post != nullptr)
				post(ctx);
			if (visualize)
				Visualize(ctx.seq);

			// Catch up with next tick.
			lastTickAt = std::chrono::steady_clock::now();
			if (lastTickAt < nextTickAt)
			{
				std::this_thread::sleep_for(nextTickAt - lastTickAt);
			}
		}
	}

	//////////////////////////////////////////////////////////////
	/// Tree Builder
	///////////////////////////////////////////////////////////////

	void _InternalBuilderBase::maintainNodeBindInfo(Node& node, RootNode* root)
	{
		node.root = root;
		root->n++;
		node.id = ++nextNodeId;
	}
	void _InternalBuilderBase::maintainSizeInfoOnRootBind(RootNode* root, std::size_t rootNodeSize,
		std::size_t blobSize)
	{
		root->size = rootNodeSize;
		root->treeSize += rootNodeSize;
		root->maxSizeNode = rootNodeSize;
		root->maxSizeNodeBlob = blobSize;
	}

	void _InternalBuilderBase::maintainSizeInfoOnSubtreeAttach(const RootNode& subtree, RootNode* root)
	{
		root->treeSize += subtree.treeSize;
		root->maxSizeNode = std::max(root->maxSizeNode, subtree.maxSizeNode);
		root->maxSizeNodeBlob = std::max(root->maxSizeNodeBlob, subtree.maxSizeNodeBlob);
	}

	void _InternalBuilderBase::onRootAttach(RootNode* root, std::size_t size, std::size_t blobSize)
	{
		maintainNodeBindInfo(*root, root);
		maintainSizeInfoOnRootBind(root, size, blobSize);
	}

	void _InternalBuilderBase::onSubtreeAttach(RootNode& subtree, RootNode* root)
	{
		// Resets root in sub tree recursively.
		TraversalCallback pre = [&](Node& node, Ptr<Node>& ptr) { maintainNodeBindInfo(node, root); };
		subtree.Traverse(pre, NullTraversalCallback, NullNodePtr);
		maintainSizeInfoOnSubtreeAttach(subtree, root);
	}

	void _InternalBuilderBase::onNodeBuild(Node* node)
	{
		node->internalOnBuild();
		node->OnBuild();
	}

	void _InternalBuilderBase::_maintainSizeInfoOnNodeAttach(Node& node, RootNode* root, std::size_t nodeSize,
		std::size_t nodeBlobSize)
	{
		node.size = nodeSize;
		root->treeSize += nodeSize;
		root->maxSizeNode = std::max(root->maxSizeNode, nodeSize);
		root->maxSizeNodeBlob = std::max(root->maxSizeNodeBlob, nodeBlobSize);
	}

	void _InternalBuilderBase::validate(const Node* node)
	{
		auto e = node->Validate();
		if (!e.empty())
		{
			std::string s = "bt build: ";
			s += node->Name();
			s += ' ';
			s += e;
			throw std::runtime_error(s);
		}
	}
	void _InternalBuilderBase::validateIndent()
	{
		if (level > stack.size())
		{
			const auto* node = stack.top();
			std::string s = "bt build: too much indent ";
			s += "below ";
			s += node->Name();
			throw std::runtime_error(s);
		}
	}

	void _InternalBuilderBase::pop()
	{
		validate(stack.top()); // validate before pop
		onNodeBuild(stack.top());
		stack.pop();
	}

	// Adjust stack to current indent level.
	void _InternalBuilderBase::adjust()
	{
		validateIndent();
		while (level < stack.size())
			pop();
	}
	void _InternalBuilderBase::attachLeafNode(Ptr<LeafNode> p)
	{
		adjust();
		// Append to stack's top as a child.
		onNodeBuild(p.get());
		stack.top()->Append(std::move(p));
		// resets level.
		level = 1;
	}

	void _InternalBuilderBase::attachInternalNode(Ptr<InternalNode> p)
	{
		adjust();
		// Append to stack's top as a child, and replace the top.
		auto parent = stack.top();
		stack.push(p.get()); // cppcheck-suppress danglingLifetime
		parent->Append(std::move(p));
		// resets level.
		level = 1;
	}

} // namespace bt
