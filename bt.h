// Copyright (c) 2024 Chao Wang <hit9@icloud.com>.
// License: BSD, Version: 0.4.3.  https://github.com/hit9/bt.cc
// A lightweight behavior tree library that separates data and behavior.

#ifndef HIT9_BT_H
#define HIT9_BT_H

#include <any>
#include <chrono>  // for milliseconds, steady_clock
#include <cstring> // for memset
#include <functional>
#include <memory> // for unique_ptr
#include <queue>  // for priority_queue
#include <stack>
#include <stdexcept> // for runtime_error
#include <string>
#include <string_view>
#include <type_traits> // for is_base_of_v
#include <utility>	   // for pair
#include <vector>

namespace bt
{
	enum class Status
	{
		UNDEFINED = 0,
		RUNNING = 1,
		SUCCESS = 2,
		FAILURE = 3
	};

	using ull = unsigned long long;

	// Node instance's id type.
	using NodeId = unsigned int;

	// Tick/Update's Context.
	struct Context
	{
		ull						 seq = 0; // Tick seq number.
		std::chrono::nanoseconds delta;	  // Delta time since last tick.
		// User data.
		// For instance, it could hold a shared_ptr to a blackboard.
		// Code example::
		//   bt::Context ctx{.data = std::make_shared<Blackboard>()};
		std::any data;

		// Constructors.
		Context() = default;
		explicit Context(std::any data)
			: data(data), seq(0) {}
	};

	////////////////////////////
	/// TreeBlob
	////////////////////////////

	// NodeBlob is the base class to store node's internal entity-related data and states.
	struct NodeBlob
	{
		bool   running = false;				   // is still running?
		Status lastStatus = Status::UNDEFINED; // status of last execution.
		ull	   lastSeq = 0;					   // seq of last execution.
	};

	// Concept TNodeBlob for all classes derived from NodeBlob.
	template <typename T>
	concept TNodeBlob = std::is_base_of_v<NodeBlob, T>;

	// ITreeBlob is an internal interface base class for FixedTreeBlob and DynamicTreeBlob.
	// A TreeBlob stores the entity-related states data for all nodes in a tree.
	// One tree blob for one entity.
	class ITreeBlob
	{
	protected:
		// allocates memory for given index, returns the pointer to the node blob.
		virtual void* allocate(const std::size_t idx, const std::size_t size) = 0;
		// returns true if an index already allocated memory.
		virtual bool exist(const std::size_t idx) = 0;
		// get the blob's pointer for the given index.
		// returns nullptr if not exist.
		virtual void* get(const std::size_t idx) = 0;
		// pre-reserve enough capacity if need.
		virtual void reserve(const std::size_t cap) {}

	public:
		// virtual destructor is required for unique_ptr.
		// this also disables move, but we don't declare default move constructor generation here, since there's no
		// member stored here.
		virtual ~ITreeBlob() = default;

		// Returns a pointer to given NodeBlob B for the node with given id.
		// Allocates if not exist.
		// Parameter cb is an optional function to be called after the blob is first allocated.
		template <TNodeBlob B>
		B* Make(const NodeId id, const std::function<void(NodeBlob*)>& cb, const std::size_t cap = 0)
		{
			auto [p, b] = make(id, sizeof(B), cap);
			if (!b)
				return static_cast<B*>(p);
			auto q = new (p) B(); // call constructor
			if (cb != nullptr)
				cb(q);
			return q;
		}

	private:
		std::pair<void*, bool> make(const NodeId id, size_t size, const std::size_t cap = 0);
	};

	// FixedTreeBlob is just a continuous buffer, implements ITreeBlob.
	template <std::size_t NumNodes, std::size_t MaxSizeNodeBlob>
	class FixedTreeBlob final : public ITreeBlob
	{
	private:
		unsigned char buf[NumNodes][MaxSizeNodeBlob + 1];

	protected:
		void* allocate(const std::size_t idx, const std::size_t size) override
		{
			if (idx >= NumNodes)
				throw std::runtime_error("bt: FixedTreeBlob NumNodes not enough");
			if (size > MaxSizeNodeBlob)
				throw std::runtime_error("bt: FixedTreeBlob MaxSizeNodeBlob not enough");
			buf[idx][0] = true;
			return get(idx);
		}
		bool  exist(const std::size_t idx) override { return static_cast<bool>(buf[idx][0]); }
		void* get(const std::size_t idx) override { return &buf[idx][1]; }

	public:
		FixedTreeBlob() { memset(buf, 0, sizeof(buf)); }
	};

	// DynamicTreeBlob contains dynamic allocated unique pointers, implements ITreeBlob.
	class DynamicTreeBlob final : public ITreeBlob
	{
	private:
		std::vector<std::unique_ptr<unsigned char[]>> m; // index => blob pointer.
		std::vector<bool>							  e; // index => exist, dynamic

	protected:
		void* allocate(const std::size_t idx, const std::size_t size) override;
		bool  exist(const std::size_t idx) override { return e.size() > idx && e[idx]; }
		void* get(const std::size_t idx) override { return m[idx].get(); }
		void  reserve(const std::size_t cap) override;

	public:
		DynamicTreeBlob() {}
	};

	////////////////////////////
	/// Node
	////////////////////////////

	// RootNode Interface.
	class IRootNode
	{
	public:
		// Returns the current binding TreeBlob's pointer.
		virtual ITreeBlob* GetTreeBlob(void) const = 0;
		// Returns the total number of nodes built on this tree.
		virtual int NumNodes() const = 0;
	};

	class Node; // forward declaration.

	// Alias
	template <typename T>
	using Ptr = std::unique_ptr<T>;

	static Ptr<Node> NullNodePtr = nullptr;

