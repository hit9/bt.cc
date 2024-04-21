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
// 1. Nodes store no data states, data and behaviors are separated.
// 2. Builds a behavior tree in tree structure codes, concise and expressive,
//    and supports to extend the builder.
// 3. Built-in multiple decorative nodes, and supports custom decoration nodes,
// 4. Supports composite nodes with priority child nodes, and random selector.
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
//   // A TreeBlob contains a continuous buffer that holds all the internal state data.
//   bt::TreeBlob blob<4*1024>;
//
// Run ticking:
//
//   bt::Context ctx;
//
//   while(...) { // In the ticking loop.
//     for (auto& blob : allBlobs) { // for each blob
//       root.BindBlob(blob);
//       root.Tick(ctx)
//       root.ClearBlob();
//     }
//   }
//
// Classes Structure
// ~~~~~~~~~~~~~~~~~
//
//   Node
//    | InternalNode
//    |   | SinleNode
//    |   |  | RootNode
//    |   |  | DecoratorNode
//    |   | CompositeNode
//    |   |  | SelectorNode
//    |   |  | ParallelNode
//    |   |  | SequenceNode
//    | LeafNode
//    |   | ActionNode
//    |   | ConditionNode

// Version: 0.2.3

#ifndef HIT9_BT_H
#define HIT9_BT_H

#include <algorithm>  // for max, min
#include <any>
#include <cassert>
#include <chrono>  // for milliseconds, high_resolution_clock
#include <cstdint>
#include <cstring>
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
#include <unordered_map>
#include <unordered_set>
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
/// TreeBlob
////////////////////////////

// TreeBlob's interface.
class ITreeBlob {
 public:
  // Allocates buffer for a node by providing a size.
  // throws runtime_error if overflows.
  virtual void* Allocate(NodeId id, std::size_t size) = 0;
  // Returns the raw pointer to the blob's buffer for a node.
  virtual void* Get(NodeId id) const = 0;
};

// A TreeBlob constains a continuous buffer that stores all nodes's states data,
// e.g. for all entities.
template <std::size_t N>
class TreeBlob : public ITreeBlob {
 private:
  unsigned int offset;
  uint8_t buffer[N];
  std::unordered_map<NodeId, unsigned int> m;  // Nodes => offset

 public:
  TreeBlob() : offset(0) { memset(buffer, 0, sizeof(buffer)); }

  void* Allocate(NodeId id, std::size_t size) override {
    if (offset + size > sizeof(buffer)) throw std::runtime_error("bt: TreeBlob overflows");
    m[id] = offset;
    auto p = buffer + offset;
    offset += size;
    return p;
  }

  void* Get(NodeId id) const override {
    if (m.find(id) == m.end()) return nullptr;
    return buffer + m.at(id);
  }
};

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

////////////////////////////
/// Node
////////////////////////////

// RootNode Interface.
class IRootNode {
 public:
  // Returns the current binding TreeBlob's pointer.
  virtual ITreeBlob* GetBlob(void) const = 0;
};

// NodeBlob is the base class to store node's internal states.
// Any member within a Node Blob should be guaranteed to be embedded inline,
// i.e. there will be no pointers to the outside.
struct NodeBlob {
  ull lastSeq;           // seq of last execution.
  Status lastStatus;     // status of last execution.
  bool running = false;  // is still running?
};

// Concept TNodeBlob for all classes derived from NodeInternalState.
template <typename T>
concept TNodeBlob = std::is_base_of_v<NodeBlob, T>;

// The most base class of all behavior nodes.
class Node {
 private:
  std::string name;

 protected:
  NodeId id;
  IRootNode* root;  // holding a pointer to the root.

