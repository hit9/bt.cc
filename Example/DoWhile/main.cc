#include <cstdlib>
#include <iostream>
#include <memory>
#include <thread>

#include "bt.h"

using namespace std::chrono_literals;

struct Blackboard
{
	int tickCount = 0;
	int round = 0;
};

class CountTicks : public bt::ActionNode
{
public:
	void OnEnter(const bt::Context& ctx) override
	{
		auto b = std::any_cast<std::shared_ptr<Blackboard>>(ctx.data);
		b->tickCount = 0;
		b->round++;
		std::cout << "Starts a new round, total rounds now: " << b->round << std::endl;
	}

	bt::Status Update(const bt::Context& ctx) override
	{
		auto b = std::any_cast<std::shared_ptr<Blackboard>>(ctx.data);
		++(b->tickCount);

		std::cout << "New tick, total ticks now inside current round: " << b->tickCount << std::endl;
		if (b->tickCount == 2)
			return bt::Status::SUCCESS;
		return bt::Status::RUNNING;
	}
	std::string_view Name() const override { return "CountTicks"; }
};

class Log : public bt::ActionNode
{
public:
	explicit Log(std::string_view message, std::string_view name = "Log")
		: ActionNode(name), message(message) {}

	bt::Status Update(const bt::Context& ctx) override
	{
		std::cout << message << std::endl;
		return bt::Status::SUCCESS;
	}

private:
	std::string message;
};

class IsNumberOfRoundsLessThan : public bt::ConditionNode
{
public:
	explicit IsNumberOfRoundsLessThan(int k)
		: k(k) {}

	std::string_view Name() const override { return "IsNumberOfRoundsLessThan"; }

	bool Check(const bt::Context& ctx) override
	{
		auto b = std::any_cast<std::shared_ptr<Blackboard>>(ctx.data);
		if (b->round < k)
			return true;
		return false;
	}

private:
	int k = 0;
};

class ResetRound : public bt::ActionNode
{
public:
	bt::Status Update(const bt::Context& ctx) override
	{
		auto b = std::any_cast<std::shared_ptr<Blackboard>>(ctx.data);
		b->round = 0;
		b->tickCount = 0;

		std::cout << "Round Reset to 0. " << std::endl;
		return bt::Status::SUCCESS;
	}
	std::string_view Name() const override { return "ResetRound"; }
};

int main(void)
{
	bt::Tree root("Root");

	// clang-format off
    root
    .Selector()
    ._().Sequence() // If >=4, reset round => 0
    ._()._().Not<IsNumberOfRoundsLessThan>(4)
    ._()._().Action<ResetRound>()
    ._().Sequence()
    ._()._().DoWhile<IsNumberOfRoundsLessThan>(3) // Otherwise, ++round to 4
    ._()._()._().Delay(1s)
    ._()._()._()._().Action<CountTicks>()
    ._()._().Action<Log>("Escaped from DoWhile")
    .End()
    ;
	// clang-format on

	bt::DynamicTreeBlob blob;

	bt::Context ctx;
	ctx.data = std::make_shared<Blackboard>();

	while (true)
	{
		root.BindTreeBlob(blob);
		++ctx.seq;
		root.Tick(ctx);
		// root.Visualize(ctx.seq);
		root.UnbindTreeBlob();
		std::this_thread::sleep_for(100ms);
	}

	return 0;
}