	template <typename T>
	using PtrList = std::vector<Ptr<T>>;

	// Type of the callback function for node traversal.
	// Parameters: current walking node and its unique pointer from parent (NullNodePtr for root).
	using TraversalCallback = std::function<void(Node& currentNode, Ptr<Node>& currentNodePtr)>;
	static TraversalCallback NullTraversalCallback = [](Node&, Ptr<Node>&) {};

	// The most base class of all behavior nodes.
	class Node
	{
	private:
		std::string name;
		// cache priority for current tick.
		unsigned int priorityCurrentTick = 0;
		// tick seq when the priority cache was set.
		ull priorityCurrentTickSeq = 0;
		// holding a pointer to the root.
		IRootNode* root = nullptr;
		// size of this node, available after tree built.
		std::size_t size = 0;

		// friend with _InternalBuilderBase to access member root, size and id etc.
		friend class _InternalBuilderBase;

	protected:
		NodeId id = 0;

		// Internal helper method to return the raw pointer to the node blob.
		template <TNodeBlob B>
		B* getNodeBlob() const
		{
			const auto cb = [&](NodeBlob* blob) { OnBlobAllocated(blob); };
			return root->GetTreeBlob()->Make<B>(id, cb, root->NumNodes()); // get or alloc
		}
		// Internal method to visualize tree.
		virtual void makeVisualizeString(std::string& s, int depth, ull seq);
		// Internal onBuild method.
		// Separating from the public hook api OnBuild, so there's no need to call parent
		// class's OnBuild for custom OnBuild overridings.
		virtual void internalOnBuild() {}

		// firend with SingleNode and CompositeNode for accessbility to makeVisualizeString.
		friend class SingleNode;
		friend class CompositeNode;

	public:
		// Every stateful Node class should declare its own Blob type member.
		using Blob = NodeBlob;

		explicit Node(std::string_view name = "Node")
			: name(name) {}
		// Destructor is required by unique_ptr.
		// And this disabled default generation for move constructors.
		virtual ~Node() = default;
		// We have to declare move constructor and assignment methods generation explicitly.
		// So that the sub classes will support fully move semantics.
		Node(Node&&) noexcept = default;			// move constructor
		Node& operator=(Node&&) noexcept = default; // move assignment operator

		// Simple Getters
		// ~~~~~~~~~~~~~~

		// Returns the id of this node.
		NodeId Id() const { return id; }
		// Returns the size of this node, available after tree built.
		std::size_t Size() const { return size; }
		// Returns the name of this node.
		virtual std::string_view Name() const { return name; }
		// Returns last status of this node.
		bt::Status LastStatus() const { return GetNodeBlob()->lastStatus; }
		// Helps to access the node blob's pointer, which stores the entity-related data and states.
		// Any Node has a NodeBlob class should override this.
		virtual NodeBlob* GetNodeBlob() const { return getNodeBlob<NodeBlob>(); }
		// Internal method to query priority of this node in current tick.
		unsigned int GetPriorityCurrentTick(const Context& ctx);

		// API
		// ~~~

		// Traverse the subtree of the current node recursively and execute the given function callback functions.
		// The callback function pre will be called pre-order, and the post will be called post-order.
		// Pass NullTraversalCallback for empty callbacks.
		virtual void Traverse(TraversalCallback& pre, TraversalCallback& post, Ptr<Node>& ptr);
		// Main entry function, should be called on every tick.
		Status Tick(const Context& ctx);

		// Public Virtual Functions To Override
		// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

		// Hook function to be called on this node's first run.
		// Nice to call parent class' OnEnter before your implementation.
		virtual void OnEnter(const Context& ctx) {};
		// Hook function to be called once this node goes into success or failure.
		// Nice to call parent class' OnTerminate after your implementation
		virtual void OnTerminate(const Context& ctx, Status status) {};
		// Hook function to be called on this node's build is finished.
		// Nice to call parent class' OnBuild after your implementation
		virtual void OnBuild() {}
		// Main update function to be implemented by all subclasses.
		// It's the body part of function Tick().
		virtual Status Update(const Context& ctx) { return Status::SUCCESS; };
		// Returns the priority of this node, should be strictly larger than 0, the larger the higher.
		// By default, all nodes' priorities are equal, to 1.
		// Providing this method is primarily for selecting children by dynamic priorities.
		// It's recommended to implement this function fast enough, since it will be called **at most once**
		// on each tick. For instance, we may not need to do the calculation on every tick if it's complex.
		// Another optimization is to separate calculation from getter, for example, pre-cache the result
		// somewhere on the blackboard, and just ask it from memory here.
		virtual unsigned int Priority(const Context& ctx) const { return 1; }
		// Hook function to be called on a blob's first allocation.
		virtual void OnBlobAllocated(NodeBlob* blob) const {}
		// Validate whether the node is builded correctly.
		// Returns error message, empty string for good.
		virtual std::string_view Validate() const { return ""; }
	};

	// Concept TNode for all classes derived from Node.
	template <typename T>
	concept TNode = std::is_base_of_v<Node, T>;

	////////////////////////////
	/// Node > LeafNode
	////////////////////////////

	// LeafNode is a class contains no children.
	class LeafNode : public Node
	{
	public:
		explicit LeafNode(std::string_view name = "LeafNode")
			: Node(name) {}
	};

	// Concept TLeafNode for all classes derived from LeafNode.
	template <typename T>
	concept TLeafNode = std::is_base_of_v<LeafNode, T>;

	////////////////////////////////////
	/// Node > LeafNode > ConditionNode
	/////////////////////////////////////

