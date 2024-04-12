// Copyright (c) 2024 Chao Wang <hit9@icloud.com>.
// License: BSD. https://github.com/hit9/bt.h
//
// A simple lightweight behavior tree library.
// Requirements: at least C++20.
//
// Code Overview
// ~~~~~~~~~~~~~
//
//   bt::Tree root;
//   root
//   .Sequence()
//   ._().Action<A>()
//   ._().Repeat(3)
//   ._()._().Action<B>()
//   ;
//
// Run Tick
// ~~~~~~~~~
//
//   bt::Context ctx;
//   root.Tick(ctx)
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

// Version: 0.1.0

#ifndef HIT9_BT_H
#define HIT9_BT_H

#include <algorithm>  // for max, min
#include <any>
#include <chrono>  // for milliseconds, high_resolution_clock
#include <functional>
#include <iostream>  // for cout, flush
#include <memory>    // for unique_ptr
#include <queue>     // for priority_queue
#include <stack>
#include <stdexcept>  // for runtime_error
#include <string>
#include <string_view>
#include <thread>       // for this_thread::sleep_for
#include <type_traits>  // for is_base_of_v
#include <unordered_set>
#include <vector>

namespace bt {

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

// The most base virtual class of all behavior nodes.
class Node {
 private:
  std::string name;

  // Internals
  ull lastSeq;           // seq of last execution.
  Status lastStatus;     // status of last execution.
  bool running = false;  // is still running?

 protected:
  // Internal method to visualize tree.
  virtual void makeVisualizeString(std::string& s, int depth, ull seq) {
    if (depth > 0) s += " |";
    for (int i = 1; i < depth; i++) s += "---|";
    if (depth > 0) s += "- ";
    if (lastSeq == seq) s += "\033[32m";  // color if catches up with seq.
    s += Name();
    s.push_back('(');
    s.push_back(statusRepr(lastStatus));
    s.push_back(')');
    if (lastSeq == seq) s += "\033[0m";
  }

  // Internal method to validate the node.
  // Returns error message, empty string for good.
  virtual std::string_view validate() const { return ""; }

  // firend with SingleNode and CompositeNode for accessbility to makeVisualizeString.
  friend class SingleNode;
  friend class CompositeNode;

  // firend with Builder for accessbility to validate.
  friend class Builder;

 public:
  Node(const std::string& name = "Node")
      : name(name), lastStatus(Status::UNDEFINED), lastSeq(0), running(false) {}
  virtual ~Node() = default;

  // Main entry function, should be called on every tick.
  Status Tick(const Context& ctx) {
    // First run of current round.
    if (!running) OnEnter();
    running = true;

    auto status = Update(ctx);
    lastStatus = status;
    lastSeq = ctx.seq;

    // Last run of current round.
    if (status == Status::FAILURE || status == Status::SUCCESS) {
      OnTerminate(status);
      running = false;  // reset
    }
    return status;
  }

  // Returns the name of this node.
  virtual std::string_view Name() const { return name; }
  // Returns last status of this node.
  bt::Status LastStatus() const { return lastStatus; }

  /////////////////////////////////////////
  // Public Virtual Functions To Override
  /////////////////////////////////////////

  // Hook function to be called on this node's first run.
  // Nice to call parent class' OnEnter before your implementation.
  virtual void OnEnter(){};

  // Hook function to be called once this node goes into success or failure.
  // Nice to call parent class' OnTerminate after your implementation
  virtual void OnTerminate(Status status){};

  // Main update function to be implemented by all subclasses.
  // It's the body part of function Tick().
  virtual Status Update(const Context& ctx) { return Status::SUCCESS; };

