/*
 * astap_solver_plugin.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#include "astap_solver_plugin.hpp"

#include "client/solver/service/solver_factory.hpp"
#include "client/solver/service/solver_type_registry.hpp"

#include "atom/log/spdlog_logger.hpp"
#include "atom/system/command.hpp"

#include <array>

namespace lithium::client::astap {

AstapSolverPlugin::AstapSolverPlugin()
    : solver::SolverPluginBase([] {
          solver::SolverPluginMetadata meta;
          meta.name = PLUGIN_NAME;
          meta.version = PLUGIN_VERSION;
          meta.description = "ASTAP (Astrometric STAcking Program) plate solver plugin";
          meta.author = "Max Qian";
          meta.license = "GPL-3.0";
          meta.solverType = SOLVER_TYPE;
          meta.supportsBlindSolve = true;
          meta.supportsAbort = true;
          meta.requiresExternalBinary = true;
          meta.supportedFormats = {"FITS", "FIT", "fits", "fit", "JPEG", "JPG", "PNG", "TIFF"};
          meta.tags = {solver::solver_tags::SOLVER_PLUGIN, solver::solver_tags::ASTAP,
                       solver::solver_tags::LOCAL, solver::solver_tags::FAST};
          meta.capabilities = {solver::solver_capabilities::BLIND_SOLVE,
                               solver::solver_capabilities::HINTED_SOLVE,
                               solver::solver_capabilities::ABORT,
                               solver::solver_capabilities::ASYNC,
                               solver::solver_capabilities::DOWNSAMPLE,
                               solver::solver_capabilities::SCALE_HINTS,
                               solver::solver_capabilities::WCS_OUTPUT,
                               solver::solver_capabilities::ANNOTATION};
          return meta;
      }()) {
    LOG_DEBUG("AstapSolverPlugin constructed");
}

AstapSolverPlugin::~AstapSolverPlugin() {
    LOG_DEBUG("AstapSolverPlugin destroyed");
}

auto AstapSolverPlugin::initialize(const nlohmann::json& config) -> bool {
    LOG_INFO("Initializing ASTAP solver plugin");

    // Check for custom binary path in config
    if (config.contains("binaryPath")) {
        std::filesystem::path customPath = config["binaryPath"].get<std::string>();
        if (setBinaryPath(customPath)) {
            LOG_INFO("Using custom ASTAP binary: {}", customPath.string());
        }
    }

    // Check for database path in config
    if (config.contains("databasePath")) {
        databasePath_ = config["databasePath"].get<std::string>();
        LOG_INFO("Using ASTAP database: {}", databasePath_->string());
    }

    // Call base initialization
    return solver::SolverPluginBase::initialize(config);
}

void AstapSolverPlugin::shutdown() {
    LOG_INFO("Shutting down ASTAP solver plugin");
    solver::SolverPluginBase::shutdown();
}

auto AstapSolverPlugin::getSolverTypes() const
    -> std::vector<solver::SolverTypeInfo> {
    return {buildTypeInfo()};
}

auto AstapSolverPlugin::registerSolverTypes(solver::SolverTypeRegistry& registry)
    -> size_t {
    auto typeInfo = buildTypeInfo();
    if (registry.registerTypeFromPlugin(typeInfo, PLUGIN_NAME)) {
        LOG_INFO("Registered ASTAP solver type");
        emitEvent(createEvent(solver::SolverPluginEventType::TypeRegistered,
                              "Registered ASTAP solver type"));
        return 1;
    }
    return 0;
}

auto AstapSolverPlugin::unregisterSolverTypes(
    solver::SolverTypeRegistry& registry) -> size_t {
    if (registry.unregisterType(SOLVER_TYPE)) {
        LOG_INFO("Unregistered ASTAP solver type");
        emitEvent(createEvent(solver::SolverPluginEventType::TypeUnregistered,
                              "Unregistered ASTAP solver type"));
        return 1;
    }
    return 0;
}

void AstapSolverPlugin::registerSolverCreators(solver::SolverFactory& factory) {
    factory.registerCreator(SOLVER_TYPE,
        [this](const std::string& id, const nlohmann::json& config)
            -> std::shared_ptr<SolverClient> {
            return createSolver(id, config);
        });
    LOG_DEBUG("Registered ASTAP solver creator");
}

void AstapSolverPlugin::unregisterSolverCreators(solver::SolverFactory& factory) {
    factory.unregisterCreator(SOLVER_TYPE);
    LOG_DEBUG("Unregistered ASTAP solver creator");
}

auto AstapSolverPlugin::createSolver(const std::string& solverId,
                                      const nlohmann::json& config)
    -> std::shared_ptr<SolverClient> {
    auto solver = std::make_shared<AstapClient>(solverId);

    // Initialize solver
    if (!solver->initialize()) {
        LOG_ERROR("Failed to initialize ASTAP solver '{}'", solverId);
        return nullptr;
    }

    // Set binary path if we have one
    if (binaryPath_) {
        if (!solver->connect(binaryPath_->string())) {
            LOG_WARN("Failed to connect ASTAP solver to binary: {}",
                     binaryPath_->string());
        }
    } else {
        // Try to find binary automatically
        auto candidates = solver->scan();
        if (!candidates.empty()) {
            solver->connect(candidates.front());
        }
    }

    // Apply configuration
    if (config.contains("options")) {
        auto& opts = config["options"];
        Options astapOpts;

        if (opts.contains("fov")) astapOpts.fov = opts["fov"];
        if (opts.contains("ra")) astapOpts.ra = opts["ra"];
        if (opts.contains("spd")) astapOpts.spd = opts["spd"];
        if (opts.contains("searchRadius")) astapOpts.searchRadius = opts["searchRadius"];
        if (opts.contains("speed")) astapOpts.speed = opts["speed"];
        if (opts.contains("maxStars")) astapOpts.maxStars = opts["maxStars"];
        if (opts.contains("downsample")) astapOpts.downsample = opts["downsample"];
        if (opts.contains("update")) astapOpts.update = opts["update"];
        if (opts.contains("useSIP")) astapOpts.useSIP = opts["useSIP"];

        if (databasePath_) {
            astapOpts.database = databasePath_->string();
        } else if (opts.contains("database")) {
            astapOpts.database = opts["database"];
        }

        solver->setAstapOptions(astapOpts);
    }

    // Register active solver
    registerActiveSolver(solverId, solver);

    LOG_INFO("Created ASTAP solver instance: {}", solverId);
    return solver;
}

auto AstapSolverPlugin::findBinary()
    -> std::optional<std::filesystem::path> {
    return scanForBinary();
}

auto AstapSolverPlugin::validateBinary(const std::filesystem::path& path)
    -> bool {
    if (!std::filesystem::exists(path)) {
        return false;
    }

    // Try to extract version - if successful, binary is valid
    auto version = extractVersion(path);
    return !version.empty();
}

auto AstapSolverPlugin::getBinaryVersion() const -> std::string {
    return binaryVersion_;
}

auto AstapSolverPlugin::setBinaryPath(const std::filesystem::path& path)
    -> bool {
    if (!validateBinary(path)) {
        LOG_ERROR("Invalid ASTAP binary: {}", path.string());
        return false;
    }

    binaryPath_ = path;
    binaryVersion_ = extractVersion(path);
    LOG_INFO("Set ASTAP binary: {} (version: {})",
             path.string(), binaryVersion_);
    return true;
}

auto AstapSolverPlugin::getBinaryPath() const
    -> std::optional<std::filesystem::path> {
    return binaryPath_;
}

auto AstapSolverPlugin::getDefaultOptions() const -> nlohmann::json {
    return nlohmann::json{
        {"speed", 2},
        {"maxStars", 500},
        {"tolerance", 0.007},
        {"searchRadius", 180.0},
        {"downsample", 0},
        {"update", false},
        {"analyse", false},
        {"annotate", false},
        {"useSIP", false}
    };
}

auto AstapSolverPlugin::validateOptions(const nlohmann::json& options)
    -> solver::SolverResult<bool> {
    // Validate speed (1-4)
    if (options.contains("speed")) {
        int speed = options["speed"];
        if (speed < 0 || speed > 4) {
            return solver::makeError<bool>("Speed must be 0-4");
        }
    }

    // Validate maxStars
    if (options.contains("maxStars")) {
        int maxStars = options["maxStars"];
        if (maxStars < 10 || maxStars > 10000) {
            return solver::makeError<bool>("maxStars must be 10-10000");
        }
    }

    // Validate searchRadius
    if (options.contains("searchRadius")) {
        double radius = options["searchRadius"];
        if (radius < 0 || radius > 180) {
            return solver::makeError<bool>("searchRadius must be 0-180 degrees");
        }
    }

    return solver::makeSuccess(true);
}

auto AstapSolverPlugin::getDatabasePaths() const
    -> std::vector<std::filesystem::path> {
    std::vector<std::filesystem::path> paths;

    // Common database locations
#ifdef _WIN32
    paths.push_back("C:/Program Files/astap/data");
    paths.push_back("C:/astap/data");
    if (auto programFiles = std::getenv("PROGRAMFILES")) {
        paths.push_back(std::filesystem::path(programFiles) / "astap" / "data");
    }
#else
    paths.push_back("/usr/share/astap/data");
    paths.push_back("/usr/local/share/astap/data");
    if (auto home = std::getenv("HOME")) {
        paths.push_back(std::filesystem::path(home) / ".astap" / "data");
        paths.push_back(std::filesystem::path(home) / "astap" / "data");
    }
#endif

    // Filter to existing paths
    std::vector<std::filesystem::path> existing;
    for (const auto& p : paths) {
        if (std::filesystem::exists(p)) {
            existing.push_back(p);
        }
    }

    return existing;
}

void AstapSolverPlugin::setPreferredDatabase(
    const std::filesystem::path& dbPath) {
    if (std::filesystem::exists(dbPath)) {
        databasePath_ = dbPath;
        LOG_INFO("Set ASTAP database path: {}", dbPath.string());
    } else {
        LOG_WARN("Database path does not exist: {}", dbPath.string());
    }
}

auto AstapSolverPlugin::isDatabaseAvailable() const -> bool {
    if (databasePath_ && std::filesystem::exists(*databasePath_)) {
        return true;
    }
    return !getDatabasePaths().empty();
}

auto AstapSolverPlugin::buildTypeInfo() const -> solver::SolverTypeInfo {
    solver::SolverTypeInfo info;
    info.typeName = SOLVER_TYPE;
    info.displayName = "ASTAP";
    info.description = "ASTAP (Astrometric STAcking Program) - Fast plate solver";
    info.pluginName = PLUGIN_NAME;
    info.version = binaryVersion_.empty() ? "Unknown" : binaryVersion_;

    info.capabilities.canBlindSolve = true;
    info.capabilities.canHintedSolve = true;
    info.capabilities.canAbort = true;
    info.capabilities.supportsDownsample = true;
    info.capabilities.supportsScale = true;
    info.capabilities.supportsSIP = true;
    info.capabilities.supportsWCSOutput = true;
    info.capabilities.supportsAnnotation = true;
    info.capabilities.supportsAsync = true;
    info.capabilities.supportsStarExtraction = false;
    info.capabilities.requiresQt = false;

    info.optionSchema = buildOptionsSchema();
    info.enabled = binaryPath_.has_value();
    info.priority = 100;  // High priority - fast solver

    return info;
}

auto AstapSolverPlugin::buildOptionsSchema() const -> nlohmann::json {
    return nlohmann::json{
        {"type", "object"},
        {"properties", {
            {"fov", {
                {"type", "number"},
                {"description", "Field of view in degrees"},
                {"minimum", 0.01},
                {"maximum", 180}
            }},
            {"ra", {
                {"type", "number"},
                {"description", "Hint RA in degrees"},
                {"minimum", 0},
                {"maximum", 360}
            }},
            {"spd", {
                {"type", "number"},
                {"description", "South Pole Distance (90 - Dec)"},
                {"minimum", 0},
                {"maximum", 180}
            }},
            {"searchRadius", {
                {"type", "number"},
                {"description", "Search radius in degrees"},
                {"default", 180}
            }},
            {"speed", {
                {"type", "integer"},
                {"description", "Speed mode (1-4, higher=faster)"},
                {"minimum", 0},
                {"maximum", 4},
                {"default", 2}
            }},
            {"maxStars", {
                {"type", "integer"},
                {"description", "Maximum stars for solving"},
                {"minimum", 10},
                {"maximum", 10000},
                {"default", 500}
            }},
            {"downsample", {
                {"type", "integer"},
                {"description", "Downsample factor"},
                {"minimum", 0},
                {"maximum", 8},
                {"default", 0}
            }},
            {"update", {
                {"type", "boolean"},
                {"description", "Update FITS header with WCS"},
                {"default", false}
            }},
            {"useSIP", {
                {"type", "boolean"},
                {"description", "Use SIP polynomial distortion"},
                {"default", false}
            }},
            {"database", {
                {"type", "string"},
                {"description", "Path to star database"}
            }}
        }}
    };
}

auto AstapSolverPlugin::scanForBinary() const
    -> std::optional<std::filesystem::path> {
    std::vector<std::filesystem::path> candidates;

#ifdef _WIN32
    candidates = {
        "C:/Program Files/astap/astap.exe",
        "C:/Program Files (x86)/astap/astap.exe",
        "C:/astap/astap.exe",
        "astap.exe"
    };
    if (auto programFiles = std::getenv("PROGRAMFILES")) {
        candidates.insert(candidates.begin(),
            std::filesystem::path(programFiles) / "astap" / "astap.exe");
    }
#else
    candidates = {
        "/usr/bin/astap",
        "/usr/local/bin/astap",
        "/opt/astap/astap"
    };
    if (auto home = std::getenv("HOME")) {
        candidates.push_back(std::filesystem::path(home) / "bin" / "astap");
        candidates.push_back(std::filesystem::path(home) / "astap" / "astap");
    }
#endif

    for (const auto& path : candidates) {
        if (std::filesystem::exists(path)) {
            LOG_DEBUG("Found ASTAP binary: {}", path.string());
            return path;
        }
    }

    // Try PATH
    std::string pathEnv;
    if (auto p = std::getenv("PATH")) {
        pathEnv = p;
    }

#ifdef _WIN32
    constexpr char pathSep = ';';
    const char* exeName = "astap.exe";
#else
    constexpr char pathSep = ':';
    const char* exeName = "astap";
#endif

    std::istringstream iss(pathEnv);
    std::string dir;
    while (std::getline(iss, dir, pathSep)) {
        auto candidate = std::filesystem::path(dir) / exeName;
        if (std::filesystem::exists(candidate)) {
            LOG_DEBUG("Found ASTAP in PATH: {}", candidate.string());
            return candidate;
        }
    }

    LOG_WARN("ASTAP binary not found");
    return std::nullopt;
}

auto AstapSolverPlugin::extractVersion(const std::filesystem::path& binary) const
    -> std::string {
    try {
        // Run astap with version flag
        std::string cmd = binary.string() + " -v";
        auto result = atom::system::executeCommand(cmd, false, 5000);

        if (!result.output.empty()) {
            // Parse version from output
            // ASTAP typically outputs: "ASTAP version X.Y.Z"
            std::string output = result.output;
            auto pos = output.find("version");
            if (pos != std::string::npos) {
                auto start = pos + 8;  // Skip "version "
                auto end = output.find_first_of(" \n\r", start);
                if (end != std::string::npos) {
                    return output.substr(start, end - start);
                }
                return output.substr(start);
            }

            // Try to find version number pattern
            std::regex versionRegex(R"((\d+\.\d+(?:\.\d+)?))");
            std::smatch match;
            if (std::regex_search(output, match, versionRegex)) {
                return match[1].str();
            }
        }
    } catch (const std::exception& e) {
        LOG_WARN("Failed to extract ASTAP version: {}", e.what());
    }

    return "Unknown";
}

// ============================================================================
// Plugin Entry Points
// ============================================================================

extern "C" {

LITHIUM_EXPORT auto createSolverPlugin() -> solver::ISolverPlugin* {
    return new AstapSolverPlugin();
}

LITHIUM_EXPORT void destroySolverPlugin(solver::ISolverPlugin* plugin) {
    delete plugin;
}

LITHIUM_EXPORT auto getSolverPluginApiVersion() -> int {
    return solver::SOLVER_PLUGIN_API_VERSION;
}

LITHIUM_EXPORT auto getSolverPluginMetadata() -> solver::SolverPluginMetadata {
    solver::SolverPluginMetadata meta;
    meta.name = AstapSolverPlugin::PLUGIN_NAME;
    meta.version = AstapSolverPlugin::PLUGIN_VERSION;
    meta.description = "ASTAP (Astrometric STAcking Program) plate solver plugin";
    meta.author = "Max Qian";
    meta.license = "GPL-3.0";
    meta.solverType = AstapSolverPlugin::SOLVER_TYPE;
    meta.supportsBlindSolve = true;
    meta.supportsAbort = true;
    meta.requiresExternalBinary = true;
    meta.supportedFormats = {"FITS", "FIT", "JPEG", "JPG", "PNG", "TIFF"};
    return meta;
}

}  // extern "C"

}  // namespace lithium::client::astap