	// ConditionNode succeeds only if Check() returns true, it never returns RUNNING.
	// Note that checker should be independent with any entities's stateful data, it's stored on the tree.
	class ConditionNode : public LeafNode
	{
	public:
		using Checker = std::function<bool(const Context&)>;
		explicit ConditionNode(Checker checker = nullptr, std::string_view name = "Condition")
			: LeafNode(name), checker(checker) {}
		Status Update(const Context& ctx) override { return Check(ctx) ? Status::SUCCESS : Status::FAILURE; }

		// Check if condition is satisfied.
		virtual bool Check(const Context& ctx) { return checker != nullptr && checker(ctx); };

	private:
		Checker checker = nullptr;
	};
	using Condition = ConditionNode; // alias

	// Concept TCondition for all classes derived from Condition.
	template <typename T>
	concept TCondition = std::is_base_of_v<ConditionNode, T>;

	////////////////////////////////////
	/// Node > LeafNode > ActionNode
	/////////////////////////////////////

	// ActionNode contains no children, it just runs a task.
	class ActionNode : public LeafNode
	{
	public:
		explicit ActionNode(std::string_view name = "Action")
			: LeafNode(name) {}

		// Subclasses must implement function Update().
	};
	using Action = ActionNode; // alias

	// Concept TAction for all classes derived from Action.
	template <typename T>
	concept TAction = std::is_base_of_v<ActionNode, T>;

	////////////////////////////
	/// Node > InternalNode
	////////////////////////////

	// InternalNode can have children nodes.
	class InternalNode : public Node
	{
	public:
		InternalNode(std::string_view name = "InternalNode")
			: Node(name) {}
		// Append a child to this node.
		virtual void Append(Ptr<Node> node) = 0;
	};

	template <typename T>
	concept TInternalNode = std::is_base_of_v<InternalNode, T>;

	////////////////////////////////////////////////
	/// Node > InternalNode > SingleNode
	////////////////////////////////////////////////

	// SingleNode contains exactly a single child.
	class SingleNode : public InternalNode
	{
	protected:
		Ptr<Node> child;

		void makeVisualizeString(std::string& s, int depth, ull seq) override;

	public:
		explicit SingleNode(std::string_view name = "SingleNode", Ptr<Node> child = nullptr)
			: InternalNode(name), child(std::move(child)) {}
		void			 Traverse(TraversalCallback& pre, TraversalCallback& post, Ptr<Node>& ptr) override;
		std::string_view Validate() const override { return child == nullptr ? "no child node provided" : ""; }
		void			 Append(Ptr<Node> node) override { child = std::move(node); }
		unsigned int	 Priority(const Context& ctx) const override { return child->GetPriorityCurrentTick(ctx); }
	};

	////////////////////////////////////////////////
	/// Node > InternalNode > CompositeNode
	////////////////////////////////////////////////

	// CompositeNode contains multiple children.
	class CompositeNode : public InternalNode
	{
	protected:
		PtrList<Node> children;

		// Should we consider partial children (not all) every tick?
		virtual bool isParatialConsidered() const { return false; }
		// Should we consider i'th child during this round?
		virtual bool considerable(int i) const { return true; }
		// Internal hook function to be called after a child goes success.
		virtual void onChildSuccess(const int i) {};
		// Internal hook function to be called after a child goes failure.
		virtual void onChildFailure(const int i) {};

		void makeVisualizeString(std::string& s, int depth, ull seq) override;

	public:
		explicit CompositeNode(std::string_view name = "CompositeNode", PtrList<Node>&& cs = {})
			: InternalNode(name)
		{
			children.swap(cs);
		}
		void			 Traverse(TraversalCallback& pre, TraversalCallback& post, Ptr<Node>& ptr) override;
		void			 Append(Ptr<Node> node) override { children.push_back(std::move(node)); }
		std::string_view Validate() const override { return children.empty() ? "children empty" : ""; }
		// Returns the max priority of considerable children.
		unsigned int Priority(const Context& ctx) const final override;
	};

	//////////////////////////////////////////////////////////////
	/// Node > InternalNode > CompositeNode > _Internal Impls
	///////////////////////////////////////////////////////////////

	// Always skip children that already succeeded or failure during current round.
	class _InternalStatefulCompositeNode : virtual public CompositeNode
	{
	protected:
		bool isParatialConsidered() const override { return true; }
		bool considerable(int i) const override { return !(getNodeBlob<Blob>()->st[i]); }
		void skip(const int i) { getNodeBlob<Blob>()->st[i] = true; }

	public:
		struct Blob : NodeBlob
		{
			// st[i] => should we skip considering children at index i ?
			std::vector<bool> st;
		};

		NodeBlob* GetNodeBlob() const override { return getNodeBlob<Blob>(); }
		void	  OnTerminate(const Context& ctx, Status status) override;
		void	  OnBlobAllocated(NodeBlob* blob) const override;
	};

	// _MixedQueueHelper is a helper queue wrapper for _InternalPriorityCompositeNode .
	// It decides which underlying queue to use for current tick. Wraps a simple queue
	// and priority queue.
	class _MixedQueueHelper
	{
		using Cmp = std::function<bool(const int, const int)>;

		// hacking a private priority_queue for the stl missing `reserve` and `clear` method.
		template <typename T, typename Container = std::vector<T>>
		class _priority_queue : public std::priority_queue<T, Container, Cmp>
		{
		public:
			_priority_queue()
				: std::priority_queue<T, Container, Cmp>() {}
			explicit _priority_queue(Cmp cmp)
				: std::priority_queue<T, Container, Cmp>(cmp) {}
			void reserve(std::size_t n) { this->c.reserve(n); }
			void clear() { this->c.clear(); }
		};

	private:
		// use a pre-allocated vector instead of a std::queue, q1 will be pushed all and then poped all,
		// so a simple vector is enough, and neither needs a circular queue.
		// And here we use a pointer, allowing temporarily replace q1's container from outside existing container.
		std::vector<int>*					   q1;
		std::vector<int>					   q1Container;
		int									   q1Front = 0;
		_priority_queue<int, std::vector<int>> q2;
		bool								   use1; // using q1? otherwise q2

