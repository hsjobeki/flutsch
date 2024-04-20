#include "common-eval-args.hh"
#include "nixexpr.hh"
#include "position.hh"
#include "search-path.hh"
#include <filesystem>
#include <nix/flake/flake.hh>
#include <iostream>
#include <optional>
#include <unordered_map>
#include <vector>
#include "eval.hh"

using namespace nix;

#ifndef FLUTSCH_H
#define FLUTSCH_H

namespace flutsch {

struct Config {
    const std::optional<std::vector<std::string>> useRecurseIntoAttrs;
    std::string releaseExpr;
    std::optional<std::string> config;
    Path gcRootsDir;

    bool flake = false;
    bool fromArgs = false;
    bool showTrace = false;
    bool impure = false;
    bool checkCacheStatus = false;
    size_t nrWorkers = 1;
    size_t maxMemorySize = 4096;

    // usually in MixFlakeOptions
    flake::LockFlags lockFlags = {.updateLockFile = false,
                                  .writeLockFile = false,
                                  .useRegistries = false,
                                  .allowUnlocked = false};
};

void getPositions(MixEvalArgs &args, flutsch::Config const &config);

void recurseValues(std::vector<AttrEntry> attrPath, nix::Value *testAttrs);

class Analyzer {

  private:
    nix::ref<EvalState> state;
    nix::Value *vRoot;

  public:
    
    explicit Analyzer(MixEvalArgs &args, flutsch::Config config);

    void init_root_value(MixEvalArgs &args, flutsch::Config config);

};
}; // namespace flutsch


#endif // FLUTSCH_H