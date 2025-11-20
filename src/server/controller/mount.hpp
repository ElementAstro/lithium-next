#ifndef LITHIUM_SERVER_CONTROLLER_MOUNT_HPP
#define LITHIUM_SERVER_CONTROLLER_MOUNT_HPP

#include <crow/json.h>
#include "controller.hpp"

#include <string>

#include "atom/log/loguru.hpp"
#include "atom/type/json.hpp"
#include "server/command/mount.hpp"

class MountController : public Controller {
private:
    static auto makeJsonResponse(const nlohmann::json &body, int code)
        -> crow::response {
        crow::response res(code);
        res.set_header("Content-Type", "application/json");
        res.write(body.dump());
        return res;
    }

    static auto makeDeviceNotFound(const std::string &deviceId)
        -> nlohmann::json {
        return {
            {"status", "error"},
            {"error",
             {{"code", "device_not_found"},
              {"message", "Mount not found"},
              {"details", {{"deviceId", deviceId}}}}}
        };
    }

    static auto isValidDeviceId(const std::string &deviceId) -> bool {
        // Currently only a single primary mount is supported.
        return deviceId == "mnt-001";
    }

public:
    void registerRoutes(crow::SimpleApp &app) override {
        CROW_ROUTE(app, "/api/v1/mounts")
            .methods("GET"_method)(&MountController::listMountsRoute, this);

        CROW_ROUTE(app, "/api/v1/mounts/<string>")
            .methods("GET"_method)(&MountController::getMountStatusRoute, this);

        CROW_ROUTE(app, "/api/v1/mounts/<string>/connect")
            .methods("POST"_method)(&MountController::connectMountRoute, this);

        CROW_ROUTE(app, "/api/v1/mounts/<string>/slew")
            .methods("POST"_method)(&MountController::slewMountRoute, this);

        CROW_ROUTE(app, "/api/v1/mounts/<string>/tracking")
            .methods("PUT"_method)(&MountController::setTrackingRoute, this);

        CROW_ROUTE(app, "/api/v1/mounts/<string>/position")
            .methods("POST"_method)(&MountController::setMountPositionRoute,
                                      this);

        CROW_ROUTE(app, "/api/v1/mounts/<string>/pulse-guide")
            .methods("POST"_method)(&MountController::pulseGuideRoute, this);

        CROW_ROUTE(app, "/api/v1/mounts/<string>/sync")
            .methods("POST"_method)(&MountController::syncMountRoute, this);

        CROW_ROUTE(app, "/api/v1/mounts/<string>/capabilities")
            .methods("GET"_method)(&MountController::capabilitiesRoute, this);

        CROW_ROUTE(app, "/api/v1/mounts/<string>/guide-rates")
            .methods("PUT"_method)(&MountController::guideRatesRoute, this);

        CROW_ROUTE(app, "/api/v1/mounts/<string>/tracking-rate")
            .methods("PUT"_method)(&MountController::trackingRateRoute, this);

        CROW_ROUTE(app, "/api/v1/mounts/<string>/stop")
            .methods("POST"_method)(&MountController::stopRoute, this);

        CROW_ROUTE(app, "/api/v1/mounts/<string>/pier-side")
            .methods("GET"_method)(&MountController::pierSideRoute, this);

        CROW_ROUTE(app, "/api/v1/mounts/<string>/meridian-flip")
            .methods("POST"_method)(&MountController::meridianFlipRoute, this);
    }

    void listMountsRoute(const crow::request &req, crow::response &res) {
        (void)req;
        auto body = lithium::middleware::listMounts();
        res = makeJsonResponse(body, 200);
    }

    void getMountStatusRoute(const crow::request &req, crow::response &res,
                             const std::string &deviceId) {
        (void)req;
        if (!isValidDeviceId(deviceId)) {
            auto err = makeDeviceNotFound(deviceId);
            res = makeJsonResponse(err, 404);
            return;
        }
        auto body = lithium::middleware::getMountStatus(deviceId);
        res = makeJsonResponse(body, 200);
    }

