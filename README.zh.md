# bt

![](https://github.com/hit9/bt/actions/workflows/tests.yml/badge.svg)
![](https://img.shields.io/badge/license-BSD3-brightgreen)

一个简单的、轻量的行为树库。

需要至少 C++20

![](misc/visualization.jpg)

## 安装

只需要拷贝走头文件 `bt.h`

## 代码总览

用 C++ 代码来组织一颗行为树，总体来看如下：

* 横向自左向右是父节点到子节点
* 纵向兄弟关系、从上到下优先级由高到低

```cpp
// 组织一颗行为树
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

// Tick 主循环中
++ctx.seq;
root.Tick(ctx);
```

## 参考手册

* 执行状态码:

  ```cpp
  bt::Status::RUNNING  // 执行中
  bt::Status::FAILURE  // 已失败
  bt::Status::SUCCESS  // 已成功
  ```

* 行为树节点的分类:

  ```
  Node                               所有节点的基类
   | InternalNode                    允许包含一个或者多个子节点的中间节点
   |   | SingleNode                  只包含一个子节点
   |   |  | RootNode                 行为树的根节点
   |   |  | DecoratorNode            装饰器节点
   |   | CompositeNode               组合节点的基类
   |   |  | SequenceNode             顺序组合节点，顺序地执行所有子节点，直到全部成功或者至少一个失败
   |   |  | SelectorNode             选择组合节点，顺序地执行所有子节点，直到全部失败或者至少一个成功
   |   |  | ParallelNode             并行地执行所有子节点，如果子节点全部成功，则成功，否则失败
   | LeafNode                        不含任何子节点的叶子节点
   |   | ActionNode                  执行一个具体任务、动作的叶子节点
   |   | ConditionNode               条件节点
  ```

* **Action**

  要定义一个 `Action` 节点，只需要继承自 `bt::Action`，并实现方法 `Update`：

  ```cpp
  class A : public bt::Action {
   public:
    // TODO: 需要重载 Update 函数
    bt::Status Update(const bt::Context& ctx) override { }

    // 指明这个 Action 的名字
    std::string_view Name() const override { return "A"; }
  };
  ```

  使用如下：

  ```cpp
  .Action<A>()
  ```

* **Condition**

  条件节点没有子节点，当它的 `Check()` 方法返回 `true` 时，算作成功。

  条件节点也没有 `RUNNING` 的状态。

  要定义一个「静态的」条件节点，可以继承自 `bt::Condition` 类，然后实现 `Check` 方法：

  ```cpp
  class C : public bt::ConditionNode {
   public:
    // TODO: 实现这个 Check
    bool Check(const bt::Context& ctx) override { return true; }
    std::string_view Name() const override { return "C"; }
  };
  ```

  使用一个静态的条件节点的例子：

  ```cpp
  root
  .Sequence()
  ._().Condition<C>() // 条件节点是一个叶子节点
  ._().Action<A>()
  ```

  我们也可以直接从一个 `lambda` 函数来动态地构造一个条件节点：

  ```cpp
  root
  .Sequence()
  ._().Condition([=](const Context& ctx) { return false; })
  ._().Action<A>()
  ;
  ```

* **Sequence**

  顺序节点会依次执行它的所有子节点，如果子节点全部成功，则它会成功，否则会立即失败。

  顺序节点的行为比较像逻辑运算中的 `AND` 操作符。

  ```cpp
  // 比如下面的例子中，A, B, C 三个行为全部成功时，整个 Sequence 会成功，否则会失败
  root
  .Sequence()
  ._().Action<A>()
  ._().Action<B>()
  ._().Action<C>()
  ;
  ```

* **Selector**

  选择节点会依次执行它的所有子节点，如果子节点全部失败，则它会失败，否则遇到一个成功的子节点，会立即成功。

  选择节点的行为比较像逻辑运算中的 `OR` 操作符。

  ```cpp
  // 比如下面的例子中，A, B, C 三个行为全部失败时，整个 Selector 会失败,
  // 否则遇到一个成功的则立即成功。

  root
  .Selector()
  ._().Action<A>()
  ._().Action<B>()
  ._().Action<C>()
  ;
  ```

* **Parallel**

  并行节点会并行地执行所有子节点，即每次 `Tick()` 都会对所有子节点跑一次 `Tick()`，然后再综合子节点的运行结果。
  如果所有节点成功，则算作成功，否则如果至少一个子节点执行失败，则算作失败。


  ```cpp
  // 比如下面的例子中，A, B, C 三个子节点会并行的被 Tick
  // 如果三个都执行成功了，那么这个并行节点会成功，否则会算作失败。

  root
  .Parallel()
  ._().Action<A>()
  ._().Action<B>()
  ._().Action<C>()
  ;
  ```

* **Decorators**

  * `If` 只有在它的条件满足时执行其装饰的子节点：

    ```cpp
    .If<SomeCondition>()
    ._().Action<Task>()
    ```

  * `Invert()` 会反转其装饰的子节点的执行状态:

    ```cpp
    .Invert()
    ._().Action<Task>()

    // 如何反转的:
    //   RUNNING => RUNNING
    //   SUCCESS => FAILURE
    //   FAILURE => SUCCESS
    ```

  * `Switch/Case` 是基于 `Selector` 和 `If` 的一种语法糖：

    ```cpp
    // 只有一个 case 会成功，或者全部失败。
    // 每个 case 会从上到下的顺序被依次测试，一旦一个失败了，则开始测试下一个。

    .Switch()
    ._().Case<ConditionX>()
    ._()._().Action<TaskX>()
    ._().Case<ConditionY>()
    ._()._().Action<TaskY>()
    ```

  * `Repeat(n)` (别名 `Loop`) 会重复执行被修饰的子节点正好 `n` 次, 如果子节点失败，它会立即失败。

    如果把节点从 开始 `RUNNING`、到 `SUCCESS` 或者 `FAILURE` 叫做一轮的话，`Repeat(n)` 的作用就是执行被修饰的子节点 `n` 轮。

    ```cpp
    // Repeat action A three times.
    .Repeat(3)
    ._().Action<A>()
    ```

  * `Timeout` 会对其修饰的子节点加一个执行时间限制，如果到时间期限子节点仍未返回成功，则它会返回失败，也不再 tick 子节点。

    ```cpp
    using namespace std::chrono_literals;

    .Timeout(3000ms)
    ._().Action<Task>()
    ```

  * `Delay` 会在执行其子节点之前，等待一段时间。

    ```cpp
    using namespace std::chrono_literals;

    .Delay(1000ms)
    ._().Action<Task>()
    ```

  * `Retry` 在其装饰的子节点执行失败时会发起重试，最多重试 `n` 次，重试间隔是 `interval` 。

    下面的代码中，在 `Task` 子树失败时会发起重试，最多执行 3 次，每次重试的间隔是 `1000ms`：

    当子节点成功时，`Retry` 节点会立刻成功，不再重试。

    ```cpp

    using namespace std::chrono_literals;

    .Retry(3, 1000ms)
    ._().Action<Task>()
    ```

  * **自定义装饰器**

    要定义一个自定义的装饰器，可以继承 `bt::DecoratorNode`:

    ```cpp
    class CustomDecorator : public bt::DecoratorNode {
     public:
      std::string_view Name() const override { return "CustomDecorator"; }

      bt::Status Update(const bt::Context& ctx) override {
          // TODO: 执行子节点的 Tick
          // child->Tick(ctx);
      }
    };
    ```

* **子树**

  一个行为树可以挂载到另一颗行为树上，作为子树存在：

  ```cpp
  bt::Tree root, subtree;

  root
  .Sequence()
  ._().Action<A>()
  ._().Subtree<A>(std::move(subtree));
  ```

* **有状态的节点**

  三种组合节点都有支持「有状态的」版本：`StatefulSequence`, `StatefulSelector` 和 `StatefulParallel`.

  「有状态的」意思是说，不再是每次执行所有子节点，而是跳过已经执行成功的子节点，以提高性能。

  ```cpp
  // 比如说，下面的 A 在它成功之后，不会再被 Tick 到, 直到 StatefulSequence 整体成功或失败之后的下一轮才会被重新 Tick。

  root
  .StatefulSequence()
  ._().Action<A>()
  ._().Action<B>()
  ```

* **钩子函数**

  对于每个节点，都支持两种钩子函数：

  ```cpp
  class MyNode : public Node {
   public:

    // 在一轮的开始时会被调用，就是说本节点的状态从其他变成 RUNNING 的时候：
    virtual void OnEnter(){};

    // 在一轮的结束时会被调用，就是说本节点的状态变成 FAILURE/SUCCESS 的时候：
    virtual void OnTerminate(Status status){};

  }
  ```

* **可视化**

  `bt.h` 实现了一个简单的实时把行为树运行状态可视化展示的函数，绿色的节点表示正在被 tick 的节点。

  用法示例：

  ```cpp
  // tick 主循环中
  ++ctx.seq;
  root.Tick(ctx)
  root.Visualize(ctx.seq)
  ```

* **Tick 的上下文结构体 `Context`**

  这个结构体会从根节点一路传递到每个被执行到的节点：

  ```cpp
  struct Context {
    ull seq;  // 当前全局的 tick 帧号
    std::chrono::nanoseconds delta;  // 全局的从上一次 tick 到本次 tick 的时间差
    std::any data; // 用户数据，比如可以存放一个指向黑板的指针
  ```

* **黑板 ?**

  实际上，如果不面向非开发人员的话，行为树和黑板是不需要序列化机制的，也就是说，可以直接使用一个 `struct` 来作为黑板，简单而高效：

  ```cpp
  struct Blackboard {
      // TODO: fields.
  };

  // 把黑板的指针放入 tick context 结构体
  auto bb = std::make_shared<Blackboard>();
  bt::Context ctx(bb);
  ```

  在 `Update()` 函数中:

  ```cpp
  bt::Status Update(const bt::Context& ctx) override {
    auto bb = std::any_cast<std::shared_ptr<Blackboard>>(ctx.data);
    // TODO 访问黑板数据 bb->field.
  }
  ```

* **Tick 循环**

  `bt.h` 中内置了一个简单额 tick 主循环：

  ```cpp
  root.TickForever(ctx, 100ms);
  ```

* **自定义行为树的构建器 Builder**

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
