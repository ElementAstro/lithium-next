#ifndef LITHIUM_SERVER_APP_HPP
#define LITHIUM_SERVER_APP_HPP

#include <crow.h>

#include "middleware/auth.hpp"

namespace lithium::server {

/**
 * @brief Central HTTP application type with middleware stack
 *
 * Middleware execution order (before_handle):
 *   1. CORS - Handle preflight OPTIONS requests
 *   2. RateLimiterMiddleware - Prevent brute force attacks (BEFORE auth)
 *   3. ApiKeyAuth - Validate API key authentication
 *   4. RequestLogger - Log request timing
 *
 * Note: after_handle runs in reverse order
 */
using ServerApp = crow::App<middleware::CORS, middleware::RateLimiterMiddleware,
                            middleware::ApiKeyAuth, middleware::RequestLogger>;

}  // namespace lithium::server

#endif  // LITHIUM_SERVER_APP_HPP
