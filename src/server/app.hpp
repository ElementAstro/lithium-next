#ifndef LITHIUM_SERVER_APP_HPP
#define LITHIUM_SERVER_APP_HPP

#include <crow.h>

#include "middleware/auth.hpp"

namespace lithium::server {

// Central HTTP application type with authentication, CORS and request logging
using ServerApp = crow::App<
    middleware::CORS,
    middleware::ApiKeyAuth,
    middleware::RequestLogger>;

}  // namespace lithium::server

#endif  // LITHIUM_SERVER_APP_HPP
