#ifndef LITHIUM_SERVER_COMMAND_SOLVER_HPP
#define LITHIUM_SERVER_COMMAND_SOLVER_HPP

#include "atom/type/json.hpp"
#include <string>

namespace lithium::middleware {

using json = nlohmann::json;

/**
 * @brief Solve an image using a plate solver (e.g. ASTAP)
 * @param filePath Path to the image file
 * @param raHint Approximate RA (degrees) - optional, default 0
 * @param decHint Approximate Dec (degrees) - optional, default 0
 * @param scaleHint Approximate pixel scale (arcsec/pixel) - optional, default 0
 * @param radiusHint Search radius (degrees) - optional, default 180 (blind)
 */
auto solveImage(const std::string& filePath, 
                double raHint = 0.0, 
                double decHint = 0.0, 
                double scaleHint = 0.0, 
                double radiusHint = 180.0) -> json;

/**
 * @brief Blind solve an image without hints
 */
auto blindSolve(const std::string& filePath) -> json;

}  // namespace lithium::middleware

#endif // LITHIUM_SERVER_COMMAND_SOLVER_HPP
