#ifndef LITHIUM_TASK_COMPONENTS_SOLVER_TASK_HPP
#define LITHIUM_TASK_COMPONENTS_SOLVER_TASK_HPP

#include "../core/task.hpp"
#include "atom/log/spdlog_logger.hpp"
#include "server/command/solver.hpp"

namespace lithium::task {

class SolverTask : public Task {
public:
    SolverTask(std::string name, const json& config)
        : Task(std::move(name),
               [this](const json& params) { this->runSolver(params); }) {
        // Initialize from config if needed
    }

private:
    void runSolver(const json& params) {
        try {
            std::string filePath;
            if (params.contains("filePath")) {
                filePath = params["filePath"];
            } else {
                throw std::invalid_argument("Missing filePath parameter");
            }

            double ra = params.value("ra", 0.0);
            double dec = params.value("dec", 0.0);
            double scale = params.value("scale", 0.0);
            double radius = params.value("radius", 180.0);

            LOG_INFO("SolverTask: Starting solve for %s", filePath.c_str());

            auto result = lithium::middleware::solveImage(filePath, ra, dec,
                                                          scale, radius);

            if (result["status"] != "success") {
                std::string errorMsg = "Solving failed";
                if (result.contains("error")) {
                    errorMsg = result["error"].value("message", errorMsg);
                } else if (result.contains("message")) {
                    errorMsg = result["message"];
                }
                throw std::runtime_error(errorMsg);
            }

            LOG_INFO("SolverTask: Solved successfully.");

        } catch (const std::exception& e) {
            LOG_ERROR("SolverTask exception: %s", e.what());
            throw;  // Re-throw to let Task handle the failure state
        }
    }
};

}  // namespace lithium::task

#endif  // LITHIUM_TASK_COMPONENTS_SOLVER_TASK_HPP
