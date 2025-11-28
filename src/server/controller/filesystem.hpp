#ifndef LITHIUM_SERVER_CONTROLLER_FILESYSTEM_HPP
#define LITHIUM_SERVER_CONTROLLER_FILESYSTEM_HPP

#include "controller.hpp"
#include "../utils/response.hpp"
#include "atom/log/spdlog_logger.hpp"
#include "atom/type/json.hpp"

#include <filesystem>
#include <fstream>
#include <string>

namespace lithium::server::controller {

/**
 * @brief File System Operations Controller
 * 
 * Handles file and directory operations including listing, reading, writing,
 * deletion, moving, copying, and searching.
 */
class FilesystemController : public Controller {
public:
    void registerRoutes(lithium::server::ServerApp& app) override {
        // List Directory
        CROW_ROUTE(app, "/api/v1/filesystem/list")
            .methods("GET"_method)
            (&FilesystemController::listDirectory, this);

        // Get File/Directory Info
        CROW_ROUTE(app, "/api/v1/filesystem/info")
            .methods("GET"_method)
            (&FilesystemController::getInfo, this);

        // Read File
        CROW_ROUTE(app, "/api/v1/filesystem/read")
            .methods("GET"_method)
            (&FilesystemController::readFile, this);

        // Write File
        CROW_ROUTE(app, "/api/v1/filesystem/write")
            .methods("POST"_method)
            (&FilesystemController::writeFile, this);

        // Delete File/Directory
        CROW_ROUTE(app, "/api/v1/filesystem/delete")
            .methods("DELETE"_method)
            (&FilesystemController::deleteItem, this);

        // Move File/Directory
        CROW_ROUTE(app, "/api/v1/filesystem/move")
            .methods("POST"_method)
            (&FilesystemController::moveItem, this);

        // Copy File/Directory
        CROW_ROUTE(app, "/api/v1/filesystem/copy")
            .methods("POST"_method)
            (&FilesystemController::copyItem, this);

        // Create Directory
        CROW_ROUTE(app, "/api/v1/filesystem/mkdir")
            .methods("POST"_method)
            (&FilesystemController::makeDirectory, this);

        // Search Files
        CROW_ROUTE(app, "/api/v1/filesystem/search")
            .methods("GET"_method)
            (&FilesystemController::searchFiles, this);
    }

private:
    crow::response listDirectory(const crow::request& req) {
        using namespace utils;
        
        auto path_param = req.url_params.get("path");
        if (!path_param) {
            return ResponseBuilder::missingField("path");
        }

        std::string path = path_param;
        bool recursive = req.url_params.get("recursive") && 
                        std::string(req.url_params.get("recursive")) == "true";

        try {
            namespace fs = std::filesystem;
            
            if (!fs::exists(path)) {
                return ResponseBuilder::notFound("path", path);
            }

            if (!fs::is_directory(path)) {
                return ResponseBuilder::error("invalid_path", 
                    "The specified path is not a directory.", 400);
            }

            nlohmann::json items = nlohmann::json::array();
            
            for (const auto& entry : fs::directory_iterator(path)) {
                nlohmann::json item = {
                    {"name", entry.path().filename().string()},
                    {"type", entry.is_directory() ? "directory" : "file"}
                };

                if (entry.is_regular_file()) {
                    item["size"] = fs::file_size(entry.path());
                    auto ftime = fs::last_write_time(entry.path());
                    // Convert to time_t and format
                    item["modified"] = "2023-11-20T10:30:00Z"; // Simplified
                }

                items.push_back(item);
            }

            nlohmann::json data = {
                {"path", path},
                {"items", items},
                {"totalItems", items.size()}
            };

            return ResponseBuilder::success(data);
        } catch (const std::exception& e) {
            LOG_ERROR( "Failed to list directory: {}", e.what());
            return ResponseBuilder::internalError(e.what());
        }
    }

    crow::response getInfo(const crow::request& req) {
        using namespace utils;
        
        auto path_param = req.url_params.get("path");
        if (!path_param) {
            return ResponseBuilder::missingField("path");
        }

        std::string path = path_param;

        try {
            namespace fs = std::filesystem;
            
            if (!fs::exists(path)) {
                return ResponseBuilder::notFound("path", path);
            }

            nlohmann::json data = {
                {"path", path},
                {"type", fs::is_directory(path) ? "directory" : "file"},
                {"exists", true}
            };

            if (fs::is_regular_file(path)) {
                data["size"] = fs::file_size(path);
            }

            return ResponseBuilder::success(data);
        } catch (const std::exception& e) {
            return ResponseBuilder::internalError(e.what());
        }
    }

