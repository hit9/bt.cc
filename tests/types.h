
#include "bt.h"

struct Blackboard {
  // Counters for every Action
  int counterA = 0;
  int counterB = 0;
  int counterE = 0;
  int counterG = 0;
  int counterH = 0;
  int counterI = 0;

  // Commands for every Action/Condition.
  bt::Status shouldA = bt::Status::RUNNING;
  bt::Status shouldB = bt::Status::RUNNING;
  bt::Status shouldE = bt::Status::RUNNING;
  bt::Status shouldG = bt::Status::RUNNING;
  bt::Status shouldH = bt::Status::RUNNING;
  bt::Status shouldI = bt::Status::RUNNING;

  bool shouldC = false;
  bool shouldD = false;
  bool shouldF = false;

  uint shouldPriorityG = 0;
  uint shouldPriorityH = 0;
  uint shouldPriorityI = 0;

  // Status snapshots for every Action.
  bt::Status statusA = bt::Status::UNDEFINED;
  bt::Status statusB = bt::Status::UNDEFINED;
  bt::Status statusE = bt::Status::UNDEFINED;
  bt::Status statusG = bt::Status::UNDEFINED;
  bt::Status statusH = bt::Status::UNDEFINED;
  bt::Status statusI = bt::Status::UNDEFINED;
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

class G : public bt::Action {
 public:
  bt::Status Update(const bt::Context& ctx) override {
    auto bb = std::any_cast<std::shared_ptr<Blackboard>>(ctx.data);
    bb->counterG++;
    return bb->statusG = bb->shouldG;
  }
  uint Priority(const bt::Context& ctx) const override {
    auto bb = std::any_cast<std::shared_ptr<Blackboard>>(ctx.data);
    return bb->shouldPriorityG;
  }
};

class H : public bt::Action {
 public:
  bt::Status Update(const bt::Context& ctx) override {
    auto bb = std::any_cast<std::shared_ptr<Blackboard>>(ctx.data);
    bb->counterH++;
    return bb->statusH = bb->shouldH;
  }
  uint Priority(const bt::Context& ctx) const override {
    auto bb = std::any_cast<std::shared_ptr<Blackboard>>(ctx.data);
    return bb->shouldPriorityH;
  }
};

class I : public bt::Action {
 public:
  bt::Status Update(const bt::Context& ctx) override {
    auto bb = std::any_cast<std::shared_ptr<Blackboard>>(ctx.data);
    bb->counterI++;
    return bb->statusI = bb->shouldI;
  }
  uint Priority(const bt::Context& ctx) const override {
    auto bb = std::any_cast<std::shared_ptr<Blackboard>>(ctx.data);
    return bb->shouldPriorityI;
  }
};