	public:
		_MixedQueueHelper() {}
		_MixedQueueHelper(Cmp cmp, const std::size_t n);
		void setflag(bool u1) { use1 = u1; }
		int	 pop();
		void push(int v);
		bool empty() const;
		void clear();
		void setQ1Container(std::vector<int>* c);
		void resetQ1Container(void) { q1 = &q1Container; }
	};

	// Priority related CompositeNode.
	class _InternalPriorityCompositeNode : virtual public CompositeNode
	{
	protected:
		// Prepare priorities of considerable children on every tick.
		// p[i] stands for i'th child's priority.
		// Since p will be refreshed on each tick, so it's stateless.
		std::vector<unsigned int> p;
		// q contains a simple queue and a simple queue, depending on:
		// if priorities of considerable children are all equal in this tick.
		_MixedQueueHelper q;
		// Are all priorities of considerable children equal on this tick?
		// Refreshed by function refresh on every tick.
		bool areAllEqual = false;
		// simpleQ1Container contains [0...n-1]
		// Used as a temp container for q1 for "non-stateful && non-priorities" compositors.
		std::vector<int> simpleQ1Container;

		// Explains that: p and q are still stateless with entities, reason:
		// p and q are consistent with respect to "which child nodes to consider".
		// If we change the blob binding, the new tick won't be affected by previous blob.

		void internalOnBuild() override;
		// Refresh priorities for considerable children.
		void refresh(const Context& ctx);
		// Enqueue considerable children.
		void enqueue();
		// update is an internal method to propagates tick() to children in the q1/q2.
		// it will be called by Update.
		virtual Status update(const Context& ctx) { return bt::Status::UNDEFINED; }

	public:
		_InternalPriorityCompositeNode() {}
		Status Update(const Context& ctx) override;
	};

	//////////////////////////////////////////////////////////////
	/// Node > InternalNode > CompositeNode > SequenceNode
	///////////////////////////////////////////////////////////////

	class _InternalSequenceNodeBase : virtual public _InternalPriorityCompositeNode
	{
	protected:
		Status update(const Context& ctx) override;
	};

	// SequenceNode runs children one by one, and succeeds only if all children succeed.
	class SequenceNode final : public _InternalSequenceNodeBase
	{
	public:
		explicit SequenceNode(std::string_view name = "Sequence", PtrList<Node>&& cs = {})
			: CompositeNode(name, std::move(cs)), _InternalPriorityCompositeNode() {}
	};

	// StatefulSequenceNode behaves like a SequenceNode, but instead of ticking children from the first, it
	// starts from the running child instead.
	class StatefulSequenceNode final : public _InternalStatefulCompositeNode, public _InternalSequenceNodeBase
	{
	protected:
		void onChildSuccess(const int i) override { skip(i); }

	public:
		explicit StatefulSequenceNode(std::string_view name = "Sequence*", PtrList<Node>&& cs = {})
			: CompositeNode(name, std::move(cs)), _InternalPriorityCompositeNode() {}
	};

	//////////////////////////////////////////////////////////////
	/// Node > InternalNode > CompositeNode > SelectorNode
	///////////////////////////////////////////////////////////////

	class _InternalSelectorNodeBase : virtual public _InternalPriorityCompositeNode
	{
	protected:
		Status update(const Context& ctx) override;
	};

	// SelectorNode succeeds if any child succeeds.
	class SelectorNode final : public _InternalSelectorNodeBase
	{
	public:
		explicit SelectorNode(std::string_view name = "Selector", PtrList<Node>&& cs = {})
			: CompositeNode(name, std::move(cs)), _InternalPriorityCompositeNode() {}
	};

	// StatefulSelectorNode behaves like a SelectorNode, but instead of ticking children from the first, it
	// starts from the running child instead.
	class StatefulSelectorNode : public _InternalStatefulCompositeNode, public _InternalSelectorNodeBase
	{
	protected:
		void onChildFailure(const int i) override { skip(i); }

	public:
		explicit StatefulSelectorNode(std::string_view name = "Selector*", PtrList<Node>&& cs = {})
			: CompositeNode(name, std::move(cs)), _InternalPriorityCompositeNode() {}
	};

	//////////////////////////////////////////////////////////////
	/// Node > InternalNode > CompositeNode > RandomSelectorNode
	///////////////////////////////////////////////////////////////

	// Weighted random selector.
	class _InternalRandomSelectorNodeBase : virtual public _InternalPriorityCompositeNode
	{
	public:
		Status Update(const Context& ctx) override;
	};

	// RandomSelectorNode selects children via weighted random selection.
	class RandomSelectorNode final : public _InternalRandomSelectorNodeBase
	{
	public:
		explicit RandomSelectorNode(std::string_view name = "RandomSelector", PtrList<Node>&& cs = {})
			: CompositeNode(name, std::move(cs)), _InternalPriorityCompositeNode() {}
	};

	// StatefulRandomSelectorNode behaves like RandomSelectorNode.
	// But it won't reconsider failure children during a round.
	class StatefulRandomSelectorNode final : virtual public _InternalStatefulCompositeNode, virtual public _InternalRandomSelectorNodeBase
	{
	protected:
		void onChildFailure(const int i) override { skip(i); }

	public:
		explicit StatefulRandomSelectorNode(std::string_view name = "RandomSelector*", PtrList<Node>&& cs = {})
			: CompositeNode(name, std::move(cs)), _InternalPriorityCompositeNode() {}
	};

