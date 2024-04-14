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

// Version: 0.2.0

#ifndef HIT9_BT_H
#define HIT9_BT_H

#include <algorithm>  // for max, min
#include <any>
#include <chrono>  // for milliseconds, high_resolution_clock
#include <cstdint>
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

  // Returns the priority of this node, should be strictly larger than 0, the larger the higher.
  // By default, all nodes' priorities are equal, to 1.
  // Providing this method is primarily for selecting children by dynamic priorities.
  // It's recommended to implement this function fast enough, since it will be called on each
  // tick. For instance, we may not need to do the calculation on every tick if it's complex.
  // Another optimization is to separate calculation from getter, for example, pre-cache the result
  // somewhere on the blackboard, and just ask it from memory here.
  virtual uint Priority(const Context& ctx) const { return 1; }
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
  // stores the index of children already succeeded or failed in this round.
  std::unordered_set<int> skipTable;
  bool considerable(int i) const override { return !skipTable.contains(i); }
  void skip(const int i) { skipTable.insert(i); }

 public:
  void OnTerminate(Status status) override { skipTable.clear(); }
};

// Priority related CompositeNode.
class _InternalPriorityCompositeNode : virtual public CompositeNode {
 protected:
  // Prepare priorities of considerable children on every tick.
  // p[i] stands for i'th child's priority.
  std::vector<uint> p;
  // Refresh priorities for considerable children.
  void refreshp(const Context& ctx) {
    if (p.size() < children.size()) p.resize(children.size());
    for (int i = 0; i < children.size(); i++)
      if (considerable(i)) p[i] = children[i]->Priority(ctx);
  }

  // Compare priorities between children, where a and b are indexes.
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
    uint total = 0;
    for (int i = 0; i < children.size(); i++)
      if (considerable(i)) total += p[i];

    // random select one, in range [1, total]
    std::uniform_int_distribution<uint> distribution(1, total);

    auto select = [&]() -> int {
      uint v = distribution(rng);  // gen random uint between [0, sum]
      uint s = 0;                  // sum of iterated children.
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
      distribution.param(std::uniform_int_distribution<uint>::param_type(1, total));
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

  // A SequenceNode executes its children one by one sequentially,
  // it succeeds only if all children succeed.
  Builder& Sequence() { return C<SequenceNode>("Sequence"); }

  // A StatefulSequenceNode behaves like a sequence node, executes its children sequentially, succeeds if all
  // children succeed, fails if any child fails. What's the difference is, a StatefulSequenceNode skips the
  // succeeded children instead of always starting from the first child.
  Builder& StatefulSequence() { return C<StatefulSequenceNode>("Sequence*"); }

  // A SelectorNode succeeds if any child succeeds, fails only if all children fail.
  Builder& Selector() { return C<SelectorNode>("Selector"); }

  // A StatefulSelectorNode behaves like a selector node, executes its children sequentially, succeeds if any
  // child succeeds, fails if all child fail. What's the difference is, a StatefulSelectorNode skips the
  // failure children instead of always starting from the first child.
  Builder& StatefulSelector() { return C<StatefulSelectorNode>("Selector*"); }

  // A ParallelNode executes its children parallelly.
  // It succeeds if all children succeed, and fails if any child fails.
  Builder& Parallel() { return C<ParallelNode>("Parallel"); }

  // A StatefulParallelNode behaves like a parallel node, executes its children parallelly, succeeds if all
  // succeed, fails if all child fail. What's the difference is, a StatefulParallelNode will skip the "already
  // success" children instead of executing every child all the time.
  Builder& StatefulParallel() { return C<StatefulParallelNode>("Parallel*"); }

  // A RandomSelectorNode determines a child via weighted random selection.
  // It continues to randomly select a child, propagating tick, until some child succeeds.
  Builder& RandomSelector() { return C<RandomSelectorNode>("RandomSelector"); }

  // A StatefulRandomSelector behaves like a random selector node, the difference is, a StatefulRandomSelector
  // will skip already failed children during a round.
  Builder& StatefulRandomSelector() { return C<StatefulRandomSelectorNode>("RandomSelector*"); }

  ///////////////////////////////////
  // LeafNode creators
  ///////////////////////////////////

  // Creates an Action node by providing implemented Action class.
  // Code example::
  //  root
  //  .Action<MyActionClass>()
  //  ;
  template <TAction Impl, typename... Args>
  Builder& Action(Args&&... args) {
    return C<Impl>(std::forward<Args>(args)...);
  }

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
  //   ._().Condition<A>();
  Builder& Invert() { return C<InvertNode>("Invert"); }

  // Alias to Invert, just named 'Not'.
  // Code exapmle::
  //   root
  //   .Not()
  //   ._().Condition<A>();
  Builder& Not() { return C<InvertNode>("Not"); }

  // Creates a invert condition of given Condition class.
  // Code exapmle::
  //   root
  //   .Sequence()
  //   ._().Not<IsXXX>()
  //   ._().Action<DoSomething>();
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
  //   ._().Action<A>();
  Builder& Repeat(int n) { return C<RepeatNode>(n, "Repeat"); }

  // Alias to Repeat.
  // Code exapmle::
  //   root
  //   .Loop(3)
  //   ._().Action<A>();
  Builder& Loop(int n) { return C<RepeatNode>(n, "Loop"); }

  // Timeout creates a TimeoutNode.
  // It executes the decorated node for at most given duration.
  // Code exapmle::
  //   root
  //   .Timeout(3000ms)
  //   ._().Action<A>();
  template <typename Clock = std::chrono::high_resolution_clock>
  Builder& Timeout(std::chrono::milliseconds duration) {
    return C<TimeoutNode<Clock>>(duration, "Timeout");
  }

  // Delay creates a DelayNode.
  // Wait for given duration before execution of decorated node.
  // Code exapmle::
  //   root
  //   .Delay(3000ms)
  //   ._().Action<A>()
  template <typename Clock = std::chrono::high_resolution_clock>
  Builder& Delay(std::chrono::milliseconds duration) {
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
  template <typename Clock = std::chrono::high_resolution_clock>
  Builder& Retry(int n, std::chrono::milliseconds interval) {
    return C<RetryNode<Clock>>(n, interval, "Retry");
  }

  // Alias for Retry(-1, interval)
  template <typename Clock = std::chrono::high_resolution_clock>
  Builder& RetryForever(std::chrono::milliseconds interval) {
    return C<RetryNode<Clock>>(-1, interval, "RetryForever");
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
  //
  //    bt::Tree subtree;
  //    subtree
  //      .Parallel()
  //      ._().Action<A>()
  //      ._().Action<B>();
  //    return subtree;
  //
  //    root
  //      .Sequence()
  //      ._().Subtree(std::move(subtree))
  //      ;
  Builder& Subtree(RootNode&& tree) { return C<RootNode>(std::move(tree)); }
};

//////////////////////////////////////////////////////////////
/// Tree
///////////////////////////////////////////////////////////////

// Behavior Tree.
class Tree : public RootNode, public Builder {
 public:
  Tree(std::string name = "Root") : RootNode(name), Builder() { bindRoot(*this); }
};

}  // namespace bt

#endif
