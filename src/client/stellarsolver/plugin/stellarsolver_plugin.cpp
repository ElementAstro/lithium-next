/*
 * stellarsolver_plugin.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#include "stellarsolver_plugin.hpp"

#include "client/solver/service/solver_factory.hpp"
#include "client/solver/service/solver_type_registry.hpp"

#include "atom/log/spdlog_logger.hpp"

#include <algorithm>

#ifdef LITHIUM_HAS_STELLARSOLVER
#include <libstellarsolver/stellarsolver.h>
#endif

namespace lithium::client::stellarsolver {

// ============================================================================
// Constructor / Destructor
// ============================================================================

StellarSolverPlugin::StellarSolverPlugin()
    : SolverPluginBase(PLUGIN_NAME, PLUGIN_VERSION) {
    LOG_DEBUG("StellarSolverPlugin constructed");
}

StellarSolverPlugin::~StellarSolverPlugin() {
    if (getState() == solver::SolverPluginState::Running) {
        shutdown();
    }
}

// ============================================================================
// IPlugin Interface
// ============================================================================

auto StellarSolverPlugin::initialize(const nlohmann::json& config) -> bool {
    std::unique_lock lock(mutex_);

    if (getState() != solver::SolverPluginState::Unloaded &&
        getState() != solver::SolverPluginState::Error) {
        LOG_WARN("StellarSolverPlugin already initialized");
        return true;
    }

    setState(solver::SolverPluginState::Initializing);

    // Store configuration
    config_ = config;

    // Check library availability
#ifdef LITHIUM_HAS_STELLARSOLVER
    libraryVersion_ = detectLibraryVersion();
    LOG_INFO("StellarSolver library version: {}", libraryVersion_);
#else
    LOG_WARN("StellarSolver library not linked");
    libraryVersion_ = "not available";
#endif

    // Initialize Qt if needed
    qtAvailable_ = initializeQt();
    if (!qtAvailable_) {
        LOG_WARN("Qt not available - StellarSolver may have limited functionality");
    }

    // Load index folders from config
    if (config.contains("indexFolders")) {
        for (const auto& folder : config["indexFolders"]) {
            auto path = std::filesystem::path(folder.get<std::string>());
            if (std::filesystem::exists(path)) {
                indexFolders_.push_back(path);
            }
        }
    }

    // Add default index folders
    auto defaultPaths = getDefaultIndexPaths();
    for (const auto& path : defaultPaths) {
        if (std::find(indexFolders_.begin(), indexFolders_.end(), path) ==
            indexFolders_.end()) {
            indexFolders_.push_back(path);
        }
    }

    setState(solver::SolverPluginState::Running);

    LOG_INFO("StellarSolverPlugin initialized (library: {}, Qt: {})",
             libraryVersion_, qtAvailable_ ? "available" : "not available");

    return true;
}

void StellarSolverPlugin::shutdown() {
    std::unique_lock lock(mutex_);

    setState(solver::SolverPluginState::ShuttingDown);

    // Clear index folders
    indexFolders_.clear();

    setState(solver::SolverPluginState::Unloaded);

    LOG_INFO("StellarSolverPlugin shut down");
}

// ============================================================================
// ISolverPlugin Interface
// ============================================================================

auto StellarSolverPlugin::getSolverTypes() const
    -> std::vector<solver::SolverTypeInfo> {
    std::vector<solver::SolverTypeInfo> types;

    // Main solver type
    types.push_back(buildSolverTypeInfo());

    // Extractor-only type (for HFR calculation, star extraction)
    types.push_back(buildExtractorTypeInfo());

    return types;
}

auto StellarSolverPlugin::registerSolverTypes(
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

auto StellarSolverPlugin::unregisterSolverTypes(
    solver::SolverTypeRegistry& registry) -> size_t {
    size_t count = 0;

    if (registry.unregisterType(SOLVER_TYPE)) {
        ++count;
    }
    if (registry.unregisterType(EXTRACTOR_TYPE)) {
        ++count;
    }

    return count;
}

void StellarSolverPlugin::registerSolverCreators(
    solver::SolverFactory& factory) {
    // Register full solver creator
    factory.registerCreator(
        SOLVER_TYPE,
        [this](const std::string& id,
               const nlohmann::json& config) -> std::shared_ptr<SolverClient> {
            return createSolver(id, config);
        });

    // Register extractor creator
    factory.registerCreator(
        EXTRACTOR_TYPE,
        [this](const std::string& id,
               const nlohmann::json& config) -> std::shared_ptr<SolverClient> {
            return createExtractor(id, config);
        });

    LOG_DEBUG("Registered StellarSolver creators");
}

void StellarSolverPlugin::unregisterSolverCreators(
    solver::SolverFactory& factory) {
    factory.unregisterCreator(SOLVER_TYPE);
    factory.unregisterCreator(EXTRACTOR_TYPE);
}

auto StellarSolverPlugin::createSolver(const std::string& solverId,
                                        const nlohmann::json& config)
    -> std::shared_ptr<SolverClient> {
#ifndef LITHIUM_HAS_STELLARSOLVER
    LOG_ERROR("StellarSolver library not available");
    return nullptr;
#else
    auto solver = std::make_shared<StellarSolverClient>(solverId);

    // Apply configuration
    StellarSolverOptions opts;

    if (config.contains("options")) {
        const auto& optsJson = config["options"];

        // Profile
        if (optsJson.contains("profile")) {
            auto profile = optsJson["profile"].get<std::string>();
            if (profile == "default") {
                opts.profile = StellarSolverProfile::Default;
            } else if (profile == "singleThread") {
                opts.profile = StellarSolverProfile::SingleThreadSolving;
            } else if (profile == "parallelLarge") {
                opts.profile = StellarSolverProfile::ParallelLargeScale;
            } else if (profile == "parallelSmall") {
                opts.profile = StellarSolverProfile::ParallelSmallScale;
            } else if (profile == "smallStars") {
                opts.profile = StellarSolverProfile::SmallScaleStars;
            } else {
                opts.profile = StellarSolverProfile::Custom;
            }
        }

        // Scale settings
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
                opts.scaleUnits = ScaleUnits::ArcMinWidth;
            } else if (units == "focalmm") {
                opts.scaleUnits = ScaleUnits::FocalMM;
            } else {
                opts.scaleUnits = ScaleUnits::ArcSecPerPix;
            }
        }

        // Position hints
        if (optsJson.contains("searchRA")) {
            opts.searchRA = optsJson["searchRA"].get<double>();
        }
        if (optsJson.contains("searchDec")) {
            opts.searchDec = optsJson["searchDec"].get<double>();
        }
        if (optsJson.contains("searchRadius")) {
            opts.searchRadius = optsJson["searchRadius"].get<double>();
        }

        // Processing options
        if (optsJson.contains("calculateHFR")) {
            opts.calculateHFR = optsJson["calculateHFR"].get<bool>();
        }
        if (optsJson.contains("downsample")) {
            opts.downsample = optsJson["downsample"].get<int>();
        }

        // Star extraction parameters
        if (optsJson.contains("minArea")) {
            opts.minArea = optsJson["minArea"].get<int>();
        }
        if (optsJson.contains("deblendNThresh")) {
            opts.deblendNThresh = optsJson["deblendNThresh"].get<double>();
        }
        if (optsJson.contains("convFilterFWHM")) {
            opts.convFilterFWHM = optsJson["convFilterFWHM"].get<double>();
        }
    }

    // Add index folders
    for (const auto& folder : indexFolders_) {
        opts.indexFolders.push_back(folder.string());
    }

    solver->setStellarSolverOptions(opts);

    // Initialize
    if (!solver->initialize()) {
        LOG_ERROR("Failed to initialize StellarSolver");
        return nullptr;
    }

    ++solveCount_;
    return solver;
#endif
}

auto StellarSolverPlugin::getBinaryVersion() const -> std::string {
    return libraryVersion_;
}

auto StellarSolverPlugin::getDefaultOptions() const -> nlohmann::json {
    return {{"profile", "default"},
            {"calculateHFR", true},
            {"downsample", 0},
            {"scaleUnits", "arcsecperpix"},
            {"minArea", 5},
            {"convFilterFWHM", 3.5}};
}

auto StellarSolverPlugin::validateOptions(const nlohmann::json& options)
    -> solver::SolverResult<bool> {
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
                "Scale values must be between 0.01 and 1000");
        }
    }

    // Validate search radius
    if (options.contains("searchRadius")) {
        auto radius = options["searchRadius"].get<double>();
        if (radius < 0.1 || radius > 180.0) {
            return solver::SolverResult<bool>::failure(
                "searchRadius must be between 0.1 and 180 degrees");
        }
    }

    // Validate downsample
    if (options.contains("downsample")) {
        auto ds = options["downsample"].get<int>();
        if (ds < 0 || ds > 16) {
            return solver::SolverResult<bool>::failure(
                "downsample must be between 0 and 16");
        }
    }

    // Validate minArea
    if (options.contains("minArea")) {
        auto area = options["minArea"].get<int>();
        if (area < 1 || area > 100) {
            return solver::SolverResult<bool>::failure(
                "minArea must be between 1 and 100");
        }
    }

    return solver::SolverResult<bool>::success(true);
}

// ============================================================================
// StellarSolver-Specific Methods
// ============================================================================

auto StellarSolverPlugin::isLibraryAvailable() const -> bool {
#ifdef LITHIUM_HAS_STELLARSOLVER
    return true;
#else
    return false;
#endif
}

auto StellarSolverPlugin::getAvailableProfiles() const
    -> std::vector<std::string> {
    return {"default",       "singleThread", "parallelLarge",
            "parallelSmall", "smallStars",   "custom"};
}

auto StellarSolverPlugin::getProfileParameters(
    const std::string& profileName) const -> nlohmann::json {
    nlohmann::json params;

    if (profileName == "default") {
        params = {{"multiAlgorithm", "FITS"},
                  {"partitionThreads", 4},
                  {"convFilterType", "default"},
                  {"minWidth", 0.1},
                  {"maxWidth", 30.0}};
    } else if (profileName == "singleThread") {
        params = {{"multiAlgorithm", "NONE"},
                  {"partitionThreads", 1},
                  {"convFilterType", "default"}};
    } else if (profileName == "parallelLarge") {
        params = {{"multiAlgorithm", "PARALLEL_SOLVE"},
                  {"partitionThreads", 8},
                  {"convFilterType", "default"},
                  {"minWidth", 5.0},
                  {"maxWidth", 180.0}};
    } else if (profileName == "parallelSmall") {
        params = {{"multiAlgorithm", "PARALLEL_SOLVE"},
                  {"partitionThreads", 4},
                  {"convFilterType", "gaussian"},
                  {"minWidth", 0.1},
                  {"maxWidth", 5.0}};
    } else if (profileName == "smallStars") {
        params = {{"multiAlgorithm", "FITS"},
                  {"convFilterType", "gaussian"},
                  {"convFilterFWHM", 2.0},
                  {"minArea", 3},
                  {"deblendNThresh", 64}};
    }

    return params;
}

auto StellarSolverPlugin::getDefaultIndexPaths() const
    -> std::vector<std::filesystem::path> {
    std::vector<std::filesystem::path> paths;

#ifdef _WIN32
    paths = {
        "C:/astrometry/data",
        std::filesystem::path(std::getenv("PROGRAMDATA") ? std::getenv("PROGRAMDATA") : "") /
            "StellarSolver" / "index",
        std::filesystem::path(std::getenv("LOCALAPPDATA") ? std::getenv("LOCALAPPDATA") : "") /
            "StellarSolver" / "index"
    };
#else
    paths = {"/usr/share/astrometry", "/usr/local/share/astrometry",
             "/opt/stellarsolver/index"};

    if (const char* home = std::getenv("HOME")) {
        paths.emplace_back(std::filesystem::path(home) / ".local" / "share" /
                           "astrometry");
    }
#endif

    // Filter to existing
    std::vector<std::filesystem::path> existing;
    for (const auto& path : paths) {
        if (std::filesystem::exists(path)) {
            existing.push_back(path);
        }
    }

    return existing;
}

void StellarSolverPlugin::addIndexFolder(const std::filesystem::path& folder) {
    if (!std::filesystem::exists(folder)) {
        LOG_WARN("Index folder does not exist: {}", folder.string());
        return;
    }

    std::unique_lock lock(mutex_);
    if (std::find(indexFolders_.begin(), indexFolders_.end(), folder) ==
        indexFolders_.end()) {
        indexFolders_.push_back(folder);
        LOG_INFO("Added index folder: {}", folder.string());
    }
}

auto StellarSolverPlugin::getIndexFolders() const
    -> std::vector<std::filesystem::path> {
    std::shared_lock lock(mutex_);
    return indexFolders_;
}

auto StellarSolverPlugin::isQtInitialized() const -> bool {
    return qtAvailable_;
}

auto StellarSolverPlugin::createExtractor(const std::string& extractorId,
                                           const nlohmann::json& config)
    -> std::shared_ptr<StellarSolverClient> {
#ifndef LITHIUM_HAS_STELLARSOLVER
    LOG_ERROR("StellarSolver library not available");
    return nullptr;
#else
    auto extractor = std::make_shared<StellarSolverClient>(extractorId);

    StellarSolverOptions opts;
    opts.extractOnly = true;
    opts.calculateHFR = true;

    if (config.contains("options")) {
        const auto& optsJson = config["options"];

        if (optsJson.contains("calculateHFR")) {
            opts.calculateHFR = optsJson["calculateHFR"].get<bool>();
        }
        if (optsJson.contains("minArea")) {
            opts.minArea = optsJson["minArea"].get<int>();
        }
        if (optsJson.contains("convFilterFWHM")) {
            opts.convFilterFWHM = optsJson["convFilterFWHM"].get<double>();
        }
    }

    extractor->setStellarSolverOptions(opts);

    if (!extractor->initialize()) {
        LOG_ERROR("Failed to initialize StellarSolver extractor");
        return nullptr;
    }

    ++extractCount_;
    return extractor;
#endif
}

// ============================================================================
// Private Methods
// ============================================================================

auto StellarSolverPlugin::buildSolverTypeInfo() const
    -> solver::SolverTypeInfo {
    solver::SolverTypeInfo info;
    info.typeName = SOLVER_TYPE;
    info.displayName = "StellarSolver";
    info.pluginName = PLUGIN_NAME;
    info.version = libraryVersion_;
    info.description = "StellarSolver library plate solver with star extraction";
    info.priority = 90;  // High priority (no external binary needed)
    info.enabled = isLibraryAvailable() && qtAvailable_;

    // Capabilities
    info.capabilities.supportedFormats = {"FITS", "JPEG", "PNG", "TIFF"};
    info.capabilities.supportsBlindSolve = true;
    info.capabilities.supportsHintedSolve = true;
    info.capabilities.supportsAbort = true;
    info.capabilities.supportsAsync = true;
    info.capabilities.maxConcurrentSolves = 4;

    // Extra capabilities
    info.capabilities.extraCapabilities["starExtraction"] = true;
    info.capabilities.extraCapabilities["hfrCalculation"] = true;
    info.capabilities.extraCapabilities["pixelToWcs"] = true;
    info.capabilities.extraCapabilities["wcsToPixel"] = true;

    info.optionSchema = buildOptionsSchema();

    return info;
}

auto StellarSolverPlugin::buildExtractorTypeInfo() const
    -> solver::SolverTypeInfo {
    solver::SolverTypeInfo info;
    info.typeName = EXTRACTOR_TYPE;
    info.displayName = "StellarSolver Extractor";
    info.pluginName = PLUGIN_NAME;
    info.version = libraryVersion_;
    info.description = "Star extraction and HFR calculation (no solving)";
    info.priority = 95;  // Higher priority for extraction-only
    info.enabled = isLibraryAvailable() && qtAvailable_;

    // Capabilities (limited - extraction only)
    info.capabilities.supportedFormats = {"FITS", "JPEG", "PNG", "TIFF"};
    info.capabilities.supportsBlindSolve = false;
    info.capabilities.supportsHintedSolve = false;
    info.capabilities.supportsAbort = true;
    info.capabilities.supportsAsync = true;
    info.capabilities.maxConcurrentSolves = 8;  // More parallel for extraction

    info.capabilities.extraCapabilities["starExtraction"] = true;
    info.capabilities.extraCapabilities["hfrCalculation"] = true;

    info.optionSchema = {
        {"type", "object"},
        {"properties",
         {{"calculateHFR",
           {{"type", "boolean"},
            {"description", "Calculate HFR for each star"},
            {"default", true}}},
          {"minArea",
           {{"type", "integer"},
            {"description", "Minimum star area in pixels"},
            {"minimum", 1},
            {"maximum", 100},
            {"default", 5}}},
          {"convFilterFWHM",
           {{"type", "number"},
            {"description", "Convolution filter FWHM"},
            {"minimum", 0.5},
            {"maximum", 10},
            {"default", 3.5}}}}}};

    return info;
}

auto StellarSolverPlugin::buildOptionsSchema() const -> nlohmann::json {
    return {
        {"type", "object"},
        {"properties",
         {{"profile",
           {{"type", "string"},
            {"description", "Solver profile"},
            {"enum", {"default", "singleThread", "parallelLarge",
                      "parallelSmall", "smallStars", "custom"}},
            {"default", "default"}}},
          {"scaleLow",
           {{"type", "number"},
            {"description", "Lower bound of image scale"},
            {"minimum", 0.01},
            {"maximum", 1000}}},
          {"scaleHigh",
           {{"type", "number"},
            {"description", "Upper bound of image scale"},
            {"minimum", 0.01},
            {"maximum", 1000}}},
          {"scaleUnits",
           {{"type", "string"},
            {"description", "Scale units"},
            {"enum",
             {"arcsecperpix", "degwidth", "arcminwidth", "focalmm"}}}},
          {"searchRA",
           {{"type", "number"},
            {"description", "Search RA hint (degrees)"},
            {"minimum", 0},
            {"maximum", 360}}},
          {"searchDec",
           {{"type", "number"},
            {"description", "Search Dec hint (degrees)"},
            {"minimum", -90},
            {"maximum", 90}}},
          {"searchRadius",
           {{"type", "number"},
            {"description", "Search radius (degrees)"},
            {"minimum", 0.1},
            {"maximum", 180},
            {"default", 15}}},
          {"calculateHFR",
           {{"type", "boolean"},
            {"description", "Calculate HFR for stars"},
            {"default", true}}},
          {"downsample",
           {{"type", "integer"},
            {"description", "Downsample factor (0=auto)"},
            {"minimum", 0},
            {"maximum", 16},
            {"default", 0}}},
          {"minArea",
           {{"type", "integer"},
            {"description", "Minimum star area"},
            {"minimum", 1},
            {"maximum", 100},
            {"default", 5}}},
          {"deblendNThresh",
           {{"type", "number"},
            {"description", "Deblending threshold count"},
            {"minimum", 1},
            {"maximum", 128},
            {"default", 32}}},
          {"convFilterFWHM",
           {{"type", "number"},
            {"description", "Convolution filter FWHM"},
            {"minimum", 0.5},
            {"maximum", 10},
            {"default", 3.5}}}}}};
}

auto StellarSolverPlugin::detectLibraryVersion() const -> std::string {
#ifdef LITHIUM_HAS_STELLARSOLVER
    // StellarSolver doesn't expose a version function directly
    // Return a fixed version or compile-time version
    return "2.5";  // Approximate version
#else
    return "not available";
#endif
}

auto StellarSolverPlugin::initializeQt() -> bool {
#ifdef LITHIUM_HAS_STELLARSOLVER
    // Check if QCoreApplication already exists
    if (QCoreApplication::instance() != nullptr) {
        return true;
    }

    // Qt application needs to be created in the main thread
    // For now, assume it's handled by the main application
    LOG_DEBUG("Qt application context should be created by main application");
    return QCoreApplication::instance() != nullptr;
#else
    return false;
#endif
}

// ============================================================================
// Plugin Entry Points
// ============================================================================

extern "C" {

LITHIUM_EXPORT auto createSolverPlugin() -> solver::ISolverPlugin* {
    return new StellarSolverPlugin();
}

LITHIUM_EXPORT void destroySolverPlugin(solver::ISolverPlugin* plugin) {
    delete plugin;
}

LITHIUM_EXPORT auto getSolverPluginApiVersion() -> int {
    return solver::SOLVER_PLUGIN_API_VERSION;
}

LITHIUM_EXPORT auto getSolverPluginMetadata() -> solver::SolverPluginMetadata {
    solver::SolverPluginMetadata meta;
    meta.name = StellarSolverPlugin::PLUGIN_NAME;
    meta.version = StellarSolverPlugin::PLUGIN_VERSION;
    meta.description =
        "StellarSolver library plugin for plate solving and star extraction";
    meta.author = "Max Qian";
    meta.license = "GPL-3.0";
    meta.solverType = "stellarsolver";
    meta.supportsBlindSolve = true;
    meta.supportsAbort = true;
    meta.requiresExternalBinary = false;  // Library-based
    meta.supportedFormats = {"FITS", "JPEG", "PNG", "TIFF"};
    return meta;
}

}  // extern "C"

}  // namespace lithium::client::stellarsolver
