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

using ResponseBuilder = utils::ResponseBuilder;

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
        auto path_param = req.url_params.get("path");
        if (!path_param) {
            return ResponseBuilder::badRequest("Missing required field: path");
        }

        std::string path = path_param;
        bool recursive = req.url_params.get("recursive") &&
                        std::string(req.url_params.get("recursive")) == "true";

        try {
            namespace fs = std::filesystem;

            if (!fs::exists(path)) {
                return ResponseBuilder::notFound(path);
            }

            if (!fs::is_directory(path)) {
                return ResponseBuilder::badRequest("The specified path is not a directory");
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
        auto path_param = req.url_params.get("path");
        if (!path_param) {
            return ResponseBuilder::badRequest("Missing required field: path");
        }

        std::string path = path_param;

        try {
            namespace fs = std::filesystem;

            if (!fs::exists(path)) {
                return ResponseBuilder::notFound(path);
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
        auto path_param = req.url_params.get("path");
        if (!path_param) {
            return ResponseBuilder::badRequest("Missing required field: path");
        }

        std::string path = path_param;

        try {
            namespace fs = std::filesystem;

            if (!fs::exists(path)) {
                return ResponseBuilder::notFound(path);
            }

            if (!fs::is_regular_file(path)) {
                return ResponseBuilder::badRequest("The specified path is not a file");
            }

            std::ifstream file(path);
            if (!file.is_open()) {
                return ResponseBuilder::internalError("Failed to open file for reading");
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
        try {
            auto body = nlohmann::json::parse(req.body);

            if (!body.contains("path")) {
                return ResponseBuilder::badRequest("Missing required field: path");
            }
            if (!body.contains("content")) {
                return ResponseBuilder::badRequest("Missing required field: content");
            }

            std::string path = body["path"];
            std::string content = body["content"];
            bool overwrite = body.value("overwrite", false);
            bool createDirs = body.value("createDirectories", false);

            namespace fs = std::filesystem;

            if (fs::exists(path) && !overwrite) {
                return ResponseBuilder::conflict("File already exists. Set overwrite to true to replace it");
            }

            if (createDirs) {
                fs::path dir = fs::path(path).parent_path();
                if (!dir.empty()) {
                    fs::create_directories(dir);
                }
            }

            std::ofstream file(path);
            if (!file.is_open()) {
                return ResponseBuilder::internalError("Failed to open file for writing");
            }

            file << content;
            file.close();

            nlohmann::json data = {
                {"path", path},
                {"message", "File written successfully"}
            };
            return ResponseBuilder::success(data);
        } catch (const nlohmann::json::exception& e) {
            return ResponseBuilder::badRequest(std::string("Invalid JSON: ") + e.what());
        } catch (const std::exception& e) {
            return ResponseBuilder::internalError(e.what());
        }
    }

    crow::response deleteItem(const crow::request& req) {
        auto path_param = req.url_params.get("path");
        if (!path_param) {
            return ResponseBuilder::badRequest("Missing required field: path");
        }

        std::string path = path_param;
        bool recursive = req.url_params.get("recursive") &&
                        std::string(req.url_params.get("recursive")) == "true";

        try {
            namespace fs = std::filesystem;

            if (!fs::exists(path)) {
                return ResponseBuilder::notFound(path);
            }

            if (fs::is_directory(path) && !recursive) {
                return ResponseBuilder::badRequest("Directory is not empty. Set recursive to true to delete it");
            }

            if (recursive) {
                fs::remove_all(path);
            } else {
                fs::remove(path);
            }

            nlohmann::json data = {
                {"path", path},
                {"message", "Item deleted successfully"}
            };
            return ResponseBuilder::success(data);
        } catch (const std::exception& e) {
            return ResponseBuilder::internalError(e.what());
        }
    }

    crow::response moveItem(const crow::request& req) {
        try {
            auto body = nlohmann::json::parse(req.body);

            if (!body.contains("source")) {
                return ResponseBuilder::badRequest("Missing required field: source");
            }
            if (!body.contains("destination")) {
                return ResponseBuilder::badRequest("Missing required field: destination");
            }

            std::string source = body["source"];
            std::string destination = body["destination"];
            bool overwrite = body.value("overwrite", false);

            namespace fs = std::filesystem;

            if (!fs::exists(source)) {
                return ResponseBuilder::notFound(source);
            }

            if (fs::exists(destination) && !overwrite) {
                return ResponseBuilder::conflict("Destination already exists. Set overwrite to true to replace it");
            }

            fs::rename(source, destination);

            nlohmann::json data = {
                {"source", source},
                {"destination", destination},
                {"message", "Item moved successfully"}
            };
            return ResponseBuilder::success(data);
        } catch (const nlohmann::json::exception& e) {
            return ResponseBuilder::badRequest(std::string("Invalid JSON: ") + e.what());
        } catch (const std::exception& e) {
            return ResponseBuilder::internalError(e.what());
        }
    }

    crow::response copyItem(const crow::request& req) {
        try {
            auto body = nlohmann::json::parse(req.body);

            if (!body.contains("source")) {
                return ResponseBuilder::badRequest("Missing required field: source");
            }
            if (!body.contains("destination")) {
                return ResponseBuilder::badRequest("Missing required field: destination");
            }

            std::string source = body["source"];
            std::string destination = body["destination"];
            bool overwrite = body.value("overwrite", false);

            namespace fs = std::filesystem;

            if (!fs::exists(source)) {
                return ResponseBuilder::notFound(source);
            }

            if (fs::exists(destination) && !overwrite) {
                return ResponseBuilder::conflict("Destination already exists. Set overwrite to true to replace it");
            }

            auto options = overwrite ?
                fs::copy_options::overwrite_existing :
                fs::copy_options::none;

            if (fs::is_directory(source)) {
                options |= fs::copy_options::recursive;
            }

            fs::copy(source, destination, options);

            nlohmann::json data = {
                {"source", source},
                {"destination", destination},
                {"message", "Item copied successfully"}
            };
            return ResponseBuilder::success(data);
        } catch (const nlohmann::json::exception& e) {
            return ResponseBuilder::badRequest(std::string("Invalid JSON: ") + e.what());
        } catch (const std::exception& e) {
            return ResponseBuilder::internalError(e.what());
        }
    }

    crow::response makeDirectory(const crow::request& req) {
        try {
            auto body = nlohmann::json::parse(req.body);

            if (!body.contains("path")) {
                return ResponseBuilder::badRequest("Missing required field: path");
            }

            std::string path = body["path"];
            bool recursive = body.value("recursive", true);

            namespace fs = std::filesystem;

            if (fs::exists(path)) {
                return ResponseBuilder::conflict("The specified path already exists");
            }

            if (recursive) {
                fs::create_directories(path);
            } else {
                fs::create_directory(path);
            }

            nlohmann::json data = {
                {"path", path},
                {"message", "Directory created successfully"}
            };
            return ResponseBuilder::success(data);
        } catch (const nlohmann::json::exception& e) {
            return ResponseBuilder::badRequest(std::string("Invalid JSON: ") + e.what());
        } catch (const std::exception& e) {
            return ResponseBuilder::internalError(e.what());
        }
    }

    crow::response searchFiles(const crow::request& req) {
        auto path_param = req.url_params.get("path");
        auto pattern_param = req.url_params.get("pattern");

        if (!path_param) {
            return ResponseBuilder::badRequest("Missing required field: path");
        }
        if (!pattern_param) {
            return ResponseBuilder::badRequest("Missing required field: pattern");
        }

        std::string path = path_param;
        std::string pattern = pattern_param;
        bool recursive = req.url_params.get("recursive") &&
                        std::string(req.url_params.get("recursive")) == "true";

        try {
            namespace fs = std::filesystem;

            if (!fs::exists(path)) {
                return ResponseBuilder::notFound(path);
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