    void connectMountRoute(const crow::request &req, crow::response &res,
                           const std::string &deviceId) {
        if (!isValidDeviceId(deviceId)) {
            auto err = makeDeviceNotFound(deviceId);
            res = makeJsonResponse(err, 404);
            return;
        }

        try {
            auto body = nlohmann::json::parse(req.body);
            bool connected = body.value("connected", true);
            auto result =
                lithium::middleware::connectMount(deviceId, connected);
            res = makeJsonResponse(result, 200);
        } catch (const std::exception &e) {
            nlohmann::json err = {{"status", "error"},
                                  {"error",
                                   {{"code", "invalid_json"},
                                    {"message", e.what()}}}};
            res = makeJsonResponse(err, 400);
        }
    }

    void slewMountRoute(const crow::request &req, crow::response &res,
                        const std::string &deviceId) {
        if (!isValidDeviceId(deviceId)) {
            auto err = makeDeviceNotFound(deviceId);
            res = makeJsonResponse(err, 404);
            return;
        }

        try {
            auto body = nlohmann::json::parse(req.body);
            std::string ra = body.value("ra", "");
            std::string dec = body.value("dec", "");
            auto result = lithium::middleware::slewMount(deviceId, ra, dec);
            res = makeJsonResponse(result, 202);
        } catch (const std::exception &e) {
            nlohmann::json err = {{"status", "error"},
                                  {"error",
                                   {{"code", "invalid_json"},
                                    {"message", e.what()}}}};
            res = makeJsonResponse(err, 400);
        }
    }

    void setTrackingRoute(const crow::request &req, crow::response &res,
                          const std::string &deviceId) {
        if (!isValidDeviceId(deviceId)) {
            auto err = makeDeviceNotFound(deviceId);
            res = makeJsonResponse(err, 404);
            return;
        }

        try {
            auto body = nlohmann::json::parse(req.body);
            bool tracking = body.value("tracking", true);
            auto result =
                lithium::middleware::setTracking(deviceId, tracking);
            res = makeJsonResponse(result, 200);
        } catch (const std::exception &e) {
            nlohmann::json err = {{"status", "error"},
                                  {"error",
                                   {{"code", "invalid_json"},
                                    {"message", e.what()}}}};
            res = makeJsonResponse(err, 400);
        }
    }

    void setMountPositionRoute(const crow::request &req, crow::response &res,
                               const std::string &deviceId) {
        if (!isValidDeviceId(deviceId)) {
            auto err = makeDeviceNotFound(deviceId);
            res = makeJsonResponse(err, 404);
            return;
        }

        try {
            auto body = nlohmann::json::parse(req.body);
            std::string command = body.value("command", "");
            auto result = lithium::middleware::setMountPosition(deviceId,
                                                                 command);
            res = makeJsonResponse(result, 202);
        } catch (const std::exception &e) {
            nlohmann::json err = {{"status", "error"},
                                  {"error",
                                   {{"code", "invalid_json"},
                                    {"message", e.what()}}}};
            res = makeJsonResponse(err, 400);
        }
    }

    void pulseGuideRoute(const crow::request &req, crow::response &res,
                         const std::string &deviceId) {
        if (!isValidDeviceId(deviceId)) {
            auto err = makeDeviceNotFound(deviceId);
            res = makeJsonResponse(err, 404);
            return;
        }

        try {
            auto body = nlohmann::json::parse(req.body);
            std::string direction = body.value("direction", "");
            int duration = body.value("duration", 0);
            auto result = lithium::middleware::pulseGuide(deviceId, direction,
                                                           duration);
            res = makeJsonResponse(result, 202);
        } catch (const std::exception &e) {
            nlohmann::json err = {{"status", "error"},
                                  {"error",
                                   {{"code", "invalid_json"},
                                    {"message", e.what()}}}};
            res = makeJsonResponse(err, 400);
        }
    }

