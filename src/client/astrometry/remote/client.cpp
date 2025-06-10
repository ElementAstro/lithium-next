#include "client.hpp"

#include <chrono>
#include <fstream>
#include <thread>

#include "atom/type/json.hpp"

#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>

namespace astrometry {

// Helper function for URL encoding
std::string url_encode(const std::string& value) {
    CURL* curl = curl_easy_init();
    if (!curl) {
        throw AstrometryError(ErrorCode::NetworkError,
                              "Failed to initialize CURL for URL encoding");
    }

    char* output =
        curl_easy_escape(curl, value.c_str(), static_cast<int>(value.length()));
    if (!output) {
        curl_easy_cleanup(curl);
        throw AstrometryError(ErrorCode::NetworkError,
                              "Failed to URL encode string");
    }

    std::string result(output);
    curl_free(output);
    curl_easy_cleanup(curl);
    return result;
}

// Convert license enum to string
std::string AstrometryClient::license_to_string(License license) {
    switch (license) {
        case License::Default:
            return "d";
        case License::Yes:
            return "y";
        case License::No:
            return "n";
        case License::ShareAlike:
            return "sa";
        default:
            return "d";
    }
}

// Convert scale units enum to string
std::string AstrometryClient::scale_units_to_string(ScaleUnits units) {
    switch (units) {
        case ScaleUnits::DegWidth:
            return "degwidth";
        case ScaleUnits::ArcMinWidth:
            return "arcminwidth";
        case ScaleUnits::ArcSecPerPix:
            return "arcsecperpix";
        default:
            return "degwidth";
    }
}

// Convert scale type enum to string
std::string AstrometryClient::scale_type_to_string(ScaleType type) {
    switch (type) {
        case ScaleType::Bounds:
            return "ul";
        case ScaleType::Estimate:
            return "ev";
        default:
            return "ul";
    }
}

// Validation for submission parameters
bool SubmissionParams::validate() const {
    // Either URL or file path must be provided
    if (!url.has_value() && !file_path.has_value()) {
        return false;
    }

    // If scale type is bounds, both lower and upper must be provided
    if (scale_type == ScaleType::Bounds) {
        if (!scale_lower.has_value() || !scale_upper.has_value()) {
            return false;
        }
        if (scale_lower.value() >= scale_upper.value()) {
            return false;
        }
    }

    // If scale type is estimate, both estimate and error must be provided
    if (scale_type == ScaleType::Estimate) {
        if (!scale_est.has_value() || !scale_err.has_value()) {
            return false;
        }
        if (scale_err.value() < 0 || scale_err.value() > 100) {
            return false;
        }
    }

    // Validate RA/DEC if provided
    if (center_ra.has_value()) {
        if (center_ra.value() < 0 || center_ra.value() > 360) {
            return false;
        }
    }

    if (center_dec.has_value()) {
        if (center_dec.value() < -90 || center_dec.value() > 90) {
            return false;
        }
    }

    return true;
}

// CURL write callback
size_t AstrometryClient::write_callback(char* ptr, size_t size, size_t nmemb,
                                        void* userdata) {
    auto* response_data = static_cast<std::string*>(userdata);
    response_data->append(ptr, size * nmemb);
    return size * nmemb;
}

// CURL header callback
size_t AstrometryClient::header_callback(char* buffer, size_t size,
                                         size_t nitems, void* userdata) {
    auto* headers = static_cast<std::map<std::string, std::string>*>(userdata);
    std::string header(buffer, size * nitems);

    auto colon_pos = header.find(':');
    if (colon_pos != std::string::npos) {
        std::string name = header.substr(0, colon_pos);
        std::string value = header.substr(colon_pos + 1);

        // Trim whitespace
        name.erase(0, name.find_first_not_of(" \t"));
        name.erase(name.find_last_not_of(" \t\r\n") + 1);
        value.erase(0, value.find_first_not_of(" \t"));
        value.erase(value.find_last_not_of(" \t\r\n") + 1);

        (*headers)[name] = value;
    }

    return nitems * size;
}

// File write callback for download functions
size_t file_write_callback(char* ptr, size_t size, size_t nmemb,
                           void* userdata) {
    auto* file = static_cast<std::ofstream*>(userdata);
    file->write(ptr, size * nmemb);
    return size * nmemb;
}

// Constructor
AstrometryClient::AstrometryClient(std::string api_key, ClientConfig config)
    : api_key_(std::move(api_key)),
      config_(std::move(config)),
      curl_(nullptr, curl_easy_cleanup) {
    initialize();
}

// Destructor - ensures logout happens if needed
AstrometryClient::~AstrometryClient() {
    if (is_logged_in()) {
        try {
            logout();
        } catch (...) {
            // Ignore exceptions during destruction
        }
    }
}

// Initialize client
void AstrometryClient::initialize() {
    // Initialize logger
    if (!logger_) {
        std::vector<spdlog::sink_ptr> sinks;

        if (config_.log_to_console) {
            sinks.push_back(
                std::make_shared<spdlog::sinks::stdout_color_sink_mt>());
        }

        if (!config_.log_file.empty()) {
            sinks.push_back(std::make_shared<spdlog::sinks::basic_file_sink_mt>(
                config_.log_file));
        }

        logger_ = std::make_shared<spdlog::logger>("astrometry_client",
                                                   sinks.begin(), sinks.end());

        // Set log level
        if (config_.log_level == "trace") {
            logger_->set_level(spdlog::level::trace);
        } else if (config_.log_level == "debug") {
            logger_->set_level(spdlog::level::debug);
        } else if (config_.log_level == "info") {
            logger_->set_level(spdlog::level::info);
        } else if (config_.log_level == "warn") {
            logger_->set_level(spdlog::level::warn);
        } else if (config_.log_level == "error") {
            logger_->set_level(spdlog::level::err);
        } else if (config_.log_level == "critical") {
            logger_->set_level(spdlog::level::critical);
        }

        logger_->flush_on(spdlog::level::err);
    }

    // Initialize libcurl
    curl_global_init(CURL_GLOBAL_ALL);
    curl_.reset(curl_easy_init());
    if (!curl_) {
        logger_->critical("Failed to initialize libcurl");
        throw AstrometryError(ErrorCode::NetworkError,
                              "Failed to initialize libcurl");
    }

    logger_->info("Astrometry.net client initialized with API URL: {}",
                  config_.api_url);
}

// Login method
bool AstrometryClient::login() {
    logger_->info("Logging in with API key");

    nlohmann::json request = {{"apikey", api_key_}};

    try {
        auto response = make_request("login", request, true);

        if (response.contains("status") && response["status"] == "success") {
            if (!response.contains("session")) {
                logger_->error("Login response missing session key");
                throw AstrometryError(ErrorCode::AuthenticationFailed,
                                      "Login response missing session key");
            }

            session_key_ = response["session"].get<std::string>();
            logger_->info("Login successful, session established");
            return true;
        } else {
            logger_->error("Login failed: {}", response.dump());
            return false;
        }
    } catch (const std::exception& e) {
        logger_->error("Login exception: {}", e.what());
        throw AstrometryError(ErrorCode::AuthenticationFailed,
                              std::string("Login failed: ") + e.what());
    }
}

// Check if logged in
bool AstrometryClient::is_logged_in() const { return !session_key_.empty(); }

// Logout method
bool AstrometryClient::logout() {
    if (!is_logged_in()) {
        logger_->warn("Attempting to log out when not logged in");
        return false;
    }

    logger_->info("Logging out from session");

    try {
        nlohmann::json request = {{"session", session_key_}};

        auto response = make_request("logout", request, true);

        if (response.contains("status") && response["status"] == "success") {
            session_key_.clear();
            logger_->info("Logout successful");
            return true;
        } else {
            logger_->error("Logout failed: {}", response.dump());
            return false;
        }
    } catch (const std::exception& e) {
        logger_->error("Logout exception: {}", e.what());
        session_key_.clear();  // Clear the session anyway
        return false;
    }
}

// URL submission
int AstrometryClient::submit_url(const SubmissionParams& params) {
    if (!is_logged_in()) {
        logger_->error("Cannot submit URL: not logged in");
        throw AstrometryError(ErrorCode::AuthenticationFailed, "Not logged in");
    }

    if (!params.url.has_value()) {
        logger_->error("Cannot submit URL: URL not provided in parameters");
        throw AstrometryError(ErrorCode::InvalidParameters, "URL not provided");
    }

    if (!params.validate()) {
        logger_->error("Invalid submission parameters");
        throw AstrometryError(ErrorCode::InvalidParameters,
                              "Invalid submission parameters");
    }

    logger_->info("Submitting URL for processing: {}", params.url.value());

    nlohmann::json request = {
        {"session", session_key_},
        {"url", params.url.value()},
        {"allow_commercial_use",
         license_to_string(params.allow_commercial_use)},
        {"allow_modifications", license_to_string(params.allow_modifications)},
        {"publicly_visible", params.publicly_visible ? "y" : "n"},
        {"scale_units", scale_units_to_string(params.scale_units)},
        {"scale_type", scale_type_to_string(params.scale_type)},
    };

    // Add optional parameters if they are set
    if (params.scale_lower)
        request["scale_lower"] = params.scale_lower.value();
    if (params.scale_upper)
        request["scale_upper"] = params.scale_upper.value();
    if (params.scale_est)
        request["scale_est"] = params.scale_est.value();
    if (params.scale_err)
        request["scale_err"] = params.scale_err.value();
    if (params.center_ra)
        request["center_ra"] = params.center_ra.value();
    if (params.center_dec)
        request["center_dec"] = params.center_dec.value();
    if (params.radius)
        request["radius"] = params.radius.value();
    if (params.downsample_factor)
        request["downsample_factor"] = params.downsample_factor.value();
    if (params.tweak_order)
        request["tweak_order"] = params.tweak_order.value();
    if (params.use_sextractor)
        request["use_sextractor"] = params.use_sextractor.value();
    if (params.crpix_center)
        request["crpix_center"] = params.crpix_center.value();
    if (params.parity)
        request["parity"] = params.parity.value();
    if (params.image_width)
        request["image_width"] = params.image_width.value();
    if (params.image_height)
        request["image_height"] = params.image_height.value();
    if (params.positional_error)
        request["positional_error"] = params.positional_error.value();

    try {
        auto response = make_request("url_upload", request, true);

        if (response.contains("status") && response["status"] == "success") {
            if (!response.contains("subid")) {
                logger_->error("Submission response missing submission ID");
                throw AstrometryError(
                    ErrorCode::ServerError,
                    "Submission response missing submission ID");
            }

            int submission_id = response["subid"].get<int>();
            logger_->info("URL submission successful, submission ID: {}",
                          submission_id);
            return submission_id;
        } else {
            logger_->error("URL submission failed: {}", response.dump());
            throw AstrometryError(ErrorCode::ServerError,
                                  "URL submission failed: " + response.dump());
        }
    } catch (const AstrometryError& e) {
        throw;  // Re-throw AstrometryError exceptions
    } catch (const std::exception& e) {
        logger_->error("URL submission exception: {}", e.what());
        throw AstrometryError(
            ErrorCode::UnknownError,
            std::string("URL submission failed: ") + e.what());
    }
}

// Asynchronous URL submission
std::future<int> AstrometryClient::submit_url_async(
    const SubmissionParams& params) {
    return std::async(std::launch::async,
                      [this, params]() { return submit_url(params); });
}

// File submission
int AstrometryClient::submit_file(const SubmissionParams& params) {
    if (!is_logged_in()) {
        logger_->error("Cannot submit file: not logged in");
        throw AstrometryError(ErrorCode::AuthenticationFailed, "Not logged in");
    }

    if (!params.file_path.has_value()) {
        logger_->error(
            "Cannot submit file: file path not provided in parameters");
        throw AstrometryError(ErrorCode::InvalidParameters,
                              "File path not provided");
    }

    if (!params.validate()) {
        logger_->error("Invalid submission parameters");
        throw AstrometryError(ErrorCode::InvalidParameters,
                              "Invalid submission parameters");
    }

    if (!std::filesystem::exists(params.file_path.value())) {
        logger_->error("File does not exist: {}",
                       params.file_path.value().string());
        throw AstrometryError(ErrorCode::FileError, "File does not exist");
    }

    logger_->info("Submitting file for processing: {}",
                  params.file_path.value().string());

    nlohmann::json request = {
        {"session", session_key_},
        {"allow_commercial_use",
         license_to_string(params.allow_commercial_use)},
        {"allow_modifications", license_to_string(params.allow_modifications)},
        {"publicly_visible", params.publicly_visible ? "y" : "n"},
        {"scale_units", scale_units_to_string(params.scale_units)},
        {"scale_type", scale_type_to_string(params.scale_type)},
    };

    // Add optional parameters
    if (params.scale_lower)
        request["scale_lower"] = params.scale_lower.value();
    if (params.scale_upper)
        request["scale_upper"] = params.scale_upper.value();
    if (params.scale_est)
        request["scale_est"] = params.scale_est.value();
    if (params.scale_err)
        request["scale_err"] = params.scale_err.value();
    if (params.center_ra)
        request["center_ra"] = params.center_ra.value();
    if (params.center_dec)
        request["center_dec"] = params.center_dec.value();
    if (params.radius)
        request["radius"] = params.radius.value();
    if (params.downsample_factor)
        request["downsample_factor"] = params.downsample_factor.value();
    if (params.tweak_order)
        request["tweak_order"] = params.tweak_order.value();
    if (params.use_sextractor)
        request["use_sextractor"] = params.use_sextractor.value();
    if (params.crpix_center)
        request["crpix_center"] = params.crpix_center.value();
    if (params.parity)
        request["parity"] = params.parity.value();
    if (params.image_width)
        request["image_width"] = params.image_width.value();
    if (params.image_height)
        request["image_height"] = params.image_height.value();
    if (params.positional_error)
        request["positional_error"] = params.positional_error.value();

    try {
        auto response =
            upload_file("upload", request, params.file_path.value());

        if (response.contains("status") && response["status"] == "success") {
            if (!response.contains("subid")) {
                logger_->error("Submission response missing submission ID");
                throw AstrometryError(
                    ErrorCode::ServerError,
                    "Submission response missing submission ID");
            }

            int submission_id = response["subid"].get<int>();
            logger_->info("File submission successful, submission ID: {}",
                          submission_id);
            return submission_id;
        } else {
            logger_->error("File submission failed: {}", response.dump());
            throw AstrometryError(ErrorCode::ServerError,
                                  "File submission failed: " + response.dump());
        }
    } catch (const AstrometryError& e) {
        throw;  // Re-throw AstrometryError exceptions
    } catch (const std::exception& e) {
        logger_->error("File submission exception: {}", e.what());
        throw AstrometryError(
            ErrorCode::UnknownError,
            std::string("File submission failed: ") + e.what());
    }
}

// Asynchronous file submission
std::future<int> AstrometryClient::submit_file_async(
    const SubmissionParams& params) {
    return std::async(std::launch::async,
                      [this, params]() { return submit_file(params); });
}

// Get submission status
nlohmann::json AstrometryClient::get_submission_status(int submission_id) {
    if (!is_logged_in()) {
        logger_->error("Cannot get submission status: not logged in");
        throw AstrometryError(ErrorCode::AuthenticationFailed, "Not logged in");
    }

    logger_->info("Getting status for submission ID: {}", submission_id);

    try {
        std::string endpoint = "submissions/" + std::to_string(submission_id);
        auto response = make_request(endpoint, {}, false);

        logger_->debug("Submission status response: {}", response.dump());
        return response;
    } catch (const std::exception& e) {
        logger_->error("Failed to get submission status: {}", e.what());
        throw AstrometryError(
            ErrorCode::NetworkError,
            std::string("Failed to get submission status: ") + e.what());
    }
}

// Get job status
nlohmann::json AstrometryClient::get_job_status(int job_id) {
    if (!is_logged_in()) {
        logger_->error("Cannot get job status: not logged in");
        throw AstrometryError(ErrorCode::AuthenticationFailed, "Not logged in");
    }

    logger_->info("Getting status for job ID: {}", job_id);

    try {
        std::string endpoint = "jobs/" + std::to_string(job_id);
        auto response = make_request(endpoint, {}, false);

        logger_->debug("Job status response: {}", response.dump());
        return response;
    } catch (const std::exception& e) {
        logger_->error("Failed to get job status: {}", e.what());
        throw AstrometryError(
            ErrorCode::NetworkError,
            std::string("Failed to get job status: ") + e.what());
    }
}

// Get job calibration
CalibrationResult AstrometryClient::get_job_calibration(int job_id) {
    if (!is_logged_in()) {
        logger_->error("Cannot get job calibration: not logged in");
        throw AstrometryError(ErrorCode::AuthenticationFailed, "Not logged in");
    }

    logger_->info("Getting calibration for job ID: {}", job_id);

    try {
        std::string endpoint =
            "jobs/" + std::to_string(job_id) + "/calibration/";
        auto response = make_request(endpoint, {}, false);

        logger_->debug("Job calibration response: {}", response.dump());

        if (!response.contains("ra") || !response.contains("dec") ||
            !response.contains("radius") || !response.contains("pixscale") ||
            !response.contains("orientation") || !response.contains("parity")) {
            logger_->error("Calibration response missing required fields");
            throw AstrometryError(
                ErrorCode::ParseError,
                "Calibration response missing required fields");
        }

        CalibrationResult result;
        result.ra = response["ra"].get<double>();
        result.dec = response["dec"].get<double>();
        result.radius = response["radius"].get<double>();
        result.pixscale = response["pixscale"].get<double>();
        result.orientation = response["orientation"].get<double>();
        result.parity = response["parity"].get<double>();

        return result;
    } catch (const AstrometryError& e) {
        throw;  // Re-throw AstrometryError exceptions
    } catch (const std::exception& e) {
        logger_->error("Failed to get job calibration: {}", e.what());
        throw AstrometryError(
            ErrorCode::NetworkError,
            std::string("Failed to get job calibration: ") + e.what());
    }
}

// Get job tags
std::vector<std::string> AstrometryClient::get_job_tags(int job_id) {
    if (!is_logged_in()) {
        logger_->error("Cannot get job tags: not logged in");
        throw AstrometryError(ErrorCode::AuthenticationFailed, "Not logged in");
    }

    logger_->info("Getting tags for job ID: {}", job_id);

    try {
        std::string endpoint = "jobs/" + std::to_string(job_id) + "/tags/";
        auto response = make_request(endpoint, {}, false);

        logger_->debug("Job tags response: {}", response.dump());

        std::vector<std::string> tags;
        if (response.contains("tags") && response["tags"].is_array()) {
            for (const auto& tag : response["tags"]) {
                tags.push_back(tag.get<std::string>());
            }
        }

        return tags;
    } catch (const std::exception& e) {
        logger_->error("Failed to get job tags: {}", e.what());
        throw AstrometryError(
            ErrorCode::NetworkError,
            std::string("Failed to get job tags: ") + e.what());
    }
}

// Get job machine tags
std::vector<std::string> AstrometryClient::get_job_machine_tags(int job_id) {
    if (!is_logged_in()) {
        logger_->error("Cannot get job machine tags: not logged in");
        throw AstrometryError(ErrorCode::AuthenticationFailed, "Not logged in");
    }

    logger_->info("Getting machine tags for job ID: {}", job_id);

    try {
        std::string endpoint =
            "jobs/" + std::to_string(job_id) + "/machine_tags/";
        auto response = make_request(endpoint, {}, false);

        logger_->debug("Job machine tags response: {}", response.dump());

        std::vector<std::string> machine_tags;
        if (response.contains("tags") && response["tags"].is_array()) {
            for (const auto& tag : response["tags"]) {
                machine_tags.push_back(tag.get<std::string>());
            }
        }

        return machine_tags;
    } catch (const std::exception& e) {
        logger_->error("Failed to get job machine tags: {}", e.what());
        throw AstrometryError(
            ErrorCode::NetworkError,
            std::string("Failed to get job machine tags: ") + e.what());
    }
}

// Get objects in field
std::vector<std::string> AstrometryClient::get_objects_in_field(int job_id) {
    if (!is_logged_in()) {
        logger_->error("Cannot get objects in field: not logged in");
        throw AstrometryError(ErrorCode::AuthenticationFailed, "Not logged in");
    }

    logger_->info("Getting objects in field for job ID: {}", job_id);

    try {
        std::string endpoint =
            "jobs/" + std::to_string(job_id) + "/objects_in_field/";
        auto response = make_request(endpoint, {}, false);

        logger_->debug("Objects in field response: {}", response.dump());

        std::vector<std::string> objects;
        if (response.contains("objects_in_field") &&
            response["objects_in_field"].is_array()) {
            for (const auto& obj : response["objects_in_field"]) {
                objects.push_back(obj.get<std::string>());
            }
        }

        return objects;
    } catch (const std::exception& e) {
        logger_->error("Failed to get objects in field: {}", e.what());
        throw AstrometryError(
            ErrorCode::NetworkError,
            std::string("Failed to get objects in field: ") + e.what());
    }
}

// Get annotations
std::vector<Annotation> AstrometryClient::get_annotations(int job_id) {
    if (!is_logged_in()) {
        logger_->error("Cannot get annotations: not logged in");
        throw AstrometryError(ErrorCode::AuthenticationFailed, "Not logged in");
    }

    logger_->info("Getting annotations for job ID: {}", job_id);

    try {
        std::string endpoint =
            "jobs/" + std::to_string(job_id) + "/annotations/";
        auto response = make_request(endpoint, {}, false);

        logger_->debug("Annotations response: {}", response.dump());

        std::vector<Annotation> annotations;
        if (response.contains("annotations") &&
            response["annotations"].is_array()) {
            for (const auto& annotation_json : response["annotations"]) {
                Annotation annotation;

                if (annotation_json.contains("type")) {
                    annotation.type =
                        annotation_json["type"].get<std::string>();
                }

                if (annotation_json.contains("names") &&
                    annotation_json["names"].is_array()) {
                    for (const auto& name : annotation_json["names"]) {
                        annotation.names.push_back(name.get<std::string>());
                    }
                }

                if (annotation_json.contains("pixelx")) {
                    annotation.pixelx = annotation_json["pixelx"].get<double>();
                }

                if (annotation_json.contains("pixely")) {
                    annotation.pixely = annotation_json["pixely"].get<double>();
                }

                if (annotation_json.contains("radius")) {
                    annotation.radius = annotation_json["radius"].get<double>();
                }

                annotations.push_back(annotation);
            }
        }

        return annotations;
    } catch (const std::exception& e) {
        logger_->error("Failed to get annotations: {}", e.what());
        throw AstrometryError(
            ErrorCode::NetworkError,
            std::string("Failed to get annotations: ") + e.what());
    }
}

// Get comprehensive job info
JobInfo AstrometryClient::get_job_info(int job_id) {
    if (!is_logged_in()) {
        logger_->error("Cannot get job info: not logged in");
        throw AstrometryError(ErrorCode::AuthenticationFailed, "Not logged in");
    }

    logger_->info("Getting info for job ID: {}", job_id);

    try {
        std::string endpoint = "jobs/" + std::to_string(job_id) + "/info/";
        auto response = make_request(endpoint, {}, false);

        logger_->debug("Job info response: {}", response.dump());

        JobInfo info;

        // Extract status
        if (response.contains("status")) {
            info.status = response["status"].get<std::string>();
        }

        // Extract machine tags
        if (response.contains("machine_tags") &&
            response["machine_tags"].is_array()) {
            for (const auto& tag : response["machine_tags"]) {
                info.machine_tags.push_back(tag.get<std::string>());
            }
        }

        // Extract tags
        if (response.contains("tags") && response["tags"].is_array()) {
            for (const auto& tag : response["tags"]) {
                info.tags.push_back(tag.get<std::string>());
            }
        }

        // Extract objects in field
        if (response.contains("objects_in_field") &&
            response["objects_in_field"].is_array()) {
            for (const auto& obj : response["objects_in_field"]) {
                info.objects_in_field.push_back(obj.get<std::string>());
            }
        }

        // Extract original filename
        if (response.contains("original_filename")) {
            info.original_filename =
                response["original_filename"].get<std::string>();
        }

        // Extract calibration if available
        if (response.contains("calibration") &&
            !response["calibration"].is_null()) {
            CalibrationResult calibration;
            auto calib_json = response["calibration"];

            if (calib_json.contains("ra") && calib_json.contains("dec") &&
                calib_json.contains("radius") &&
                calib_json.contains("pixscale") &&
                calib_json.contains("orientation") &&
                calib_json.contains("parity")) {
                calibration.ra = calib_json["ra"].get<double>();
                calibration.dec = calib_json["dec"].get<double>();
                calibration.radius = calib_json["radius"].get<double>();
                calibration.pixscale = calib_json["pixscale"].get<double>();
                calibration.orientation =
                    calib_json["orientation"].get<double>();
                calibration.parity = calib_json["parity"].get<double>();

                info.calibration = calibration;
            }
        }

        // Get annotations separately - they're not included in the info
        // endpoint
        try {
            info.annotations = get_annotations(job_id);
        } catch (const std::exception& e) {
            logger_->warn("Failed to get annotations for job {}: {}", job_id,
                          e.what());
            // Continue without annotations
        }

        return info;
    } catch (const std::exception& e) {
        logger_->error("Failed to get job info: {}", e.what());
        throw AstrometryError(
            ErrorCode::NetworkError,
            std::string("Failed to get job info: ") + e.what());
    }
}

// Download job file
bool AstrometryClient::download_job_file(
    int job_id, const std::string& file_type,
    const std::filesystem::path& output_path) {
    if (!is_logged_in()) {
        logger_->error("Cannot download job file: not logged in");
        throw AstrometryError(ErrorCode::AuthenticationFailed, "Not logged in");
    }

    logger_->info("Downloading {} file for job ID: {} to {}", file_type, job_id,
                  output_path.string());

    try {
        // Create parent directories if they don't exist
        std::filesystem::path parent_path = output_path.parent_path();
        if (!parent_path.empty() && !std::filesystem::exists(parent_path)) {
            std::filesystem::create_directories(parent_path);
        }

        // Open output file
        std::ofstream output_file(output_path, std::ios::binary);
        if (!output_file.is_open()) {
            logger_->error("Failed to open output file: {}",
                           output_path.string());
            throw AstrometryError(
                ErrorCode::FileError,
                "Failed to open output file: " + output_path.string());
        }

        // Base URL for files - not using the API base
        std::string file_base_url = "http://nova.astrometry.net";
        std::string url =
            file_base_url + "/" + file_type + "_file/" + std::to_string(job_id);

        // Special case for image displays
        if (file_type == "annotated" || file_type == "red_green_image" ||
            file_type == "extraction_image") {
            url = file_base_url + "/" + file_type + "_display/" +
                  std::to_string(job_id);
        }

        std::unique_lock<std::mutex> lock(curl_mutex_);

        // Reset CURL options
        curl_easy_reset(curl_.get());

        // Set CURL options for file download
        curl_easy_setopt(curl_.get(), CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl_.get(), CURLOPT_WRITEFUNCTION,
                         file_write_callback);
        curl_easy_setopt(curl_.get(), CURLOPT_WRITEDATA, &output_file);
        curl_easy_setopt(curl_.get(), CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(curl_.get(), CURLOPT_USERAGENT,
                         config_.user_agent.c_str());
        curl_easy_setopt(curl_.get(), CURLOPT_TIMEOUT,
                         static_cast<long>(config_.timeout.count()));

        // Perform request
        CURLcode res = curl_easy_perform(curl_.get());

        // Close the file
        output_file.close();

        if (res != CURLE_OK) {
            logger_->error("Failed to download file: {}",
                           curl_easy_strerror(res));
            throw AstrometryError(ErrorCode::NetworkError,
                                  std::string("Failed to download file: ") +
                                      curl_easy_strerror(res));
        }

        // Check HTTP status code
        long http_code = 0;
        curl_easy_getinfo(curl_.get(), CURLINFO_RESPONSE_CODE, &http_code);

        if (http_code != 200) {
            logger_->error("HTTP error while downloading file: {}", http_code);

            // Remove the file if it was created
            if (std::filesystem::exists(output_path)) {
                std::filesystem::remove(output_path);
            }

            throw AstrometryError(ErrorCode::NetworkError,
                                  "HTTP error while downloading file: " +
                                      std::to_string(http_code));
        }

        logger_->info("File downloaded successfully to {}",
                      output_path.string());
        return true;
    } catch (const AstrometryError& e) {
        throw;  // Re-throw AstrometryError exceptions
    } catch (const std::exception& e) {
        logger_->error("Failed to download job file: {}", e.what());
        throw AstrometryError(
            ErrorCode::UnknownError,
            std::string("Failed to download job file: ") + e.what());
    }
}

// Download job file asynchronously
std::future<bool> AstrometryClient::download_job_file_async(
    int job_id, const std::string& file_type,
    const std::filesystem::path& output_path) {
    return std::async(
        std::launch::async, [this, job_id, file_type, output_path]() {
            return download_job_file(job_id, file_type, output_path);
        });
}

// Wait for job completion implementation
int AstrometryClient::wait_for_job_completion(int submission_id,
                                              int timeout_seconds,
                                              int poll_interval_seconds) {
    if (!is_logged_in()) {
        logger_->error("Cannot wait for job: not logged in");
        throw AstrometryError(ErrorCode::AuthenticationFailed, "Not logged in");
    }

    logger_->info(
        "Waiting for submission {} to complete (timeout: {}s, poll: {}s)",
        submission_id, timeout_seconds, poll_interval_seconds);

    auto start_time = std::chrono::steady_clock::now();
    auto timeout_duration = std::chrono::seconds(timeout_seconds);

    while (true) {
        try {
            auto submission_status = get_submission_status(submission_id);

            // Check if the submission has jobs
            if (submission_status.contains("jobs") &&
                submission_status["jobs"].is_array() &&
                !submission_status["jobs"].empty()) {
                int job_id = submission_status["jobs"][0].get<int>();

                // Check if the job has calibrations (indicating success)
                if (submission_status.contains("job_calibrations") &&
                    submission_status["job_calibrations"].is_array() &&
                    !submission_status["job_calibrations"].empty()) {
                    logger_->info("Job completed successfully: {}", job_id);
                    return job_id;
                }

                // Check job status
                auto job_status = get_job_status(job_id);
                if (job_status.contains("status")) {
                    std::string status =
                        job_status["status"].get<std::string>();

                    if (status == "success") {
                        logger_->info("Job completed successfully: {}", job_id);
                        return job_id;
                    } else if (status == "failure") {
                        logger_->error("Job failed: {}", job_id);
                        throw AstrometryError(
                            ErrorCode::ServerError,
                            "Job failed: " + std::to_string(job_id));
                    }
                }
            }

            // Check if we've exceeded the timeout
            auto elapsed = std::chrono::steady_clock::now() - start_time;
            if (elapsed > timeout_duration) {
                logger_->error("Timeout waiting for job completion");
                throw AstrometryError(ErrorCode::Timeout,
                                      "Timeout waiting for job completion");
            }

            // Sleep before polling again
            std::this_thread::sleep_for(
                std::chrono::seconds(poll_interval_seconds));

        } catch (const AstrometryError& e) {
            throw;  // Re-throw AstrometryError exceptions
        } catch (const std::exception& e) {
            logger_->error("Error while waiting for job: {}", e.what());
            throw AstrometryError(
                ErrorCode::UnknownError,
                std::string("Error while waiting for job: ") + e.what());
        }
    }
}

// Asynchronous wait for job completion
std::future<int> AstrometryClient::wait_for_job_completion_async(
    int submission_id, int timeout_seconds, int poll_interval_seconds) {
    return std::async(std::launch::async, [this, submission_id, timeout_seconds,
                                           poll_interval_seconds]() {
        return wait_for_job_completion(submission_id, timeout_seconds,
                                       poll_interval_seconds);
    });
}

// Core HTTP request handler implementation
nlohmann::json AstrometryClient::make_request(const std::string& endpoint,
                                              const nlohmann::json& params,
                                              bool use_post) {
    std::unique_lock<std::mutex> lock(curl_mutex_);

    std::string url = config_.api_url + "/" + endpoint;
    std::string json_str;

    if (!params.empty()) {
        json_str = params.dump();
    }

    logger_->debug("Making {} request to {}", use_post ? "POST" : "GET", url);
    if (!json_str.empty()) {
        logger_->debug("Request parameters: {}", json_str);
    }

    // Reset CURL options
    curl_easy_reset(curl_.get());

    // Set up response data
    std::string response_data;
    std::map<std::string, std::string> response_headers;

    // Set CURL options
    curl_easy_setopt(curl_.get(), CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl_.get(), CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl_.get(), CURLOPT_WRITEDATA, &response_data);
    curl_easy_setopt(curl_.get(), CURLOPT_HEADERFUNCTION, header_callback);
    curl_easy_setopt(curl_.get(), CURLOPT_HEADERDATA, &response_headers);
    curl_easy_setopt(curl_.get(), CURLOPT_TIMEOUT,
                     static_cast<long>(config_.timeout.count()));
    curl_easy_setopt(curl_.get(), CURLOPT_USERAGENT,
                     config_.user_agent.c_str());
    curl_easy_setopt(curl_.get(), CURLOPT_FOLLOWLOCATION, 1L);

    // SSL verification
    if (!config_.verify_ssl) {
        curl_easy_setopt(curl_.get(), CURLOPT_SSL_VERIFYPEER, 0L);
        curl_easy_setopt(curl_.get(), CURLOPT_SSL_VERIFYHOST, 0L);
    }

    // Set up for POST or GET
    if (use_post && !json_str.empty()) {
        curl_easy_setopt(curl_.get(), CURLOPT_POST, 1L);

        // Prepare the x-www-form-urlencoded data
        std::string post_fields = "request-json=" + url_encode(json_str);
        curl_easy_setopt(curl_.get(), CURLOPT_POSTFIELDS, post_fields.c_str());
    }

    // Perform request with retries
    CURLcode res = CURLE_OK;
    int retry_count = 0;
    bool success = false;

    while (!success && retry_count <= config_.retry_count) {
        if (retry_count > 0) {
            logger_->info("Retrying request ({}/{})", retry_count,
                          config_.retry_count);
            std::this_thread::sleep_for(config_.retry_delay * retry_count);
        }

        res = curl_easy_perform(curl_.get());

        if (res == CURLE_OK) {
            long http_code = 0;
            curl_easy_getinfo(curl_.get(), CURLINFO_RESPONSE_CODE, &http_code);

            if (http_code >= 200 && http_code < 300) {
                success = true;
            } else {
                logger_->error("HTTP error: {} (attempt {}/{})", http_code,
                               retry_count + 1, config_.retry_count + 1);
            }
        } else {
            logger_->error("CURL error: {} (attempt {}/{})",
                           curl_easy_strerror(res), retry_count + 1,
                           config_.retry_count + 1);
        }

        retry_count++;
    }

    if (!success) {
        handle_curl_error(res, "Request to " + url);
    }

    // Parse the JSON response
    try {
        if (response_data.empty()) {
            logger_->error("Empty response from server");
            throw AstrometryError(ErrorCode::ParseError,
                                  "Empty response from server");
        }

        auto json_response = nlohmann::json::parse(response_data);

        // Check for API error responses
        if (json_response.contains("status") &&
            json_response["status"] == "error") {
            std::string error_message = "API error";
            if (json_response.contains("errormessage")) {
                error_message +=
                    ": " + json_response["errormessage"].get<std::string>();
            }

            logger_->error(error_message);
            throw AstrometryError(ErrorCode::ServerError, error_message);
        }

        return json_response;
    } catch (const nlohmann::json::parse_error& e) {
        logger_->error("JSON parse error: {} - Response: {}", e.what(),
                       response_data);
        throw AstrometryError(
            ErrorCode::ParseError,
            "Failed to parse JSON response: " + std::string(e.what()));
    }
}

// Implementation of upload_file method for multipart form data
nlohmann::json AstrometryClient::upload_file(
    const std::string& endpoint, const nlohmann::json& params,
    const std::filesystem::path& file_path) {
    std::unique_lock<std::mutex> lock(curl_mutex_);

    std::string url = config_.api_url + "/" + endpoint;
    std::string json_str = params.dump();

    logger_->debug("Uploading file {} to {}", file_path.string(), url);
    logger_->debug("Request parameters: {}", json_str);

    // Reset CURL options
    curl_easy_reset(curl_.get());

    // Set up response data
    std::string response_data;
    std::map<std::string, std::string> response_headers;

    // Set CURL options for file upload
    curl_easy_setopt(curl_.get(), CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl_.get(), CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl_.get(), CURLOPT_WRITEDATA, &response_data);
    curl_easy_setopt(curl_.get(), CURLOPT_HEADERFUNCTION, header_callback);
    curl_easy_setopt(curl_.get(), CURLOPT_HEADERDATA, &response_headers);
    curl_easy_setopt(curl_.get(), CURLOPT_TIMEOUT,
                     static_cast<long>(config_.timeout.count() *
                                       3));  // Longer timeout for uploads
    curl_easy_setopt(curl_.get(), CURLOPT_USERAGENT,
                     config_.user_agent.c_str());
    curl_easy_setopt(curl_.get(), CURLOPT_FOLLOWLOCATION, 1L);

    // SSL verification
    if (!config_.verify_ssl) {
        curl_easy_setopt(curl_.get(), CURLOPT_SSL_VERIFYPEER, 0L);
        curl_easy_setopt(curl_.get(), CURLOPT_SSL_VERIFYHOST, 0L);
    }

    // Set up multipart form data
    curl_mime* mime = curl_mime_init(curl_.get());

    // Add the JSON parameters
    curl_mimepart* part = curl_mime_addpart(mime);
    curl_mime_name(part, "request-json");
    curl_mime_data(part, json_str.c_str(), CURL_ZERO_TERMINATED);
    curl_mime_type(part, "text/plain");

    // Add the file
    part = curl_mime_addpart(mime);
    curl_mime_name(part, "file");
    curl_mime_filedata(part, file_path.string().c_str());
    curl_mime_filename(part, file_path.filename().string().c_str());
    curl_mime_type(part, "application/octet-stream");

    // Set multipart form data
    curl_easy_setopt(curl_.get(), CURLOPT_MIMEPOST, mime);

    // Perform request with retries
    CURLcode res = CURLE_OK;
    int retry_count = 0;
    bool success = false;

    while (!success && retry_count <= config_.retry_count) {
        if (retry_count > 0) {
            logger_->info("Retrying file upload ({}/{})", retry_count,
                          config_.retry_count);
            std::this_thread::sleep_for(config_.retry_delay * retry_count);
        }

        res = curl_easy_perform(curl_.get());

        if (res == CURLE_OK) {
            long http_code = 0;
            curl_easy_getinfo(curl_.get(), CURLINFO_RESPONSE_CODE, &http_code);

            if (http_code >= 200 && http_code < 300) {
                success = true;
            } else {
                logger_->error(
                    "HTTP error during file upload: {} (attempt {}/{})",
                    http_code, retry_count + 1, config_.retry_count + 1);
            }
        } else {
            logger_->error("CURL error during file upload: {} (attempt {}/{})",
                           curl_easy_strerror(res), retry_count + 1,
                           config_.retry_count + 1);
        }

        retry_count++;
    }

    // Free mime data
    curl_mime_free(mime);

    if (!success) {
        handle_curl_error(res, "File upload to " + url);
    }

    // Parse the JSON response
    try {
        if (response_data.empty()) {
            logger_->error("Empty response from server during file upload");
            throw AstrometryError(
                ErrorCode::ParseError,
                "Empty response from server during file upload");
        }

        auto json_response = nlohmann::json::parse(response_data);

        // Check for API error responses
        if (json_response.contains("status") &&
            json_response["status"] == "error") {
            std::string error_message = "API error during file upload";
            if (json_response.contains("errormessage")) {
                error_message +=
                    ": " + json_response["errormessage"].get<std::string>();
            }

            logger_->error(error_message);
            throw AstrometryError(ErrorCode::ServerError, error_message);
        }

        return json_response;
    } catch (const nlohmann::json::parse_error& e) {
        logger_->error("JSON parse error during file upload: {} - Response: {}",
                       e.what(), response_data);
        throw AstrometryError(
            ErrorCode::ParseError,
            "Failed to parse JSON response during file upload: " +
                std::string(e.what()));
    }
}

// Error handling for CURL operations
void AstrometryClient::handle_curl_error(CURLcode result,
                                         const std::string& operation) {
    if (result == CURLE_OK) {
        return;
    }

    std::string error_message =
        "CURL error during " + operation + ": " + curl_easy_strerror(result);
    logger_->error(error_message);

    switch (result) {
        case CURLE_COULDNT_CONNECT:
        case CURLE_COULDNT_RESOLVE_HOST:
        case CURLE_COULDNT_RESOLVE_PROXY:
            throw AstrometryError(ErrorCode::NetworkError, error_message);
        case CURLE_OPERATION_TIMEDOUT:
            throw AstrometryError(ErrorCode::Timeout, error_message);
        default:
            throw AstrometryError(ErrorCode::NetworkError, error_message);
    }
}

// Error handling for API responses
void AstrometryClient::handle_response_error(const nlohmann::json& response,
                                             const std::string& operation) {
    std::string error_message = "API error during " + operation;

    if (response.contains("errormessage")) {
        error_message += ": " + response["errormessage"].get<std::string>();
    }

    logger_->error(error_message);
    throw AstrometryError(ErrorCode::ServerError, error_message);
}

// Validation
void AstrometryClient::validate_session() const {
    if (!is_logged_in()) {
        throw AstrometryError(ErrorCode::AuthenticationFailed, "Not logged in");
    }
}

// Set logger
void AstrometryClient::set_logger(std::shared_ptr<spdlog::logger> logger) {
    logger_ = logger;
}

// Get logger
std::shared_ptr<spdlog::logger> AstrometryClient::get_logger() const {
    return logger_;
}

// Set API URL
void AstrometryClient::set_api_url(const std::string& url) {
    config_.api_url = url;
}

// Get API URL
std::string AstrometryClient::get_api_url() const { return config_.api_url; }

}  // namespace astrometry