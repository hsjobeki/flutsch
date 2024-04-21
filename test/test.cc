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
#include <cstdlib>  // for getenv

using namespace nix;


std::string getAssetPath(const std::string& relativePath) {
    const char* basePath = std::getenv("TEST_ASSET_PATH");
    if (!basePath) {
        FAIL("TEST_ASSET_PATH is not set. 'export TEST_ASSET_PATH=<abs_path_to_assets>'");  // Default path if environment variable is not set
    }
    return std::string(basePath) + "/" + relativePath; 
}

struct TestArgs : nix::MixEvalArgs,
                  nix::MixCommonArgs,
                  nix::RootArgs,
                  flutsch::Config {

    TestArgs() : nix::MixCommonArgs("flutsch") {}
};

static TestArgs args;

template<typename Func>
void init(std::string test_file, Func test_fn) {
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

        evalSettings.pureEval = false;

        Path gcRootsDir;
        std::optional<std::vector<std::string>> emptyList({});

        std::string expect_file = test_file;
        std::size_t pos = expect_file.rfind(".nix");
        if (pos != std::string::npos) {
            expect_file.replace(pos, 4, ".expect");
            expect_file = getAssetPath(expect_file);
        }
        // Read the file content into a string
        std::string content;
        std::ifstream file(expect_file);
        if (file) {
            content = std::string((std::istreambuf_iterator<char>(file)),
                                std::istreambuf_iterator<char>());
            file.close();  // Close the file after reading
        } else {
            std::cerr << "Could not open the file: " << expect_file << std::endl;
            FAIL("Could not open the file: " << expect_file); 
        }
        WARN("EXPECTED: " << content);


        std::string releaseExpr = getAssetPath(test_file);
        auto config = flutsch::Config{
            emptyList,       releaseExpr,        args.config,
            args.gcRootsDir, args.flake,         args.fromArgs,
            args.showTrace,  args.impure,        args.checkCacheStatus,
            args.nrWorkers,  args.maxMemorySize, args.lockFlags};

        WARN("Expr from: " << releaseExpr);

        // test comes here
        auto analyzer = flutsch::Analyzer(args, config);

        analyzer.init_root_value();

        std::cout << "Analyzer created" << std::endl;



        test_fn(analyzer, content);
    });
}

TEST_CASE("Create Analyzer", "simple.nix") {
    init(std::string("simple.nix"),[&](flutsch::Analyzer &test, std::string expected) {
        REQUIRE(expected == test.print_root_value());
    });
}