  // Returns the priority of this node, should be larger than 0, the larger the higher.
  // By default, all nodes' priorities are equal, to 0.
  // Providing this method is primarily for selecting children by dynamic priorities.
  // It's recommended to implement this function fast enough, since it will be called on each
  // tick. For instance, we may not need to do the calculation on every tick if it's complex.
  // Another optimization is to seperate calculation from getter, for example, pre-cache the result
  // somewhere on the blackboard, and just ask it from memory here.
  virtual uint Priority(const Context& ctx) const { return 0; }
};

// Concept TNode for all classes derived from Node.
template <typename T>
concept TNode = std::is_base_of_v<Node, T>;

// Alias
template <typename T>
using Ptr = std::unique_ptr<T>;

template <typename T>
using PtrList = std::vector<std::unique_ptr<T>>;

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

  std::string_view validate() const override { return child == nullptr ? "no child node provided" : ""; }

 public:
  SingleNode(const std::string& name = "SingleNode", Ptr<Node> child = nullptr)
      : InternalNode(name), child(std::move(child)) {}
  void Append(Ptr<Node> node) override { child = std::move(node); }
  uint Priority(const Context& ctx) const override { return child->Priority(ctx); }
};

////////////////////////////////////////////////
/// Node > InternalNode > CompositeNode
////////////////////////////////////////////////

// CompositeNode contains multiple children.
class CompositeNode : public InternalNode {
 protected:
  PtrList<Node> children;

  std::string_view validate() const override { return children.empty() ? "children empty" : ""; }
  // Should we consider ith child during this round?
  virtual bool considerable(int i) const { return true; }
  // Internal hook function to be called after a child is success.
  virtual void onChildSuccess(const int i){};
  // Internal hook function to be called after a child is failure.
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

  // Returns the max priority of considerable children.
  uint Priority(const Context& ctx) const override {
    uint ans = 0;
    for (int i = 0; i < children.size(); i++)
      if (considerable(i)) ans = std::max(ans, children[i]->Priority(ctx));
    return ans;
  }
};

//////////////////////////////////////////////////////////////
/// Node > InternalNode > CompositeNode > _Internal Impls
///////////////////////////////////////////////////////////////

// Always skip children that already succeeded or failure during current round.
class _InternalStatefulCompositeNode : virtual public CompositeNode {
 protected:
  // stores the index of children already succeeded or failure in this round.
  std::unordered_set<int> skiptable;
  bool considerable(int i) const override { return !skiptable.contains(i); }
  void tableInsert(const int i) { skiptable.insert(i); }

 public:
  using CompositeNode::CompositeNode;
  void OnTerminate(Status status) override { skiptable.clear(); }
};

// Priority related CompositeNode.
class _InternalPriorityCompositeNode : virtual public CompositeNode {
 protected:
  // Prepare priorities of considerable children on every tick.
  std::vector<uint> p;
  // Refresh priorities for considerable children.
  void refreshp(const Context& ctx) {
    if (p.size() < children.size()) p.resize(children.size());
    for (int i = 0; i < children.size(); i++)
      if (considerable(i)) p[i] = children[i]->Priority(ctx);
  }

 public:
  using CompositeNode::CompositeNode;
};

// SequenceNode and SelectorNode are sequential composite node, but not ParallelNode.
class _InternalSequentialCompositeNode : virtual public _InternalPriorityCompositeNode {
 protected:
  // Compare priorities between children, where a and b index.
  using PQ = std::priority_queue<int, std::vector<int>, std::function<bool(const int, const int)>>;
  PQ q;

  // SequenceNode and SelectorNode should implement update.
  // It should propagates tick() to chilren in the q.
  virtual Status update(const Context& ctx) = 0;

 public:
  _InternalSequentialCompositeNode(const std::string& name = "_InternalSequentialCompositeNode",
                                   PtrList<Node>&& cs = {})
      : _InternalPriorityCompositeNode(name, std::move(cs)) {
    //  priority from large to smaller, so use less, pa < pb
    //  order: from small to larger, so use greater, a > b
    auto cmp = [&](const int a, const int b) { return p[a] < p[b] || a > b; };
    PQ q1(cmp);
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
    // propagates ticks, one by one sequentially.
    return update(ctx);
  }
};