    void syncMountRoute(const crow::request &req, crow::response &res,
                        const std::string &deviceId) {
        if (!isValidDeviceId(deviceId)) {
            auto err = makeDeviceNotFound(deviceId);
            res = makeJsonResponse(err, 404);
            return;
        }

        try {
            auto body = nlohmann::json::parse(req.body);
            std::string ra = body.value("ra", "");
            std::string dec = body.value("dec", "");
            auto result = lithium::middleware::syncMount(deviceId, ra, dec);
            res = makeJsonResponse(result, 200);
        } catch (const std::exception &e) {
            nlohmann::json err = {{"status", "error"},
                                  {"error",
                                   {{"code", "invalid_json"},
                                    {"message", e.what()}}}};
            res = makeJsonResponse(err, 400);
        }
    }

    void capabilitiesRoute(const crow::request &req, crow::response &res,
                           const std::string &deviceId) {
        (void)req;
        if (!isValidDeviceId(deviceId)) {
            auto err = makeDeviceNotFound(deviceId);
            res = makeJsonResponse(err, 404);
            return;
        }
        auto result =
            lithium::middleware::getMountCapabilities(deviceId);
        res = makeJsonResponse(result, 200);
    }

    void guideRatesRoute(const crow::request &req, crow::response &res,
                         const std::string &deviceId) {
        if (!isValidDeviceId(deviceId)) {
            auto err = makeDeviceNotFound(deviceId);
            res = makeJsonResponse(err, 404);
            return;
        }

        try {
            auto body = nlohmann::json::parse(req.body);
            double raRate = body.value("raRate", 0.5);
            double decRate = body.value("decRate", 0.5);
            auto result = lithium::middleware::setGuideRates(deviceId, raRate,
                                                             decRate);
            res = makeJsonResponse(result, 200);
        } catch (const std::exception &e) {
            nlohmann::json err = {{"status", "error"},
                                  {"error",
                                   {{"code", "invalid_json"},
                                    {"message", e.what()}}}};
            res = makeJsonResponse(err, 400);
        }
    }

    void trackingRateRoute(const crow::request &req, crow::response &res,
                           const std::string &deviceId) {
        if (!isValidDeviceId(deviceId)) {
            auto err = makeDeviceNotFound(deviceId);
            res = makeJsonResponse(err, 404);
            return;
        }

        try {
            auto body = nlohmann::json::parse(req.body);
            std::string rate = body.value("rate", "Sidereal");
            auto result =
                lithium::middleware::setTrackingRate(deviceId, rate);
            res = makeJsonResponse(result, 200);
        } catch (const std::exception &e) {
            nlohmann::json err = {{"status", "error"},
                                  {"error",
                                   {{"code", "invalid_json"},
                                    {"message", e.what()}}}};
            res = makeJsonResponse(err, 400);
        }
    }

    void stopRoute(const crow::request &req, crow::response &res,
                   const std::string &deviceId) {
        (void)req;
        if (!isValidDeviceId(deviceId)) {
            auto err = makeDeviceNotFound(deviceId);
            res = makeJsonResponse(err, 404);
            return;
        }
        auto result = lithium::middleware::stopMount(deviceId);
        res = makeJsonResponse(result, 200);
    }

    void pierSideRoute(const crow::request &req, crow::response &res,
                       const std::string &deviceId) {
        (void)req;
        if (!isValidDeviceId(deviceId)) {
            auto err = makeDeviceNotFound(deviceId);
            res = makeJsonResponse(err, 404);
            return;
        }
        auto result = lithium::middleware::getPierSide(deviceId);
        res = makeJsonResponse(result, 200);
    }

    void meridianFlipRoute(const crow::request &req, crow::response &res,
                           const std::string &deviceId) {
        (void)req;
        if (!isValidDeviceId(deviceId)) {
            auto err = makeDeviceNotFound(deviceId);
            res = makeJsonResponse(err, 404);
            return;
        }
        auto result = lithium::middleware::performMeridianFlip(deviceId);
        res = makeJsonResponse(result, 202);
    }
};

#endif  // LITHIUM_SERVER_CONTROLLER_MOUNT_HPP