    crow::response readFile(const crow::request& req) {
        using namespace utils;
        
        auto path_param = req.url_params.get("path");
        if (!path_param) {
            return ResponseBuilder::missingField("path");
        }

        std::string path = path_param;

        try {
            namespace fs = std::filesystem;
            
            if (!fs::exists(path)) {
                return ResponseBuilder::notFound("file", path);
            }

            if (!fs::is_regular_file(path)) {
                return ResponseBuilder::error("invalid_path",
                    "The specified path is not a file.", 400);
            }

            std::ifstream file(path);
            if (!file.is_open()) {
                return ResponseBuilder::error("file_read_error",
                    "Failed to open file for reading.", 500);
            }

            std::string content((std::istreambuf_iterator<char>(file)),
                               std::istreambuf_iterator<char>());

            nlohmann::json data = {
                {"content", content},
                {"size", content.size()},
                {"truncated", false}
            };

            return ResponseBuilder::success(data);
        } catch (const std::exception& e) {
            return ResponseBuilder::internalError(e.what());
        }
    }

    crow::response writeFile(const crow::request& req) {
        using namespace utils;
        
        try {
            auto body = nlohmann::json::parse(req.body);
            
            if (!body.contains("path")) {
                return ResponseBuilder::missingField("path");
            }
            if (!body.contains("content")) {
                return ResponseBuilder::missingField("content");
            }

            std::string path = body["path"];
            std::string content = body["content"];
            bool overwrite = body.value("overwrite", false);
            bool createDirs = body.value("createDirectories", false);

            namespace fs = std::filesystem;

            if (fs::exists(path) && !overwrite) {
                return ResponseBuilder::error("file_exists",
                    "File already exists. Set overwrite to true to replace it.", 409);
            }

            if (createDirs) {
                fs::path dir = fs::path(path).parent_path();
                if (!dir.empty()) {
                    fs::create_directories(dir);
                }
            }

            std::ofstream file(path);
            if (!file.is_open()) {
                return ResponseBuilder::error("file_write_error",
                    "Failed to open file for writing.", 500);
            }

            file << content;
            file.close();

            return ResponseBuilder::successWithMessage("File written successfully.");
        } catch (const nlohmann::json::exception& e) {
            return ResponseBuilder::invalidJson(e.what());
        } catch (const std::exception& e) {
            return ResponseBuilder::internalError(e.what());
        }
    }

    crow::response deleteItem(const crow::request& req) {
        using namespace utils;
        
        auto path_param = req.url_params.get("path");
        if (!path_param) {
            return ResponseBuilder::missingField("path");
        }

        std::string path = path_param;
        bool recursive = req.url_params.get("recursive") && 
                        std::string(req.url_params.get("recursive")) == "true";

        try {
            namespace fs = std::filesystem;
            
            if (!fs::exists(path)) {
                return ResponseBuilder::notFound("path", path);
            }

            if (fs::is_directory(path) && !recursive) {
                return ResponseBuilder::error("directory_not_empty",
                    "Directory is not empty. Set recursive to true to delete it.", 400);
            }

            if (recursive) {
                fs::remove_all(path);
            } else {
                fs::remove(path);
            }

            return ResponseBuilder::successWithMessage("Item deleted successfully.");
        } catch (const std::exception& e) {
            return ResponseBuilder::internalError(e.what());
        }
    }

    crow::response moveItem(const crow::request& req) {
        using namespace utils;
        
        try {
            auto body = nlohmann::json::parse(req.body);
            
            if (!body.contains("source")) {
                return ResponseBuilder::missingField("source");
            }
            if (!body.contains("destination")) {
                return ResponseBuilder::missingField("destination");
            }

            std::string source = body["source"];
            std::string destination = body["destination"];
            bool overwrite = body.value("overwrite", false);

            namespace fs = std::filesystem;

            if (!fs::exists(source)) {
                return ResponseBuilder::notFound("source", source);
            }

            if (fs::exists(destination) && !overwrite) {
                return ResponseBuilder::error("destination_exists",
                    "Destination already exists. Set overwrite to true to replace it.", 409);
            }

            fs::rename(source, destination);

            return ResponseBuilder::successWithMessage("Item moved successfully.");
        } catch (const nlohmann::json::exception& e) {
            return ResponseBuilder::invalidJson(e.what());
        } catch (const std::exception& e) {
            return ResponseBuilder::internalError(e.what());
        }
    }

