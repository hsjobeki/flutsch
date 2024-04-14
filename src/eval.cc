#include "eval.hh"
#include <iostream>


namespace flutsch {

std::string attrPathJoin(std::vector<std::string> path);

// impl for '<' operator
bool AttrEntry::operator<(const AttrEntry &other) const {
    std::uintptr_t ptr_self = reinterpret_cast<std::uintptr_t>(value);
    std::uintptr_t ptr_other = reinterpret_cast<std::uintptr_t>(other.value);
    return ptr_self < ptr_other;
}
bool AttrEntry::operator==(const AttrEntry &other) const {
    std::uintptr_t ptr_self = reinterpret_cast<std::uintptr_t>(value);
    std::uintptr_t ptr_other = reinterpret_cast<std::uintptr_t>(other.value);
    return ptr_self == ptr_other;
}

std::ostream &operator<<(std::ostream &os, const AttrEntry &e) {
    // std::uintptr_t ptr_self = reinterpret_cast<std::uintptr_t>(e.value);
    os << "(";
    if (e.isRoot) {
        os << "<root>";
    } else {
        os << e.name.value_or("<unnamed>");
    }
    std::stringstream ss;

    // Format the size_t value as a hexadecimal string
    ss << std::hex << AttrEntryHash()(e) ;
    std::string hexStr = ss.str(); // Extract the formatted string

    

    os << " @ 0x" << hexStr << ")" << " - ";
    
    if(e.bindPos.has_value() ) {
        os << e.bindPos.value();
    } else {
        os << "noPos";
    }

    return os;
}

std::ostream &operator<<(std::ostream &os, const ValueIntrospection &e) {
    os << attrPathJoin(e.path) << " - (" << e.valueType.value_or("unknown")
       << " @ ";
    if (e.valuePos.has_value()) {
        const Pos *const pos = &e.valuePos.value();
        os << *pos;
    } else {
        os << "noPos";
    }
    os << ")";
    if (!e.children.empty()) {
        os << " children: \n";
        for (auto r : e.children) {
            os << "\t - " << r.first << " : " << r.second << "\n";
        }
    }
    if(e.lambdaIntrospections.has_value()) {
        os << "\tnFunction introspections: \n";
        for(auto r : e.lambdaIntrospections.value()) {
            os << "\t - " << r.first+1 << ". " << r.second.type << ": ";
            if(r.second.pos.has_value()) {
                os << r.second.pos.value();
            }else {
                os << "noPos";
            }
            os << "\n";
        }
    }
    return os;
}

}

namespace std {
template <> struct hash<flutsch::AttrEntry> {
    size_t operator()(const flutsch::AttrEntry &e) const noexcept {
        std::uintptr_t ptr_val = reinterpret_cast<std::uintptr_t>(e.value);
        return std::hash<std::uintptr_t>()(ptr_val);
    }
};
} // namespace std

// namespace flutsch {

// }
