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
* Vertically, the sibling relationship, from top to bottom, prioritizes from high to low (by default).
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
 .End()
;

bt::Context ctx;

// In the tick loop.
++ctx.seq;
root.Tick(ctx);
```

## Manual

Reference: <span id="ref"></span>

[中文说明](README.zh.md)

- [Build](#build)
- [Status](#status)
- [Node Classification](#classes)
- Leaf Nodes:
  - [Action](#action)
  - [Condition](#condition)
- Composite Nodes:
  - [Sequence](#sequence)
  - [Selector](#selector)
  - [Parallel](#parallel)
  - [RandomSelector](#random-selector)
  - [Priority](#priority)
  - [Stateful Composite nodes](#stateful)
- [Decorators](#decorators)
  - [If](#if)
  - [Invert](#invert)
  - [Switch Case](#switchcase)
  - [Repeat](#repeat)
  - [Timeout](#timeout)
  - [Delay](#delay)
  - [Retry](#retry)
  - [Custom Decorator](#custom-decorator)
- [Sub Tree](#subtree)
- [Tick Context](#context)
- Others:
  - [Hook Methods](#hooks)
  - [Visualization](#visualization)
  - [Blackboard?](#blackboard)
  - [Ticker Loop](#ticker-loop)
  - [Custom Builder](#custom-builder)
  - [Working with Signals/Events](#signals)

* **Build a tree**: <span id="build"></span> <a href="#ref">[↑]</a>:

  1. The function `_()` increases the indent level by `1`.
  2. The function `End()` should be called after the tree's build done.

  For example, the following tree:

  1. `root` contains a single child, which is a `Sequence` node.
  2. The `Sequence` node contains `2` children:
     1. The first one is a decorator `ConditionalRunNode`, and it contains a single action node `B`.
        Once the condition `A` is satisfied, the `B` is fired.
     2. The second child is an action node `C`.
  3. And finally, don't forget the `End()`.

  ```cpp
  root
  .Sequence()
   ._().If<A>()
   ._()._().Action<B>()
   ._().Action<C>()
   .End();
  ```


* Execution status enums <span id="status"></span> <a href="#ref">[↑]</a>:

  ```cpp
  bt::Status::RUNNING
  bt::Status::FAILURE
  bt::Status::SUCCESS
  ```

* Classification of nodes <span id="classes"></span> :  <a href="#ref">[↑]</a>

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

* **Action**  <span id="action"></span> <a href="#ref">[↑]</a>

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

* **Condition**  <span id="condition"></span> <a href="#ref">[↑]</a>

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

* **Sequence**  <span id="sequence"></span> <a href="#ref">[↑]</a>

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

* **Selector** <span id="selector"></span> <a href="#ref">[↑]</a>

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

* **Parallel**  <span id="parallel"></span> <a href="#ref">[↑]</a>

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

* **RandomSelector**  <span id="random-selector"></span> <a href="#ref">[↑]</a>

  RandomSelector will randomly select a child node to execute until it encounters a successful one.

  If its children nodes implement the function `Priority()`,
  it will follow a weighted random approach, that is to say, the node with a greater weight will be easier to be selected.

  One use of randomly selecting nodes is to make the AI's behavior less rigid.

  ```cpp
  root
  .RandomSelector()
  ._().Action<A>()
  ._().Action<B>()
  ._().Action<C>()
  ;
  ```

* **Priority**  <span id="priority"></span> <a href="#ref">[↑]</a>

  By default, children nodes are equal in weight, that is, their priorities are equal (all are defaulted to 1).
  For composite nodes, their child nodes are examined from top to bottom.

  However, in order to support the "dynamic priority" feature, for example,
  the behavior of each child node has a dynamic scoring mechanism,
  and the child node with the highest score should be selected for execution each tick,
  for such cases, the class `Node` supports overriding a `Priority` function.

  ```cpp
  class A : public bt::Action {
   public:
    unsigned int Priority(const bt::Context& ctx) const override {
        // TODO, returns a number > 0
    }
  };
  ```

  Child nodes with higher priority will be considered firstly.
  If they're equal, then in order, the one above will take precedence.

  It's recommended to implement this function fast enough, since it will be called on each
  tick. For instance, we may not need to do the calculation on every tick if it's complex.
  Another optimization is to separate calculation from queries, for example, pre-cache the result
  somewhere on the blackboard, and just ask it from memory here.

  All composite nodes, including stateful ones, will respect to its children's `Priority()` functions.

* **Stateful Nodes**  <span id="stateful"></span> <a href="#ref">[↑]</a>

  The 4 composite nodes all support stateful ticking: `StatefulSequence`, `StatefulSelector`, `StatefulRandomSelector` and `StatefulParallel`.

  The word "stateful" means that skipping the children already succeeded (failed for selectors), instead of ticking every child.

  ```cpp
  // For instance, the following A will be skipped by Tick() once it
  // turns SUCCESS, only B will got future ticks.

  root
  .StatefulSequence()
  ._().Action<A>()
  ._().Action<B>()
  ```

  Another example:

  ```cpp
  // the following A will be skipped by Tick() once it
  // turns FAILURE, only B will got future ticks.

  root
  .StatefulSelector()
  ._().Action<A>()
  ._().Action<B>()
  ```

* **Decorators** <span id="decorators"></span> <a href="#ref">[↑]</a>

  * `If` executes its child node only if given condition turns `true`. <span id="if"></span> <a href="#ref">[↑]</a>

    ```cpp
    .If<SomeCondition>()
    ._().Action<Task>()
    ```

  * `Invert()` inverts its child node's execution status. <span id="invert"></span> <a href="#ref">[↑]</a>

    ```cpp
    .Invert()
    ._().Action<Task>()

    // how it inverts:
    //   RUNNING => RUNNING
    //   SUCCESS => FAILURE
    //   FAILURE => SUCCESS
    ```

  * `Switch/Case` are just syntax sugar based on `Selector` and `If`: <span id="switchcase"></span> <a href="#ref">[↑]</a>

    ```cpp
    // Only one case will success, or all fail.
    // Cases will be tested sequentially from top to bottom, one fails and then another.

    .Switch()
    ._().Case<ConditionX>()
    ._()._().Action<TaskX>()
    ._().Case<ConditionY>()
    ._()._().Action<TaskY>()
    ```

  * `Repeat(n)` (alias `Loop`) repeats its child node' execution for exactly `n` times, it fails immediately if its child fails. <span id="repeat"></span> <a href="#ref">[↑]</a>

    We can name the process a "round", that from the start (turns `RUNNING`) to the termination (turns `SUCCESS` or `FAILURE`).
    The `Repeat(n)` node performs `n` rounds of its child's execution.

    ```cpp
    // Repeat action A three times.
    .Repeat(3)
    ._().Action<A>()
    ```

  * `Timeout` executes its child node with a time duration limitation, fails on timeout.  <span id="timeout"></span> <a href="#ref">[↑]</a>

    ```cpp
    using namespace std::chrono_literals;

    .Timeout(3000ms)
    ._().Action<Task>()
    ```

  * `Delay` waits for a certain time duration before executing its child node.   <span id="delay"></span> <a href="#ref">[↑]</a>

    ```cpp
    using namespace std::chrono_literals;

    .Delay(1000ms)
    ._().Action<Task>()
    ```

  * `Retry` retries its child node on failure, for a maximum of `n` times, with a given `interval`.  <span id="retry"></span> <a href="#ref">[↑]</a>

    The following code retry the `Task` on failure for at most 3 times, the retry interval is `1000ms`:

    It immediately returns `SUCCESS` if the child succeeds.

    ```cpp

    using namespace std::chrono_literals;

    .Retry(3, 1000ms)
    ._().Action<Task>()
    ```

  * **Custom Decorator** <span id="custom-decorator"></span> <a href="#ref">[↑]</a>

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

* **Sub Tree** <span id="subtree"></span> <a href="#ref">[↑]</a>

  A behavior tree can be used as another behavior tree's child:

  ```cpp
  bt::Tree root, subtree;

  root
  .Sequence()
  ._().Action<A>()
  ._().Subtree<A>(std::move(subtree));
  ```

* **The tick `Context`** <span id="context"></span> <a href="#ref">[↑]</a>

  ```cpp
  struct Context {
    ull seq;  // ticking seq number.
    std::chrono::nanoseconds delta;  // delta time since last tick, to current tick.
    std::any data; // user data.
  }
  ```

* **Hook Methods**  <span id="hooks"></span> <a href="#ref">[↑]</a>

  For each `Node`, there are three hook functions:

  ```cpp
  class MyNode : public Node {
   public:

    // Will be called on the node's first run of a round.
    // Each time the node changes from other status to RUNNING.
    virtual void OnEnter(const Context& ctx){};

    // Will be called on the node's last run of a round.
    // Each time the node goes to FAILURE/SUCCESS.
    virtual void OnTerminate(const Context& ctx, Status status){};

    // Hook function to be called on this node's build is finished.
    virtual void OnBuild() {}
  }
  ```

* **Visualization**  <span id="visualization"></span> <a href="#ref">[↑]</a>

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


* **Blackboard ?**  <span id="blackboard"></span> <a href="#ref">[↑]</a>

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

* **Ticker Loop**   <span id="ticker-loop"></span> <a href="#ref">[↑]</a>

  There's a simple builtin ticker loop implemented in `bt.h`, to use it:

  ```cpp
  root.TickForever(ctx, 100ms);
  ```

* **Custom Builder**  <span id="custom-builder"></span> <a href="#ref">[↑]</a>

  ```cpp
  // Supposes that we need to add a custom decorator.
  class MyCustomMethodNode : public bt::DecoratorNode {
   public:
    MyCustomMethodNode(const std::string& name, ..) : bt::DecoratorNode(name) {}
    // Implements the Update function.
    bt::Status Update(const bt::Context& ctx) override {
      // Propagates ticking to the child.
      // child->Tick(ctx)
      return status;
    }
  };

  // Make a custom Tree class.
  class MyTree : public bt::RootNode, public bt::Builder<MyTree> {
   public:
    // Bind the builder to this tree inside the construct function.
    MyTree(std::string name = "Root") : bt::RootNode(name) { bindRoot(*this); }
    // Implements the custom builder method.
    // C is the method to creates a custom Node.
    auto& MyCustomMethod(...) { return C<MyCustomMethodNode>(...); }
  };

  MyTree root;

  root
  .MyCustomMethod(...)
  ;
  ```

* **Working with Signals/Events** <span id="signals"></span> <a href="#ref">[↑]</a>

  It's a common case to emit and receive signals in a behavior tree.
  But signal (or event) handling is a complex stuff, I don't want to couple with it in this small library.

  General ideas to introduce signal handling into bt.h is:

  1. Creates a custom decorator node, supposed named `OnSignalNode`.
  2. Creates a custom builder class, and add a method named `OnSignal`.
  3. The `OnSignal` decorator propagates the tick down to its child only if the corresponding signal fired.
  4. The data passing along with the fired signal, can be placed onto the blackboard temporary.
  5. You can move the OnSignal node as high as possible to make the entire behavior tree more event-driven.

  Here's an example in detail to combine my tiny signal library [blinker.h](https://github.com/hit9/blinker.h) with bt.h,
  please checkout the code example in folder [example/onsignal](example/onsignal).

  ```cpp
  root
    .Parallel()
    ._().Action<C>()
    ._().OnSignal("a.*")
    ._()._().Parallel()
    ._()._()._().OnSignal("a.a")
    ._()._()._()._().Action<A>()
    ._()._()._().OnSignal("a.b")
    ._()._()._()._().Action<B>()
    .End()
    ;
  ```

## License

BSD.