	//////////////////////////////////////////////////////////////
	/// Node > InternalNode > CompositeNode > ParallelNode
	///////////////////////////////////////////////////////////////

	class _InternalParallelNodeBase : virtual public _InternalPriorityCompositeNode
	{
	protected:
		Status update(const Context& ctx) override;
	};

	// ParallelNode succeeds if all children succeed but runs all children
	// parallelly.
	class ParallelNode final : public _InternalParallelNodeBase
	{
	public:
		explicit ParallelNode(std::string_view name = "Parallel", PtrList<Node>&& cs = {})
			: CompositeNode(name, std::move(cs)), _InternalPriorityCompositeNode() {}
	};

	// StatefulParallelNode behaves like a ParallelNode, but instead of ticking every child, it only tick the
	// running children, aka skipping the succeeded children..
	class StatefulParallelNode final : public _InternalStatefulCompositeNode, public _InternalParallelNodeBase
	{
	protected:
		void onChildSuccess(const int i) override { skip(i); }

	public:
		explicit StatefulParallelNode(std::string_view name = "Parallel*", PtrList<Node>&& cs = {})
			: CompositeNode(name, std::move(cs)), _InternalPriorityCompositeNode() {}
	};

	//////////////////////////////////////////////////////////////
	/// Node > InternalNode > CompositeNode > Decorator
	///////////////////////////////////////////////////////////////

	// DecoratorNode decorates a single child node.
	class DecoratorNode : public SingleNode
	{
	public:
		explicit DecoratorNode(std::string_view name = "Decorator", Ptr<Node> child = nullptr)
			: SingleNode(name, std::move(child)) {}

		// To create a custom DecoratorNode: https://github.com/hit9/bt.cc#custom-decorator
		// You should derive from DecoratorNode and override the function Update.
		// Status Update(const Context&) override;
	};

	// InvertNode inverts its child's status.
	class InvertNode : public DecoratorNode
	{
	public:
		explicit InvertNode(std::string_view name = "Invert", Ptr<Node> child = nullptr)
			: DecoratorNode(name, std::move(child)) {}
		Status Update(const Context& ctx) override;
	};

	// ConditionalRunNode executes its child if given condition returns true.
	class ConditionalRunNode : public DecoratorNode
	{
	private:
		// Condition node to check.
		Ptr<Node> condition;

	public:
		explicit ConditionalRunNode(Ptr<ConditionNode> condition = nullptr,
			std::string_view name = "ConditionalRun", Ptr<Node> child = nullptr)
			: DecoratorNode(std::string(name) + '<' + std::string(condition->Name()) + '>', std::move(child)), condition(std::move(condition)) {}
		void   Traverse(TraversalCallback& pre, TraversalCallback& post, Ptr<Node>& ptr) override;
		Status Update(const Context& ctx) override;
	};

	// RepeatNode repeats its child for exactly n times.
	// Fails immediately if its child fails.
	class RepeatNode : public DecoratorNode
	{
	protected:
		// Times to repeat, -1 for forever, 0 for immediately success.
		int n;

	public:
		struct Blob : NodeBlob
		{
			int cnt = 0; // how many times of execution in this round.
		};
		explicit RepeatNode(int n, std::string_view name = "Repeat", Ptr<Node> child = nullptr)
			: DecoratorNode(name, std::move(child)), n(n) {}

		NodeBlob* GetNodeBlob() const override { return getNodeBlob<Blob>(); }
		// Clears counter on enter.
		void OnEnter(const Context& ctx) override { getNodeBlob<Blob>()->cnt = 0; }
		// Reset counter on termination.
		void   OnTerminate(const Context& ctx, Status status) override { getNodeBlob<Blob>()->cnt = 0; }
		Status Update(const Context& ctx) override;
	};

	using Timepoint = std::chrono::time_point<std::chrono::steady_clock>;

	// Timeout runs its child for at most given duration, fails on timeout.
	class TimeoutNode : public DecoratorNode
	{
	protected:
		std::chrono::milliseconds duration;

	public:
		struct Blob : NodeBlob
		{
			Timepoint startAt; // timepoint when this node starts.
		};
		explicit TimeoutNode(std::chrono::milliseconds d, std::string_view name = "Timeout",
			Ptr<Node> child = nullptr)
			: DecoratorNode(name, std::move(child)), duration(d) {}
		NodeBlob* GetNodeBlob() const override { return getNodeBlob<Blob>(); }
		void	  OnEnter(const Context& ctx) override;
		Status	  Update(const Context& ctx) override;
	};

	// DelayNode runs its child node after given duration.
	class DelayNode : public DecoratorNode
	{
	protected:
		// Duration to wait.
		std::chrono::milliseconds duration;

	public:
		struct Blob : NodeBlob
		{
			Timepoint firstRunAt; // timepoint this node first run.
		};
		explicit DelayNode(std::chrono::milliseconds duration, std::string_view name = "Delay",
			Ptr<Node> c = nullptr)
			: DecoratorNode(name, std::move(c)), duration(duration) {}

		NodeBlob* GetNodeBlob() const override { return getNodeBlob<Blob>(); }
		void	  OnEnter(const Context& ctx) override;
		void	  OnTerminate(const Context& ctx, Status status) override;
		Status	  Update(const Context& ctx) override;
	};

	// RetryNode retries its child node on failure.
	class RetryNode : public DecoratorNode
	{
	protected:
		// Max retry times, -1 for unlimited.
		int maxRetries = -1;
		// Retry interval in milliseconds
		std::chrono::milliseconds interval;

	public:
		struct Blob : NodeBlob
		{
			int		  cnt = 0;	   // Times already retried.
			Timepoint lastRetryAt; // Timepoint last retried at.
		};
		RetryNode(int maxRetries, std::chrono::milliseconds interval, std::string_view name = "Retry",
			Ptr<Node> child = nullptr)
			: DecoratorNode(name, std::move(child)), maxRetries(maxRetries), interval(interval) {}

