// Copyright (c) 2024 Chao Wang <hit9@icloud.com>.
// License: BSD. https://github.com/hit9/bt.h
//
// A lightweight behavior tree library that separates data and behavior.
//
// Requirements: at least C++20.
//
// Features
// ~~~~~~~~
//
// 1. Nodes store no data states, behaviors and data are separated.
// 2. Builds a behavior tree in tree structure codes, concise and expressive,
//    and supports to extend the builder.
// 3. Built-in multiple decorators, and supports custom decoration nodes,
// 4. Supports composite nodes with priority child nodes, and random selector.
// 5. Also supports continuous memory fixed sized tree blob.
//
// Code Example
// ~~~~~~~~~~~~
//
// To structure a tree:
//
//   bt::Tree root;
//   root
//   .Sequence()
//   ._().Action<A>()
//   ._().Repeat(3)
//   ._()._().Action<B>()
//   .End();
//
// Prepares a TreeBlob, e.g. for each entity:
//
//   // A TreeBlob holds all the internal state data.
//   bt::DynamicTreeBlob blob;
//
//   // Or use a fixed size tree blob:
//   bt::FixedTreeBlob<8, 64> blob;
//
// Run ticking:
//
//   bt::Context ctx;
//
//   // In the ticking loop.
//   while(...) {
//     // for each blob
//     for (auto& blob : allBlobs) {
//       root.BindTreeBlob(blob);
//       root.Tick(ctx)
//       root.UnbindTreeBlob();
//     }
//   }
//
// Classes Structure
// ~~~~~~~~~~~~~~~~~
//
//   Node
//    | InternalNode
//    |   | SingleNode
//    |   |  | RootNode
//    |   |  | DecoratorNode
//    |   | CompositeNode
//    |   |  | SelectorNode
//    |   |  | ParallelNode
//    |   |  | SequenceNode
//    | LeafNode
//    |   | ActionNode
//    |   | ConditionNode

// Version: 0.3.1

#ifndef HIT9_BT_H
#define HIT9_BT_H

#include <algorithm>  // for max, min
#include <any>
#include <cassert>  // for NDEBUG mode
#include <chrono>   // for milliseconds, high_resolution_clock
#include <cstring>  // for memset
#include <functional>
#include <iostream>  // for cout, flush
#include <memory>    // for unique_ptr
#include <queue>     // for priority_queue
#include <random>    // for mt19937
#include <stack>
#include <stdexcept>  // for runtime_error
#include <string>
#include <string_view>
#include <thread>       // for this_thread::sleep_for
#include <type_traits>  // for is_base_of_v
#include <vector>

