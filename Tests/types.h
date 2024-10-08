#include <string>

#include "bt.h"

struct Blackboard
{
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

	uint shouldPriorityG = 1;
	uint shouldPriorityH = 1;
	uint shouldPriorityI = 1;

	// Status snapshots for every Action.
	bt::Status statusA = bt::Status::UNDEFINED;
	bt::Status statusB = bt::Status::UNDEFINED;
	bt::Status statusE = bt::Status::UNDEFINED;
	bt::Status statusG = bt::Status::UNDEFINED;
	bt::Status statusH = bt::Status::UNDEFINED;
	bt::Status statusI = bt::Status::UNDEFINED;

	bool onEnterCalledA = false;
	bool onTerminatedCalledA = false;

	int customDecoratorCounter = 0;
};

struct Entity
{
	bt::DynamicTreeBlob blob;
};

template <std::size_t N, std::size_t M = sizeof(bt::NodeBlob)>
struct EntityFixedBlob
{
	bt::FixedTreeBlob<N, M> blob;
};

class A : public bt::ActionNode
{
public:
	void OnEnter(const bt::Context& ctx) override
	{
		auto bb = std::any_cast<std::shared_ptr<Blackboard>>(ctx.data);
		bb->onEnterCalledA = true;
	}
	bt::Status Update(const bt::Context& ctx) override
	{
		auto bb = std::any_cast<std::shared_ptr<Blackboard>>(ctx.data);
		bb->counterA++;
		return bb->statusA = bb->shouldA;
	}
	void OnTerminate(const bt::Context& ctx, bt::Status status) override
	{
		auto bb = std::any_cast<std::shared_ptr<Blackboard>>(ctx.data);
		bb->onTerminatedCalledA = true;
	}
};

class B : public bt::ActionNode
{
public:
	bt::Status Update(const bt::Context& ctx) override
	{
		auto bb = std::any_cast<std::shared_ptr<Blackboard>>(ctx.data);
		bb->counterB++;
		return bb->statusB = bb->shouldB;
	}
};

class C : public bt::ConditionNode
{
public:
	bool Check(const bt::Context& ctx) override
	{
		auto bb = std::any_cast<std::shared_ptr<Blackboard>>(ctx.data);
		return bb->shouldC;
	}
};

class D : public bt::ConditionNode
{
public:
	bool Check(const bt::Context& ctx) override
	{
		auto bb = std::any_cast<std::shared_ptr<Blackboard>>(ctx.data);
		return bb->shouldD;
	}
};

class E : public bt::ActionNode
{
public:
	bt::Status Update(const bt::Context& ctx) override
	{
		auto bb = std::any_cast<std::shared_ptr<Blackboard>>(ctx.data);
		bb->counterE++;
		return bb->statusE = bb->shouldE;
	}
};

class F : public bt::ConditionNode
{
public:
	bool Check(const bt::Context& ctx) override
	{
		auto bb = std::any_cast<std::shared_ptr<Blackboard>>(ctx.data);
		return bb->shouldF;
	}
};

class G : public bt::ActionNode
{
public:
	bt::Status Update(const bt::Context& ctx) override
	{
		auto bb = std::any_cast<std::shared_ptr<Blackboard>>(ctx.data);
		bb->counterG++;
		return bb->statusG = bb->shouldG;
	}
	uint Priority(const bt::Context& ctx) const override
	{
		auto bb = std::any_cast<std::shared_ptr<Blackboard>>(ctx.data);
		return bb->shouldPriorityG;
	}
};

class H : public bt::ActionNode
{
public:
	bt::Status Update(const bt::Context& ctx) override
	{
		auto bb = std::any_cast<std::shared_ptr<Blackboard>>(ctx.data);
		bb->counterH++;
		return bb->statusH = bb->shouldH;
	}
	uint Priority(const bt::Context& ctx) const override
	{
		auto bb = std::any_cast<std::shared_ptr<Blackboard>>(ctx.data);
		return bb->shouldPriorityH;
	}
};

class I : public bt::ActionNode
{
public:
	bt::Status Update(const bt::Context& ctx) override
	{
		auto bb = std::any_cast<std::shared_ptr<Blackboard>>(ctx.data);
		bb->counterI++;
		return bb->statusI = bb->shouldI;
	}
	uint Priority(const bt::Context& ctx) const override
	{
		auto bb = std::any_cast<std::shared_ptr<Blackboard>>(ctx.data);
		return bb->shouldPriorityI;
	}
};

class J : public bt::ActionNode
{
public:
	std::string s;
	J(const std::string& name, const std::string& s)
		: bt::ActionNode(name), s(s) {}
};
