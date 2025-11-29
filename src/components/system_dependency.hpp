#ifndef LITHIUM_ADDON_DEPENDENCY_MANAGER_HPP
#define LITHIUM_ADDON_DEPENDENCY_MANAGER_HPP

#include "system/dependency_exception.hpp"
#include "system/dependency_manager.hpp"
#include "system/dependency_types.hpp"

namespace lithium {

using DependencyException = system::DependencyException;
using DependencyErrorCode = system::DependencyErrorCode;
using DependencyError = system::DependencyError;
template <typename T>
using DependencyResult = system::DependencyResult<T>;
using VersionInfo = system::VersionInfo;
using DependencyInfo = system::DependencyInfo;
using PackageManagerInfo = system::PackageManagerInfo;
using DependencyManager = system::DependencyManager;

}  // namespace lithium

#endif  // LITHIUM_ADDON_DEPENDENCY_MANAGER_HPP
