#include "args.hh"
#include "ref.hh"
#include <algorithm>
#include <cstddef>
#include <map>
#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <thread>
#include <fstream>
#include <filesystem>
#include <regex>
#include <nix/eval-settings.hh>
#include <nix/config.h>
#include <nix/shared.hh>
#include <nix/store-api.hh>
#include <nix/eval.hh>
#include <nix/eval-inline.hh>
#include <nix/util.hh>
#include <nix/get-drvs.hh>
#include <nix/globals.hh>
#include <nix/common-eval-args.hh>
#include <nix/flake/flakeref.hh>
#include <nix/flake/flake.hh>
#include <nix/position.hh>
#include <nix/attr-path.hh>
#include <nix/derivations.hh>
#include <nix/local-fs-store.hh>
#include <nix/logging.hh>
#include <nix/error.hh>
#include <nix/installables.hh>
#include <nix/path-with-outputs.hh>
#include <nix/installable-flake.hh>

#include <nix/value-to-json.hh>

#include <sys/types.h>
#include <sys/wait.h>
#include <sys/resource.h>

#include <vector>

#include <catch2/catch_test_macros.hpp>
#include "catch2/catch_all.hpp"
#include <flutsch.hh>

#include <iostream>
#include <sstream>

class AutoRestoreRdbuf {
    std::ostream& out;
    std::streambuf* old;

public:
    ~AutoRestoreRdbuf()
    {
        out.rdbuf(old);
    }
    AutoRestoreRdbuf(const AutoRestoreRdbuf&) = delete;
    AutoRestoreRdbuf(AutoRestoreRdbuf&&) = delete;

    AutoRestoreRdbuf(std::ostream& out)
        : out { out }
        , old { out.rdbuf() }
    {
    }
};

std::string stringWrittentToStream(std::function<void()> f, std::ostream& out = std::cout)
{
    AutoRestoreRdbuf restore { std::cout };
    std::ostringstream oss;
    std::cout.rdbuf(oss.rdbuf());
    f();
    return oss.str();
}

using namespace nix;

// struct CliArgs : nix::MixEvalArgs, nix::MixCommonArgs, RootArgs,
// flutsch::Config {
// };

void foo()
{
    std::cout << "foo";
}


void pre() {
    handleExceptions("abc", [&]() {
        initNix();
        initGC();
        // /* FIXME: The build hook in conjunction with import-from-derivation
        // is
        //  * causing "unexpected EOF" during eval */
        settings.builders = "";

        // /* Prevent access to paths outside of the Nix search path and
        //    to the environment. */
        evalSettings.restrictEval = false;

        evalSettings.pureEval = true;

        Path gcRootsDir;
        std::optional<std::vector<std::string>> emptyList({});

        std::string releaseExpr = "./test.nix";
        auto config = flutsch::Config{
            emptyList,
            releaseExpr,
            {},
            "",
        };
        std::cout << config.releaseExpr << std::endl;

        nix::MixEvalArgs args = nix::MixEvalArgs({});

        // test comes here
        auto analyzer = flutsch::Analyzer(args,config);

        std::cout << "Analyzer created" << std::endl;
    });
}

TEST_CASE("Create Analyzer", "[single-file]") {
    // auto analyzer = flutsch::Analyzer();
    // pre();
    auto s = stringWrittentToStream(&pre);
    REQUIRE(s == "\nAnalyzer created\n");
}