namespace bt {

////////////////////////////
/// Status
////////////////////////////

enum class Status { UNDEFINED = 0, RUNNING = 1, SUCCESS = 2, FAILURE = 3 };

// Returns char representation of given status.
static const char statusRepr(Status s) {
  switch (s) {
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

using ull = unsigned long long;

// Node instance's id type.
using NodeId = unsigned int;

////////////////////////////
/// Context
////////////////////////////

// Tick/Update's Context.
struct Context {
  ull seq;                         // Tick seq number.
  std::chrono::nanoseconds delta;  // Delta time since last tick.
  // User data.
  // For instance, it could hold a shared_ptr to a blackboard.
  // Code example::
  //   bt::Context ctx{.data = std::make_shared<Blackboard>()};
  std::any data;

  // Constructors.
  Context() = default;

  template <typename T>
  Context(T data) : data(data), seq(0) {}
};

// NodeBlob is the base class to store node's internal entity-related data and states.
struct NodeBlob {
  bool running = false;                   // is still running?
  Status lastStatus = Status::UNDEFINED;  // status of last execution.
  ull lastSeq = 0;                        // seq of last execution.
};

// Concept TNodeBlob for all classes derived from NodeBlob.
template <typename T>
concept TNodeBlob = std::is_base_of_v<NodeBlob, T>;

////////////////////////////
/// TreeBlob
////////////////////////////

// ITreeBlob is an internal interface base class for FixedTreeBlob and DynamicTreeBlob.
// A TreeBlob stores the entity-related states data for all nodes in a tree.
// One tree blob for one entity.
class ITreeBlob {
 protected:
  // allocates memory for given index, returns the pointer to the node blob.
  virtual void* allocate(const std::size_t idx, std::size_t size) = 0;
  // returns true if a index already allocated memory.
  virtual bool exist(const std::size_t idx) = 0;
  // get the blob's pointer for the given index.
  // returns nullptr if not exist.
  virtual void* get(const std::size_t idx) = 0;
  // pre-reserve enough capacity if need.
  virtual void reserve(const std::size_t cap) = 0;

 public:
  virtual ~ITreeBlob() {}
  // Returns a pointer to given NodeBlob B for the node with given id.
  // Allocates if not exist.
  // Parameter cb is an optional function to be called after the blob is first allocated.
  template <TNodeBlob B>
  B* Make(const NodeId id, const std::function<void(NodeBlob*)>& cb, const std::size_t cap = 0) {
    if (cap) reserve(cap);
    std::size_t idx = id - 1;
    if (exist(idx)) return static_cast<B*>(get(idx));
    auto p = static_cast<B*>(allocate(idx, sizeof(B)));
    new (p) B();  // call constructor
    if (cb != nullptr) cb(p);
    return p;
  }
};

// FixedTreeBlob is just a continuous buffer, implements ITreeBlob.
template <std::size_t NumNodes, std::size_t MaxSizeNodeBlob>
class FixedTreeBlob final : public ITreeBlob {
 private:
  unsigned char buf[NumNodes][MaxSizeNodeBlob + 1];

 protected:
  void* allocate(const std::size_t idx, std::size_t size) override {
    if (idx >= NumNodes) throw std::runtime_error("bt: FixedTreeBlob NumNodes not enough");
    if (size > MaxSizeNodeBlob) throw std::runtime_error("bt: FixedTreeBlob MaxSizeNodeBlob not enough");
    buf[idx][0] = true;
    return get(idx);
  };
  bool exist(const std::size_t idx) override { return static_cast<bool>(buf[idx][0]); };
  void* get(const std::size_t idx) override { return &buf[idx][1]; };
  void reserve(const std::size_t cap) override{};

 public:
  FixedTreeBlob() { memset(buf, 0, sizeof(buf)); }
};

// DynamicTreeBlob contains dynamic allocated unique pointers, implements ITreeBlob.
class DynamicTreeBlob final : public ITreeBlob {
 private:
  std::vector<std::unique_ptr<unsigned char[]>> m;  // index => blob pointer.
  std::vector<bool> e;                              // index => exist, dynamic

 protected:
  void* allocate(const std::size_t idx, std::size_t size) override {
    if (m.size() <= idx) {
      m.resize(idx + 1);
      e.resize(idx + 1, false);
    }
    auto p = std::make_unique_for_overwrite<unsigned char[]>(size);
    auto rp = p.get();
    std::fill_n(rp, size, 0);
    m[idx] = std::move(p);
    e[idx] = true;
    return rp;
  };
  bool exist(const std::size_t idx) override { return e.size() > idx && e[idx]; };
  void* get(const std::size_t idx) override { return m[idx].get(); };
  void reserve(const std::size_t cap) override {
    if (m.capacity() < cap) {
      m.reserve(cap);
      e.reserve(cap);
    };
  }

 public:
  DynamicTreeBlob() {}
};

////////////////////////////
/// Node
////////////////////////////

// RootNode Interface.
class IRootNode {
 public:
  // Returns the current binding TreeBlob's pointer.
  virtual ITreeBlob* GetTreeBlob(void) const = 0;
  // Returns the total number of nodes built on this tree.
  virtual int NumNodes() const = 0;
};

class Node;  // forward declaration.

// Alias
template <typename T>
using Ptr = std::unique_ptr<T>;

static Ptr<Node> NullNodePtr = nullptr;

template <typename T>
using PtrList = std::vector<Ptr<T>>;

// Type of the callback function for node traversal.
// Parameters: curren walking node and its unique pointer from parent (NullNodePtr for root).
using TraversalCallback = std::function<void(Node& currentNode, Ptr<Node>& currentNodePtr)>;
static TraversalCallback NullTraversalCallback = [](Node&, Ptr<Node>&) {};

// The most base class of all behavior nodes.
class Node {
 private:
  std::string name;

 protected:
  NodeId id = 0;
  // holding a pointer to the root.
  IRootNode* root = nullptr;
  // size of this node, available after tree built.
  std::size_t size = 0;

  // Internal helper method to return the raw pointer to the node blob.
  template <TNodeBlob B>
  B* getNodeBlob() const {
    assert(id != 0);
    assert(root != nullptr);
    auto b = root->GetTreeBlob();
    assert(b != nullptr);
    const auto cb = [&](NodeBlob* blob) { OnBlobAllocated(blob); };
    // allocate, or get if exist
    return b->Make<B>(id, cb, root->NumNodes());
  }

  // Internal method to visualize tree.
  virtual void makeVisualizeString(std::string& s, int depth, ull seq) {
    auto b = GetNodeBlob();
    if (depth > 0) s += " |";
    for (int i = 1; i < depth; i++) s += "---|";
    if (depth > 0) s += "- ";
    if (b->lastSeq == seq) s += "\033[32m";  // color if catches up with seq.
    s += Name();
    s.push_back('(');
    s.push_back(statusRepr(b->lastStatus));
    s.push_back(')');
    if (b->lastSeq == seq) s += "\033[0m";
  }

  // Internal onBuild method.
  // Separating from the api hook OnBuild, so there's no need to call parent
  // class's OnBuild for custom OnBuild overridings.
  virtual void internalOnBuild() {}

  // firend with SingleNode and CompositeNode for accessbility to makeVisualizeString.
  friend class SingleNode;
  friend class CompositeNode;

  // friend with _InternalBuilderBase to access member root, size and id etc.
  friend class _InternalBuilderBase;

 public:
  // Every stateful Node class should declare its own Blob type member.
  using Blob = NodeBlob;

  Node(std::string_view name = "Node") : name(name) {}
  virtual ~Node() = default;

  /////////////////////////////////////////
  // Simple Getters
  /////////////////////////////////////////

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

  /////////////////////////////////////////
  // API
  /////////////////////////////////////////

  // Traverse the subtree of the current node recursively and execute the given function callback functions.
  // The callback function pre will be called pre-order, and the post will be called post-order.
  // Pass NullTraversalCallback for empty callbacks.
  virtual void Traverse(TraversalCallback& pre, TraversalCallback& post, Ptr<Node>& ptr) {
    pre(*this, ptr);
    post(*this, ptr);
  }

  // Main entry function, should be called on every tick.
  Status Tick(const Context& ctx) {
    auto b = GetNodeBlob();
    // First run of current round.
    if (!b->running) OnEnter(ctx);
    b->running = true;

    auto status = Update(ctx);
    b->lastStatus = status;
    b->lastSeq = ctx.seq;

    // Last run of current round.
    if (status == Status::FAILURE || status == Status::SUCCESS) {
      OnTerminate(ctx, status);
      b->running = false;  // reset
    }
    return status;
  }

  /////////////////////////////////////////
  // Public Virtual Functions To Override
  /////////////////////////////////////////

  // Hook function to be called on this node's first run.
  // Nice to call parent class' OnEnter before your implementation.
  virtual void OnEnter(const Context& ctx){};

  // Hook function to be called once this node goes into success or failure.
  // Nice to call parent class' OnTerminate after your implementation
  virtual void OnTerminate(const Context& ctx, Status status){};

  // Hook function to be called on this node's build is finished.
  // Nice to call parent class' OnBuild after your implementation
  virtual void OnBuild() {}

  // Main update function to be implemented by all subclasses.
  // It's the body part of function Tick().
  virtual Status Update(const Context& ctx) { return Status::SUCCESS; };

  // Returns the priority of this node, should be strictly larger than 0, the larger the higher.
  // By default, all nodes' priorities are equal, to 1.
  // Providing this method is primarily for selecting children by dynamic priorities.
  // It's recommended to implement this function fast enough, since it will be called on each
  // tick. For instance, we may not need to do the calculation on every tick if it's complex.
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
class LeafNode : public Node {
 public:
  LeafNode(std::string_view name = "LeafNode") : Node(name) {}
};

// Concept TLeafNode for all classes derived from LeafNode.
template <typename T>
concept TLeafNode = std::is_base_of_v<LeafNode, T>;

////////////////////////////////////
/// Node > LeafNode > ConditionNode
/////////////////////////////////////

// ConditionNode succeeds only if Check() returns true, it never returns
// RUNNING.
// The checker is stored directly on the tree structure.
// Note that the provided checker should be independent with any entities's stateful data.
class ConditionNode : public LeafNode {
 public:
  using Checker = std::function<bool(const Context&)>;
  ConditionNode(Checker checker = nullptr, std::string_view name = "Condition")
      : LeafNode(name), checker(checker) {}
  Status Update(const Context& ctx) override { return Check(ctx) ? Status::SUCCESS : Status::FAILURE; }

  // Check if condition is satisfied.
  virtual bool Check(const Context& ctx) { return checker != nullptr && checker(ctx); };

 private:
  Checker checker = nullptr;
};

using Condition = ConditionNode;  // alias

// Concept TCondition for all classes derived from Condition.
template <typename T>
concept TCondition = std::is_base_of_v<ConditionNode, T>;

////////////////////////////////////
/// Node > LeafNode > ActionNode
/////////////////////////////////////

// ActionNode contains no children, it just runs a task.
class ActionNode : public LeafNode {
 public:
  ActionNode(std::string_view name = "Action") : LeafNode(name) {}

  // Subclasses must implement function Update().
};
using Action = ActionNode;  // alias

// Concept TAction for all classes derived from Action.
template <typename T>
concept TAction = std::is_base_of_v<ActionNode, T>;

////////////////////////////
/// Node > InternalNode
////////////////////////////

// InternalNode can have children nodes.
class InternalNode : public Node {
 public:
  InternalNode(std::string_view name = "InternalNode") : Node(name) {}
  // Append a child to this node.
  virtual void Append(Ptr<Node> node) = 0;
};

template <typename T>
concept TInternalNode = std::is_base_of_v<InternalNode, T>;

////////////////////////////////////////////////
/// Node > InternalNode > SingleNode
////////////////////////////////////////////////

// SingleNode contains exactly a single child.
class SingleNode : public InternalNode {
 protected:
  Ptr<Node> child;

  void makeVisualizeString(std::string& s, int depth, ull seq) override {
    Node::makeVisualizeString(s, depth, seq);
    if (child != nullptr) {
      s.push_back('\n');
      child->makeVisualizeString(s, depth + 1, seq);
    }
  }
 public:
  SingleNode(std::string_view name = "SingleNode", Ptr<Node> child = nullptr)
      : InternalNode(name), child(std::move(child)) {}
  void Traverse(TraversalCallback& pre, TraversalCallback& post, Ptr<Node>& ptr) override {
    pre(*this, ptr);
    if (child != nullptr) child->Traverse(pre, post, child);
    post(*this, ptr);
  }
  std::string_view Validate() const override { return child == nullptr ? "no child node provided" : ""; }
  void Append(Ptr<Node> node) override { child = std::move(node); }
  unsigned int Priority(const Context& ctx) const override { return child->Priority(ctx); }
};

////////////////////////////////////////////////
/// Node > InternalNode > CompositeNode
////////////////////////////////////////////////

// CompositeNode contains multiple children.
class CompositeNode : public InternalNode {
 protected:
  PtrList<Node> children;

  // Should we consider partial children (not all) every tick?
  virtual bool isParatialConsidered() const { return false; }
  // Should we consider i'th child during this round?
  virtual bool considerable(int i) const { return true; }
  // Internal hook function to be called after a child goes success.
  virtual void onChildSuccess(const int i){};
  // Internal hook function to be called after a child goes failure.
  virtual void onChildFailure(const int i){};

  void makeVisualizeString(std::string& s, int depth, ull seq) override {
    Node::makeVisualizeString(s, depth, seq);
    for (auto& child : children) {
      if (child != nullptr) {
        s.push_back('\n');
        child->makeVisualizeString(s, depth + 1, seq);
      }
    }
  }

 public:
  CompositeNode(std::string_view name = "CompositeNode", PtrList<Node>&& cs = {}) : InternalNode(name) {
    children.swap(cs);
  }
  void Traverse(TraversalCallback& pre, TraversalCallback& post, Ptr<Node>& ptr) override {
    pre(*this, ptr);
    for (auto& child : children)
      if (child != nullptr) child->Traverse(pre, post, child);
    post(*this, ptr);
  }

  void Append(Ptr<Node> node) override { children.push_back(std::move(node)); }
  std::string_view Validate() const override { return children.empty() ? "children empty" : ""; }

  // Returns the max priority of considerable children.
  unsigned int Priority(const Context& ctx) const final override {
    unsigned int ans = 0;
    for (int i = 0; i < children.size(); i++)
      if (considerable(i)) ans = std::max(ans, children[i]->Priority(ctx));
    return ans;
  }
};

//////////////////////////////////////////////////////////////
/// Node > InternalNode > CompositeNode > _Internal Impls
///////////////////////////////////////////////////////////////

struct _InternalStatefulCompositeNodeBlob : NodeBlob {
  // st[i] => should we skip considering children at index i ?
  std::vector<bool> st;
};

// Always skip children that already succeeded or failure during current round.
class _InternalStatefulCompositeNode : virtual public CompositeNode {
 protected:
  bool isParatialConsidered() const override { return true; }
  bool considerable(int i) const override { return !(getNodeBlob<Blob>()->st[i]); }
  void skip(const int i) { getNodeBlob<Blob>()->st[i] = true; }

 public:
  using Blob = _InternalStatefulCompositeNodeBlob;
  NodeBlob* GetNodeBlob() const override { return getNodeBlob<Blob>(); }
  void OnTerminate(const Context& ctx, Status status) override {
    auto& t = getNodeBlob<Blob>()->st;
    std::fill(t.begin(), t.end(), false);
  }
  void OnBlobAllocated(NodeBlob* blob) const override {
    auto ptr = static_cast<Blob*>(blob);
    ptr->st.resize(children.size(), false);
  }
};

// _MixedQueueHelper is a helper queue wrapper for _InternalPriorityCompositeNode .
// It decides which underlying queue to use for current tick. Wraps a simple queue
// and priority queue.
class _MixedQueueHelper {
  using Cmp = std::function<bool(const int, const int)>;

  // hacking a private priority_queue for the stl missing `reserve` and `clear` method.
  template <typename T, typename Container = std::vector<T>>
  class _priority_queue : public std::priority_queue<T, Container, Cmp> {
   public:
    _priority_queue() : std::priority_queue<T, Container, Cmp>() {}
    _priority_queue(Cmp cmp) : std::priority_queue<T, Container, Cmp>(cmp) {}
    void reserve(std::size_t n) { this->c.reserve(n); }
    void clear() { this->c.clear(); }
  };

 private:
  // use a pre-allocated vector instead of a std::queue
  // The q1 will be pushed all and then poped all, so a simple vector is enough,
  // and neither needs a circular queue.
  // And here we use a pointer, allowing temporarily replace q1's container from
  // outside existing container.
  std::vector<int>* q1;
  std::vector<int> q1Container;
  int q1Front = 0;

  _priority_queue<int, std::vector<int>> q2;

  bool use1;  // using q1? otherwise q2

 public:
  _MixedQueueHelper() {}
  _MixedQueueHelper(Cmp cmp, const std::size_t n) {
    // reserve capacity for q1.
    q1Container.reserve(n);
    q1 = &q1Container;
    // reserve capacity for q2.
    decltype(q2) _q2(cmp);
    q2.swap(_q2);
    q2.reserve(n);
  }
  void setflag(bool u1) { use1 = u1; }
  int pop() {
    if (use1) return (*q1)[q1Front++];
    int v = q2.top();
    q2.pop();
    return v;
  }
  void push(int v) {
    if (use1) {
      if (q1 != &q1Container) throw std::runtime_error("bt: cant push on outside q1 container");
      return q1->push_back(v);
    }
    q2.push(v);
  }
  bool empty() const {
    if (use1) return q1Front == q1->size();
    return q2.empty();
  }
  void clear() {
    if (use1) {
      if (q1 != &q1Container) throw std::runtime_error("bt: cant clear on outside q1 container");
      q1->resize(0);
      q1Front = 0;
      return;
    }
    q2.clear();
  }
  void setQ1Container(std::vector<int>* c) {
    q1 = c;
    q1Front = 0;
  }
  void resetQ1Container(void) { q1 = &q1Container; }
};

// Priority related CompositeNode.
class _InternalPriorityCompositeNode : virtual public CompositeNode {
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
  bool areAllEqual;

  // simpleQ1Container contains [0...n-1]
  // Used as a temp container for q1 for "non-stateful && non-priorities" compositors.
  std::vector<int> simpleQ1Container;

  // Although only considerable children's priorities are refreshed,
  // and the q only picks considerable children, but p and q are still stateless with entities.
  // p and q are consistent with respect to "which child nodes to consider".
  // If we change the blob binding, the new tick won't be affected by previous blob.

  // Refresh priorities for considerable children.
  void refresh(const Context& ctx) {
    areAllEqual = true;
    // v is the first valid priority value.
    unsigned int v = 0;

    for (int i = 0; i < children.size(); i++) {
      if (!considerable(i)) continue;
      p[i] = children[i]->Priority(ctx);
      if (!v) v = p[i];
      if (v != p[i]) areAllEqual = false;
    }
  }

  void enqueue() {
    // if all priorities are equal, use q1 O(N)
    // otherwise, use q2 O(n*logn)
    q.setflag(areAllEqual);

    // We have to consider all children, and all priorities are equal,
    // then, we should just use a pre-exist vector to avoid a O(n) copy to q1.
    if ((!isParatialConsidered()) && areAllEqual) {
      q.setQ1Container(&simpleQ1Container);
      return;  // no need to perform enqueue
    }

    q.resetQ1Container();

    // Clear and enqueue.
    q.clear();
    for (int i = 0; i < children.size(); i++)
      if (considerable(i)) q.push(i);
  }

  // update is an internal method to propagates tick() to children in the q1/q2.
  // it will be called by Update.
  virtual Status update(const Context& ctx) = 0;

  void internalOnBuild() override {
    CompositeNode::internalOnBuild();
    // pre-allocate capacity for p.
    p.resize(children.size());
    // initialize simpleQ1Container;
    for (int i = 0; i < children.size(); i++) simpleQ1Container.push_back(i);
    // Compare priorities between children, where a and b are indexes.
    // priority from large to smaller, so use `less`: pa < pb
    // order: from small to larger, so use `greater`: a > b
    auto cmp = [&](const int a, const int b) { return p[a] < p[b] || a > b; };
    q = _MixedQueueHelper(cmp, children.size());
  }

 public:
  _InternalPriorityCompositeNode() {}

  Status Update(const Context& ctx) override {
    refresh(ctx);
    enqueue();
    // propagates ticks
    return update(ctx);
  }
};

//////////////////////////////////////////////////////////////
/// Node > InternalNode > CompositeNode > SequenceNode
///////////////////////////////////////////////////////////////

class _InternalSequenceNodeBase : virtual public _InternalPriorityCompositeNode {
 protected:
  Status update(const Context& ctx) override {
    // propagates ticks, one by one sequentially.
    while (!q.empty()) {
      auto i = q.pop();
      auto status = children[i]->Tick(ctx);
      if (status == Status::RUNNING) return Status::RUNNING;
      // F if any child F.
      if (status == Status::FAILURE) {
        onChildFailure(i);
        return Status::FAILURE;
      }
      // S
      onChildSuccess(i);
    }
    // S if all children S.
    return Status::SUCCESS;
  }
};

// SequenceNode runs children one by one, and succeeds only if all children succeed.
class SequenceNode final : public _InternalSequenceNodeBase {
 public:
  SequenceNode(std::string_view name = "Sequence", PtrList<Node>&& cs = {})
      : CompositeNode(name, std::move(cs)), _InternalPriorityCompositeNode() {}
};

// StatefulSequenceNode behaves like a SequenceNode, but instead of ticking children from the first, it
// starts from the running child instead.
class StatefulSequenceNode final : public _InternalStatefulCompositeNode, public _InternalSequenceNodeBase {
 protected:
  void onChildSuccess(const int i) override { skip(i); }

 public:
  StatefulSequenceNode(std::string_view name = "Sequence*", PtrList<Node>&& cs = {})
      : CompositeNode(name, std::move(cs)), _InternalPriorityCompositeNode() {}
};

//////////////////////////////////////////////////////////////
/// Node > InternalNode > CompositeNode > SelectorNode
///////////////////////////////////////////////////////////////

class _InternalSelectorNodeBase : virtual public _InternalPriorityCompositeNode {
 protected:
  Status update(const Context& ctx) override {
    // select a success children.
    while (!q.empty()) {
      auto i = q.pop();
      auto status = children[i]->Tick(ctx);
      if (status == Status::RUNNING) return Status::RUNNING;
      // S if any child S.
      if (status == Status::SUCCESS) {
        onChildSuccess(i);
        return Status::SUCCESS;
      }
      // F
      onChildFailure(i);
    }
    // F if all children F.
    return Status::FAILURE;
  }
};

// SelectorNode succeeds if any child succeeds.
class SelectorNode final : public _InternalSelectorNodeBase {
 public:
  SelectorNode(std::string_view name = "Selector", PtrList<Node>&& cs = {})
      : CompositeNode(name, std::move(cs)), _InternalPriorityCompositeNode() {}
};

// StatefulSelectorNode behaves like a SelectorNode, but instead of ticking children from the first, it
// starts from the running child instead.
class StatefulSelectorNode : public _InternalStatefulCompositeNode, public _InternalSelectorNodeBase {
 protected:
  void onChildFailure(const int i) override { skip(i); }

 public:
  StatefulSelectorNode(std::string_view name = "Selector*", PtrList<Node>&& cs = {})
      : CompositeNode(name, std::move(cs)), _InternalPriorityCompositeNode() {}
};

//////////////////////////////////////////////////////////////
/// Node > InternalNode > CompositeNode > RandomSelectorNode
///////////////////////////////////////////////////////////////

static std::mt19937 rng(std::random_device{}());  // seed random

// Weighted random selector.
class _InternalRandomSelectorNodeBase : virtual public _InternalPriorityCompositeNode {
 protected:
  Status update(const Context& ctx) override {
    // Sum of weights/priorities.
    unsigned int total = 0;
    for (int i = 0; i < children.size(); i++)
      if (considerable(i)) total += p[i];

    // random select one, in range [1, total]
    std::uniform_int_distribution<unsigned int> distribution(1, total);

    auto select = [&]() -> int {
      unsigned int v = distribution(rng);  // gen random unsigned int between [0, sum]
      unsigned int s = 0;                  // sum of iterated children.
      for (int i = 0; i < children.size(); i++) {
        if (!considerable(i)) continue;
        s += p[i];
        if (v <= s) return i;
      }
      return 0;  // won't reach here.
    };

    // While still have children considerable.
    // total reaches 0 only if no children left,
    // notes that Priority() always returns a positive value.
    while (total) {
      int i = select();
      auto status = children[i]->Tick(ctx);
      if (status == Status::RUNNING) return Status::RUNNING;
      // S if any child S.
      if (status == Status::SUCCESS) {
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

 public:
  Status Update(const Context& ctx) override {
    refresh(ctx);
    return update(ctx);
  }
};

// RandomSelectorNode selects children via weighted random selection.
class RandomSelectorNode final : public _InternalRandomSelectorNodeBase {
 public:
  RandomSelectorNode(std::string_view name = "RandomSelector", PtrList<Node>&& cs = {})
      : CompositeNode(name, std::move(cs)), _InternalPriorityCompositeNode() {}
};

// StatefulRandomSelectorNode behaves like RandomSelectorNode.
// But it won't reconsider failure children during a round.
class StatefulRandomSelectorNode final : virtual public _InternalStatefulCompositeNode,
                                         virtual public _InternalRandomSelectorNodeBase {
 protected:
  void onChildFailure(const int i) override { skip(i); }

 public:
  StatefulRandomSelectorNode(std::string_view name = "RandomSelector*", PtrList<Node>&& cs = {})
      : CompositeNode(name, std::move(cs)), _InternalPriorityCompositeNode() {}
};

//////////////////////////////////////////////////////////////
/// Node > InternalNode > CompositeNode > ParallelNode
///////////////////////////////////////////////////////////////

class _InternalParallelNodeBase : virtual public _InternalPriorityCompositeNode {
  Status update(const Context& ctx) override {
    // Propagates tick to all considerable children.
    int cntFailure = 0, cntSuccess = 0, total = 0;
    while (!q.empty()) {
      auto i = q.pop();
      auto status = children[i]->Tick(ctx);
      total++;
      if (status == Status::FAILURE) {
        cntFailure++;
        onChildFailure(i);
      }
      if (status == Status::SUCCESS) {
        cntSuccess++;
        onChildSuccess(i);
      }
    }

    // S if all children S.
    if (cntSuccess == total) return Status::SUCCESS;
    // F if any child F.
    if (cntFailure > 0) return Status::FAILURE;
    return Status::RUNNING;
  }
};

// ParallelNode succeeds if all children succeed but runs all children
// parallelly.
class ParallelNode final : public _InternalParallelNodeBase {
 public:
  ParallelNode(std::string_view name = "Parallel", PtrList<Node>&& cs = {})
      : CompositeNode(name, std::move(cs)), _InternalPriorityCompositeNode() {}
};

// StatefulParallelNode behaves like a ParallelNode, but instead of ticking every child, it only tick the
// running children, aka skipping the succeeded children..
class StatefulParallelNode final : public _InternalStatefulCompositeNode, public _InternalParallelNodeBase {
 protected:
  void onChildSuccess(const int i) override { skip(i); }

 public:
  StatefulParallelNode(std::string_view name = "Parallel*", PtrList<Node>&& cs = {})
      : CompositeNode(name, std::move(cs)), _InternalPriorityCompositeNode() {}
};

//////////////////////////////////////////////////////////////
/// Node > InternalNode > CompositeNode > Decorator
///////////////////////////////////////////////////////////////

// DecoratorNode decorates a single child node.
class DecoratorNode : public SingleNode {
 public:
  DecoratorNode(std::string_view name = "Decorator", Ptr<Node> child = nullptr)
      : SingleNode(name, std::move(child)) {}

  // To create a custom DecoratorNode.
  // You should derive from DecoratorNode and override the function Update.
  // Status Update(const Context&) override;
};

// InvertNode inverts its child's status.
class InvertNode : public DecoratorNode {
 public:
  InvertNode(std::string_view name = "Invert", Ptr<Node> child = nullptr)
      : DecoratorNode(name, std::move(child)) {}

  Status Update(const Context& ctx) override {
    auto status = child->Tick(ctx);
    switch (status) {
      case Status::RUNNING:
        return Status::RUNNING;
      case Status::FAILURE:
        return Status::SUCCESS;
      default:
        return Status::FAILURE;
    }
  }
};

// ConditionalRunNode executes its child if given condition returns true.
class ConditionalRunNode : public DecoratorNode {
 private:
  // Condition node to check.
  Ptr<Node> condition;
  Node* condition_view;

 protected:
  void internalOnBuild() override {
    DecoratorNode::internalOnBuild();
    condition_view = condition.get();
  }

 public:
  ConditionalRunNode(Ptr<ConditionNode> condition = nullptr, std::string_view name = "ConditionalRun",
                     Ptr<Node> child = nullptr)
      : DecoratorNode(std::string(name) + '<' + std::string(condition->Name()) + '>', std::move(child)),
        condition(std::move(condition)) {}

  void Traverse(TraversalCallback& pre, TraversalCallback& post, Ptr<Node>& ptr) override {
    pre(*this, ptr);
    if (condition != nullptr) condition->Traverse(pre, post, condition);
    if (child != nullptr) child->Traverse(pre, post, child);
    post(*this, ptr);
  }
  Status Update(const Context& ctx) override {
    if (condition_view->Tick(ctx) == Status::SUCCESS) return child->Tick(ctx);
    return Status::FAILURE;
  }
};

struct RepeatNodeBlob : NodeBlob {
  // Count how many times already executed for this round.
  int cnt = 0;
};

// RepeatNode repeats its child for exactly n times.
// Fails immediately if its child fails.
class RepeatNode : public DecoratorNode {
 protected:
  // Times to repeat, -1 for forever, 0 for immediately success.
  int n;

 public:
  using Blob = RepeatNodeBlob;
  RepeatNode(int n, std::string_view name = "Repeat", Ptr<Node> child = nullptr)
      : DecoratorNode(name, std::move(child)), n(n) {}

  NodeBlob* GetNodeBlob() const override { return getNodeBlob<Blob>(); }
  // Clears counter on enter.
  void OnEnter(const Context& ctx) override { getNodeBlob<Blob>()->cnt = 0; }
  // Reset counter on termination.
  void OnTerminate(const Context& ctx, Status status) override { getNodeBlob<Blob>()->cnt = 0; }

  Status Update(const Context& ctx) override {
    if (n == 0) return Status::SUCCESS;
    auto status = child->Tick(ctx);
    if (status == Status::RUNNING) return Status::RUNNING;
    if (status == Status::FAILURE) return Status::FAILURE;
    // Count success until n times, -1 will never stop.
    if (++(getNodeBlob<Blob>()->cnt) == n) return Status::SUCCESS;
    // Otherwise, it's still running.
    return Status::RUNNING;
  }
};

template <typename Clock = std::chrono::high_resolution_clock>
using TimePoint = std::chrono::time_point<Clock>;

template <typename Clock = std::chrono::high_resolution_clock>
struct TimeoutNodeBlob : NodeBlob {
  // Time point of this node starts.
  TimePoint<Clock> startAt = TimePoint<Clock>::min();
};

// Timeout runs its child for at most given duration, fails on timeout.
template <typename Clock = std::chrono::high_resolution_clock>
class TimeoutNode : public DecoratorNode {
 protected:
  std::chrono::milliseconds duration;

 public:
  using Blob = TimeoutNodeBlob<Clock>;
  TimeoutNode(std::chrono::milliseconds d, std::string_view name = "Timeout", Ptr<Node> child = nullptr)
      : DecoratorNode(name, std::move(child)), duration(d) {}

  NodeBlob* GetNodeBlob() const override { return getNodeBlob<Blob>(); }
  void OnEnter(const Context& ctx) override { getNodeBlob<Blob>()->startAt = Clock::now(); };

  Status Update(const Context& ctx) override {
    // Check if timeout at first.
    auto now = Clock::now();
    if (now > getNodeBlob<Blob>()->startAt + duration) return Status::FAILURE;
    return child->Tick(ctx);
  }
};

template <typename Clock = std::chrono::high_resolution_clock>
struct DelayNodeBlob : NodeBlob {
  // The time point this node first run.
  TimePoint<Clock> firstRunAt;
};

// DelayNode runs its child node after given duration.
template <typename Clock = std::chrono::high_resolution_clock>
class DelayNode : public DecoratorNode {
 protected:
  // Duration to wait.
  std::chrono::milliseconds duration;

 public:
  using Blob = DelayNodeBlob<Clock>;
  DelayNode(std::chrono::milliseconds duration, std::string_view name = "Delay", Ptr<Node> c = nullptr)
      : DecoratorNode(name, std::move(c)), duration(duration) {}

  NodeBlob* GetNodeBlob() const override { return getNodeBlob<Blob>(); }
  void OnEnter(const Context& ctx) override { getNodeBlob<Blob>()->firstRunAt = Clock::now(); };
  void OnTerminate(const Context& ctx, Status status) override {
    getNodeBlob<Blob>()->firstRunAt = TimePoint<Clock>::min();
  };

  Status Update(const Context& ctx) override {
    auto now = Clock::now();
    if (now < getNodeBlob<Blob>()->firstRunAt + duration) return Status::RUNNING;
    return child->Tick(ctx);
  }
};

template <typename Clock = std::chrono::high_resolution_clock>
struct RetryNodeBlob : NodeBlob {
  // Times already retried.
  int cnt = 0;
  // Timepoint last retried at.
  TimePoint<Clock> lastRetryAt;
};

// RetryNode retries its child node on failure.
template <typename Clock = std::chrono::high_resolution_clock>
class RetryNode : public DecoratorNode {
 protected:
  // Max retry times, -1 for unlimited.
  int maxRetries = -1;
  // Retry interval in milliseconds
  std::chrono::milliseconds interval;

 public:
  using Blob = RetryNodeBlob<Clock>;
  RetryNode(int maxRetries, std::chrono::milliseconds interval, std::string_view name = "Retry",
            Ptr<Node> child = nullptr)
      : DecoratorNode(name, std::move(child)), maxRetries(maxRetries), interval(interval) {}

  NodeBlob* GetNodeBlob() const override { return getNodeBlob<Blob>(); }
  void OnEnter(const Context& ctx) override {
    auto b = getNodeBlob<Blob>();
    b->cnt = 0;
    b->lastRetryAt = TimePoint<Clock>::min();
  }
  void OnTerminate(const Context& ctx, Status status) override {
    auto b = getNodeBlob<Blob>();
    b->cnt = 0;
    b->lastRetryAt = status == Status::FAILURE ? Clock::now() : TimePoint<Clock>::min();
  }

  Status Update(const Context& ctx) override {
    auto b = getNodeBlob<Blob>();

    if (maxRetries != -1 && b->cnt > maxRetries) return Status::FAILURE;

    // If has failures before, and retry timepoint isn't arriving.
    auto now = Clock::now();
    if (b->cnt > 0 && now < b->lastRetryAt + interval) return Status::RUNNING;

    // Time to run/retry.
    auto status = child->Tick(ctx);
    switch (status) {
      case Status::RUNNING:
        [[fallthrough]];
      case Status::SUCCESS:
        return status;
      default:
        // Failure
        if (++b->cnt > maxRetries && maxRetries != -1) return Status::FAILURE;  // exeeds max retries.
        return Status::RUNNING;                                                 // continues retry
    }
  }
};

//////////////////////////////////////////////////////////////
/// Node > SingleNode > RootNode
///////////////////////////////////////////////////////////////

// RootNode is a SingleNode.
class RootNode : public SingleNode, public IRootNode {
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

  friend class _InternalBuilderBase;  // for access to n, treeSize, maxSizeNode, maxSizeNodeBlob;

 public:
  RootNode(std::string_view name = "Root") : SingleNode(name) {}
  Status Update(const Context& ctx) override { return child->Tick(ctx); }

  //////////////////////////
  /// Blob Apis
  //////////////////////////

  // Binds a tree blob.
  void BindTreeBlob(ITreeBlob& b) { blob = &b; }
  // Returns current tree blob.
  ITreeBlob* GetTreeBlob(void) const override { return blob; }
  // Unbind current tree blob.
  void UnbindTreeBlob() { blob = nullptr; }

  //////////////////////////
  /// Size Info
  //////////////////////////

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

  //////////////////////////
  /// Visualization
  //////////////////////////

  // Visualize the tree to console.
  void Visualize(ull seq) {
    // CSI[2J clears screen.
    // CSI[H moves the cursor to top-left corner
    std::cout << "\x1B[2J\x1B[H" << std::flush;
    // Make a string.
    std::string s;
    makeVisualizeString(s, 0, seq);
    std::cout << s << std::flush;
  }

  //////////////////////////
  /// Ticking
  //////////////////////////

  // Handy function to run tick loop forever.
  // Parameter interval specifies the time interval between ticks.
  // Parameter visualize enables debugging visualization on the console.
  // Parameter is a hook to be called after each tick.
  template <typename Clock = std::chrono::high_resolution_clock>
  void TickForever(Context& ctx, std::chrono::nanoseconds interval, bool visualize = false,
                   std::function<void(const Context&)> post = nullptr) {
    auto lastTickAt = Clock::now();

    while (true) {
      auto nextTickAt = lastTickAt + interval;

      // Time delta between last tick and current tick.
      ctx.delta = Clock::now() - lastTickAt;
      ++ctx.seq;
      Tick(ctx);
      if (post != nullptr) post(ctx);
      if (visualize) Visualize(ctx.seq);

      // Catch up with next tick.
      lastTickAt = Clock::now();
      if (lastTickAt < nextTickAt) {
        std::this_thread::sleep_for(nextTickAt - lastTickAt);
      }
    }
  }
};

//////////////////////////////////////////////////////////////
/// Tree Builder
///////////////////////////////////////////////////////////////

// A internal proxy base class to setup Node's internal bindings.
class _InternalBuilderBase {
 private:
  // Node id incrementer for a tree.
  // unique inside this builder instance.
  NodeId nextNodeId = 0;

  void maintainNodeBindInfo(Node& node, RootNode* root) {
    node.root = root;
    root->n++;
    node.id = ++nextNodeId;
  }

  void maintainSizeInfoOnRootBind(RootNode* root, std::size_t rootNodeSize, std::size_t blobSize) {
    root->size = rootNodeSize;
    root->treeSize += rootNodeSize;
    root->maxSizeNode = rootNodeSize;
    root->maxSizeNodeBlob = blobSize;
  }

  template <TNode T>
  void maintainSizeInfoOnNodeAttach(T& node, RootNode* root) {
    node.size = sizeof(T);
    root->treeSize += sizeof(T);
    root->maxSizeNode = std::max(root->maxSizeNode, sizeof(T));
    root->maxSizeNodeBlob = std::max(root->maxSizeNodeBlob, sizeof(typename T::Blob));
  }

  void maintainSizeInfoOnSubtreeAttach(RootNode& subtree, RootNode* root) {
    root->treeSize += subtree.treeSize;
    root->maxSizeNode = std::max(root->maxSizeNode, subtree.maxSizeNode);
    root->maxSizeNodeBlob = std::max(root->maxSizeNodeBlob, subtree.maxSizeNodeBlob);
  }

 protected:
  template <TNode T>
  void onNodeAttach(T& node, RootNode* root) {
    maintainNodeBindInfo(node, root);
    maintainSizeInfoOnNodeAttach<T>(node, root);
  }
  void onRootAttach(RootNode* root, std::size_t size, std::size_t blobSize) {
    maintainNodeBindInfo(*root, root);
    maintainSizeInfoOnRootBind(root, size, blobSize);
  }
  void onSubtreeAttach(RootNode& subtree, RootNode* root) {
    // Resets root in sub tree recursively.
    TraversalCallback pre = [&](Node& node, Ptr<Node>& ptr) { maintainNodeBindInfo(node, root); };
    subtree.Traverse(pre, NullTraversalCallback, NullNodePtr);
    maintainSizeInfoOnSubtreeAttach(subtree, root);
  }
  void onNodeBuild(Node* node) {
    node->internalOnBuild();
    node->OnBuild();
  }
};

// Builder helps to build a tree.
// The template parameter D is the derived class, a behavior tree class.
template <typename D = void>
class Builder : public _InternalBuilderBase {
 private:
  std::stack<InternalNode*> stack;
  // indent level to insert new node, starts from 1.
  int level;
  RootNode* root = nullptr;
  // Validate node.
  void validate(Node* node) {
    auto e = node->Validate();
    if (!e.empty()) {
      std::string s = "bt build: ";
      s += node->Name();
      s += ' ';
      s += e;
      throw std::runtime_error(s);
    }
  }

  // Validate indent level.
  void validateIndent() {
    if (level > stack.size()) {
      auto node = stack.top();
      std::string s = "bt build: too much indent ";
      s += "below ";
      s += node->Name();
      throw std::runtime_error(s);
    }
  }

  // pops an internal node from the stack.
  void pop() {
    validate(stack.top());  // validate before pop
    onNodeBuild(stack.top());
    stack.pop();
  }

  // Adjust stack to current indent level.
  void adjust() {
    validateIndent();
    while (level < stack.size()) pop();
  }

 protected:
  // Bind a tree root onto this builder.
  void bindRoot(RootNode& r) {
    stack.push(&r);
    root = &r;
    onRootAttach(root, sizeof(D), sizeof(typename D::Blob));
  }

  // Creates a leaf node.
  auto& attachLeafNode(Ptr<LeafNode> p) {
    adjust();
    // Append to stack's top as a child.
    onNodeBuild(p.get());
    stack.top()->Append(std::move(p));
    // resets level.
    level = 1;
    return *static_cast<D*>(this);
  }

  // Creates an internal node with optional children.
  auto& attachInternalNode(Ptr<InternalNode> p) {
    adjust();
    // Append to stack's top as a child, and replace the top.
    auto parent = stack.top();
    stack.push(p.get());
    parent->Append(std::move(p));
    // resets level.
    level = 1;
    return *static_cast<D*>(this);
  }

  // make a new node onto this tree, returns the unique_ptr.
  // Any node creation should use this function.
  template <TNode T, typename... Args>
  Ptr<T> make(bool skipActtach, Args... args) {
    auto p = std::make_unique<T>(std::forward<Args>(args)...);
    if (!skipActtach) onNodeAttach<T>(*p, root);
    return p;
  };

 public:
  Builder() : level(1), root(nullptr) {}
  ~Builder() {}

  // Increases indent level to append node.
  auto& _() {
    level++;
    return static_cast<D&>(*this);
  }

  // Should be called on the end of the build process.
  void End() {
    while (stack.size()) pop();  // clears the stack
  }

  ///////////////////////////////////
  // General creators.
  ///////////////////////////////////

  // C is a function to attach an arbitrary new Node.
  // It can be used to attach custom node implementation.
  // Code exapmle::
  //    root
  //    .C<MyCustomDecoratorNode>()
  //    ._().Action<A>()
  //    .End()
  //    ;
  template <TNode T, typename... Args>
  auto& C(Args... args) {
    if constexpr (std::is_base_of_v<LeafNode, T>)  // LeafNode
      return attachLeafNode(make<T>(false, std::forward<Args>(args)...));
    else  // InternalNode.
      return attachInternalNode(make<T>(false, std::forward<Args>(args)...));
  }

  // Attach a node through move, rarely used.
  template <TNode T>
  auto& M(T&& inst) {
    if constexpr (std::is_base_of_v<LeafNode, T>)  // LeafNode
      return attachLeafNode(make<T>(true, std::move(inst)));
    else  // InternalNode.
      return attachInternalNode(make<T>(true, std::move(inst)));
  }

  ///////////////////////////////////
  // CompositeNode creators
  ///////////////////////////////////

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

  ///////////////////////////////////
  // LeafNode creators
  ///////////////////////////////////

  // Creates an Action node by providing implemented Action class.
  // Code example::
  //  root
  //  .Action<MyActionClass>()
  //  .End();
  template <TAction Impl, typename... Args>
  auto& Action(Args&&... args) {
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
  auto& Condition(Args&&... args) {
    return C<Impl>(std::forward<Args>(args)...);
  }

  ///////////////////////////////////
  // DecoratorNode creators.
  ///////////////////////////////////

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
  auto& Not(ConditionArgs... args) {
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
  template <typename Clock = std::chrono::high_resolution_clock>
  auto& Timeout(std::chrono::milliseconds duration) {
    return C<TimeoutNode<Clock>>(duration, "Timeout");
  }

  // Delay creates a DelayNode.
  // Wait for given duration before execution of decorated node.
  // Code exapmle::
  //   root
  //   .Delay(3000ms)
  //   ._().Action<A>()
  //   .End();
  template <typename Clock = std::chrono::high_resolution_clock>
  auto& Delay(std::chrono::milliseconds duration) {
    return C<DelayNode<Clock>>(duration, "Delay");
  }

  // Retry creates a RetryNode.
  // It executes the decorated node for at most n times.
  // A retry will only be initiated if the decorated node fails.
  // Providing n=-1 for unlimited retry times.
  // Code exapmle::
  //   root
  //   .Retry(1, 3000ms)
  //   ._().Action<A>()
  //   .End();
  template <typename Clock = std::chrono::high_resolution_clock>
  auto& Retry(int n, std::chrono::milliseconds interval) {
    return C<RetryNode<Clock>>(n, interval, "Retry");
  }

  // Alias for Retry(-1, interval)
  template <typename Clock = std::chrono::high_resolution_clock>
  auto& RetryForever(std::chrono::milliseconds interval) {
    return C<RetryNode<Clock>>(-1, interval, "RetryForever");
  }

  // If creates a ConditionalRunNode.
  // It executes the decorated node only if the condition goes true.
  // Code example::
  //   root
  //   .If<CheckSomething>()
  //   ._().Action(DoSomething)()
  //   .End();
  template <TCondition Condition, typename... ConditionArgs>
  auto& If(ConditionArgs&&... args) {
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
  //   ._()._().Sequence()
  //   ._()._()._().Action<B>()
  //   ._()._()._().Action<C>()
  //   ._().Case([=](const Context& ctx) { return false; })
  //   ._()._().Action<D>()
  //   .End();
  auto& Switch() { return C<SelectorNode>("Switch"); }

  // Stateful version `Switch` based on StatefulSelectorNode.
  auto& StatefulSwitch() { return C<StatefulSelectorNode>("Switch*"); }

  // Alias to If, for working alongs with Switch.
  template <TCondition Condition, typename... ConditionArgs>
  auto& Case(ConditionArgs&&... args) {
    auto condition = make<Condition>(false, std::forward<ConditionArgs>(args)...);
    return C<ConditionalRunNode>(std::move(condition), "Case");
  }

  // Case creates a ConditionalRunNode from lambda function.
  auto& Case(ConditionNode::Checker checker) { return Case<ConditionNode>(checker); }

  ///////////////////////////////////
  // Subtree creators.
  ///////////////////////////////////

  // Attach a sub behavior tree into this tree.
  // Code example::
  //
  //    bt::Tree subtree;
  //    subtree
  //      .Parallel()
  //      ._().Action<A>()
  //      ._().Action<B>(;
  //      .End();
  //
  //    root
  //      .Sequence()
  //      ._().Subtree(std::move(subtree))
  //      .End();
  auto& Subtree(RootNode&& subtree) {
    onSubtreeAttach(subtree, root);
    // move to the tree
    return M<RootNode>(std::move(subtree));
  }
};

//////////////////////////////////////////////////////////////
/// Tree
///////////////////////////////////////////////////////////////

// Behavior Tree.
// Please keep this class simple enough.
class Tree : public RootNode, public Builder<Tree> {
 public:
  Tree(std::string_view name = "Root") : RootNode(name), Builder() { bindRoot(*this); }
};

}  // namespace bt

#endif
