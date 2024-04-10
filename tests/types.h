
#include "bt.h"

struct Blackboard {
  // Counters for every Action
  int counterA = 0;
  int counterB = 0;
  int counterE = 0;

  // Commands for every Action/Condition.
  bt::Status shouldA = bt::Status::RUNNING;
  bt::Status shouldB = bt::Status::RUNNING;
  bt::Status shouldE = bt::Status::RUNNING;

  bool shouldC = false;
  bool shouldD = false;
  bool shouldF = false;

  // Status snapshots for every Action.
  bt::Status statusA = bt::Status::UNDEFINED;
  bt::Status statusB = bt::Status::UNDEFINED;
  bt::Status statusE = bt::Status::UNDEFINED;
};

class A : public bt::Action {
 public:
  bt::Status Update(const bt::Context& ctx) override {
    auto bb = std::any_cast<std::shared_ptr<Blackboard>>(ctx.data);
    bb->counterA++;
    return bb->statusA = bb->shouldA;
  }
};

class B : public bt::Action {
 public:
  bt::Status Update(const bt::Context& ctx) override {
    auto bb = std::any_cast<std::shared_ptr<Blackboard>>(ctx.data);
    bb->counterB++;
    return bb->statusB = bb->shouldB;
  }
};

class C : public bt::Condition {
 public:
  bool Check(const bt::Context& ctx) override {
    auto bb = std::any_cast<std::shared_ptr<Blackboard>>(ctx.data);
    return bb->shouldC;
  }
};

class D : public bt::Condition {
 public:
  bool Check(const bt::Context& ctx) override {
    auto bb = std::any_cast<std::shared_ptr<Blackboard>>(ctx.data);
    return bb->shouldD;
  }
};

class E : public bt::Action {
 public:
  bt::Status Update(const bt::Context& ctx) override {
    auto bb = std::any_cast<std::shared_ptr<Blackboard>>(ctx.data);
    bb->counterE++;
    return bb->statusE = bb->shouldE;
  }
};

class F : public bt::Condition {
 public:
  bool Check(const bt::Context& ctx) override {
    auto bb = std::any_cast<std::shared_ptr<Blackboard>>(ctx.data);
    return bb->shouldF;
  }
};