		NodeBlob* GetNodeBlob() const override { return getNodeBlob<Blob>(); }
		void	  OnEnter(const Context& ctx) override;
		void	  OnTerminate(const Context& ctx, Status status) override;
		Status	  Update(const Context& ctx) override;
	};

	// ForceSuccessNode returns RUNNING if the decorated node is RUNNING, else always SUCCESS.
	class ForceSuccessNode : public DecoratorNode
	{
	public:
		ForceSuccessNode(std::string_view name = "ForceSuccess", Ptr<Node> child = nullptr)
			: DecoratorNode(name, std::move(child)) {}
		Status Update(const Context& ctx) override;
	};

	// ForceFailureNode returns Failure if the decorated node is RUNNING, else always FAILURE.
	class ForceFailureNode : public DecoratorNode
	{
	public:
		ForceFailureNode(std::string_view name = "ForceFailure", Ptr<Node> child = nullptr)
			: DecoratorNode(name, std::move(child)) {}
		Status Update(const Context& ctx) override;
	};

	//////////////////////////////////////////////////////////////
	/// Node > SingleNode > RootNode
	///////////////////////////////////////////////////////////////

	// RootNode is a SingleNode.
	class RootNode : public SingleNode, public IRootNode
	{
	protected:
		// Current binding tree blob.
		ITreeBlob* blob = nullptr;
		// Number of nodes on this tree, including the root itself.
		int n = 0;
		// Size of this tree.
		std::size_t treeSize = 0;
		// MaxSizeNode is the max size of tree node.
		std::size_t maxSizeNode = 0;
		// MaxSizeNodeBlob is the max size of tree node blobs.
		std::size_t maxSizeNodeBlob = 0;

		friend class _InternalBuilderBase; // for access to n, treeSize, maxSizeNode, maxSizeNodeBlob;

	public:
		explicit RootNode(std::string_view name = "Root")
			: SingleNode(name) {}
		Status Update(const Context& ctx) override { return child->Tick(ctx); }

		// Visualize the tree to console.
		void Visualize(ull seq);
		// Handy function to run tick loop forever.
		// Parameter interval specifies the time interval between ticks.
		// Parameter visualize enables debugging visualization on the console.
		// Parameter is a hook to be called after each tick.
		void TickForever(Context& ctx, std::chrono::nanoseconds interval, bool visualize = false,
			std::function<void(const Context&)> post = nullptr);

		/// Blob Apis
		/// ~~~~~~~~~

		// Binds a tree blob.
		void BindTreeBlob(ITreeBlob& b) { blob = &b; }
		// Returns current tree blob.
		ITreeBlob* GetTreeBlob(void) const override { return blob; }
		// Unbind current tree blob.
		void UnbindTreeBlob() { blob = nullptr; }

		/// Size Info
		/// ~~~~~~~~~

		// Returns the total number of nodes in this tree.
		// Available once the tree is built.
		int NumNodes() const override final { return n; }
		// Returns the total size of the node classes in this tree.
		// Available once the tree is built.
		std::size_t TreeSize() const { return treeSize; }
		// Returns the max size of the node class in this tree.
		// Available once the tree is built.
		std::size_t MaxSizeNode() const { return maxSizeNode; }
		// Returns the max size of the node blob struct for this tree.
		// Available once the tree is built.
		std::size_t MaxSizeNodeBlob() const { return maxSizeNodeBlob; }
	};

	//////////////////////////////////////////////////////////////
	/// Tree Builder
	///////////////////////////////////////////////////////////////

	class _InternalBuilderBase
	{
	private:
		// Node id incrementer for a tree.
		// unique inside this builder instance.
		NodeId nextNodeId = 0;

		void maintainNodeBindInfo(Node& node, RootNode* root);
		void maintainSizeInfoOnRootBind(RootNode* root, std::size_t rootNodeSize, std::size_t blobSize);
		void _maintainSizeInfoOnNodeAttach(Node& node, RootNode* root, std::size_t nodeSize,
			std::size_t nodeBlobSize);
		template <TNode T>
		void maintainSizeInfoOnNodeAttach(T& node, RootNode* root)
		{
			_maintainSizeInfoOnNodeAttach(node, root, sizeof(T), sizeof(typename T::Blob));
		}
		void maintainSizeInfoOnSubtreeAttach(const RootNode& subtree, RootNode* root);

	protected:
		std::stack<InternalNode*> stack;
		// indent level to insert new node, starts from 1.
		int		  level = 1;
		RootNode* root = nullptr;

		_InternalBuilderBase()
			: level(1) {}

		template <TNode T>
		void onNodeAttach(T& node, RootNode* root)
		{
			maintainNodeBindInfo(node, root);
			maintainSizeInfoOnNodeAttach<T>(node, root);
		}
		void onRootAttach(RootNode* root, std::size_t size, std::size_t blobSize);
		void onSubtreeAttach(RootNode& subtree, RootNode* root);
		void onNodeBuild(Node* node);

		// Validate node.
		void validate(const Node* node);
		// Validate indent level.
		void validateIndent();
		// Pops an internal node from the stack.
		void pop();
		// Adjust stack to current indent level.
		void adjust();

		void attachLeafNode(Ptr<LeafNode> p);
		void attachInternalNode(Ptr<InternalNode> p);
	};

