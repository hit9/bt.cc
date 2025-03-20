#include <cstdlib>
#include <iostream>

#include "bt.h"

using namespace std::chrono_literals;

class LiteralBooleanCondition : public bt::ConditionNode
{
public:
	explicit LiteralBooleanCondition(bool value)
		: value(value) {}
	std::string_view Name() const override { return "LiteralBooleanCondition"; }
	bool			 Check(const bt::Context& ctx) override { return value; }

private:
	bool value = false;
};

class Print : public bt::Action
{
public:
	explicit Print(std::string_view message)
		: message(message)
	{
	}

	bt::Status Update(const bt::Context& ctx) override
	{
		std::cout << message << std ::endl;
		return bt::Status::SUCCESS;
	}
	std::string_view Name() const override { return "Print"; }

private:
	const std::string message;
};

int main(void)
{

	bt::Tree root("Root");

	// clang-format off
    root
    .Selector()
    ._().Sequence()
    ._()._().If<bt::Not<LiteralBooleanCondition>>(false)  // !false
    ._()._()._().Action<Print>("bt::Not<LiteralBooleanCondition> works!")
    ._()._().IfNot<LiteralBooleanCondition>(false)  // !false
    ._()._()._().Action<Print>("IfNot<LiteralBooleanCondition> works!")
    ._().Action<Print>("Or nothing works ??? !!!") // turn the two `false` literals to `true` will result this log out
    .End();
	// clang-format on

	bt::DynamicTreeBlob blob;
	root.BindTreeBlob(blob);
	bt::Context ctx;
	root.TickForever(ctx, 300ms, false);
	root.UnbindTreeBlob();
	return 0;
};