//////////////////////////////////////////////////////////////
/// Node > InternalNode > CompositeNode > SequenceNode
///////////////////////////////////////////////////////////////

class _InternalSequenceNodeBase : virtual public _InternalSequentialCompositeNode {
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

 public:
  using _InternalSequentialCompositeNode::_InternalSequentialCompositeNode;
};

// SequenceNode run children one by one, and succeeds only if all children succeed.
class SequenceNode final : public _InternalSequenceNodeBase {
 public:
  SequenceNode(const std::string& name = "Sequence", PtrList<Node>&& cs = {})
      : _InternalSequenceNodeBase(name, std::move(cs)) {}
};

// StatefulSequenceNode behaves like a SequenceNode, but instead of ticking children from the first, it
// starts from the running child instead.
class StatefulSequenceNode final : public _InternalStatefulCompositeNode, public _InternalSequenceNodeBase {
 protected:
  void onChildSuccess(const int i) override { tableInsert(i); }

 public:
  StatefulSequenceNode(const std::string& name = "Sequence*", PtrList<Node>&& cs = {})
      : _InternalSequenceNodeBase(name, std::move(cs)), _InternalStatefulCompositeNode() {}
};

//////////////////////////////////////////////////////////////
/// Node > InternalNode > CompositeNode > SelectorNode
///////////////////////////////////////////////////////////////

class _InternalSelectorNodeBase : virtual public _InternalSequentialCompositeNode {
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

 public:
  using _InternalSequentialCompositeNode::_InternalSequentialCompositeNode;
};

// SelectorNode succeeds if any child succeeds.
class SelectorNode final : public _InternalSelectorNodeBase {
 public:
  SelectorNode(const std::string& name = "Selector", PtrList<Node>&& cs = {})
      : _InternalSelectorNodeBase(name, std::move(cs)) {}
};

// StatefulSelectorNode behaves like a SelectorNode, but instead of ticking children from the first, it
// starts from the running child instead.
class StatefulSelectorNode : public _InternalStatefulCompositeNode, public _InternalSelectorNodeBase {
 protected:
  void onChildFailure(const int i) override { tableInsert(i); }

 public:
  StatefulSelectorNode(const std::string& name = "Selector*", PtrList<Node>&& cs = {})
      : _InternalSelectorNodeBase(name, std::move(cs)), _InternalStatefulCompositeNode() {}
};

//////////////////////////////////////////////////////////////
/// Node > InternalNode > CompositeNode > ParallelNode
///////////////////////////////////////////////////////////////

