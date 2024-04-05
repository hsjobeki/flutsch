#include "nixexpr.hh"
#include "position.hh"
#include <iostream>
#include <optional>
#include <unordered_map>
#include <vector>

#ifndef EVAL_H
#define EVAL_H

using namespace nix;

namespace flutsch {

struct AttrEntry {
    // A pointer to the value.
    Value *value;
    std::optional<std::string> name;
    std::optional<Pos> bindPos;

    bool isRoot = false;
    // Comparison logic needed for use in std::set.
    bool operator<(const AttrEntry &other) const;
    bool operator==(const AttrEntry &other) const;
    friend std::ostream &operator<<(std::ostream &os, const AttrEntry &obj);
};


struct SetHash {
    size_t operator()(const std::set<std::string>& s) const {
        std::hash<std::string> stringHash;
        size_t result = 0;
        for (const auto& str : s) {
            // Combine the hash of the current string with the results so far
            result ^= stringHash(str) + 0x9e3779b9 + (result << 6) + (result >> 2);
        }
        return result;
    }
};

struct AttrEntryHash {
    size_t operator()(const AttrEntry &e) const noexcept {
        std::uintptr_t ptr_val = reinterpret_cast<std::uintptr_t>(e.value);
        
        std::hash<std::uintptr_t> ptr_hasher;
        std::hash<std::string> string_hasher;

        // Hash individual components
        size_t hash1 = ptr_hasher(ptr_val);

        size_t hash2 = 0; // Use a fixed value in case there is no name
        if (e.name) {
            hash2 = string_hasher(e.name.value());
        }

        // Combine the two hash values. Shift one hash and XOR with the other to mix the bits.
        return hash1 ^ (hash2 << 16 | hash2 >> (32 - 16));
    }
};

struct FormalIntrospection {
    std::string name;
    std::optional<Pos> pos;
    bool required;
};

struct LambdaIntrospection {
    std::string type; // lambda, primop, primopApp, functor
    std::optional<Pos> pos;
    std::optional<std::string> arg;
    std::optional<std::vector<FormalIntrospection>> formals;
};

struct ValueIntrospection {

    std::vector<std::string> path;
    // can be "lambda" | "attrset" | ...errorType
    std::optional<std::string> valueType;
    std::optional<Pos> valuePos;

    std::unordered_map<std::string, const AttrEntry> children = {};
    // std::optional<Pos*> symbolPos;
    bool isIntrospected = false;

    bool isError = false;
    std::optional<std::string> errorDescription;
    
    std::optional<std::unordered_map<uint,LambdaIntrospection>> lambdaIntrospections;
    // ValueIntrospection(ValueIntrospection &&o) : path(std::move(o.path)),
    // valueType(std::move(valueType)), valuePos(std::move(valuePos)) {}

    friend std::ostream &operator<<(std::ostream &os,
                                    const ValueIntrospection &obj);
};

}; // namespace flutsch


#endif // EVAL_H
