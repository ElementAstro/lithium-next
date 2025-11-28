#ifndef LITHIUM_SERVER_CONTROLLER_SKY_HPP
#define LITHIUM_SERVER_CONTROLLER_SKY_HPP

#include "controller.hpp"
#include "../utils/response.hpp"
#include "atom/log/spdlog_logger.hpp"
#include "atom/type/json.hpp"
#include "server/command/solver.hpp"

namespace lithium::server::controller {

/**
 * @brief Sky Atlas and Astronomical Utilities Controller
 * 
 * Handles celestial object search, name resolution, and plate solving operations.
 */
class SkyController : public Controller {
public:
    void registerRoutes(lithium::server::ServerApp& app) override {
        // Resolve Object Name
        CROW_ROUTE(app, "/api/v1/sky/resolve")
            .methods("GET"_method)
            ([this](const crow::request& req) {
                return this->resolveObjectName(req);
            });

        // Basic Sky Object Search
        CROW_ROUTE(app, "/api/v1/sky/search")
            .methods("GET"_method)
            ([this](const crow::request& req) {
                return this->searchObjects(req);
            });

        // Advanced Sky Object Search
        CROW_ROUTE(app, "/api/v1/sky/search/advanced")
            .methods("POST"_method)
            ([this](const crow::request& req) {
                return this->advancedSearch(req);
            });

        // Plate Solve Image
        CROW_ROUTE(app, "/api/v1/plate-solve")
            .methods("POST"_method)
            ([this](const crow::request& req) {
                return this->plateSolve(req);
            });
    }

private:
    crow::response resolveObjectName(const crow::request& req) {
        using namespace utils;
        
        auto name_param = req.url_params.get("name");
        if (!name_param) {
            return ResponseBuilder::missingField("name");
        }

        std::string name = name_param;

        // Simulate object resolution
        nlohmann::json data = {
            {"name", "Andromeda Galaxy"},
            {"ra", "00:42:44.3"},
            {"dec", "+41:16:09"}
        };

        return ResponseBuilder::success(data);
    }

    crow::response searchObjects(const crow::request& req) {
        using namespace utils;
        
        // Parse query parameters
        int limit = 50;
        if (auto limit_param = req.url_params.get("limit")) {
            limit = std::stoi(limit_param);
        }

        nlohmann::json data = {
            {"results", nlohmann::json::array({
                {
                    {"id", "M31"},
                    {"name", "Andromeda Galaxy"},
                    {"alternateNames", nlohmann::json::array({"NGC 224"})},
                    {"type", "Galaxy"},
                    {"ra", "00:42:44.3"},
                    {"dec", "+41:16:09"},
                    {"magnitude", 3.4},
                    {"constellation", "Andromeda"},
                    {"catalog", "messier"}
                }
            })},
            {"totalResults", 1},
            {"limit", limit},
            {"offset", 0},
            {"hasMore", false}
        };

        return ResponseBuilder::success(data);
    }

    crow::response advancedSearch(const crow::request& req) {
        using namespace utils;
        
        try {
            auto filters = nlohmann::json::parse(req.body);

            nlohmann::json data = {
                {"results", nlohmann::json::array()},
                {"totalResults", 0},
                {"limit", 100},
                {"offset", 0},
                {"hasMore", false},
                {"appliedFilters", filters.value("filters", nlohmann::json::object())}
            };

            return ResponseBuilder::success(data);
        } catch (const nlohmann::json::exception& e) {
            return ResponseBuilder::invalidJson(e.what());
        }
    }

    crow::response plateSolve(const crow::request& req) {
        using namespace utils;
        
        try {
            auto body = nlohmann::json::parse(req.body);
            
            if (!body.contains("filePath")) {
                return ResponseBuilder::missingField("filePath");
            }

            std::string filePath = body["filePath"];
            double ra = body.value("ra", 0.0);
            double dec = body.value("dec", 0.0);
            double scale = body.value("scale", 0.0);
            double radius = body.value("radius", 180.0);

            auto result = lithium::middleware::solveImage(filePath, ra, dec, scale, radius);
            
            // Assuming solveImage returns standard JSON structure
            if (result["status"] == "success") {
                return ResponseBuilder::success(result["data"]);
            } else {
                std::string msg = result.value("message", "Solving failed");
                if (result.contains("error")) {
                     msg = result["error"].value("message", msg);
                }
                return ResponseBuilder::error("solver_error", msg, 500);
            }

        } catch (const nlohmann::json::exception& e) {
            return ResponseBuilder::invalidJson(e.what());
        }
    }
};

}  // namespace lithium::server::controller

#endif  // LITHIUM_SERVER_CONTROLLER_SKY_HPP