class _InternalParallelNodeBase : virtual public CompositeNode {
 public:
  using CompositeNode::CompositeNode;
  Status Update(const Context& ctx) {
    // Propagates tick to all considerable children.
    int cntFailure = 0, cntSuccess = 0, total = 0;

    for (int i = 0; i < children.size(); i++) {
      if (!considerable(i)) continue;
      total++;
      auto status = children[i]->Tick(ctx);
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
class ParallelNode final : virtual public _InternalParallelNodeBase {
 protected:
 public:
  ParallelNode(const std::string& name = "Parallel", PtrList<Node>&& cs = {})
      : _InternalParallelNodeBase(name, std::move(cs)) {}
};

// StatefulParallelNode behaves like a ParallelNode, but instead of ticking every child, it only tick the
// running children, aka skipping the succeeded children..
class StatefulParallelNode final : public _InternalStatefulCompositeNode, public _InternalParallelNodeBase {
 protected:
  void onChildSuccess(const int i) override { tableInsert(i); }

 public:
  StatefulParallelNode(const std::string& name = "Parallel*", PtrList<Node>&& cs = {})
      : _InternalParallelNodeBase(name, std::move(cs)), _InternalStatefulCompositeNode() {}
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

// RepeatNode repeats its child for exactly n times.
// Fails immediately if its child fails.
class RepeatNode : public DecoratorNode {
 private:
  // Count how many times already executed for this round.
  int cnt = 0;

 protected:
  // Times to repeat, -1 for forever, 0 for immediately success.
  int n;

 public:
  RepeatNode(int n, const std::string& name = "Repeat", Ptr<Node> child = nullptr)
      : DecoratorNode(name, std::move(child)), n(n) {}

  // Clears counter on enter.
  void OnEnter() override { cnt = 0; };
  // Reset counter on termination.
  void OnTerminate(Status status) override { cnt = 0; };

  Status Update(const Context& ctx) override {
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

// Timeout runs its child for at most given duration, fails on timeout.
template <typename Clock = std::chrono::high_resolution_clock>
class TimeoutNode : public DecoratorNode {
  using TimePoint = std::chrono::time_point<Clock>;

 private:
  // Time point of this node starts.
  TimePoint startAt = TimePoint::min();

 protected:
  std::chrono::milliseconds duration;

 public:
  TimeoutNode(std::chrono::milliseconds d, const std::string& name = "Timeout", Ptr<Node> child = nullptr)
      : DecoratorNode(name, std::move(child)), duration(d) {}

  void OnEnter() override { startAt = Clock::now(); };

  Status Update(const Context& ctx) override {
    // Check if timeout at first.
    auto now = Clock::now();
    if (now > startAt + duration) return Status::FAILURE;
    return child->Tick(ctx);
  }
};

// DelayNode runs its child node after given duration.
template <typename Clock = std::chrono::high_resolution_clock>
class DelayNode : public DecoratorNode {
  using Timepoint = std::chrono::time_point<Clock>;

 private:
  // The time point this node first run.
  Timepoint firstRunAt;

 protected:
  // Duration to wait.
  std::chrono::milliseconds duration;

 public:
  DelayNode(std::chrono::milliseconds duration, const std::string& name = "Delay", Ptr<Node> c = nullptr)
      : DecoratorNode(name, std::move(c)), duration(duration) {}

  void OnEnter() override { firstRunAt = Clock::now(); };
  void OnTerminate(Status status) override { firstRunAt = Timepoint::min(); };

  Status Update(const Context& ctx) override {
    auto now = Clock::now();
    if (now < firstRunAt + duration) return Status::RUNNING;
    return child->Tick(ctx);
  }
};

// RetryNode retries its child node on failure.
template <typename Clock = std::chrono::high_resolution_clock>
class RetryNode : public DecoratorNode {
  using TimePoint = std::chrono::time_point<Clock>;

 private:
  // Times already retried.
  int cnt = 0;
  // Timepoint last retried at.
  TimePoint lastRetryAt;

 protected:
  // Max retry times, -1 for unlimited.
  int maxRetries = -1;
  // Retry interval in milliseconds
  std::chrono::milliseconds interval;

 public:
  RetryNode(int maxRetries, std::chrono::milliseconds interval, const std::string& name = "Retry",
            Ptr<Node> child = nullptr)
      : DecoratorNode(name, std::move(child)),
        maxRetries(maxRetries),
        interval(interval),
        lastRetryAt(TimePoint::min()) {}

  void OnEnter() override {
    cnt = 0;
    lastRetryAt = TimePoint::min();
  }
  void OnTerminate(Status status) override {
    cnt = 0;
    lastRetryAt = status == Status::FAILURE ? Clock::now() : TimePoint::min();
  }

  Status Update(const Context& ctx) override {
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
class RootNode : public SingleNode {
 public:
  RootNode(const std::string& name = "Root") : SingleNode(name) {}
  Status Update(const Context& ctx) override { return child->Tick(ctx); }

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
};

//////////////////////////////////////////////////////////////
/// Tree Builder
///////////////////////////////////////////////////////////////

// Builder helps to build a tree.
class Builder {
 private:
  std::stack<InternalNode*> stack;
  int level;  // indent level to insert new node, starts from 1.

  // Validate node.
  void validate(Node* node) {
    auto e = node->validate();
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

  // Adjust stack to current indent level.
  void adjust() {
    validateIndent();
    while (level < stack.size()) {
      validate(stack.top());  // validate before pop
      stack.pop();
    }
  }

 protected:
  // Bind a tree root onto this builder.
  void bindRoot(RootNode& root) { stack.push(&root); }

  // Creates a leaf node.
  Builder& attachLeafNode(Ptr<LeafNode> p) {
    adjust();
    // Append to stack's top as a child.
    stack.top()->Append(std::move(p));
    // resets level.
    level = 1;
    return *this;
  }

  // Creates an internal node with optional children.
  Builder& attachInternalNode(Ptr<InternalNode> p) {
    adjust();
    // Append to stack's top as a child, and replace the top.
    auto parent = stack.top();
    stack.push(p.get());
    parent->Append(std::move(p));
    // resets level.
    level = 1;
    return *this;
  }

 public:
  Builder() : level(1) {}
  ~Builder() {}

  // Increases indent level to append node.
  Builder& _() {
    level++;
    return *this;
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
  template <TNode T, typename... Args>
  Builder& C(Args... args) {
    if constexpr (std::is_base_of_v<LeafNode, T>)  // LeafNode
      return attachLeafNode(std::make_unique<T>(std::forward<Args>(args)...));
    else  // InternalNode.
      return attachInternalNode(std::make_unique<T>(std::forward<Args>(args)...));
  }

  ///////////////////////////////////
  // CompositeNode creators
  ///////////////////////////////////

  // Creates a sequence node.
  // Parameter `cs` is the optional initial children for this node.
  // A SequenceNode executes its children one by one sequentially,
  // it succeeds only if all children succeed.
  Builder& Sequence(PtrList<Node>&& cs = {}) { return C<SequenceNode>("Sequence", std::move(cs)); }

  // Creates a stateful sequence node.
  // It behaves like a sequence node, executes its children sequentially, succeeds if all children succeed,
  // fails if any child fails. What's the difference is, a StatefulSequenceNode starts from running child
  // instead of always starting from the first child.
  Builder& StatefulSequence(PtrList<Node>&& cs = {}) {
    return C<StatefulSequenceNode>("Sequence*", std::move(cs));
  }

  // Creates a selector node.
  // Parameter `cs` is the optional initial children for this node.
  // A SelectorNode succeeds if any child succeeds, fails only if all children fail.
  Builder& Selector(PtrList<Node>&& cs = {}) { return C<SelectorNode>("Selector", std::move(cs)); }

  // Creates a stateful selector node.
  // It behaves like a selector node, executes its children sequentially, succeeds if any child succeeds,
  // fails if all child fail. What's the difference is, a StatefulSelectorNode starts from running child
  // instead of always starting from the first child.
  Builder& StatefulSelector(PtrList<Node>&& cs = {}) {
    return C<StatefulSelectorNode>("Selector*", std::move(cs));
  }

  // Creates a parallel node.
  // Parameter `cs` is the optional initial children for this node.
  // A ParallelNode executes its children parallelly.
  // It succeeds if all children succeed, and fails if any child fails.
  Builder& Parallel(PtrList<Node>&& cs = {}) { return C<ParallelNode>("Parallel", std::move(cs)); }

  // Creates a stateful parallel node.
  // It behaves like a parallel node, executes its children parallelly, succeeds if all succeed, fails if all
  // child fail. What's the difference is, a StatefulParallelNode will skip the "already success" children
  // instead of executing every child all the time.
  Builder& StatefulParallel(PtrList<Node>&& cs = {}) {
    return C<StatefulParallelNode>("Parallel*", std::move(cs));
  }

  ///////////////////////////////////
  // LeafNode creators
  ///////////////////////////////////

  // Creates an ActionNode by providing an unique_ptr to implemented Action object.
  // Code example::
  //  root
  //  .Action(std::make_unique<MyActionClass>())
  //  ;
  Builder& Action(Ptr<ActionNode> node) { return attachLeafNode(std::move(node)); }

  // Creates an Action node by providing implemented Action class.
  // Code example::
  //  root
  //  .Action<MyActionClass>()
  //  ;
  template <TAction Impl, typename... Args>
  Builder& Action(Args&&... args) {
    return C<Impl>(std::forward<Args>(args)...);
  }

  // Creates a ConditionNode by providing an unique_ptr to implemented Action object.
  // Code example::
  //   root
  //   .Sequence()
  //   ._().Condition(std::make_unique<MyConditionClass>())
  //   ._().Action<A>()
  //   ;
  Builder& Condition(Ptr<ConditionNode> node) { return attachLeafNode(std::move(node)); }

  // Creates a ConditionNode from a lambda function.
  // Code example::
  //   root
  //   .Sequence()
  //   ._().Condition([=](const Context& ctx) { return false;})
  //   ._().Action<A>()
  //   ;
  Builder& Condition(ConditionNode::Checker checker) { return C<ConditionNode>(checker); }

  // Creates a ConditionNode by providing implemented Condition class.
  // Code example::
  //   root
  //   .Sequence()
  //   ._().Condition<MyConditionClass>()
  //   ._().Action<A>()
  //   ;
  template <TCondition Impl, typename... Args>
  Builder& Condition(Args&&... args) {
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
  Builder& Invert(Ptr<Node> child = nullptr) { return C<InvertNode>("Invert", std::move(child)); }

  // Alias to Invert, just named 'Not'.
  // Code exapmle::
  //   root
  //   .Not()
  //   ._().Condition<A>()
  Builder& Not(Ptr<Node> child = nullptr) { return C<InvertNode>("Not", std::move(child)); }

  // Creates a invert condition.
  // Code exapmle::
  //   root
  //   .Sequence()
  //   ._().Not<IsXXX>()
  //   ._().Action<DoSomething>()
  template <TCondition Condition, typename... ConditionArgs>
  Builder& Not(ConditionArgs... args) {
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
  Builder& Repeat(int n, Ptr<Node> child = nullptr) { return C<RepeatNode>(n, "Repeat", std::move(child)); }

  // Alias to Repeat.
  // Code exapmle::
  //   root
  //   .Loop(3)
  //   ._().Action<A>()
  Builder& Loop(int n, Ptr<Node> child = nullptr) { return C<RepeatNode>(n, "Loop", std::move(child)); }

  // Timeout creates a TimeoutNode.
  // It executes the decorated node for at most given duration.
  // Code exapmle::
  //   root
  //   .Timeout(3000ms)
  //   ._().Action<A>()
  template <typename Clock = std::chrono::high_resolution_clock>
  Builder& Timeout(std::chrono::milliseconds duration, Ptr<Node> child = nullptr) {
    return C<TimeoutNode<Clock>>(duration, "Timeout", std::move(child));
  }

  // Delay creates a DelayNode.
  // Wait for given duration before execution of decorated node.
  // Code exapmle::
  //   root
  //   .Delay(3000ms)
  //   ._().Action<A>()
  template <typename Clock = std::chrono::high_resolution_clock>
  Builder& Delay(std::chrono::milliseconds duration, Ptr<Node> child = nullptr) {
    return C<DelayNode<Clock>>(duration, "Delay", std::move(child));
  }

  // Retry creates a RetryNode.
  // It executes the decorated node for at most n times.
  // A retry will only be initiated if the decorated node fails.
  // Providing n=-1 for unlimited retry times.
  // Code exapmle::
  //   root
  //   .Retry(1, 3000ms)
  //   ._().Action<A>()
  template <typename Clock = std::chrono::high_resolution_clock>
  Builder& Retry(int n, std::chrono::milliseconds interval, Ptr<Node> child = nullptr) {
    return C<RetryNode<Clock>>(n, interval, "Retry", std::move(child));
  }

  // Alias for Retry(-1, interval)
  template <typename Clock = std::chrono::high_resolution_clock>
  Builder& RetryForever(std::chrono::milliseconds interval, Ptr<Node> child = nullptr) {
    return C<RetryNode<Clock>>(-1, interval, "RetryForever", std::move(child));
  }

  // If creates a ConditionalRunNode.
  // It executes the decorated node only if the condition goes true.
  // Code example::
  //   root
  //   .If<CheckSomething>()
  //   ._().Action(DoSomething)()
  template <TCondition Condition, typename... ConditionArgs>
  Builder& If(ConditionArgs&&... args) {
    auto condition = std::make_unique<Condition>(std::forward<ConditionArgs>(args)...);
    return C<ConditionalRunNode>(std::move(condition), "If");
  }

  // If creates a ConditionalRunNode from lambda function.
  // Code example::
  //  root
  //  .If([=](const Context& ctx) { return false; })
  //  ;
  Builder& If(ConditionNode::Checker checker) { return If<ConditionNode>(checker); }

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
  //   ;
  Builder& Switch() { return C<SelectorNode>("Switch"); }

  // Stateful version `Switch` based on StatefulSelectorNode.
  Builder& StatefulSwitch() { return C<StatefulSelectorNode>("Switch*"); }

  // Alias to If, for working alongs with Switch.
  template <TCondition Condition, typename... ConditionArgs>
  Builder& Case(ConditionArgs&&... args) {
    auto condition = std::make_unique<Condition>(std::forward<ConditionArgs>(args)...);
    return C<ConditionalRunNode>(std::move(condition), "Case");
  }

  // Case creates a ConditionalRunNode from lambda function.
  Builder& Case(ConditionNode::Checker checker) { return Case<ConditionNode>(checker); }

  ///////////////////////////////////
  // Subtree creators.
  ///////////////////////////////////

  // Attach a sub behavior tree into this tree.
  // Code example::
  //    bt::Tree subtree;
  //    subtree
  //      .Parallel()
  //      ._().Action<A>()
  //      ._().Action<B>();
  //    root
  //      .Sequence()
  //      ._().Subtree(std::move(subtree))
  //      ;
  Builder& Subtree(RootNode&& tree) { return C<RootNode>(std::move(tree)); }

  // Subtree function that receives an unique_ptr.
  Builder& Subtree(Ptr<RootNode> tree) { return attachInternalNode(std::move(tree)); }
};

//////////////////////////////////////////////////////////////
/// Tree
///////////////////////////////////////////////////////////////

// Behavior Tree.
class Tree : public RootNode, public Builder {
 public:
  Tree(std::string name = "Root") : RootNode(name), Builder() { bindRoot(*this); }

  // Handy function to run tick loop forever.
  // Parameter interval specifies the time interval between ticks.
  // Parameter visualize enables debugging visualization on the console.
  template <typename Clock = std::chrono::high_resolution_clock>
  void TickForever(Context& ctx, std::chrono::nanoseconds interval, bool visualize = false) {
    auto lastTickAt = Clock::now();

    while (true) {
      auto nextTickAt = lastTickAt + interval;

      // Time delta between last tick and current tick.
      ctx.delta = Clock::now() - lastTickAt;
      ++ctx.seq;
      Tick(ctx);
      if (visualize) Visualize(ctx.seq);

      // Catch up with next tick.
      lastTickAt = Clock::now();
      if (lastTickAt < nextTickAt) {
        std::this_thread::sleep_for(nextTickAt - lastTickAt);
      }
    }
  }
};

}  // namespace bt

#endif