	// Builder helps to build a tree.
	// The template parameter D is the derived class, a behavior tree class.
	template <typename D = void>
	class Builder : public _InternalBuilderBase
	{
	protected:
		// Bind a tree root onto this builder.
		void bindRoot(RootNode& r)
		{
			stack.push(&r);
			root = &r;
			onRootAttach(root, sizeof(D), sizeof(typename D::Blob));
		}
		// Creates a leaf node.
		auto& attachLeafNode(Ptr<LeafNode> p)
		{
			_InternalBuilderBase::attachLeafNode(std::move(p));
			return *static_cast<D*>(this);
		}
		// Creates an internal node with optional children.
		auto& attachInternalNode(Ptr<InternalNode> p)
		{
			_InternalBuilderBase::attachInternalNode(std::move(p));
			return *static_cast<D*>(this);
		}
		// make a new node onto this tree, returns the unique_ptr.
		// Any node creation should use this function.
		template <TNode T, typename... Args>
		Ptr<T> make(bool skipActtach, Args... args)
		{
			auto p = std::make_unique<T>(std::forward<Args>(args)...);
			if (!skipActtach)
				onNodeAttach<T>(*p, root);
			return p;
		};

	public:
		Builder()
			: _InternalBuilderBase() {} // cppcheck-suppress uninitMemberVar

		// Should be called on the end of the build process.
		void End()
		{
			while (stack.size())
				pop(); // clears the stack
		}
		// Increases indent level to append node.
		auto& _()
		{
			level++;
			return static_cast<D&>(*this);
		}

		// General creators
		// ~~~~~~~~~~~~~~~~

		// C is a function to attach an arbitrary new Node.
		// It can be used to attach custom node implementation.
		// Code exapmle::
		//    root
		//    .C<MyCustomDecoratorNode>()
		//    ._().Action<A>()
		//    .End();
		template <TNode T, typename... Args>
		auto& C(Args... args)
		{
			if constexpr (std::is_base_of_v<LeafNode, T>) // LeafNode
				return attachLeafNode(make<T>(false, std::forward<Args>(args)...));
			else // InternalNode.
				return attachInternalNode(make<T>(false, std::forward<Args>(args)...));
		}
		// Attach a node through move, rarely used.
		// The inst should be a node object that supports move semantics.
		template <TNode T>
		auto& M(T&& inst)
		{
			if constexpr (std::is_base_of_v<LeafNode, T>) // LeafNode
				return attachLeafNode(make<T>(true, std::move(inst)));
			else // InternalNode.
				return attachInternalNode(make<T>(true, std::move(inst)));
		}

		// CompositeNode creators
		// ~~~~~~~~~~~~~~~~~~~~~~

		// A SequenceNode executes its children one by one sequentially,
		// it succeeds only if all children succeed.
		auto& Sequence() { return C<SequenceNode>("Sequence"); }
		// A StatefulSequenceNode behaves like a sequence node, executes its children sequentially, succeeds if
		// all children succeed, fails if any child fails. What's the difference is, a StatefulSequenceNode skips
		// the succeeded children instead of always starting from the first child.
		auto& StatefulSequence() { return C<StatefulSequenceNode>("Sequence*"); }
		// A SelectorNode succeeds if any child succeeds, fails only if all children fail.
		auto& Selector() { return C<SelectorNode>("Selector"); }
		// A StatefulSelectorNode behaves like a selector node, executes its children sequentially, succeeds if
		// any child succeeds, fails if all child fail. What's the difference is, a StatefulSelectorNode skips the
		// failure children instead of always starting from the first child.
		auto& StatefulSelector() { return C<StatefulSelectorNode>("Selector*"); }
		// A ParallelNode executes its children parallelly.
		// It succeeds if all children succeed, and fails if any child fails.
		auto& Parallel() { return C<ParallelNode>("Parallel"); }
		// A StatefulParallelNode behaves like a parallel node, executes its children parallelly, succeeds if all
		// succeed, fails if all child fail. What's the difference is, a StatefulParallelNode will skip the
		// "already success" children instead of executing every child all the time.
		auto& StatefulParallel() { return C<StatefulParallelNode>("Parallel*"); }
		// A RandomSelectorNode determines a child via weighted random selection.
		// It continues to randomly select a child, propagating tick, until some child succeeds.
		auto& RandomSelector() { return C<RandomSelectorNode>("RandomSelector"); }
		// A StatefulRandomSelector behaves like a random selector node, the difference is, a
		// StatefulRandomSelector will skip already failed children during a round.
		auto& StatefulRandomSelector() { return C<StatefulRandomSelectorNode>("RandomSelector*"); }

		// LeafNode creators
		// ~~~~~~~~~~~~~~~~~

		// Creates an Action node by providing implemented Action class.
		// Code example::
		//  root
		//  .Action<MyActionClass>()
		//  .End();
		template <TAction Impl, typename... Args>
		auto& Action(Args&&... args)
		{
			return C<Impl>(std::forward<Args>(args)...);
		}
		// Creates a ConditionNode from a lambda function.
		// Code example::
		//   root
		//   .Sequence()
		//   ._().Condition([=](const Context& ctx) { return false;})
		//   ._().Action<A>()
		//   .End();
		auto& Condition(ConditionNode::Checker checker) { return C<ConditionNode>(checker); }
		// Creates a ConditionNode by providing implemented Condition class.
		// Code example::
		//   root
		//   .Sequence()
		//   ._().Condition<MyConditionClass>()
		//   ._().Action<A>()
		//   .End();
		template <TCondition Impl, typename... Args>
		auto& Condition(Args&&... args)
		{
			return C<Impl>(std::forward<Args>(args)...);
		}

		// DecoratorNode creators
		// ~~~~~~~~~~~~~~~~~~~~~~

