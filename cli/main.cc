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

#include <nlohmann/json.hpp>
#include <vector>

#include <flutsch.hh>

using namespace nix;
using namespace nlohmann;

struct CliArgs : MixEvalArgs, MixCommonArgs, RootArgs, flutsch::Config {
    // std::string releaseExpr;
    // std::optional<std::string> config;
    // Path gcRootsDir;
    // bool flake = false;
    // bool fromArgs = false;
    // bool showTrace = false;
    // bool impure = false;
    // bool checkCacheStatus = false;
    // size_t nrWorkers = 1;
    // size_t maxMemorySize = 4096;

    // // usually in MixFlakeOptions
    // flake::LockFlags lockFlags = {.updateLockFile = false,
    //                               .writeLockFile = false,
    //                               .useRegistries = false,
    //                               .allowUnlocked = false};

    CliArgs() : MixCommonArgs("flutsch") {
        addFlag({
            .longName = "help",
            .description = "show usage information",
            .handler = {[&]() {
                printf("USAGE: flutsch [options] expr\n\n");
                for (const auto &[name, flag] : longFlags) {
                    if (hiddenCategories.count(flag->category)) {
                        continue;
                    }
                    printf("  --%-20s %s\n", name.c_str(),
                           flag->description.c_str());
                }
                ::exit(0);
            }},
        });

        addFlag({.longName = "config",
                 .description = "path to flutsch config file",
                 .labels = {"config"},
                 .handler = {&config}});

        addFlag({.longName = "impure",
                 .description = "allow impure expressions",
                 .handler = {&impure, true}});

        addFlag({.longName = "gc-roots-dir",
                 .description = "garbage collector roots directory",
                 .labels = {"path"},
                 .handler = {&gcRootsDir}});

        addFlag({.longName = "flake",
                 .description = "evaluate a flake",
                 .handler = {&flake, true}});

        addFlag({.longName = "show-trace",
                 .description =
                     "print out a stack trace in case of evaluation errors",
                 .handler = {&showTrace, true}});

        addFlag({.longName = "expr",
                 .shortName = 'E',
                 .description = "treat the argument as a Nix expression",
                 .handler = {&fromArgs, true}});

        // usually in MixFlakeOptions
        addFlag({
            .longName = "override-input",
            .description =
                "Override a specific flake input (e.g. `dwarffs/nixpkgs`).",
            .category = category,
            .labels = {"input-path", "flake-url"},
            .handler = {[&](std::string inputPath, std::string flakeRef) {
                // overriden inputs are unlocked
                lockFlags.allowUnlocked = true;
                lockFlags.inputOverrides.insert_or_assign(
                    flake::parseInputPath(inputPath),
                    parseFlakeRef(flakeRef, absPath("."), true));
            }},
        });

        expectArg("expr", &releaseExpr);
    }
};
#ifdef __GNUC__
#pragma GCC diagnostic ignored "-Wnon-virtual-dtor"
#elif __clang__
#pragma clang diagnostic ignored "-Wnon-virtual-dtor"
#endif

static CliArgs cliArgs;

int main(int argc, char **argv) {
    return handleExceptions(argv[0], [&]() {
        initNix();
        initGC();
        // /* FIXME: The build hook in conjunction with import-from-derivation is
        //  * causing "unexpected EOF" during eval */
        settings.builders = "";

        // /* Prevent access to paths outside of the Nix search path and
        //    to the environment. */
        evalSettings.restrictEval = false;

        // /* When building a flake, use pure evaluation (no access to
        //    'getEnv', 'currentSystem' etc. */
        if (cliArgs.impure) {
            evalSettings.pureEval = false;
        } else if (cliArgs.flake) {
            evalSettings.pureEval = true;
        }

        cliArgs.parseCmdline(argvToStrings(argc, argv));

        if (cliArgs.releaseExpr == "")
            throw UsageError("no expression specified");

        

        
        json config({});
        if( cliArgs.config.has_value() ){
            std::cout << cliArgs.config.value() << std::endl;
            std::ifstream f(cliArgs.config.value());
            config = json::parse(f, nullptr,true,true);
        }
        std::cout << config << std::endl;



        if (cliArgs.gcRootsDir == "") {
            printMsg(lvlError, "warning: `--gc-roots-dir' not specified");
        } else {
            cliArgs.gcRootsDir = std::filesystem::absolute(cliArgs.gcRootsDir);
        }

        if (cliArgs.showTrace) {
            loggerSettings.showTrace.assign(true);
        }
        
        std::cout << cliArgs.releaseExpr << std::endl;

    
        std::optional<std::vector<std::string>> emptyList({});
        auto flutsch_conf = flutsch::Config {
            config.value("useRecurseIntoAttrs", emptyList),
            // emptyList,
            cliArgs.releaseExpr,
            cliArgs.config,
            cliArgs.gcRootsDir,
            cliArgs.flake,
            cliArgs.fromArgs,
            cliArgs.showTrace,
            cliArgs.impure,
            cliArgs.checkCacheStatus,
            cliArgs.nrWorkers,
            cliArgs.maxMemorySize,
            cliArgs.lockFlags
        };
        std::cout << "rootDir" << cliArgs.gcRootsDir << std::endl;

        flutsch::getPositions(cliArgs,flutsch_conf);
    });
}
