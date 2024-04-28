#include <any>
#include <chrono>
#include <cstdlib>
#include <initializer_list>
#include <iostream>
#include <memory>
#include <string>
#include <string_view>

#include "bt.h"
#include "third_party/blinker.h"

// Max number of nodes in the board.
const std::size_t N = 1024;

using namespace std::chrono_literals;

// Blackboard.
struct Blackboard {
  // tmp signal data
  std::any signalData;
};

// Signal board
blinker::Board<N> board;

// Signals (shared ptrs)
auto signalAA = board.NewSignal("a.a");
auto signalAB = board.NewSignal("a.b");

// Custom Decorator OnSignalNode.
class OnSignalNode : public bt::DecoratorNode {
 private:
  // Connection to subscrible signals
  std::unique_ptr<blinker::Connection<N>> connection;

 public:
  OnSignalNode(const std::string& name, std::initializer_list<std::string_view> patterns)
      : bt::DecoratorNode(name) {
    // Subscrible a connection to signals matching given patterns.
    connection = board.Connect(patterns);
  }

  bt::Status Update(const bt::Context& ctx) override {
    auto status = bt::Status::FAILURE;
    // Poll interested signals from board
    connection->Poll([&](const blinker::SignalId id, std::any data) {
      auto blackboard = std::any_cast<std::shared_ptr<Blackboard>>(ctx.data);
      blackboard->signalData = data;
      status = child->Tick(ctx);
      blackboard->signalData.reset();  // clear
    });
    return status;
  }
};

// Action A prints a simple statement.
class A : public bt::ActionNode {
 public:
  std::string_view Name() const override { return "A"; }
  bt::Status Update(const bt::Context& ctx) override {
    auto blackboard = std::any_cast<std::shared_ptr<Blackboard>>(ctx.data);
    auto value = std::any_cast<int>(blackboard->signalData);
    std::cout << "action a, data: " << value << std::endl;
    return bt::Status::SUCCESS;
  }
};

// Action B prints a simple statement.
class B : public bt::ActionNode {
 public:
  std::string_view Name() const override { return "B"; }
  bt::Status Update(const bt::Context& ctx) override {
    auto blackboard = std::any_cast<std::shared_ptr<Blackboard>>(ctx.data);
    auto value = std::any_cast<std::string>(blackboard->signalData);
    std::cout << "action b, data: " << value << std::endl;
    return bt::Status::SUCCESS;
  }
};

// Action C emits signals randomly.
class C : public bt::ActionNode {
 public:
  std::string_view Name() const override { return "C"; }
  bt::Status Update(const bt::Context& ctx) override {
    // randomly emits signal
    int i = std::rand();
    if (i % 10 < 3) signalAA->Emit(i);
    if (i % 10 < 6) signalAB->Emit(std::string("abc" + std::to_string(i)));
    return bt::Status::SUCCESS;
  }
};

// Custom Tree.
class MyTree : public bt::RootNode, public bt::Builder<MyTree> {
 public:
  MyTree(std::string name = "Root") : bt::RootNode(name) { bindRoot(*this); }
  auto& OnSignal(std::initializer_list<std::string_view> patterns) {
    return C<OnSignalNode>("OnSignal", patterns);
  }
  auto& OnSignal(std::string_view pattern) { return OnSignal({pattern}); }
};

int main(void) {
  MyTree root("Root");

  bt::DynamicTreeBlob blob;

  // clang-format off
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

  // clang-format on

  auto blackboard = std::make_shared<Blackboard>();
  bt::Context ctx(blackboard);

  // Flip the board's internal double buffers post ticking.
  root.BindTreeBlob(blob);
  root.TickForever(ctx, 300ms, false, [&](const bt::Context& ctx) { board.Flip(); });
  root.UnbindTreeBlob();
  return 0;
}
