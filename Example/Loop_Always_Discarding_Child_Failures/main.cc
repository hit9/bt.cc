#include <cstdlib>

#include "bt.h"

using namespace std::chrono_literals;

class RandomFailureAction : public bt::Action
{
public:
	bt::Status Update(const bt::Context& ctx) override
	{
		return (std::rand() & 1) ? bt::Status::SUCCESS : bt::Status::FAILURE;
	}
	std::string_view Name() const override { return "RandomFailureAction"; }
};

int main(void)
{

	bt::Tree root("Root");

	// clang-format off
    root
    .Loop(-1)
    ._().ForceSuccess()
    ._()._().Action<RandomFailureAction>()
    .End();
	// clang-format on

	bt::DynamicTreeBlob blob;
	root.BindTreeBlob(blob);
	bt::Context ctx;
	root.TickForever(ctx, 500ms, true);
	root.UnbindTreeBlob();
	return 0;
};
