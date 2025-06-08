#include "dependency_types.hpp"

namespace lithium::system {

std::ostream& operator<<(std::ostream& os, const VersionInfo& vi) {
    os << vi.major << "." << vi.minor << "." << vi.patch;
    if (!vi.prerelease.empty()) {
        os << "-" << vi.prerelease;
    }
    return os;
}

}  // namespace lithium::system
