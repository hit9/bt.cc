# bt.h

![](https://github.com/hit9/bt/actions/workflows/tests.yml/badge.svg)
![](https://img.shields.io/badge/license-BSD3-brightgreen)

[中文说明](README.zh.md)

A simple lightweight behavior tree library.

Requires at least C++20.

![](misc/visualization.jpg)

## Installation

Just copy the header file `bt.h` and include it.

## The Big Picture

Code overview to structure a behavior tree in C++:

* Horizontally from left to right represents the relationship from parent node to child node.
* Vertically, the sibling relationship, from top to bottom, prioritizes from high to low.
* In a depth-first way, prioritize recursively ticking all descendant nodes.

```cpp
// Build the tree.
bt::Tree root;

root
 .Sequence()
 ._().If<A>()
 ._()._().Action<B>()
 ._().Selector()
 ._()._().Action<C>()
 ._()._().Parallel()
 ._()._()._().Action<D>()
 ._()._()._().Action<E>()
;

bt::Context ctx;

// In the tick loop.
++ctx.seq;
root.Tick(ctx);
```

## Manual Reference

* Execution status enums <span id="status"></span>:

  ```cpp
  bt::Status::RUNNING
  bt::Status::FAILURE
  bt::Status::SUCCESS
  ```

* Classification of nodes:

  ```
  Node                               Base class of all kinds of nodes.
   | InternalNode                    Contains one or more children.
   |   | SingleNode                  Contains exactly a single child.
   |   |  | RootNode                 The BT tree root.
   |   |  | DecoratorNode            Decorates its child node.
   |   | CompositeNode               Combining behaviors for multiple nodes.
   |   |  | SequenceNode             Run children nodes sequentially until all SUCCESS or one FAILURE.
   |   |  | SelectorNode             Run children nodes sequentially until one SUCCESS or all FAILURE.
   |   |  | ParallelNode             Run children nodes parallelly, SUCCESS if all SUCCESS, otherwise FAILURE.
   | LeafNode                        Contains no children.
   |   | ActionNode                  Executes a specific task/action.
   |   | ConditionNode               Tests a specific condition.
  ```

* **Action**

  Define a class that inherits from `bt::Action`, and implement the `Update` method:

  ```cpp
  class A : public bt::Action {
   public:
    // TODO: Implements this
    bt::Status Update(const bt::Context& ctx) override { }

    // The name of this Action.
    std::string_view Name() const override { return "A"; }
  };
  ```

  To use a `Action`:

  ```cpp
  .Action<A>()
  ```

* **Condition**

  A `Condition` is a leaf node without children, it succeeds only if the `Check()` method returns `true`.

  And there's no `RUNNING` status for a condition node.

  To implement a "static condition", just define a class derived from class `bt::Condition`, and implements the `Check` method:

  ```cpp
  class C : public bt::ConditionNode {
   public:
    // TODO: Implements this
    bool Check(const bt::Context& ctx) override { return true; }
    std::string_view Name() const override { return "C"; }
  };
  ```

  Example to use a static condition:

  ```cpp
  root
  .Sequence()
  ._().Condition<C>() // Condition is a leaf node.
  ._().Action<A>()
  ```

  We can also make a `Condition` directly from a lambda function dynamically:

  ```cpp
  root
  .Sequence()
  ._().Condition([=](const Context& ctx) { return false; })
  ._().Action<A>()
  ;
  ```

* **Sequence**

  A `SequenceNode` executes its child nodes sequentially, succeeding only if all children succeed.
  It behaves akin to the logical **AND** operation, especially for the `Condition` children nodes.

  ```cpp
  // If A, B, C are all SUCCESS, the sequence node goes SUCCESS, otherwise FAILURE.
  root
  .Sequence()
  ._().Action<A>()
  ._().Action<B>()
  ._().Action<C>()
  ;
  ```

* **Selector**

  A `SelectorNode` executes its child nodes sequentially, one by one.
  It succeeds if any child succeeds and fails only if all children fail.
  It behaves similarly to the logical **OR** operation

  ```cpp
  // If A, B, C are all FAILURE, the selector node goes FAILURE, otherwise SUCCESS.
  root
  .Selector()
  ._().Action<A>()
  ._().Action<B>()
  ._().Action<C>()
  ;
  ```

* **Parallel**

  A `ParallelNode` achieves success when all of its child nodes succeed.
  It will execute all of its children "simultaneously", until some child fails.
  In detail, it executes all children, then aggregates the results of the child nodes' execution to get the `ParallelNode`'s status.

  ```cpp
  // A, B, C are executed parallelly.
  // If all children SUCCESS, the parallel node goes SUCCESS, otherwise FAILURE.
  root
  .Parallel()
  ._().Action<A>()
  ._().Action<B>()
  ._().Action<C>()
  ;
  ```

* **Decorators**

  * `If` executes its child node only if given condition turns `true`.

    ```cpp
    .If<SomeCondition>()
    ._().Action<Task>()
    ```

  * `Invert()` inverts its child node's execution status.

    ```cpp
    .Invert()
    ._().Action<Task>()

    // how it inverts:
    //   RUNNING => RUNNING
    //   SUCCESS => FAILURE
    //   FAILURE => SUCCESS
    ```

  * `Switch/Case` are just syntax sugar based on `Selector` and `If`:

    ```cpp
    // Only one case will success, or all fail.
    // Cases will be tested sequentially from top to bottom, one fails and then another.

    .Switch()
    ._().Case<ConditionX>()
    ._()._().Action<TaskX>()
    ._().Case<ConditionY>()
    ._()._().Action<TaskY>()
    ```

  * `Repeat(n)` (alias `Loop`) repeats its child node' execution for exactly `n` times, it fails immediately if its child fails.

    We can name the process a "round", that from the start (turns `RUNNING`) to the termination (turns `SUCCESS` or `FAILURE`).
    The `Repeat(n)` node performs `n` rounds of its child's execution.

    ```cpp
    // Repeat action A three times.
    .Repeat(3)
    ._().Action<A>()
    ```

  * `Timeout` executes its child node with a time duration limitation, fails on timeout.

    ```cpp
    using namespace std::chrono_literals;

    .Timeout(3000ms)
    ._().Action<Task>()
    ```

  * `Delay` waits for a certain time duration before executing its child node.

    ```cpp
    using namespace std::chrono_literals;

    .Delay(1000ms)
    ._().Action<Task>()
    ```

  * `Retry` retries its child node on failure, for a maximum of `n` times, with a given `interval`.

    The following code retry the `Task` on failure for at most 3 times, the retry interval is `1000ms`:

    It immediately returns `SUCCESS` if the child succeeds.

    ```cpp

    using namespace std::chrono_literals;

    .Retry(3, 1000ms)
    ._().Action<Task>()
    ```

  * **Custom Decorator**

    To implement a custom decorator, just inherits from `bt::DecoratorNode`:

    ```cpp
    class CustomDecorator : public bt::DecoratorNode {
     public:
      std::string_view Name() const override { return "CustomDecorator"; }

      bt::Status Update(const bt::Context& ctx) override {
          // TODO: run the child node.
          // child->Tick(ctx);
      }
    };
    ```

* **Sub Tree**

  A behavior tree can be used as another behavior tree's child:

  ```cpp
  bt::Tree root, subtree;

  root
  .Sequence()
  ._().Action<A>()
  ._().Subtree<A>(std::move(subtree));
  ```

* **Stateful Nodes**

  The three composite nodes all support stateful ticking: `StatefulSequence`, `StatefulSelector` and `StatefulParallel`.

  The word "stateful" means that skipping the children already succeeded, instead of ticking every child.

  ```cpp
  // For instance, the following A will be skipped by Tick() once it turns SUCCESS.

  root
  .StatefulSequence()
  ._().Action<A>()
  ._().Action<B>()
  ```

* **Hook Methods**

  For each `Node`, there are two hook functions:

  ```cpp
  class MyNode : public Node {
   public:

    // Will be called on the node's first run of a round.
    // Each time the node changes from other status to RUNNING.
    virtual void OnEnter(){};

    // Will be called on the node's last run of a round.
    // Each time the node goes to FAILURE/SUCCESS.
    virtual void OnTerminate(Status status){};

  }
  ```

* **Visualization**

  There's a simple real time behavior tree visualization function implemented in `bt.h`.

  It simply displays the tree structure and execution status on the console.
  The nodes colored in green are those currently executing and are synchronized with the latest tick cycles.

  Example code to call the `Visualize` function:

  ```cpp
  // In the tick loop
  ++ctx.seq;
  root.Tick(ctx)
  root.Visualize(ctx.seq)
  ```

* **The tick `Context`**

  ```cpp
  struct Context {
    ull seq;  // ticking seq number.
    std::chrono::nanoseconds delta;  // delta time since last tick, to current tick.
    std::any data; // user data.
  }
  ```

* **Blackboard ?**

  In fact, if there's no need for non-programmer usage, behavior trees and blackboards don't require a serialization mechanism.
  In such cases, using a plain `struct` as the blackboard is a simple and fast approach.

  ```cpp
  struct Blackboard {
      // TODO: fields.
  };

  // Pass a pointer to blackboard to the tick context.
  auto bb = std::make_shared<Blackboard>();
  bt::Context ctx(bb);
  ```

  In the `Update()` function:

  ```cpp
  bt::Status Update(const bt::Context& ctx) override {
    auto bb = std::any_cast<std::shared_ptr<Blackboard>>(ctx.data);
    // TODO Access bb->field.
  }
  ```

* **Ticker Loop**

  There's a simple builtin ticker loop implemented in `bt.h`, to use it:

  ```cpp
  root.TickForever(ctx, 100ms);
  ```

* **Custom Builder**

  ```cpp
  class MyTree : public bt::Tree {
   public:
    MyTree(std::string name = "Root") : MyTree(name) { bindRoot(*this); }

    // Implements custom builder functions.
  }

  MyTree root;
  ```

## License

BSD.
