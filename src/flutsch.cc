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

#include "flutsch.hh"
#include "eval.hh"
#include "value.hh"

#include <sys/types.h>
#include <sys/wait.h>
#include <sys/resource.h>

#include <nlohmann/json.hpp>
#include <unordered_map>
#include <utility>
#include <vector>

using namespace nix;
using namespace nlohmann;

// Safe to ignore - the args will be static.
#ifdef __GNUC__
#pragma GCC diagnostic ignored "-Wnon-virtual-dtor"
#elif __clang__
#pragma clang diagnostic ignored "-Wnon-virtual-dtor"
#endif


namespace flutsch {

typedef std::unordered_map<AttrEntry, ValueIntrospection, AttrEntryHash> FlutschMap;
static FlutschMap valueMap;


std::string attrPathJoin(std::vector<std::string> path) {
    return std::accumulate(path.begin(), path.end(), std::string(),
                           [](std::string ss, std::string s) {
                               auto name = std::string(s);
                               //    Escape token if containing dots
                               if (name.find(".") != std::string::npos) {
                                   name = "\"" + name + "\"";
                               }
                               return ss.empty() ? name : ss + "." + name;
                           });
}
std::string attrPathJoin(std::vector<AttrEntry> path) {
    return std::accumulate(path.begin(), path.end(), std::string(),
                           [](std::string ss, AttrEntry s) {
                               auto name =
                                   std::string(s.name.value_or("<unnamed>"));
                               if (s.isRoot) {
                                   name = "<root>";
                               }
                               //    Escape token if containing dots
                               if (name.find(".") != std::string::npos) {
                                   name = "\"" + name + "\"";
                               }
                               return ss.empty() ? name : ss + "." + name;
                           });
}

std::vector<std::string> attrPathToPath(std::vector<AttrEntry> path) {
    std::vector<std::string> result;
    std::transform(path.begin(), path.end(), std::back_inserter(result),
                   [](AttrEntry &e) {
                       if (e.isRoot) {
                           return std::string("<root>");
                       } else {

                           return e.name.value_or("<unnamed>");
                       }
                   });

    return result;
}

static std::string errorToString(nix::Error *error) {
    if (nullptr != dynamic_cast<nix::RestrictedPathError *>(error)) {
        return std::string("RestrictedPathError " + error->msg());
    } else if (nullptr != dynamic_cast<nix::MissingArgumentError *>(error)) {
        return std::string("MissingArgumentError");
    } else if (nullptr != dynamic_cast<nix::UndefinedVarError *>(error)) {
        return std::string("UndefinedVarError");
    } else if (nullptr != dynamic_cast<nix::TypeError *>(error)) {
        return std::string("TypeError");
    } else if (nullptr != dynamic_cast<nix::Abort *>(error)) {
        return std::string("Abort");
    } else if (nullptr != dynamic_cast<nix::ThrownError *>(error)) {
        return std::string("Throw '" + error->info().msg.str() + "' ");
    } else if (nullptr != dynamic_cast<nix::AssertionError *>(error)) {
        return std::string("AssertionError");
    } else if (nullptr != dynamic_cast<nix::ParseError *>(error)) {
        return std::string("ParseError");
    } else if (nullptr != dynamic_cast<nix::EvalError *>(error)) {
        return std::string("EvalError");
    } else {
        return std::string("Error");
    }
}

void displaySet(const std::set<std::string> &s) {
    // Printing the elements of
    // the set
    for (auto itr : s) {
        std::cout << itr << " ";
    }
}
void displaySet(const std::set<std::vector<AttrEntry>> &s) {
    // Printing the elements of
    // the set
    for (auto itr : s) {
        // std::cout << itr << " ";
    }
}

void displayAttrs(nix::Value *attrs, ref<EvalState> state) {
    std::cout << "{ ";
    for (auto &i : attrs->attrs->lexicographicOrder(state->symbols)) {
        const std::string &name = state->symbols[i->name];
        std::cout << name << "=...; ";
    }
    std::cout << "}" << std::endl;
}

void displayLambda(nix::Value *lambda, ref<EvalState> state) {
    std::cout << "<lambda";
    PosIdx posIdx = lambda->lambda.fun->pos;
    if (posIdx) {
        Pos pos = state->positions[posIdx];
        std::cout << pos;
    }
    if (lambda->lambda.fun->hasFormals()) {
        std::cout << " {";
        for (auto i : lambda->lambda.fun->formals->formals) {
            std::cout << state->symbols[i.name] << ",";
        }
        std::cout << "}";
    }

    std::cout << ">" << std::endl;
}

void displayValueMap(
    const FlutschMap &s) {
    std::vector<std::pair<const AttrEntry &, const ValueIntrospection &>> list(
        s.begin(), s.end());

    for (auto pair = list.rbegin(); pair != list.rend(); ++pair) {
        std::cout << "Key: " << pair->first << ", Value: " << pair->second
                  << std::endl;
    }
}

static Value *releaseExprTopLevelValue(EvalState &state, Bindings &autoArgs,
                                       const flutsch::Config &config) {
    Value vTop;

    if (config.fromArgs) {
        Expr *e = state.parseExprFromString(
            config.releaseExpr, state.rootPath(CanonPath::fromCwd()));
        state.eval(e, vTop);
    } else {
        state.evalFile(lookupFileArg(state, config.releaseExpr), vTop);
    }

    auto vRoot = state.allocValue();

    state.autoCallFunction(autoArgs, vTop, *vRoot);

    return vRoot;
}

std::optional<Pos> getPos(ref<EvalState> state, PosIdx &posIdx) {
    Pos pos = state->positions[posIdx];
    if (posIdx == noPos) {
        std::cout << " doesn't have explicit source position" << std::endl;
        return {};
    }

    if (auto path = std::get_if<SourcePath>(&pos.origin)) {
        return {pos};
    }

    std::cout << "Warning: unsupported Nix source. Only files "
                 "are suported. "
              << std::endl;
    return {};
}

void getPositions(MixEvalArgs &args, flutsch::Config const &config) {
    std::cout << "positionsEval" << std::endl;

    auto state = ref<EvalState>(std::make_shared<EvalState>(
        args.searchPath, openStore(*args.evalStoreUrl)));
    Bindings &autoArgs = *args.getAutoArgs(*state);

    nix::Value *vRoot = [&]() {
        if (config.flake) {
            auto [flakeRef, fragment, outputSpec] =
                parseFlakeRefWithFragmentAndExtendedOutputsSpec(
                    config.releaseExpr, absPath("."));
            InstallableFlake flake{
                {}, state, std::move(flakeRef), fragment, outputSpec,
                {}, {},    config.lockFlags};

            return flake.toValue(*state).first;
        } else {
            return releaseExprTopLevelValue(*state, autoArgs, config);
        }
    }();

    if (vRoot->type() != nAttrs) {
        throw EvalError("Top level attribute is not an attrset");
    }
    // create an empty list []
    json out = json::array({});

    // Add a single entry from nixValue
    const auto introspectValue = [&](std::vector<AttrEntry> attrPath,
                                     nix::Value *test) {
        std::string attr = attrPathJoin(attrPath);
        std::cout << "Introspecting value of " << attr << " @ " << &test
                  << std::endl;
        // ValueMap entry where we will add the value introspection results.
        auto data = valueMap.find(attrPath.back());

        try {
            state->forceValue(*test, noPos);
            PosIdx posIdx;
            std::string type;

            if (test->type() == nAttrs) {
                state->forceAttrs(*test, noPos, "error");
                std::cout << "Got: ";
                displayAttrs(test, state);
                posIdx = test->attrs->pos;
                type = std::string("attrset");
            }

            if (test->isLambda()) {
                state->forceFunction(*test, noPos, "error");
                std::cout << "Got: ";
                displayLambda(test, state);
                posIdx = test->lambda.fun->getPos();
                type = std::string("lambda");
            }

            if (data != valueMap.end()) {
                if (auto p = getPos(state, posIdx)) {
                    data->second.valuePos.emplace(*p);
                } else {
                    data->second.valuePos.reset();
                }
                data->second.valueType = type;
            }

        } catch (nix::Error &e) {
            std::cout << "errPos: " << e.info().errPos << std::endl;
            if (data != valueMap.end()) {
                auto pos = e.info().errPos;
                std::cout << "inserting into: " << data->first << std::endl;
                data->second.valueType = errorToString(&e);
                bool hasPos = pos && *pos;
                if (hasPos) {
                    data->second.valuePos.emplace(*pos);
                }
            }
        }
        // Finally
        if (data != valueMap.end()) {
            // Set this to avoid duplicate analysis for the same value
            data->second.isIntrospected = true;
        }
    };

    // Recurse into test attrset
    std::function<void(std::vector<AttrEntry>, nix::Value *)> recurseValues;
    recurseValues = [&](std::vector<AttrEntry> attrPath,
                        nix::Value *testAttrs) -> void {
        // std::cout << "recursing: " << attrPathJoin(attrPath) << std::endl;

        for (auto &i : testAttrs->attrs->lexicographicOrder(state->symbols)) {
            // might not have a name, if its the root attrset;
            // value: testAttrs

            const std::string &name = state->symbols[i->name];
            Pos currPos = state->positions[i->pos];
            std::cout << "looking into symbol: " << name << std::endl;

            // Copy and append current attribute
            std::vector<AttrEntry> curAttrPath = attrPath;
            const auto attrEntry = AttrEntry(i->value, name, i->pos);
            curAttrPath.push_back(attrEntry);
            auto path = attrPathToPath(curAttrPath);

            // Create an entry for the attribute if none exists.
            auto entry = valueMap.find(attrEntry);
            if (entry == valueMap.end()) {
                std::cout << "valueMap - inserting: " << attrPathJoin(path)
                          << std::endl;
                valueMap.emplace(attrEntry, ValueIntrospection(path));
                entry = valueMap.find(attrEntry);
            }

            // Important: Use this only as a key to find the actual parent in
            // the map
            auto parentAttrsKey = AttrEntry(testAttrs);
            auto parent = valueMap.find(parentAttrsKey);

            if (parent != valueMap.end() && entry != valueMap.end()) {
                std::cout << "adding: " << entry->first
                          << " as child to: " << parent->first << std::endl;
                parent->second.children.emplace(name, entry->first);
            }

            // Skip if value exists and is already analyzed
            if (entry != valueMap.end() && entry->second.isIntrospected) {
                std::cout << "SKIPPING: " << attrEntry << " already analyzed - "
                          << entry->second << std::endl;
                continue;
            }

            introspectValue(curAttrPath, i->value);

            // If value is an attrset recurse further into tree
            {
                nix::Value *value = i->value;
                try {
                    state->forceValue(*value, noPos);
                    if (value->type() == nAttrs) {
                        recurseValues(curAttrPath, value);
                    }
                } catch (nix::Error &e) {
                    // std::cout << "skipping ErrorValue" << std::endl;
                }
            }
        }
    };

    auto rootKey = AttrEntry(vRoot, {}, vRoot->attrs->pos);
    rootKey.isRoot = true;

    auto initPath = std::vector<AttrEntry>({rootKey});
    valueMap.emplace(rootKey, ValueIntrospection());
    introspectValue(initPath, vRoot);
    recurseValues(initPath, vRoot);
    std::cout << "\n---\nValueMap: \n";
    displayValueMap(valueMap);
    std::cout << "---\n";

    std::string pretty = out.dump(4);
    // std::cout << pretty << std::endl;
}

} // namespace flutsch