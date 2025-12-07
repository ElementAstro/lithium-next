/*
 * astrometry_solver_plugin.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#include "astrometry_solver_plugin.hpp"

#include "client/solver/service/solver_factory.hpp"
#include "client/solver/service/solver_type_registry.hpp"

#include "atom/log/spdlog_logger.hpp"
#include "atom/system/command.hpp"

#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <regex>

namespace lithium::client::astrometry {

// ============================================================================
// Constructor / Destructor
// ============================================================================

AstrometrySolverPlugin::AstrometrySolverPlugin()
    : SolverPluginBase(PLUGIN_NAME, PLUGIN_VERSION) {
    LOG_DEBUG("AstrometrySolverPlugin constructed");
}

AstrometrySolverPlugin::~AstrometrySolverPlugin() {
    if (getState() == solver::SolverPluginState::Running) {
        shutdown();
    }
}

// ============================================================================
// IPlugin Interface
// ============================================================================

auto AstrometrySolverPlugin::initialize(const nlohmann::json& config) -> bool {
    std::unique_lock lock(mutex_);

    if (getState() != solver::SolverPluginState::Unloaded &&
        getState() != solver::SolverPluginState::Error) {
        LOG_WARN("AstrometrySolverPlugin already initialized");
        return true;
    }

    setState(solver::SolverPluginState::Initializing);

    // Store configuration
    config_ = config;

    // Initialize index directories
    indexDirectories_ = getDefaultIndexDirectories();

    // Add custom index directories from config
    if (config.contains("indexDirectories")) {
        for (const auto& dir : config["indexDirectories"]) {
            auto path = std::filesystem::path(dir.get<std::string>());
            if (std::filesystem::exists(path)) {
                indexDirectories_.push_back(path);
            }
        }
    }

    // Load remote API configuration
    if (config.contains("apiKey")) {
        apiKey_ = config["apiKey"].get<std::string>();
    }
    if (config.contains("apiUrl")) {
        apiUrl_ = config["apiUrl"].get<std::string>();
    }

    // Set solve mode
    if (config.contains("solveMode")) {
        auto mode = config["solveMode"].get<std::string>();
        if (mode == "local") {
            solveMode_ = SolveMode::Local;
        } else if (mode == "remote") {
            solveMode_ = SolveMode::Remote;
        } else {
            solveMode_ = SolveMode::Auto;
        }
    }

    // Try to find solve-field binary
    if (config.contains("binaryPath")) {
        auto customPath =
            std::filesystem::path(config["binaryPath"].get<std::string>());
        if (validateBinary(customPath)) {
            binaryPath_ = customPath;
            binaryVersion_ = extractVersion(customPath);
        }
    } else {
        auto found = scanForBinary();
        if (found) {
            binaryPath_ = *found;
            binaryVersion_ = extractVersion(*found);
        }
    }

    // Initialize remote client if API key is available
    if (!apiKey_.empty()) {
        try {
            ::astrometry::ClientConfig clientConfig;
            clientConfig.api_url = apiUrl_;
            remoteClient_ = std::make_unique<::astrometry::AstrometryClient>(
                apiKey_, clientConfig);
        } catch (const std::exception& e) {
            LOG_WARN("Failed to initialize remote client: {}", e.what());
        }
    }

    setState(solver::SolverPluginState::Running);

    LOG_INFO(
        "AstrometrySolverPlugin initialized (local: {}, remote: {}, mode: {})",
        binaryPath_.has_value() ? "available" : "not found",
        remoteClient_ ? "configured" : "not configured",
        solveMode_ == SolveMode::Local    ? "local"
        : solveMode_ == SolveMode::Remote ? "remote"
                                          : "auto");

    return true;
}

void AstrometrySolverPlugin::shutdown() {
    std::unique_lock lock(mutex_);

    setState(solver::SolverPluginState::ShuttingDown);

    // Clear remote client
    remoteClient_.reset();

    // Clear paths
    binaryPath_.reset();
    binaryVersion_.clear();
    indexDirectories_.clear();

    setState(solver::SolverPluginState::Unloaded);

    LOG_INFO("AstrometrySolverPlugin shut down");
}

// ============================================================================
// ISolverPlugin Interface
// ============================================================================

auto AstrometrySolverPlugin::getSolverTypes() const
    -> std::vector<solver::SolverTypeInfo> {
    std::vector<solver::SolverTypeInfo> types;

    // Always add local type (may be disabled if binary not found)
    types.push_back(buildLocalTypeInfo());

    // Add remote type if API key is configured
    if (!apiKey_.empty() || config_.contains("apiKey")) {
        types.push_back(buildRemoteTypeInfo());
    }

    return types;
}

auto AstrometrySolverPlugin::registerSolverTypes(
    solver::SolverTypeRegistry& registry) -> size_t {
    size_t count = 0;

    for (const auto& type : getSolverTypes()) {
        if (registry.registerType(type)) {
            ++count;
            LOG_DEBUG("Registered solver type: {}", type.typeName);
        }
    }

    return count;
}

auto AstrometrySolverPlugin::unregisterSolverTypes(
    solver::SolverTypeRegistry& registry) -> size_t {
    size_t count = 0;

    if (registry.unregisterType(SOLVER_TYPE_LOCAL)) {
        ++count;
    }
    if (registry.unregisterType(SOLVER_TYPE_REMOTE)) {
        ++count;
    }

    return count;
}

void AstrometrySolverPlugin::registerSolverCreators(
    solver::SolverFactory& factory) {
    // Register local solver creator
    factory.registerCreator(
        SOLVER_TYPE_LOCAL,
        [this](const std::string& id,
               const nlohmann::json& config) -> std::shared_ptr<SolverClient> {
            return createLocalSolver(id, config);
        });

    // Register remote solver creator
    factory.registerCreator(
        SOLVER_TYPE_REMOTE,
        [this](const std::string& id,
               const nlohmann::json& config) -> std::shared_ptr<SolverClient> {
            return createRemoteSolver(id, config);
        });

    LOG_DEBUG("Registered Astrometry solver creators");
}

void AstrometrySolverPlugin::unregisterSolverCreators(
    solver::SolverFactory& factory) {
    factory.unregisterCreator(SOLVER_TYPE_LOCAL);
    factory.unregisterCreator(SOLVER_TYPE_REMOTE);
}

auto AstrometrySolverPlugin::createSolver(const std::string& solverId,
                                           const nlohmann::json& config)
    -> std::shared_ptr<SolverClient> {
    // Determine which solver to create based on mode
    auto mode = solveMode_;
    if (config.contains("mode")) {
        auto modeStr = config["mode"].get<std::string>();
        if (modeStr == "local") {
            mode = SolveMode::Local;
        } else if (modeStr == "remote") {
            mode = SolveMode::Remote;
        }
    }

    switch (mode) {
        case SolveMode::Local:
            return createLocalSolver(solverId, config);

        case SolveMode::Remote:
            return createRemoteSolver(solverId, config);

        case SolveMode::Auto:
        default:
            // Prefer local if available
            if (binaryPath_) {
                return createLocalSolver(solverId, config);
            } else if (remoteClient_) {
                return createRemoteSolver(solverId, config);
            }
            LOG_ERROR("No Astrometry solver available");
            return nullptr;
    }
}

auto AstrometrySolverPlugin::findBinary()
    -> std::optional<std::filesystem::path> {
    if (binaryPath_) {
        return binaryPath_;
    }
    return scanForBinary();
}

auto AstrometrySolverPlugin::validateBinary(const std::filesystem::path& path)
    -> bool {
    if (!std::filesystem::exists(path)) {
        return false;
    }

    // Try to execute with --help to verify it's solve-field
    try {
        std::string cmd = path.string() + " --help";
        auto [output, exitCode] = atom::system::executeCommand(cmd, false);

        // Check for typical solve-field help output
        return output.find("solve-field") != std::string::npos ||
               output.find("astrometry") != std::string::npos;
    } catch (...) {
        return false;
    }
}

auto AstrometrySolverPlugin::getBinaryVersion() const -> std::string {
    return binaryVersion_;
}

auto AstrometrySolverPlugin::setBinaryPath(const std::filesystem::path& path)
    -> bool {
    if (!validateBinary(path)) {
        return false;
    }

    std::unique_lock lock(mutex_);
    binaryPath_ = path;
    binaryVersion_ = extractVersion(path);

    LOG_INFO("Set Astrometry binary path: {} (version: {})", path.string(),
             binaryVersion_);
    return true;
}

auto AstrometrySolverPlugin::getBinaryPath() const
    -> std::optional<std::filesystem::path> {
    return binaryPath_;
}

auto AstrometrySolverPlugin::getDefaultOptions() const -> nlohmann::json {
    return {{"noPlots", true},
            {"overwrite", true},
            {"cpuLimit", 120},
            {"downsample", 2},
            {"scaleUnits", "arcsecperpix"},
            {"radius", 10.0}};
}

auto AstrometrySolverPlugin::validateOptions(const nlohmann::json& options)
    -> solver::SolverResult<bool> {
    // Validate timeout/cpuLimit
    if (options.contains("cpuLimit")) {
        auto limit = options["cpuLimit"].get<int>();
        if (limit < 0 || limit > 3600) {
            return solver::SolverResult<bool>::failure(
                "cpuLimit must be between 0 and 3600");
        }
    }

    // Validate scale range
    if (options.contains("scaleLow") && options.contains("scaleHigh")) {
        auto low = options["scaleLow"].get<double>();
        auto high = options["scaleHigh"].get<double>();
        if (low >= high) {
            return solver::SolverResult<bool>::failure(
                "scaleLow must be less than scaleHigh");
        }
        if (low < 0.01 || high > 1000) {
            return solver::SolverResult<bool>::failure(
                "Scale values must be between 0.01 and 1000 arcsec/pixel");
        }
    }

    // Validate radius
    if (options.contains("radius")) {
        auto radius = options["radius"].get<double>();
        if (radius < 0.0 || radius > 180.0) {
            return solver::SolverResult<bool>::failure(
                "radius must be between 0 and 180 degrees");
        }
    }

    // Validate downsample
    if (options.contains("downsample")) {
        auto ds = options["downsample"].get<int>();
        if (ds < 1 || ds > 16) {
            return solver::SolverResult<bool>::failure(
                "downsample must be between 1 and 16");
        }
    }

    return solver::SolverResult<bool>::success(true);
}

// ============================================================================
// Astrometry-Specific Methods
// ============================================================================

auto AstrometrySolverPlugin::getIndexDirectories() const
    -> std::vector<std::filesystem::path> {
    std::shared_lock lock(mutex_);
    return indexDirectories_;
}

void AstrometrySolverPlugin::addIndexDirectory(
    const std::filesystem::path& directory) {
    if (!std::filesystem::exists(directory)) {
        LOG_WARN("Index directory does not exist: {}", directory.string());
        return;
    }

    std::unique_lock lock(mutex_);
    if (std::find(indexDirectories_.begin(), indexDirectories_.end(),
                  directory) == indexDirectories_.end()) {
        indexDirectories_.push_back(directory);
        LOG_INFO("Added index directory: {}", directory.string());
    }
}

auto AstrometrySolverPlugin::scanIndexFiles() const
    -> std::vector<std::filesystem::path> {
    std::vector<std::filesystem::path> indexFiles;

    std::shared_lock lock(mutex_);
    for (const auto& dir : indexDirectories_) {
        if (!std::filesystem::exists(dir)) {
            continue;
        }

        try {
            for (const auto& entry :
                 std::filesystem::directory_iterator(dir)) {
                if (entry.is_regular_file()) {
                    auto filename = entry.path().filename().string();
                    // Match index-*.fits pattern
                    if (filename.find("index-") == 0 &&
                        (filename.find(".fits") != std::string::npos ||
                         filename.find(".FITS") != std::string::npos)) {
                        indexFiles.push_back(entry.path());
                    }
                }
            }
        } catch (const std::exception& e) {
            LOG_WARN("Error scanning index directory {}: {}", dir.string(),
                     e.what());
        }
    }

    return indexFiles;
}

auto AstrometrySolverPlugin::getIndexCoverage() const -> nlohmann::json {
    auto indexFiles = scanIndexFiles();

    nlohmann::json coverage;
    coverage["totalFiles"] = indexFiles.size();
    coverage["directories"] = nlohmann::json::array();

    std::shared_lock lock(mutex_);
    for (const auto& dir : indexDirectories_) {
        nlohmann::json dirInfo;
        dirInfo["path"] = dir.string();
        dirInfo["exists"] = std::filesystem::exists(dir);

        int fileCount = 0;
        for (const auto& file : indexFiles) {
            if (file.parent_path() == dir) {
                ++fileCount;
            }
        }
        dirInfo["fileCount"] = fileCount;
        coverage["directories"].push_back(dirInfo);
    }

    // Parse index scales from filenames
    nlohmann::json scales = nlohmann::json::array();
    std::regex scaleRegex(R"(index-(\d{4})([a-z]?)\.fits)",
                          std::regex::icase);

    for (const auto& file : indexFiles) {
        std::smatch match;
        std::string filename = file.filename().string();
        if (std::regex_match(filename, match, scaleRegex)) {
            int scaleNum = std::stoi(match[1].str());
            scales.push_back(scaleNum);
        }
    }

    // Remove duplicates and sort
    std::sort(scales.begin(), scales.end());
    scales.erase(std::unique(scales.begin(), scales.end()), scales.end());
    coverage["availableScales"] = scales;

    return coverage;
}

auto AstrometrySolverPlugin::isRemoteAvailable() const -> bool {
    if (!remoteClient_) {
        return false;
    }

    try {
        // Try to login to verify API key
        return remoteClient_->login();
    } catch (...) {
        return false;
    }
}

void AstrometrySolverPlugin::setRemoteConfig(const std::string& apiKey,
                                              const std::string& apiUrl) {
    std::unique_lock lock(mutex_);

    apiKey_ = apiKey;
    if (!apiUrl.empty()) {
        apiUrl_ = apiUrl;
    }

    // Recreate remote client
    if (!apiKey_.empty()) {
        try {
            ::astrometry::ClientConfig clientConfig;
            clientConfig.api_url = apiUrl_;
            remoteClient_ = std::make_unique<::astrometry::AstrometryClient>(
                apiKey_, clientConfig);
            LOG_INFO("Configured remote Astrometry client: {}", apiUrl_);
        } catch (const std::exception& e) {
            LOG_ERROR("Failed to configure remote client: {}", e.what());
            remoteClient_.reset();
        }
    }
}

auto AstrometrySolverPlugin::getSolveMode() const -> SolveMode {
    return solveMode_;
}

void AstrometrySolverPlugin::setSolveMode(SolveMode mode) {
    solveMode_ = mode;
}

auto AstrometrySolverPlugin::createLocalSolver(const std::string& solverId,
                                                const nlohmann::json& config)
    -> std::shared_ptr<SolverClient> {
    if (!binaryPath_) {
        LOG_ERROR("Cannot create local solver: solve-field not found");
        return nullptr;
    }

    auto solver = std::make_shared<AstrometryClient>(solverId);

    // Apply configuration
    if (config.contains("options")) {
        Options opts;
        const auto& optsJson = config["options"];

        if (optsJson.contains("scaleLow")) {
            opts.scaleLow = optsJson["scaleLow"].get<double>();
        }
        if (optsJson.contains("scaleHigh")) {
            opts.scaleHigh = optsJson["scaleHigh"].get<double>();
        }
        if (optsJson.contains("scaleUnits")) {
            auto units = optsJson["scaleUnits"].get<std::string>();
            if (units == "degwidth") {
                opts.scaleUnits = ScaleUnits::DegWidth;
            } else if (units == "arcminwidth") {
                opts.scaleUnits = ScaleUnits::ArcminWidth;
            } else if (units == "focalmm") {
                opts.scaleUnits = ScaleUnits::FocalMm;
            } else {
                opts.scaleUnits = ScaleUnits::ArcsecPerPix;
            }
        }
        if (optsJson.contains("ra")) {
            opts.ra = optsJson["ra"].get<double>();
        }
        if (optsJson.contains("dec")) {
            opts.dec = optsJson["dec"].get<double>();
        }
        if (optsJson.contains("radius")) {
            opts.radius = optsJson["radius"].get<double>();
        }
        if (optsJson.contains("cpuLimit")) {
            opts.cpuLimit = optsJson["cpuLimit"].get<int>();
        }
        if (optsJson.contains("downsample")) {
            opts.downsample = optsJson["downsample"].get<int>();
        }
        if (optsJson.contains("depth")) {
            opts.depth = optsJson["depth"].get<int>();
        }
        if (optsJson.contains("noPlots")) {
            opts.noPlots = optsJson["noPlots"].get<bool>();
        }
        if (optsJson.contains("overwrite")) {
            opts.overwrite = optsJson["overwrite"].get<bool>();
        }

        solver->setAstrometryOptions(opts);
    }

    // Initialize solver
    if (!solver->initialize()) {
        LOG_ERROR("Failed to initialize local Astrometry solver");
        return nullptr;
    }

    ++localSolveCount_;
    return solver;
}

auto AstrometrySolverPlugin::createRemoteSolver(const std::string& solverId,
                                                 const nlohmann::json& config)
    -> std::shared_ptr<SolverClient> {
    // Note: Remote solver would need a wrapper class that adapts
    // ::astrometry::AstrometryClient to the SolverClient interface.
    // For now, we'll create a local solver as fallback.

    LOG_WARN(
        "Remote solver creation not fully implemented, falling back to local");

    if (binaryPath_) {
        return createLocalSolver(solverId, config);
    }

    LOG_ERROR("Cannot create remote solver: implementation pending");
    return nullptr;
}

// ============================================================================
// Private Methods
// ============================================================================

auto AstrometrySolverPlugin::buildLocalTypeInfo() const
    -> solver::SolverTypeInfo {
    solver::SolverTypeInfo info;
    info.typeName = SOLVER_TYPE_LOCAL;
    info.displayName = "Astrometry.net (Local)";
    info.pluginName = PLUGIN_NAME;
    info.version = binaryVersion_.empty() ? "unknown" : binaryVersion_;
    info.description = "Local solve-field plate solver from Astrometry.net";
    info.priority = 85;  // High priority, but below ASTAP
    info.enabled = binaryPath_.has_value();

    // Capabilities
    info.capabilities.supportedFormats = {"FITS", "JPEG", "PNG", "TIFF"};
    info.capabilities.supportsBlindSolve = true;
    info.capabilities.supportsHintedSolve = true;
    info.capabilities.supportsAbort = true;
    info.capabilities.supportsAsync = true;
    info.capabilities.maxConcurrentSolves = 2;

    info.optionSchema = buildLocalOptionsSchema();

    return info;
}

auto AstrometrySolverPlugin::buildRemoteTypeInfo() const
    -> solver::SolverTypeInfo {
    solver::SolverTypeInfo info;
    info.typeName = SOLVER_TYPE_REMOTE;
    info.displayName = "Astrometry.net (Remote)";
    info.pluginName = PLUGIN_NAME;
    info.version = "API";
    info.description = "Remote plate solving via nova.astrometry.net";
    info.priority = 50;  // Lower priority (network dependent)
    info.enabled = !apiKey_.empty();

    // Capabilities
    info.capabilities.supportedFormats = {"FITS", "JPEG", "PNG", "GIF"};
    info.capabilities.supportsBlindSolve = true;
    info.capabilities.supportsHintedSolve = true;
    info.capabilities.supportsAbort = false;  // Cannot cancel remote jobs
    info.capabilities.supportsAsync = true;
    info.capabilities.maxConcurrentSolves = 5;

    info.optionSchema = buildRemoteOptionsSchema();

    return info;
}

auto AstrometrySolverPlugin::buildLocalOptionsSchema() const -> nlohmann::json {
    return {
        {"type", "object"},
        {"properties",
         {{"scaleLow",
           {{"type", "number"},
            {"description", "Lower bound of image scale (arcsec/pixel)"},
            {"minimum", 0.01},
            {"maximum", 1000}}},
          {"scaleHigh",
           {{"type", "number"},
            {"description", "Upper bound of image scale (arcsec/pixel)"},
            {"minimum", 0.01},
            {"maximum", 1000}}},
          {"scaleUnits",
           {{"type", "string"},
            {"description", "Scale units"},
            {"enum",
             {"arcsecperpix", "degwidth", "arcminwidth", "focalmm"}}}},
          {"ra",
           {{"type", "number"},
            {"description", "Right ascension hint (degrees)"},
            {"minimum", 0},
            {"maximum", 360}}},
          {"dec",
           {{"type", "number"},
            {"description", "Declination hint (degrees)"},
            {"minimum", -90},
            {"maximum", 90}}},
          {"radius",
           {{"type", "number"},
            {"description", "Search radius (degrees)"},
            {"minimum", 0.1},
            {"maximum", 180},
            {"default", 10}}},
          {"cpuLimit",
           {{"type", "integer"},
            {"description", "CPU time limit (seconds)"},
            {"minimum", 10},
            {"maximum", 3600},
            {"default", 120}}},
          {"downsample",
           {{"type", "integer"},
            {"description", "Downsample factor"},
            {"minimum", 1},
            {"maximum", 16},
            {"default", 2}}},
          {"depth",
           {{"type", "integer"},
            {"description", "Object detection depth"},
            {"minimum", 1},
            {"maximum", 200}}},
          {"noPlots",
           {{"type", "boolean"},
            {"description", "Disable plot generation"},
            {"default", true}}},
          {"overwrite",
           {{"type", "boolean"},
            {"description", "Overwrite existing output files"},
            {"default", true}}},
          {"noBackgroundSubtraction",
           {{"type", "boolean"},
            {"description", "Disable background subtraction"},
            {"default", false}}},
          {"invert",
           {{"type", "boolean"},
            {"description", "Invert image"},
            {"default", false}}}}}};
}

auto AstrometrySolverPlugin::buildRemoteOptionsSchema() const
    -> nlohmann::json {
    return {{"type", "object"},
            {"properties",
             {{"scaleLower",
               {{"type", "number"},
                {"description", "Lower bound of image scale"}}},
              {"scaleUpper",
               {{"type", "number"},
                {"description", "Upper bound of image scale"}}},
              {"scaleUnits",
               {{"type", "string"},
                {"description", "Scale units"},
                {"enum", {"degwidth", "arcminwidth", "arcsecperpix"}}}},
              {"centerRa",
               {{"type", "number"},
                {"description", "Center RA hint (degrees)"}}},
              {"centerDec",
               {{"type", "number"},
                {"description", "Center Dec hint (degrees)"}}},
              {"radius",
               {{"type", "number"},
                {"description", "Search radius (degrees)"}}},
              {"downsampleFactor",
               {{"type", "number"}, {"description", "Downsample factor"}}},
              {"publiclyVisible",
               {{"type", "boolean"},
                {"description", "Make submission public"},
                {"default", false}}}}}};
}

auto AstrometrySolverPlugin::scanForBinary() const
    -> std::optional<std::filesystem::path> {
    std::vector<std::filesystem::path> searchPaths;

#ifdef _WIN32
    // Windows locations
    searchPaths = {
        "C:/cygwin64/lib/astrometry/bin/solve-field.exe",
        "C:/astrometry/bin/solve-field.exe",
        std::filesystem::path(std::getenv("PROGRAMFILES") ? std::getenv("PROGRAMFILES") : "") /
            "Astrometry" / "bin" / "solve-field.exe",
        std::filesystem::path(std::getenv("LOCALAPPDATA") ? std::getenv("LOCALAPPDATA") : "") /
            "Astrometry" / "bin" / "solve-field.exe"
    };
#else
    // Unix locations
    searchPaths = {"/usr/bin/solve-field",
                   "/usr/local/bin/solve-field",
                   "/usr/local/astrometry/bin/solve-field",
                   "/opt/astrometry/bin/solve-field"};

    // Check PATH
    if (const char* pathEnv = std::getenv("PATH")) {
        std::string path = pathEnv;
        std::istringstream iss(path);
        std::string dir;
        while (std::getline(iss, dir, ':')) {
            searchPaths.emplace_back(std::filesystem::path(dir) /
                                     "solve-field");
        }
    }
#endif

    for (const auto& path : searchPaths) {
        if (std::filesystem::exists(path)) {
            LOG_DEBUG("Found solve-field at: {}", path.string());
            return path;
        }
    }

    // Try using 'which' command on Unix
#ifndef _WIN32
    try {
        auto [output, exitCode] =
            atom::system::executeCommand("which solve-field", false);
        if (exitCode == 0 && !output.empty()) {
            // Trim whitespace
            output.erase(output.find_last_not_of(" \n\r\t") + 1);
            if (std::filesystem::exists(output)) {
                return std::filesystem::path(output);
            }
        }
    } catch (...) {
    }
#endif

    LOG_WARN("solve-field binary not found");
    return std::nullopt;
}

auto AstrometrySolverPlugin::extractVersion(
    const std::filesystem::path& binary) const -> std::string {
    try {
        std::string cmd = binary.string() + " --version 2>&1";
        auto [output, exitCode] = atom::system::executeCommand(cmd, false);

        // Parse version from output like "solve-field 0.94"
        std::regex versionRegex(R"(solve-field\s+(\d+\.\d+(?:\.\d+)?))");
        std::smatch match;
        if (std::regex_search(output, match, versionRegex)) {
            return match[1].str();
        }

        // Alternative pattern
        std::regex altRegex(R"((\d+\.\d+(?:\.\d+)?))");
        if (std::regex_search(output, match, altRegex)) {
            return match[1].str();
        }
    } catch (const std::exception& e) {
        LOG_WARN("Failed to extract solve-field version: {}", e.what());
    }

    return "unknown";
}

auto AstrometrySolverPlugin::getDefaultIndexDirectories() const
    -> std::vector<std::filesystem::path> {
    std::vector<std::filesystem::path> directories;

#ifdef _WIN32
    directories = {
        "C:/astrometry/data",
        std::filesystem::path(std::getenv("PROGRAMDATA") ? std::getenv("PROGRAMDATA") : "") /
            "astrometry" / "data",
        std::filesystem::path(std::getenv("LOCALAPPDATA") ? std::getenv("LOCALAPPDATA") : "") /
            "astrometry" / "data"
    };
#else
    directories = {"/usr/share/astrometry", "/usr/local/share/astrometry",
                   "/usr/local/astrometry/data", "/opt/astrometry/data"};

    // User directory
    if (const char* home = std::getenv("HOME")) {
        directories.emplace_back(std::filesystem::path(home) / ".local" /
                                 "share" / "astrometry");
    }
#endif

    // Filter to existing directories
    std::vector<std::filesystem::path> existing;
    for (const auto& dir : directories) {
        if (std::filesystem::exists(dir)) {
            existing.push_back(dir);
        }
    }

    return existing;
}

// ============================================================================
// Plugin Entry Points
// ============================================================================

extern "C" {

LITHIUM_EXPORT auto createSolverPlugin() -> solver::ISolverPlugin* {
    return new AstrometrySolverPlugin();
}

LITHIUM_EXPORT void destroySolverPlugin(solver::ISolverPlugin* plugin) {
    delete plugin;
}

LITHIUM_EXPORT auto getSolverPluginApiVersion() -> int {
    return solver::SOLVER_PLUGIN_API_VERSION;
}

LITHIUM_EXPORT auto getSolverPluginMetadata() -> solver::SolverPluginMetadata {
    solver::SolverPluginMetadata meta;
    meta.name = AstrometrySolverPlugin::PLUGIN_NAME;
    meta.version = AstrometrySolverPlugin::PLUGIN_VERSION;
    meta.description = "Astrometry.net plate solver plugin (local and remote)";
    meta.author = "Max Qian";
    meta.license = "GPL-3.0";
    meta.solverType = "astrometry";
    meta.supportsBlindSolve = true;
    meta.supportsAbort = true;
    meta.requiresExternalBinary = true;
    meta.supportedFormats = {"FITS", "JPEG", "PNG", "TIFF"};
    return meta;
}

}  // extern "C"

}  // namespace lithium::client::astrometry