  // Internal method to visualize tree.
  virtual void makeVisualizeString(std::string& s, int depth, ull seq) {
    auto b = GetNodeBlob<NodeBlob>();
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

  // firend with SingleNode and CompositeNode for accessbility to makeVisualizeString.
  friend class SingleNode;
  friend class CompositeNode;

 public:
  Node(const std::string& name = "Node") : name(name), id(0), root(nullptr) {}
  virtual ~Node() = default;

  // Return a raw pointer to the node blob for this node.
  // Use this method to access entity-related data and states.
  template <TNodeBlob B>
  B* GetNodeBlob() const {
    assert(root != nullptr);
    assert(root->GetBlob() != nullptr);
    auto p = root->GetBlob()->Get(id);
    if (p == nullptr) p = root->GetBlob()->Allocate(id, sizeof(B));
    return static_cast<B*>(p);
  }

  // Main entry function, should be called on every tick.
  Status Tick(const Context& ctx) {
    auto b = GetNodeBlob<NodeBlob>();
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

  // Returns the name of this node.
  virtual std::string_view Name() const { return name; }
  // Returns last status of this node.
  bt::Status LastStatus() const {
    auto b = GetNodeBlob<NodeBlob>();
    return b->lastStatus;
  }

  // Validate whether the node is builded correctly.
  // Returns error message, empty string for good.
  virtual std::string_view Validate() const { return ""; }

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
};

// Concept TNode for all classes derived from Node.
template <typename T>
concept TNode = std::is_base_of_v<Node, T>;

// Alias
template <typename T>
using Ptr = std::unique_ptr<T>;

template <typename T>
using PtrList = std::vector<Ptr<T>>;

////////////////////////////
/// Node > LeafNode
////////////////////////////

// LeafNode is a class contains no children.
class LeafNode : public Node {
 public:
  LeafNode(const std::string& name = "LeafNode") : Node(name) {}
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
  ConditionNode(Checker checker = nullptr, const std::string& name = "Condition")
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
  ActionNode(const std::string& name = "Action") : LeafNode(name) {}

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
  InternalNode(const std::string& name = "InternalNode") : Node(name) {}
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
  SingleNode(const std::string& name = "SingleNode", Ptr<Node> child = nullptr)
      : InternalNode(name), child(std::move(child)) {}
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
  CompositeNode(const std::string& name = "CompositeNode", PtrList<Node>&& cs = {}) : InternalNode(name) {
    children.swap(cs);
  }
  void Append(Ptr<Node> node) override { children.push_back(std::move(node)); }
  std::string_view Validate() const override { return children.empty() ? "children empty" : ""; }

  // Returns the max priority of considerable children.
  unsigned int Priority(const Context& ctx) const override {
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
  // stores the index of children already succeeded or failed in this round.
  std::unordered_set<int> skipTable;
};

// Always skip children that already succeeded or failure during current round.
class _InternalStatefulCompositeNode : virtual public CompositeNode {
 protected:
  bool considerable(int i) const override {
    auto& skipTable = GetNodeBlob<_InternalStatefulCompositeNodeBlob>()->skipTable;
    return !(skipTable.contains(i));
  }
  void skip(const int i) {
    auto& skipTable = GetNodeBlob<_InternalStatefulCompositeNodeBlob>()->skipTable;
    skipTable.insert(i);
  }

 public:
  void OnTerminate(const Context& ctx, Status status) override {
    auto& skipTable = GetNodeBlob<_InternalStatefulCompositeNodeBlob>()->skipTable;
    skipTable.clear();
  }
};

// Priority related CompositeNode.
class _InternalPriorityCompositeNode : virtual public CompositeNode {
 protected:
  // Prepare priorities of considerable children on every tick.
  // p[i] stands for i'th child's priority.
  // Since p will be refreshed on each tick, so it's stateless.
  std::vector<unsigned int> p;
  // Refresh priorities for considerable children.
  void refreshp(const Context& ctx) {
    if (p.size() < children.size()) p.resize(children.size());
    for (int i = 0; i < children.size(); i++)
      if (considerable(i)) p[i] = children[i]->Priority(ctx);
  }

  // Compare priorities between children, where a and b are indexes.
  // q is cleared on each tick, so it's also stateless.
  std::priority_queue<int, std::vector<int>, std::function<bool(const int, const int)>> q;

  // update is an internal method to propagates tick() to chilren in the q.
  // it will be called by Update.
  virtual Status update(const Context& ctx) = 0;

 public:
  _InternalPriorityCompositeNode() {
    // priority from large to smaller, so use `less`: pa < pb
    // order: from small to larger, so use `greater`: a > b
    auto cmp = [&](const int a, const int b) { return p[a] < p[b] || a > b; };
    // swap in the real comparator
    decltype(q) q1(cmp);
    q.swap(q1);
  }

  Status Update(const Context& ctx) override {
    // clear q
    while (q.size()) q.pop();
    // refresh priorities
    refreshp(ctx);
    // enqueue
    for (int i = 0; i < children.size(); i++)
      if (considerable(i)) q.push(i);
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
    while (q.size()) {
      auto i = q.top();
      q.pop();
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
  SequenceNode(const std::string& name = "Sequence", PtrList<Node>&& cs = {})
      : CompositeNode(name, std::move(cs)), _InternalPriorityCompositeNode() {}
};

// StatefulSequenceNode behaves like a SequenceNode, but instead of ticking children from the first, it
// starts from the running child instead.
class StatefulSequenceNode final : public _InternalStatefulCompositeNode, public _InternalSequenceNodeBase {
 protected:
  void onChildSuccess(const int i) override { skip(i); }

 public:
  StatefulSequenceNode(const std::string& name = "Sequence*", PtrList<Node>&& cs = {})
      : CompositeNode(name, std::move(cs)), _InternalPriorityCompositeNode() {}
};

//////////////////////////////////////////////////////////////
/// Node > InternalNode > CompositeNode > SelectorNode
///////////////////////////////////////////////////////////////

class _InternalSelectorNodeBase : virtual public _InternalPriorityCompositeNode {
 protected:
  Status update(const Context& ctx) override {
    // select a success children.
    while (q.size()) {
      auto i = q.top();
      q.pop();
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
  SelectorNode(const std::string& name = "Selector", PtrList<Node>&& cs = {})
      : CompositeNode(name, std::move(cs)), _InternalPriorityCompositeNode() {}
};

// StatefulSelectorNode behaves like a SelectorNode, but instead of ticking children from the first, it
// starts from the running child instead.
class StatefulSelectorNode : public _InternalStatefulCompositeNode, public _InternalSelectorNodeBase {
 protected:
  void onChildFailure(const int i) override { skip(i); }

 public:
  StatefulSelectorNode(const std::string& name = "Selector*", PtrList<Node>&& cs = {})
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
    refreshp(ctx);
    return update(ctx);
  }
};

// RandomSelectorNode selects children via weighted random selection.
class RandomSelectorNode final : public _InternalRandomSelectorNodeBase {
 public:
  RandomSelectorNode(const std::string& name = "RandomSelector", PtrList<Node>&& cs = {})
      : CompositeNode(name, std::move(cs)), _InternalPriorityCompositeNode() {}
};

// StatefulRandomSelectorNode behaves like RandomSelectorNode.
// But it won't reconsider failure children during a round.
class StatefulRandomSelectorNode final : virtual public _InternalStatefulCompositeNode,
                                         virtual public _InternalRandomSelectorNodeBase {
 protected:
  void onChildFailure(const int i) override { skip(i); }

 public:
  StatefulRandomSelectorNode(const std::string& name = "RandomSelector*", PtrList<Node>&& cs = {})
      : CompositeNode(name, std::move(cs)), _InternalPriorityCompositeNode() {}
};

//////////////////////////////////////////////////////////////
/// Node > InternalNode > CompositeNode > ParallelNode
///////////////////////////////////////////////////////////////

class _InternalParallelNodeBase : virtual public _InternalPriorityCompositeNode {
  Status update(const Context& ctx) override {
    // Propagates tick to all considerable children.
    int cntFailure = 0, cntSuccess = 0, total = 0;
    while (q.size()) {
      auto i = q.top();
      q.pop();
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
  ParallelNode(const std::string& name = "Parallel", PtrList<Node>&& cs = {})
      : CompositeNode(name, std::move(cs)), _InternalPriorityCompositeNode() {}
};

// StatefulParallelNode behaves like a ParallelNode, but instead of ticking every child, it only tick the
// running children, aka skipping the succeeded children..
class StatefulParallelNode final : public _InternalStatefulCompositeNode, public _InternalParallelNodeBase {
 protected:
  void onChildSuccess(const int i) override { skip(i); }

 public:
  StatefulParallelNode(const std::string& name = "Parallel*", PtrList<Node>&& cs = {})
      : CompositeNode(name, std::move(cs)), _InternalPriorityCompositeNode() {}
};

//////////////////////////////////////////////////////////////
/// Node > InternalNode > CompositeNode > Decorator
///////////////////////////////////////////////////////////////

// DecoratorNode decorates a single child node.
class DecoratorNode : public SingleNode {
 public:
  DecoratorNode(const std::string& name = "Decorator", Ptr<Node> child = nullptr)
      : SingleNode(name, std::move(child)) {}

  // To create a custom DecoratorNode.
  // You should derive from DecoratorNode and override the function Update.
  // Status Update(const Context&) override;
};

// InvertNode inverts its child's status.
class InvertNode : public DecoratorNode {
 public:
  InvertNode(const std::string& name = "Invert", Ptr<Node> child = nullptr)
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
  Ptr<ConditionNode> condition;

 public:
  ConditionalRunNode(Ptr<ConditionNode> condition = nullptr, const std::string& name = "ConditionalRun",
                     Ptr<Node> child = nullptr)
      : DecoratorNode(name + '<' + std::string(condition->Name()) + '>', std::move(child)),
        condition(std::move(condition)) {}

  Status Update(const Context& ctx) override {
    if (condition->Tick(ctx) == Status::SUCCESS) return child->Tick(ctx);
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
  RepeatNode(int n, const std::string& name = "Repeat", Ptr<Node> child = nullptr)
      : DecoratorNode(name, std::move(child)), n(n) {}

  // Clears counter on enter.
  void OnEnter(const Context& ctx) override { GetNodeBlob<RepeatNodeBlob>()->cnt = 0; }
  // Reset counter on termination.
  void OnTerminate(const Context& ctx, Status status) override { GetNodeBlob<RepeatNodeBlob>()->cnt = 0; }

  Status Update(const Context& ctx) override {
    auto& cnt = GetNodeBlob<RepeatNodeBlob>()->cnt;
    if (n == 0) return Status::SUCCESS;
    auto status = child->Tick(ctx);
    if (status == Status::RUNNING) return Status::RUNNING;
    if (status == Status::FAILURE) return Status::FAILURE;
    // Count success until n times, -1 will never stop.
    if (++cnt == n) return Status::SUCCESS;
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
  TimeoutNode(std::chrono::milliseconds d, const std::string& name = "Timeout", Ptr<Node> child = nullptr)
      : DecoratorNode(name, std::move(child)), duration(d) {}

  void OnEnter(const Context& ctx) override {
    GetNodeBlob<TimeoutNodeBlob<Clock>>()->startAt = Clock::now();
  };

  Status Update(const Context& ctx) override {
    auto& startAt = GetNodeBlob<TimeoutNodeBlob<Clock>>()->startAt;
    // Check if timeout at first.
    auto now = Clock::now();
    if (now > startAt + duration) return Status::FAILURE;
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
  DelayNode(std::chrono::milliseconds duration, const std::string& name = "Delay", Ptr<Node> c = nullptr)
      : DecoratorNode(name, std::move(c)), duration(duration) {}

  void OnEnter(const Context& ctx) override {
    GetNodeBlob<DelayNodeBlob<Clock>>()->firstRunAt = Clock::now();
  };
  void OnTerminate(const Context& ctx, Status status) override {
    GetNodeBlob<DelayNodeBlob<Clock>>()->ffirstRunAt = TimePoint<Clock>::min();
  };

  Status Update(const Context& ctx) override {
    auto b = GetNodeBlob<DelayNodeBlob<Clock>>();
    auto now = Clock::now();
    if (now < b->firstRunAt + duration) return Status::RUNNING;
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
  RetryNode(int maxRetries, std::chrono::milliseconds interval, const std::string& name = "Retry",
            Ptr<Node> child = nullptr)
      : DecoratorNode(name, std::move(child)), maxRetries(maxRetries), interval(interval) {}

  void OnEnter(const Context& ctx) override {
    auto b = GetNodeBlob<RetryNodeBlob>();
    b->cnt = 0;
    b->lastRetryAt = TimePoint<Clock>::min();
  }
  void OnTerminate(const Context& ctx, Status status) override {
    auto b = GetNodeBlob<RetryNodeBlob>();
    b->cnt = 0;
    b->lastRetryAt = status == Status::FAILURE ? Clock::now() : TimePoint<Clock>::min();
  }

  Status Update(const Context& ctx) override {
    auto b = GetNodeBlob<RetryNodeBlob>();
    auto& cnt = b->cnt;
    auto& lastRetryAt = b->lastRetryAt;

    if (maxRetries != -1 && cnt > maxRetries) return Status::FAILURE;

    // If has failures before, and retry timepoint isn't arriving.
    auto now = Clock::now();
    if (cnt > 0 && now < lastRetryAt + interval) return Status::RUNNING;

    // Time to run/retry.
    auto status = child->Tick(ctx);
    switch (status) {
      case Status::RUNNING:
        [[fallthrough]];
      case Status::SUCCESS:
        return status;
      default:
        // Failure
        if (++cnt > maxRetries && maxRetries != -1) return Status::FAILURE;  // exeeds max retries.
        return Status::RUNNING;                                              // continues retry
    }
  }
};

//////////////////////////////////////////////////////////////
/// Node > SingleNode > RootNode
///////////////////////////////////////////////////////////////

// RootNode is a SingleNode.
class RootNode : public SingleNode, public IRootNode {
 protected:
  ITreeBlob* blob = nullptr;  // Current blob.

 public:
  RootNode(const std::string& name = "Root") : SingleNode(name) {}
  Status Update(const Context& ctx) override { return child->Tick(ctx); }

  //////////////////////////
  /// Blob Apis
  //////////////////////////

  // Binds a tree blob.
  void BindBlob(ITreeBlob& b) { blob = &b; }
  // Returns current tree blob.
  ITreeBlob* GetBlob(void) const override { return blob; }
  // Clears current tree blob.
  void ClearBlob() { blob = nullptr; }

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

// Builder helps to build a tree.
// The template parameter D is the derived class, a behavior tree class.
template <typename D = void>
class Builder {
 private:
  std::stack<InternalNode*> stack;
  int level;  // indent level to insert new node, starts from 1.
  IRootNode* root;

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
    stack.top()->OnBuild();
    stack.pop();
  }

  // Adjust stack to current indent level.
  void adjust() {
    validateIndent();
    while (level < stack.size()) pop();
  }

 protected:
  // Bind a tree root onto this builder.
  void bindRoot(RootNode& r) { stack.push(&r), root = &r; }

  // Creates a leaf node.
  auto& attachLeafNode(Ptr<LeafNode> p) {
    adjust();
    // Append to stack's top as a child.
    p->OnBuild();
    stack.top()->Append(std::move(p));
    // resets level.
    level = 1;
    return static_cast<D&>(*this);
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
    return static_cast<D&>(*this);
  }

 public:
  Builder() : level(1) {}
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

  // C is a function to attach an arbitrary Node.
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
      return attachLeafNode(std::make_unique<T>(std::forward<Args>(args)...));
    else  // InternalNode.
      return attachInternalNode(std::make_unique<T>(std::forward<Args>(args)...));
  }

  ///////////////////////////////////
  // CompositeNode creators
  ///////////////////////////////////

  // A SequenceNode executes its children one by one sequentially,
  // it succeeds only if all children succeed.
  auto& Sequence() { return C<SequenceNode>("Sequence"); }

  // A StatefulSequenceNode behaves like a sequence node, executes its children sequentially, succeeds if all
  // children succeed, fails if any child fails. What's the difference is, a StatefulSequenceNode skips the
  // succeeded children instead of always starting from the first child.
  auto& StatefulSequence() { return C<StatefulSequenceNode>("Sequence*"); }

  // A SelectorNode succeeds if any child succeeds, fails only if all children fail.
  auto& Selector() { return C<SelectorNode>("Selector"); }

  // A StatefulSelectorNode behaves like a selector node, executes its children sequentially, succeeds if any
  // child succeeds, fails if all child fail. What's the difference is, a StatefulSelectorNode skips the
  // failure children instead of always starting from the first child.
  auto& StatefulSelector() { return C<StatefulSelectorNode>("Selector*"); }

  // A ParallelNode executes its children parallelly.
  // It succeeds if all children succeed, and fails if any child fails.
  auto& Parallel() { return C<ParallelNode>("Parallel"); }

  // A StatefulParallelNode behaves like a parallel node, executes its children parallelly, succeeds if all
  // succeed, fails if all child fail. What's the difference is, a StatefulParallelNode will skip the "already
  // success" children instead of executing every child all the time.
  auto& StatefulParallel() { return C<StatefulParallelNode>("Parallel*"); }

  // A RandomSelectorNode determines a child via weighted random selection.
  // It continues to randomly select a child, propagating tick, until some child succeeds.
  auto& RandomSelector() { return C<RandomSelectorNode>("RandomSelector"); }

  // A StatefulRandomSelector behaves like a random selector node, the difference is, a StatefulRandomSelector
  // will skip already failed children during a round.
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
    return C<InvertNode>("Not", std::make_unique<Condition>(std::forward<ConditionArgs>(args)...));
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
    auto condition = std::make_unique<Condition>(std::forward<ConditionArgs>(args)...);
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
    auto condition = std::make_unique<Condition>(std::forward<ConditionArgs>(args)...);
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
  auto& Subtree(RootNode&& tree) { return C<RootNode>(std::move(tree)); }
};

//////////////////////////////////////////////////////////////
/// Tree
///////////////////////////////////////////////////////////////

// Behavior Tree.
// Please keep this class simple enough.
class Tree : public RootNode, public Builder<Tree> {
 public:
  Tree(std::string name = "Root") : RootNode(name), Builder() { bindRoot(*this); }
};

}  // namespace bt

#endif
