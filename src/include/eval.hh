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
    PosIdx posIdx;

    bool isRoot = false;
    // Comparison logic needed for use in std::set.
    bool operator<(const AttrEntry &other) const;
    bool operator==(const AttrEntry &other) const;
    friend std::ostream &operator<<(std::ostream &os, const AttrEntry &obj);
};

struct AttrEntryHash {
    size_t operator()(const AttrEntry &e) const noexcept {
        std::uintptr_t ptr_val = reinterpret_cast<std::uintptr_t>(e.value);
        return std::hash<std::uintptr_t>()(ptr_val);
    }
};

struct ValueIntrospection {
    std::vector<std::string> path;
    // can be "lambda" | "attrset" | null
    std::optional<std::string> valueType;
    std::optional<Pos> valuePos;

    std::unordered_map<std::string, const AttrEntry> children = {};
    // std::optional<Pos*> symbolPos;
    bool isIntrospected = false;
    // ValueIntrospection(ValueIntrospection &&o) : path(std::move(o.path)),
    // valueType(std::move(valueType)), valuePos(std::move(valuePos)) {}

    friend std::ostream &operator<<(std::ostream &os,
                                    const ValueIntrospection &obj);
};

}; // namespace flutsch


#endif // EVAL_H
