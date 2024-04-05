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

typedef std::unordered_map<AttrEntry, ValueIntrospection, AttrEntryHash>
    FlutschMap;
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

static std::pair<std::string,std::optional<std::string>> errorInfo(nix::Error *error) {
    if (nullptr != dynamic_cast<nix::RestrictedPathError *>(error)) {
        return {std::string("RestrictedPathError"), error->msg()};
    } else if (nullptr != dynamic_cast<nix::MissingArgumentError *>(error)) {
        return {std::string("MissingArgumentError"),{}};
    } else if (nullptr != dynamic_cast<nix::UndefinedVarError *>(error)) {
        return {std::string("UndefinedVarError"),{}};
    } else if (nullptr != dynamic_cast<nix::TypeError *>(error)) {
        return {std::string("TypeError"),{}};
    } else if (nullptr != dynamic_cast<nix::Abort *>(error)) {
        return {std::string("Abort"),{}};
    } else if (nullptr != dynamic_cast<nix::ThrownError *>(error)) {
        return {std::string("Throw"), error->info().msg.str()};
    } else if (nullptr != dynamic_cast<nix::AssertionError *>(error)) {
        return {std::string("AssertionError"),{}};
    } else if (nullptr != dynamic_cast<nix::ParseError *>(error)) {
        return {std::string("ParseError"),{}};
    } else if (nullptr != dynamic_cast<nix::EvalError *>(error)) {
        return {std::string("EvalError"),{}};
    } else {
        return {std::string("Error"),{}};
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
        for (auto e : itr) {
            std::cout << e << " ";
        }
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

void displayValueMap(const FlutschMap &s) {
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

std::optional<Pos> getPos(ref<EvalState> state, const PosIdx &posIdx) {
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

// json LambdaIntrospectionToJson(std::optional<LambdaIntrospection> meta) {
//     if (!meta.has_value()) {
//         json j_null;
//         return j_null;
//     }
//     meta = meta.value();
    
//     meta.value().arg;
//     if(meta.value().formals.has_value()){
//         for(auto formal: meta.value().formals.value()):{
//         }
//     }

//     meta.value().pos;
//     meta.value().type;
//     // auto source = std::optional<std::string>({});
//     // if (auto path = std::get_if<SourcePath>(&pos.value().origin)) {
//     //     source = path->path.c_str();
//     // }
//     return json::object({{"column", pos.value().column},
//                          {"line", pos.value().line},
//                          {"file", source}});
// }

json posToJson(std::optional<Pos> pos) {
    if (!pos.has_value()) {
        json j_null;
        return j_null;
    }
    auto source = std::optional<std::string>({});
    if (auto path = std::get_if<SourcePath>(&pos.value().origin)) {
        source = path->path.c_str();
    }
    return json::object({{"column", pos.value().column},
                         {"line", pos.value().line},
                         {"file", source}});
}

json attrEntryToJson(const AttrEntry& entry) {
    return json::object({
        {"name", entry.name},
        {"pos", posToJson(entry.bindPos)},
        {"is_root", entry.isRoot}
    });
}

json childrenToJson(std::unordered_map<std::string, const AttrEntry>& children) {
    json list = json::array({});
    for (auto child : children) {
        list.push_back(attrEntryToJson(child.second));
    }
    return list;
}

// struct FormalIntrospection {
//     std::string name;
//     std::optional<Pos> pos;
//     bool required;
// };

// struct LambdaIntrospection {
//     std::string type; // lambda, primop, primopApp, functor
//     std::optional<Pos> pos;
//     std::optional<std::string> arg;
//     std::optional<std::vector<FormalIntrospection>> formals;
// };

json formalIntrospectionToJson(FormalIntrospection & formal) {
    return json::object({
        {"name", formal.name},
        {"pos", posToJson(formal.pos)},
        {"required", formal.required}
    });
}

json lambdaIntrospectionToJson(LambdaIntrospection & meta) {
    json j = json::object({
        {"type", meta.type},
        {"pos", posToJson(meta.pos)},
        {"arg", meta.arg},
        {"formals", json::array({})}
    });

    if(meta.formals.has_value()){
        for(auto formal: meta.formals.value()){
            j["formals"].push_back(formalIntrospectionToJson(formal));
        }
    }
    return j;
}

json lambdaMapToJson(std::optional<std::unordered_map<uint,LambdaIntrospection>> & lambdas) {
    if(!lambdas.has_value()){
        json j_null;
        return j_null;
    }

    json j = json::object({});
    for (auto &i : lambdas.value()) {
        j[std::to_string(i.first)]=lambdaIntrospectionToJson(i.second);
    }
    return j;
}


void displayFormals(std::vector<FormalIntrospection> & formals){
    for (auto &i : formals){
        std::cout << "\tFormal: " << i.name << " - ";

        if(i.pos.has_value()){
            std::cout << i.pos.value();
        }else{ 
            std::cout << "noPos";
        }
        
        std::cout << " - " << (i.required ? std::string("(required)") : std::string("(optional)")) << std::endl;
    }
}



void displayUnwrappedLambda(std::unordered_map<uint,LambdaIntrospection> & res){
    std::cout << "displayUnwrappedLambda" << std::endl;
    for (auto &i : res){
        std::cout << "---" << std::endl;
        std::cout << i.second.type << ": " << i.first << " - ";

        if(i.second.pos.has_value()){
            std::cout << i.second.pos.value();
        }else{
            std::cout << "noPos";
        }
        
        std::cout << std::endl;

        if(i.second.arg.has_value()){
            std::cout << "- Arg: " << i.second.arg.value() << std::endl;
        }
        if(i.second.formals.has_value()){
            displayFormals(i.second.formals.value());
        }
        std::cout << "---" << std::endl;
    }
}


std::uintptr_t getPtr(Value *value) {
    std::uintptr_t ptr_val = reinterpret_cast<std::uintptr_t>(value);
    return ptr_val;
}


std::string describe(Value &v){

    if(v.isLambda()){
        return "lambda";
    }
    if(v.isPrimOp()){
        return "primop - " + v.primOp->name;
    }
    if(v.isPrimOpApp()){
        return "primopApp - " + v.primOpAppPrimOp()->name;
    }
    if(v.isList()){
        return "list";
    }
    return std::string("trivial");
}
// Call a function with autoArgs
// - {a, ...}: attrset with all required formals set to true
// - a: single argument value
nix::Value callFnWithAutoAttrs(nix::Value &value, ref<EvalState> state, std::optional<std::vector<FormalIntrospection>> formals){
    if(!value.isLambda()){
        std::cout << "callFnWithAutoAttrs: cannot be called with " << describe(value) << std::endl;
        return value;
    }
    PosIdx currPos = value.lambda.fun->getPos();


    Value res;
    if(!value.lambda.fun->hasFormals()){
        Value dummy;
        // TODO: try different values for unwrapping?
        // TODO: configure via flutsch config, which value to use for unwrapping.
        dummy.mkNull();
        state->callFunction(value, dummy, res, currPos);
    }else{
        // If the function has formals, we need to pass an attrset with at least all required formals
        Value autoAttrs;

        auto attrs = state->buildBindings(formals.value().size());
        for (auto & i : formals.value()){
            if(i.required){
                // Unwrap with each required formal set to a boolean
                // TODO: configure via flutsch config, which value to use for unwrapping.
                attrs.alloc(i.name, noPos).mkBool(i.required);
            }
        }
        autoAttrs.mkAttrs(attrs);
        state->callFunction(value, autoAttrs, res, currPos);
    }
    return res;
}


LambdaIntrospection introspectLambda(Value &value, ref<EvalState> state) {
    if(!value.isLambda()){
        std::cout << "introspectLambda: called with non lambda value " << value.type() << std::endl; 
        return {};
    }
    PosIdx currPos = value.lambda.fun->getPos();
    std::optional<std::string> arg;
    std::optional<std::vector<FormalIntrospection>> formals = {};
    // Collect informations about the current lambda
    // -----------------------------------------
    // 1. argument name if it has one
    if(!value.lambda.fun->hasFormals()) {
        arg = state->symbols[value.lambda.fun->arg];
    }
    // 2. all formals, if it has formals.
    if(value.lambda.fun->hasFormals()) {
        // A formal looks like this:
        //
        // {a, b, ... }@args: body
        //
        // add information about the formals
        // Name of each formal
        // Position of each formal
        // Whether the formal is required or not
        // if it has an ellipsis
        //
        // TODO: can we add the name of @args?
        std::vector<FormalIntrospection> formalsResult;
        std::cout << "Introspecting formals: " << value.lambda.fun->formals->formals.size() << std::endl;

        for (Formal formal : value.lambda.fun->formals->formals ) {
            // Find out if the formal is required
            Value reqValue;
            reqValue.mkBool(formal.def);
            bool required = !reqValue.boolean;

            formalsResult.push_back(FormalIntrospection({state->symbols[formal.name],getPos(state,formal.pos), required}));
        }

        // Add the ellipsis at the end if exists
        if(value.lambda.fun->formals->ellipsis){
            formalsResult.push_back(FormalIntrospection({"...",{}, false}));
        }

        formals.emplace(formalsResult);
    }

    // 3. the source position
    auto pos = getPos(state, currPos);

    return LambdaIntrospection({"lambda",pos,arg,formals});
}

std::unordered_map<uint,LambdaIntrospection> unwrapLambda(nix::Value lambdaOrFunctor, ref<EvalState> state) {
    // The result of the lambda will be stored in vTmp
    Value vTmp = lambdaOrFunctor;

    PosIdx currPos;
    Symbol sFunctor = state->symbols.create("__functor");
    Attr * functor;

    std::unordered_map<uint,LambdaIntrospection> result;
    int counter = 0;
    while(true){
        try {
            std::cout << "C: " << counter << " - Type: " << describe(vTmp) << std::endl;
            // TODO: find a better way for unwrapping limit.
            // We cannot really compare pointers
            if(counter >= 20 ){
                std::cout << "STOP: max recursion depth" << std::endl;
                return result;
            }
    

            switch (vTmp.type()) {
                case nAttrs: {
                    // state->forceAttrs(vTmp, currPos, "unwrapped value is not an attribute set.");
                    Attr * f = vTmp.attrs->get(sFunctor);
                    if(f != nullptr){
                        // The Attribute set is a functor. (self: x: body)
                        std::cout << "functor" << std::endl;
                        currPos = f->pos;

                        Value publicFunctor;
                        // Apply 'self' to get the actual user facing argument(s)
                        state->callFunction(*f->value, vTmp, publicFunctor, currPos);

                        // Collect information about the public interface of the functor
                        state->forceFunction(publicFunctor, currPos, "functor must take at least two arguments. Value is not a function.");
                        LambdaIntrospection info = introspectLambda(publicFunctor,state);
                        info.type = "functor";

                        result.emplace(counter, info);

                        // Unwrap the next lambda.
                        vTmp = callFnWithAutoAttrs(publicFunctor, state, info.formals);

                    }else{
                        // return if we got just an attrset
                        return result;
                    }
                    break;
                }
                case nFunction: {
                    // state->forceFunction(vTmp, currPos, "unwrapped value is not a function.");
                    currPos = noPos;
                    if(vTmp.isLambda()){
                        std::cout << "lambda" << std::endl;

                        currPos = vTmp.lambda.fun->pos;
                        LambdaIntrospection info = introspectLambda(vTmp,state);
                        // Write the lambda info to the result 
                        result.emplace(counter, info);
                        // Unwrap the next lambda.
                        vTmp = callFnWithAutoAttrs(vTmp, state, info.formals);
                    }
                    
                    // Primop and primopApp are not unwrapped.
                    // TODO: e.g. head [ (x: x) ] returns another lambda. 
                    if(vTmp.isPrimOp()){
                        std::cout << "primop" << std::endl;

                    }
                    if(vTmp.isPrimOpApp()){
                        std::cout << "primopApp" << std::endl;
                    }
                    break;
                }
                default:
                    std::cout << "STOP: cannot unwrap: " << vTmp.type() << std::endl;
                    return result;
            }

            //Security mechanism to avoid loops for now
            if(counter >= 10 ){
                return result;
            }

            state->forceValue(vTmp, currPos);
            std::cout << "Unwrapped lambda: " <<  counter << " - continue" << std::endl;
            counter++;

        }catch(nix::Error &e){
            std::cout << "STOP - Cannot unwrap further - " << e.msg() << std::endl;
            return result;
        }
    }
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

                displayAttrs(test, state);
                posIdx = test->attrs->pos;
                type = std::string("attrset");
                Attr *functor = test->attrs->get(state->symbols.create("__functor"));
                if (functor != nullptr) {
                    std::cout << "is functor" << std::endl;
                    type = std::string("attrset/functor");
                    auto meta = unwrapLambda(*test, state);

                    data->second.lambdaIntrospections.emplace(meta);

                    displayUnwrappedLambda(meta);
                }
                // If the value is an attrset, add all its attributes as children
                for (auto &i : test->attrs->lexicographicOrder(state->symbols)) {
                    const std::string &name = state->symbols[i->name];

                    if (data != valueMap.end()) {
                        if (auto p = getPos(state, posIdx)) {
                            const auto attrEntry = AttrEntry(i->value, name, *p);
                            data->second.children.emplace(name,attrEntry);
                        }else{
                            const auto attrEntry = AttrEntry(i->value, name, {});
                            data->second.children.emplace(name,attrEntry);
                        }
                    }
                }
            }

            if (test->isLambda()) {
                // attrPath.back().name;
                // There are 3 different types of functions:
                // - lambda

                // Those two cannot be unwrapped
                // - app
                // - primopApp
                // We will add them as is

                state->forceFunction(*test, noPos, "error");

                displayLambda(test, state);
                posIdx = test->lambda.fun->getPos();
                type = std::string("lambda");
                // If the value is a lambda then we want to unwrap it until we get something else
                if(attrPath.back().name.value_or("") != "__functor"){
                    auto meta = unwrapLambda(*test, state);
                    data->second.lambdaIntrospections.emplace(meta);
                    displayUnwrappedLambda(meta);
                }else{
                    std::cout << "Skipping functor. Those are handled separately" << std::endl;
                }
            }

            if (data != valueMap.end()) {
                if (auto p = getPos(state, posIdx)) {
                    data->second.valuePos.emplace(*p);
                } else {
                    data->second.valuePos.reset();
                }


                std::string t;
                switch (test->type()) {
                    case nInt: t = "int"; break;
                    case nBool: t = "bool"; break;
                    case nString: t = "string"; break;
                    case nPath: t = "path"; break;
                    case nNull: t = "null"; break;
                    case nAttrs: t = type; break;
                    case nList: t = "list"; break;
                    case nFunction: t = "lambda"; break;
                    case nExternal:
                        t = test->external->typeOf();
                        break;
                    case nFloat: t = "float"; break;
                    case nThunk: t = "thunk"; break;
                    default: t = "unknown"; break;
                }
                data->second.valueType = t;
                
            }

        } catch (nix::Error &e) {
            if (data != valueMap.end()) {
                data->second.isError = true;

                auto pos = e.info().errPos;

                std::cout << "inserting error: " << data->first << std::endl;
                bool hasPos = pos && *pos;
                if (hasPos) {
                    data->second.valuePos.emplace(*pos);
                }
                auto errorPair = errorInfo(&e);
                data->second.valueType = errorPair.first;
                data->second.errorDescription = errorPair.second;
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

        for (auto &i : testAttrs->attrs->lexicographicOrder(state->symbols)) {
            // might not have a name, if its the root attrset;
            // value: testAttrs

            const std::string &name = state->symbols[i->name];
            Pos currPos = state->positions[i->pos];
            std::cout << "looking into symbol: " << name << std::endl;

            // Copy and append current attribute
            std::vector<AttrEntry> curAttrPath = attrPath;

            const auto attrEntry = AttrEntry(i->value, name, currPos);
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

            // Skip value if exists and is already analyzed
            if (entry != valueMap.end() && entry->second.isIntrospected) {
                std::cout << "SKIPPING: " << attrEntry << " already analyzed - "
                          << entry->second << std::endl;
                // Important!: Add the attrName and link it to the already analyzed value
                valueMap.emplace(attrEntry, entry->second);
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
                    
                }
            }
        }
    };

    auto posIdx = vRoot->attrs->pos;

    auto rootKey = AttrEntry(vRoot, "<root>", state->positions[posIdx]);
    rootKey.isRoot = true;

    auto initPath = std::vector<AttrEntry>({rootKey});
    valueMap.emplace(rootKey, ValueIntrospection({"<root>"}));
    introspectValue(initPath, vRoot);
    recurseValues(initPath, vRoot);

    std::cout << "\n---\nValueMap: \n";
    displayValueMap(valueMap);
    std::cout << "---\n";
    // create an empty list []

    // Some pretty printing.
    json out = json::array({});
    for (auto pair : valueMap) {
        out.push_back({
            // Infos from ValueIntrospection
            {"value", {
                {"path", pair.second.path},
                {"pos", posToJson(pair.second.valuePos)},
                {"children", childrenToJson(pair.second.children)},
                {"type", pair.second.valueType},
                {"error", pair.second.isError},
                {"error_description", pair.second.errorDescription},
                {"lambda", lambdaMapToJson(pair.second.lambdaIntrospections)}
            }},
            // Infos from AttrEntry
            {"binding", {
                {"pos", posToJson(pair.first.bindPos)},
                {"name", pair.first.name},
                {"is_root", pair.first.isRoot}
            }},
            
        });
    }

    // std::string pretty = out.dump(4);
    // std::cout << pretty << std::endl;

    std::string filename = "values.json"; // Name of the file to create/write

    // Open the file in write mode. This will create the file if it doesn't exist,
    // or overwrite it if it does.
    std::ofstream file(filename);

    if (!file.is_open()) {
        std::cerr << "Failed to open or create file." << filename << std::endl;
    }

    // Write the string to the file
    file << out.dump(4);
    file.close();
    std::cout << "Success: Value introspection written to: " << filename << std::endl;

}

} // namespace flutsch