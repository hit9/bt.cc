# bt.cc

![](https://github.com/hit9/bt/actions/workflows/tests.yml/badge.svg)
![](https://img.shields.io/badge/license-BSD3-brightgreen)

**在版本 1.0.0 之前可能会不稳定 !**

一个轻量的、数据和行为分离的行为树库。

需要至少 C++20

![](Misc/visualization.jpg)

## 安装

只需要拷贝走 `Source` 目录下的 `bt.h` 和 `bt.cc`

## 特点


1. 节点本身不存储实体相关的数据状态，行为和实体的数据是分离的。

   **适合：多个实体共享一套行为树的情形**。

2. 用树状的代码结构来组织一颗行为树，简洁直观，同时支持自定义扩展构建器。
3. 自带多种装饰器节点，支持自定义装饰器节点。
4. 支持带优先级的组合节点、带状态的组合节点 和 随机选择器。
5. 也支持采用连续固定内存块的实体数据 Blob


## 代码总览

用 C++ 代码来组织一颗行为树，总体来看如下：

* 横向自左向右是父节点到子节点
* 纵向兄弟关系、从上到下执行顺序 (默认的)
* 行为和实体数据是分离的，树存储行为及其结构，blob 存储实体相关的状态数据

在负责行为的模块（系统）中：

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
 .End()
;
```

在负责实体/组件数据的模块中：

```cpp
struct Entity {
  // DynamicTreeBlob 存储行为树中所有和实体相关的状态数据
  bt::DynamicTreeBlob blob;

  // 或者用一个固定大小的 TreeBlob, 会直接内嵌到 Entity 结构体内
  // 最多 8 个节点 x 每个节点最多64个字节, 固定大小二维数组
  bt::FixedTreeBlob<8, 64> blob;
};
```

在 Tick 循环中：

```cpp
bt::Context ctx;

// Tick 主循环中
while(...) {
  // 遍历每个实体的 blob 数据块
  for (auto& e : entities) {
    // 绑定一个实体的数据块
    root.BindTreeBlob(e.blob);
    ++ctx.seq;
    root.Tick(ctx)
    // 解绑
    root.UnbindTreeBlob();
  }
}
```

## 参考手册

目录: <span id="ref"></span>

- [构建过程](#build)
- [状态码](#status)
- [节点分类](#classes)
- [TreeBlob](#tree-blob)
- 叶子节点:
  - [动作节点 Action](#action)
    - [带实体状态的节点](#node-blob)
  - [条件节点 Condition](#condition)
- 组合节点:
  - [顺序节点 Sequence](#sequence)
  - [选择节点 Selector](#selector)
  - [并行节点 Parallel](#parallel)
  - [随机选择节点 RandomSelector](#random-selector)
  - [优先级](#priority)
  - [有状态的组合节点](#stateful)
  - [Switch Case](#switchcase)
- [装饰器 Decorators](#decorators)
  - [If](#if)
  - [Invert 反转](#invert)
  - [Repeat 重复](#repeat)
  - [Timeout 超时](#timeout)
  - [Delay 延时](#delay)
  - [Retry 重试](#retry)
  - [ForceSuccess/ForceFailure 强制成功 和 强制失败](#force-success-and-force-failure)
  - [自定义装饰器](#custom-decorator)
- [子树](#subtree)
- [Tick 上下文](#context)
- 其他:
  - [钩子函数](#hooks)
  - [可视化](#visualization)
  - [黑板 ?](#blackboard)
  - [Tick 循环](#ticker-loop)
  - [自定义 Builder](#custom-builder)
  - [信号和事件](#signals)
  - [树的遍历](#traversal)

* **构建一棵树**: <span id="build"></span> <a href="#ref">[↑]</a>:

  1. 函数 `_()`  用来递增缩进层级.
  2. 在构建的最后, 需要调用函数 `End()`.

  比如说,下面的树:

  1. `root` 节点包含一个子节点, 是一个 `Sequence` 顺序节点.
  2. 这个顺序节点, 进一步包含了 `2` 个子节点:
     1. 第一个是一个叫做 `ConditionalRunNode` 的装饰器节点, 它的子节点是一个动作节点 `B`.
        一旦条件 `A` 检查到满足, 那么 `B` 就会被激活.
     2. 第二个子节点就是一个动作节点 `C`.
  3. 最后不要忘记调用 `End()`.

  ```cpp
  root
  .Sequence()
   ._().If<A>()
   ._()._().Action<B>()
   ._().Action<C>()
   .End();
  ```

  重要的一点是，行为树本身只存储树的结构信息、不含任何和实体相关的数据。

* 执行状态码 <span id="status"></span> <a href="#ref">[↑]</a>:

  ```cpp
  bt::Status::RUNNING  // 执行中
  bt::Status::FAILURE  // 已失败
  bt::Status::SUCCESS  // 已成功
  ```

* 行为树节点的分类:  <span id="classes"></span> :  <a href="#ref">[↑]</a>

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

* **TreeBlob**   <span id="tree-blob"></span> <a href="#ref">[↑]</a>

  TreeBlob 负责存储一颗行为树的和实体相关的数据.

  一颗行为树 和 一个实体对象, 对应一个 TreeBlob 实例.

  有两种类型的 TreeBlob:

  1. `bt::DynamicTreeBlob` 包含一个 `vector`, 里面是动态内存申请的节点的 NodeBlob 的指针.
  2. `bt::FixedTreeBlob` 则包含一个固定大小的连续内存的二维数组.

     ```cpp
     // NumNodes 是对应的行为树的可能的最多的节点数目,作为行数
     // MaxSizeNodeBlob 是对应的行为树中可能出现的最大的 NodeBlob 的大小,作为列数
     bt::FixedTreeBlob<NumNodes, MaxSizeNodeBlob> blob;
     ```

     `FixedTreeBlob` 表现上比 `DynamicTreeBlob` 稍微快一小点

     这两个模板参数,可以通过接口 `root.NumNodes()` 和 `MaxSizeNodeBlob()` 来获取,
     这需要先把构建好的行为树先编译, 执行一下, 输出这些信息, 然后再填写到实体中定义这些 FixedTreeBlob 的代码中去.

  关于如何定义一个使用 TreeBlob 的行为节点, 请看下面的 [node blob](#node-blob).

* **Action**  <span id="action"></span> <a href="#ref">[↑]</a>

  要定义一个 `Action` 节点，只需要继承自 `bt::ActionNode`，并实现方法 `Update`：

  ```cpp
  class A : public bt::ActionNode {
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

  如果要实现一个带实体状态的行为节点，可以先定义一个 `NodeBlob` 结构：      <span id="node-blob"></span> <a href="#ref">[↑]</a>:

  ```cpp
  struct ANodeBlob : bt::NodeBlob {
    // 数据字段, 建议加上默认值
  };
  ```

  然后，实现接口 `GetNodeBlob`:

  ```cpp
  class A : public bt::ActionNode {
   public:
    // 任何一个有实体状态的节点都应该定义一个类型成员, 叫做 Blob
    using Blob = ANodeBlob;
    // 需要重载这个方法, 返回一个指向基础类 NodeBlob 类型的指针
    // getNodeBlob 是库提供的方法, 定义在 `Node` 中
    NodeBlob* GetNodeBlob() const override { return getNodeBlob<ANodeBlob>(); }

    // 在这个类的方法中，可以用 getNodeBlob<ANodeBlob>() 来获取数据块的指针, 来查询和修改实体相关的数据。
    bt::Status Update(const bt::Context& ctx) override {
        ANodeBlob* b = getNodeBlob<ANodeBlob>();
        b->data = 1; // 示例
    }
  };
  ```

  对于其他节点来说，要实现一个和实体相关的状态化的节点，都是如法炮制的。

* **Condition**  <span id="condition"></span> <a href="#ref">[↑]</a>

  条件节点没有子节点，当它的 `Check()` 方法返回 `true` 时，算作成功。

  条件节点也没有 `RUNNING` 的状态。

  要定义一个「静态的」条件节点，可以继承自 `bt::ConditionNode` 类，然后实现 `Check` 方法：

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

* **Sequence** <span id="sequence"></span> <a href="#ref">[↑]</a>

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

* **Selector**  <span id="selector"></span> <a href="#ref">[↑]</a>

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

* **Parallel** <span id="parallel"></span> <a href="#ref">[↑]</a>

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

* **RandomSelector**  <span id="random-selector"></span> <a href="#ref">[↑]</a>

  随机选择节点 会随机选择一个子节点来执行，直到遇到成功的。

  如果它的子节点实现了优先级函数  `Priority()` ，那么会按照加权随机的方式，也就是说权重越大的节点，越容易被选择。

  随机选择节点的一大用处是让 AI 的行为不那么刻板.

  ```cpp
  root
  .RandomSelector()
  ._().Action<A>()
  ._().Action<B>()
  ._().Action<C>()
  ;
  ```

* **Priority**  <span id="priority"></span> <a href="#ref">[↑]</a>

  默认的，节点之间是平权的，也就是优先级相等（都预设为 1）。

  对于组合节点而言，会从上向下考察子节点。

  不过，为了支持「动态的优先级」，比如说，每个子节点的行为有一种动态的评分机制，
  每次要选择最高分的子节点来执行，因此 Node 类支持重载一个  Priority 的函数。

  ```cpp
  class A : public bt::ActionNode {
   public:
    unsigned int Priority(const bt::Context& ctx) const override {
        // TODO, 返回一个正整数
    }
  };
  ```

  优先级更高的子节点会被优先考虑, 如果平权,那么就按顺序,上面的优先.

  建议把这个函数实现地足够快，因为它将在每个 tick 执行。
  例如，我们可能不需要在每一帧进行计算。 此外, 可以将计算与查询分离，例如预先缓存结果到黑板上，
  然后在这里只是从内存中取出。

  所有复合节点，包括有状态节点，都会考虑其子节点的 `Priority()` 函数。

* **有状态的组合节点**  <span id="stateful"></span> <a href="#ref">[↑]</a>

  三种组合节点都有支持「有状态的」版本：`StatefulSequence`, `StatefulSelector` 和 `StatefulParallel`.

  「有状态的」意思是说，不再是每次执行所有子节点，而是跳过已经执行成功的子节点(对于 Selector 来说是跳过已经执行失败的子节点)，以提高性能。

  ```cpp
  // 比如说，下面的 A 在它成功之后，不会再被 Tick 到, 直到 StatefulSequence 整体成功或失败之后的下一轮才会被重新 Tick。

  root
  .StatefulSequence()
  ._().Action<A>()
  ._().Action<B>()
  ```

  另一个例子:

  ```cpp
  // 下面的 A 在它失败之后，不会再被 Tick 到, 后续只会 Tick B 节点

  root
  .StatefulSelector()
  ._().Action<A>()
  ._().Action<B>()
  ```

  有状态的组合节点的状态数据都存储在了 `NodeBlob` 中。

* `Switch/Case` 是基于 `Selector` 和 `If` 的一种语法糖：  <span id="switchcase"></span> <a href="#ref">[↑]</a>

  ```cpp
  // 只有一个 case 会成功，或者全部失败。
  // 每个 case 会从上到下的顺序被依次测试，一旦一个失败了，则开始测试下一个。

  .Switch()
  ._().Case<ConditionX>()
  ._()._().Action<TaskX>()
  ._().Case<ConditionY>()
  ._()._().Action<TaskY>()
  ```


* **Decorators** <span id="decorators"></span> <a href="#ref">[↑]</a>

  * `If` 只有在它的条件满足时执行其装饰的子节点：  <span id="if"></span> <a href="#ref">[↑]</a>

    ```cpp
    .If<SomeCondition>()
    ._().Action<Task>()
    ```

  * `Invert()` 会反转其装饰的子节点的执行状态:  <span id="invert"></span> <a href="#ref">[↑]</a>

    ```cpp
    .Invert()
    ._().Action<Task>()

    // 如何反转的:
    //   RUNNING => RUNNING
    //   SUCCESS => FAILURE
    //   FAILURE => SUCCESS
    ```

    `Not` 是 `Invert` 的一个别名:

    ```cpp
    .Not()
    ._().Condition<A>()
    ```

  * `Repeat(n)` (别名 `Loop`) 会重复执行被修饰的子节点正好 `n` 次, 如果子节点失败，它会立即失败。  <span id="repeat"></span> <a href="#ref">[↑]</a>

    如果把节点从 开始 `RUNNING`、到 `SUCCESS` 或者 `FAILURE` 叫做一轮的话，`Repeat(n)` 的作用就是执行被修饰的子节点 `n` 轮。

    ```cpp
    // Repeat action A three times.
    .Repeat(3)
    ._().Action<A>()
    ```

    设置 `n=-1` 意味着无限循环.

    ```cpp
    // Repeat action A forever.
    .Repeat(-1)
    ._().Action<A>()
    ```

    设置 `n=0` 将会立即成功, 而不会执行被修饰额节点.

    ```cpp
    // immediately success without executing A.
    .Repeat(0)
    ._().Action<A>()
    ```
  * `Timeout` 会对其修饰的子节点加一个执行时间限制，如果到时间期限子节点仍未返回成功，则它会返回失败，也不再 tick 子节点。   <span id="timeout"></span> <a href="#ref">[↑]</a>

    ```cpp
    using namespace std::chrono_literals;

    .Timeout(3000ms)
    ._().Action<Task>()
    ```

  * `Delay` 会在执行其子节点之前，等待一段时间。   <span id="delay"></span> <a href="#ref">[↑]</a>

    ```cpp
    using namespace std::chrono_literals;

    .Delay(1000ms)
    ._().Action<Task>()
    ```

  * `Retry` 在其装饰的子节点执行失败时会发起重试，最多重试 `n` 次，重试间隔是 `interval` 。  <span id="retry"></span> <a href="#ref">[↑]</a>

    下面的代码中，在 `Task` 子树失败时会发起重试，最多执行 3 次，每次重试的间隔是 `1000ms`：

    当子节点成功时，`Retry` 节点会立刻成功，不再重试。

    ```cpp

    using namespace std::chrono_literals;

    .Retry(3, 1000ms)
    ._().Action<Task>()
    ```

  * `ForceSuccess` 会执行它装饰的节点, 如果仍在执行, 则返回执行, 否则强制返回成功. <span id="force-success-and-force-failure"></span> <a href="#ref">[↑]</a>
    `ForceFailure` 是类似的, 如果所装饰的节点仍在执行, 则返回执行, 否则强制返回失败.

    ```cpp
    .ForceSuccess()
    ._().Actino<Task>()
    ```

  * **自定义装饰器** <span id="custom-decorator"></span>  <a href="#ref">[↑]</a>

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

    一些装饰节点需要利用状态化的数据，如果是实体相关的（也就是非行为树结构的数据），那么可以定义一个 `NodeBlob` 来搭配使用。
    方法和 [上面所讲的带状态的 Action 节点](#node-blob) 一样。

* **子树**  <span id="subtree"></span> <a href="#ref">[↑]</a>

  一个行为树可以挂载到另一颗行为树上，作为子树存在：

  ```cpp
  bt::Tree root, subtree;

  root
  .Sequence()
  ._().Action<A>()
  ._().Subtree<A>(std::move(subtree));
  ```

  一旦一个子树挂载到另外一颗树上时，它本身就没有什么作用了，因为它的所有数据和资源都已经属于新的父树了。

  如果你想要把一颗树复制多份，达到把一套行为做出来几个副本的效果，那么可以使用一个函数来构造子树：


  ```cpp
  auto Walk = [&](int poi) {
    bt::Tree subtree("Walk");
    // clang-format off
      subtree
        .Sequence()
        ._().Action<Movement>(poi)
        ._().Action<Standby>()
        .End()
      ;
    // clang-format on
    return subtree;
  };

  root.
    .RandomSelector()
    ._().Subtree(Walk(point1))
    ._().Subtree(Walk(point2))
    .End();
  ```

* **Tick 的上下文结构体 `Context`**  <span id="context"></span> <a href="#ref">[↑]</a>

  这个结构体会从根节点一路传递到每个被执行到的节点：

  ```cpp
  struct Context {
    ull seq;  // 当前全局的 tick 帧号
    std::chrono::nanoseconds delta;  // 全局的从上一次 tick 到本次 tick 的时间差
    std::any data; // 用户数据，比如可以存放一个指向黑板的指针
  }
  ```

  上下文结构体的主要作用，是可以在行为内部接触外部数据，比如世界中的环境信息。


* **钩子函数**  <span id="hooks"></span> <a href="#ref">[↑]</a>

  对于每个节点，都支持 3 种钩子函数：

  ```cpp
  class MyNode : public Node {
   public:

    // 在一轮的开始时会被调用，就是说本节点的状态从其他变成 RUNNING 的时候：
    virtual void OnEnter(const Context& ctx){};

    // 在一轮的结束时会被调用，就是说本节点的状态变成 FAILURE/SUCCESS 的时候：
    virtual void OnTerminate(const Context& ctx, Status status){};

    // 在这个节点刚被构建完成时调用
    virtual void OnBuild() {}
  }
  ```

* **可视化**  <span id="visualization"></span> <a href="#ref">[↑]</a>

  `bt.cc` 实现了一个简单的实时把行为树运行状态可视化展示的函数，绿色的节点表示正在被 tick 的节点。

  用法示例：

  ```cpp
  // tick 主循环中
  ++ctx.seq;
  root.Tick(ctx)
  root.Visualize(ctx.seq)
  ```

* **黑板 ?**  <span id="blackboard"></span> <a href="#ref">[↑]</a>

  实际上，如果不面向非开发人员的话，行为树和黑板是不需要序列化机制的，也就是说，可以直接使用一个 `struct` 来作为黑板，简单而高效：

  当一颗行为树面向多个实体时，也就是说，多个实体、一套行为的时候，黑板可以是一个「世界」、或者可以访问到世界中的所有实体的一个句柄。

  ```cpp
  struct Blackboard {
      World* world;
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

* **Tick 循环**   <span id="ticker-loop"></span> <a href="#ref">[↑]</a>

  `bt.cc` 中内置了一个简单额 tick 主循环：

  ```cpp
  root.TickForever(ctx, 100ms);
  ```

* **自定义行为树的构建器 Builder**  <span id="custom-builder"></span> <a href="#ref">[↑]</a>

  ```cpp
  // 假设我们要添加一个自定义的装饰节点
  // 先定义一个 Node class
  class MyCustomMethodNode : public bt::DecoratorNode {
   public:
    MyCustomMethodNode(std::string_veiw name, ..) : bt::DecoratorNode(name) {}
    // 实现核心的 Update 方法
    bt::Status Update(const bt::Context& ctx) override {
      // 向下传递 tick 到子节点
      // child->Tick(ctx)
      return status;
    }
  };

  // 定义一个自己的 Tree 类
  class MyTree : public bt::RootNode, public bt::Builder<MyTree> {
   public:
    // 在构造函数中注意绑定到 builder
    MyTree(std::string_view name = "Root") : bt::RootNode(name), Builder()  { bindRoot(*this); }
    // 实现自定义方法 MyCustomMethod 来创建一个 MyCustomMethodNode
    // C 是一个通用的创建节点的方法
    auto& MyCustomMethod(...) { return C<MyCustomMethodNode>(...); }
  };

  MyTree root;

  root
  .MyCustomMethod(...)
  ;
  ```

* **信号和事件** <span id="signals"></span> <a href="#ref">[↑]</a>

  在行为树中释放和处理信号是常见的情况, 但是信号和事件的处理是一个复杂的事情, 我并不想让其和 bt.cc 这个很小的库耦合起来.

  一般来说, 要想把信号处理的带入 bt.cc 中, 思路如下:

  1. 创建一个自定义的装饰节点, 比如说叫做 `OnSignalNode`.
  2. 创建一个自定义的 Builder 类, 添加一个方法叫做 `OnSignal`.
  3. `OnSignal` 装饰器只有在关心的信号发生时才向下传递 tick 到子节点.
  4. 跟随信号一起传递的数据, 可以临时放在黑板上, tick 后记得清除.
  5. 可以把 `OnSignal` 节点尽量向上提, 这样会使得行为树更像事件驱动的一点, 提高效率.

  下面是一个具体的例子, 采用的是我的另一个小的事件库 [blinker.h](https://github.com/hit9/blinker.h),
  具体的代码可以参考目录 [example/onsignal](example/onsignal).

  ```cpp
  root
    .Parallel()
    ._().Action<C>()
    ._().OnSignal("a.*")    // 一旦这里没匹配到信号，就不会向下 tick 了
    ._()._().Parallel()
    ._()._()._().OnSignal("a.a")
    ._()._()._()._().Action<A>()
    ._()._()._().OnSignal("a.b")
    ._()._()._()._().Action<B>()
    .End()
    ;
  ```

* **树的遍历** <span id="traversal"></span> <a href="#ref">[↑]</a>

  有一个简单的方法可以来深度优先遍历一棵行为树:

  ```cpp
  // 前序回调方法, 在节点 node 访问之前执行, ptr 是上游持有 node 节点的 unique_ptr 指针的引用
  // 注意 ptr 可能是空指针 nullptr (当访问 root 节点时)
  bt::TraversalCallback preOrder = [&](bt:Node& node, bt::Ptr<bt::Node>& ptr) {
      // TODO
  };

  // 后序回调方法, 在节点 node 和其所有子孙访问之后执行, ptr 含义和前面所说一样
  bt::TraversalCallback postOrder = [&](bt::Node& node, bt::Ptr<bt::Node>& ptr) {
      // TODO
  };

  // 此外, 可以用 bt::NullTraversalCallback 来表示一个空回调
  root.Traverse(preOrder, postOrder, NullNodePtr);
  ```

## License

BSD.
