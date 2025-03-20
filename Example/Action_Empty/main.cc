#include <cstdlib>
#include <iostream>

#include "bt.h"

using namespace std::chrono_literals;

class True : public bt::ConditionNode
{
	std::string_view Name() const override { return "True"; }
	bool			 Check(const bt::Context& ctx) override { return true; }
};

class False : public bt::ConditionNode
{
	std::string_view Name() const override { return "False"; }
	bool			 Check(const bt::Context& ctx) override { return false; }
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
    .Switch()
    ._().Case<False>()
    ._()._().Action<bt::Empty>("TODO")  // Always returns SUCCESS
    ._().Case<True>()
    ._()._().Action<Print>("Here run Some Action")
    .End();
	// clang-format on

	bt::DynamicTreeBlob blob;
	root.BindTreeBlob(blob);
	bt::Context ctx;
	root.TickForever(ctx, 100ms, true);
	root.UnbindTreeBlob();
	return 0;
};