    crow::response copyItem(const crow::request& req) {
        using namespace utils;
        
        try {
            auto body = nlohmann::json::parse(req.body);
            
            if (!body.contains("source")) {
                return ResponseBuilder::missingField("source");
            }
            if (!body.contains("destination")) {
                return ResponseBuilder::missingField("destination");
            }

            std::string source = body["source"];
            std::string destination = body["destination"];
            bool overwrite = body.value("overwrite", false);

            namespace fs = std::filesystem;

            if (!fs::exists(source)) {
                return ResponseBuilder::notFound("source", source);
            }

            if (fs::exists(destination) && !overwrite) {
                return ResponseBuilder::error("destination_exists",
                    "Destination already exists. Set overwrite to true to replace it.", 409);
            }

            auto options = overwrite ? 
                fs::copy_options::overwrite_existing : 
                fs::copy_options::none;

            if (fs::is_directory(source)) {
                options |= fs::copy_options::recursive;
            }

            fs::copy(source, destination, options);

            return ResponseBuilder::successWithMessage("Item copied successfully.");
        } catch (const nlohmann::json::exception& e) {
            return ResponseBuilder::invalidJson(e.what());
        } catch (const std::exception& e) {
            return ResponseBuilder::internalError(e.what());
        }
    }

    crow::response makeDirectory(const crow::request& req) {
        using namespace utils;
        
        try {
            auto body = nlohmann::json::parse(req.body);
            
            if (!body.contains("path")) {
                return ResponseBuilder::missingField("path");
            }

            std::string path = body["path"];
            bool recursive = body.value("recursive", true);

            namespace fs = std::filesystem;

            if (fs::exists(path)) {
                return ResponseBuilder::error("path_exists",
                    "The specified path already exists.", 409);
            }

            if (recursive) {
                fs::create_directories(path);
            } else {
                fs::create_directory(path);
            }

            return ResponseBuilder::successWithMessage("Directory created successfully.");
        } catch (const nlohmann::json::exception& e) {
            return ResponseBuilder::invalidJson(e.what());
        } catch (const std::exception& e) {
            return ResponseBuilder::internalError(e.what());
        }
    }

    crow::response searchFiles(const crow::request& req) {
        using namespace utils;
        
        auto path_param = req.url_params.get("path");
        auto pattern_param = req.url_params.get("pattern");
        
        if (!path_param) {
            return ResponseBuilder::missingField("path");
        }
        if (!pattern_param) {
            return ResponseBuilder::missingField("pattern");
        }

        std::string path = path_param;
        std::string pattern = pattern_param;
        bool recursive = req.url_params.get("recursive") && 
                        std::string(req.url_params.get("recursive")) == "true";

        try {
            namespace fs = std::filesystem;
            
            if (!fs::exists(path)) {
                return ResponseBuilder::notFound("path", path);
            }

            nlohmann::json results = nlohmann::json::array();

            // Simple pattern matching (in production, use more sophisticated matching)
            auto searchFunc = recursive ? 
                fs::recursive_directory_iterator(path) : 
                fs::directory_iterator(path);

            for (const auto& entry : searchFunc) {
                if (entry.path().filename().string().find(pattern) != std::string::npos) {
                    nlohmann::json item = {
                        {"path", entry.path().string()},
                        {"type", entry.is_directory() ? "directory" : "file"}
                    };

                    if (entry.is_regular_file()) {
                        item["size"] = fs::file_size(entry.path());
                    }

                    results.push_back(item);
                }
            }

            nlohmann::json data = {
                {"results", results},
                {"totalResults", results.size()}
            };

            return ResponseBuilder::success(data);
        } catch (const std::exception& e) {
            return ResponseBuilder::internalError(e.what());
        }
    }
};

}  // namespace lithium::server::controller

#endif  // LITHIUM_SERVER_CONTROLLER_FILESYSTEM_HPP
