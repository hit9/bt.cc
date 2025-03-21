#include <cstdlib>
#include <iostream>

#include "bt.h"

using namespace std::chrono_literals;

using bt::False;
using bt::True;

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
    .Parallel()
    ._().If<bt::And<True,True>>()
    ._()._().Action<Print>("True && True fires!")
    ._().If<bt::And<True,False>>()
    ._()._().Action<Print>("True && False wont fire!")
    ._().If<bt::Or<False, True>>()
    ._()._().Action<Print>("False || True fires!")
    ._().If<bt::And<True,bt::Not<False>>>()
    ._()._().Action<Print>("True && !False fires!")
    ._().If<bt::And<bt::Or<True, False>, True>>()
    ._()._().Action<Print>("(True && False) || True fires!")
    ._().If<bt::Or<False, False>>()
    ._()._().Action<Print>("False || False wont fire!")
    .End();
	// clang-format on

	bt::DynamicTreeBlob blob;
	root.BindTreeBlob(blob);
	bt::Context ctx;
	root.TickForever(ctx, 100ms, false);
	root.UnbindTreeBlob();
	return 0;
};