		// Inverts the status of decorated node.
		// Parameter `child` the node to be decorated.
		// Code exapmle::
		//   root
		//   .Invert()
		//   ._().Condition<A>()
		//   .End();
		auto& Invert() { return C<InvertNode>("Invert"); }
		// Alias to Invert, just named 'Not'.
		// Code exapmle::
		//   root
		//   .Not()
		//   ._().Condition<A>()
		//   .End();
		auto& Not() { return C<InvertNode>("Not"); }
		// Creates a invert condition of given Condition class.
		// Code exapmle::
		//   root
		//   .Sequence()
		//   ._().Not<IsXXX>()
		//   ._().Action<DoSomething>()
		//   .End();
		template <TCondition Condition, typename... ConditionArgs>
		auto& Not(ConditionArgs... args)
		{
			return C<InvertNode>("Not", make<Condition>(false, std::forward<ConditionArgs>(args)...));
		}
		// Repeat creates a RepeatNode.
		// It will repeat the decorated node for exactly n times.
		// Providing n=-1 means to repeat forever.
		// Providing n=0 means immediately success without executing the decorated node.
		// Code exapmle::
		//   root
		//   .Repeat(3)
		//   ._().Action<A>()
		//   .End();
		auto& Repeat(int n) { return C<RepeatNode>(n, "Repeat"); }
		// Alias to Repeat.
		// Code exapmle::
		//   root
		//   .Loop(3)
		//   ._().Action<A>();
		auto& Loop(int n) { return C<RepeatNode>(n, "Loop"); }
		// Timeout creates a TimeoutNode.
		// It executes the decorated node for at most given duration.
		// Code exapmle::
		//   root
		//   .Timeout(3000ms)
		//   ._().Action<A>()
		//   .End();
		auto& Timeout(std::chrono::milliseconds duration) { return C<TimeoutNode>(duration, "Timeout"); }
		// Delay creates a DelayNode.
		// Wait for given duration before execution of decorated node.
		// Code exapmle::
		//   root
		//   .Delay(3000ms)
		//   ._().Action<A>()
		//   .End();
		auto& Delay(std::chrono::milliseconds duration) { return C<DelayNode>(duration, "Delay"); }
		// Retry creates a RetryNode.
		// It executes the decorated node for at most n times.
		// A retry will only be initiated if the decorated node fails.
		// Providing n=-1 for unlimited retry times.
		// Code exapmle::
		//   root
		//   .Retry(1, 3000ms)
		//   ._().Action<A>()
		//   .End();
		auto& Retry(int n, std::chrono::milliseconds interval) { return C<RetryNode>(n, interval, "Retry"); }
		// Alias for Retry(-1, interval)
		auto& RetryForever(std::chrono::milliseconds interval)
		{
			return C<RetryNode>(-1, interval, "RetryForever");
		}
		// Forces a node to be success.
		// It executes the decorated node and checks its status.
		// Returns RUNNING if the decorated node is RUNNING, else always returns SUCCESS.
		auto& ForceSuccess() { return C<ForceSuccessNode>("ForceSuccess"); }
		// Forces a node to be failure.
		// It executes the decorated node and checks its status.
		// Returns RUNNING if the decorated node is RUNNING, else always returns FAILURE.
		auto& ForceFailure() { return C<ForceFailureNode>("ForceFailure"); }
		// If creates a ConditionalRunNode.
		// It executes the decorated node only if the condition goes true.
		// Code example::
		//   root
		//   .If<CheckSomething>()
		//   ._().Action(DoSomething)()
		//   .End();
		template <TCondition Condition, typename... ConditionArgs>
		auto& If(ConditionArgs&&... args)
		{
			auto condition = make<Condition>(false, std::forward<ConditionArgs>(args)...);
			return C<ConditionalRunNode>(std::move(condition), "If");
		}
		// If creates a ConditionalRunNode from lambda function.
		// Code example::
		//  root
		//  .If([=](const Context& ctx) { return false; })
		//  .End();
		auto& If(ConditionNode::Checker checker) { return If<ConditionNode>(checker); }
		// Switch is just an alias to Selector.
		// Code example::
		//   root
		//   .Switch()
		//   ._().Case<Condition1>()
		//   ._()._().Action<A>()
		//   ._().Case<Condition2>()
		//   ._()._().Action<B>()
		//   ._().Case([=](const Context& ctx) { return false; })
		//   ._()._().Action<D>()
		//   .End();
		auto& Switch() { return C<SelectorNode>("Switch"); }
		// Stateful version `Switch` based on StatefulSelectorNode.
		auto& StatefulSwitch() { return C<StatefulSelectorNode>("Switch*"); }
		// Alias to If, for working alongs with Switch.
		template <TCondition Condition, typename... ConditionArgs>
		auto& Case(ConditionArgs&&... args)
		{
			auto condition = make<Condition>(false, std::forward<ConditionArgs>(args)...);
			return C<ConditionalRunNode>(std::move(condition), "Case");
		}
		// Case creates a ConditionalRunNode from lambda function.
		auto& Case(ConditionNode::Checker checker) { return Case<ConditionNode>(checker); }

		// Subtree creators
		// ~~~~~~~~~~~~~~~~

		// Attach a sub behavior tree into this tree.
		// Code example::
		//    bt::Tree subtree;
		//    subtree
		//      .Action<B>()
		//      .End();
		//    root
		//      .Sequence()
		//      ._().Subtree(std::move(subtree))
		//      .End();
		auto& Subtree(RootNode&& subtree)
		{
			onSubtreeAttach(subtree, root);
			return M<RootNode>(std::move(subtree)); // move the subtree object
		}
	};

	//////////////////////////////////////////////////////////////
	/// Tree
	///////////////////////////////////////////////////////////////

	// Behavior Tree, please keep this class simple enough.
	class Tree : public RootNode, public Builder<Tree>
	{
	public:
		explicit Tree(std::string_view name = "Root")
			: RootNode(name), Builder() { bindRoot(*this); }
	};

} // namespace bt

#endif
