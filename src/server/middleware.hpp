#ifndef MIDDLEWARE_H
#define MIDDLEWARE_H

#include <crow.h>
#include <string>

#include "atom/log/loguru.hpp"

const std::string ADMIN_IP = "192.168.1.100";

struct AdminAreaGuard {
    struct context {};

    void before_handle(crow::request &req, crow::response &res, context &ctx) {
        // 检查请求的 IP 地址
        if (req.remote_ip_address != ADMIN_IP) {
            res.code = 403;  // 禁止访问
            res.end();
            LOG_F(WARNING, "Access denied for IP: {}", req.remote_ip_address);
        }
    }

    void after_handle(crow::request &req, crow::response &res, context &ctx) {
        // 在响应后可以执行的额外操作
    }
};

// 局部中间件：RequestLogger
struct RequestLogger : crow::ILocalMiddleware {
    struct context {};

    void before_handle(crow::request &req, crow::response &res, context &ctx) {
        LOG_F(INFO, "Request received: {}", req.url);
    }

    void after_handle(crow::request &req, crow::response &res, context &ctx) {
        LOG_F(INFO, "Response sent for: {}", req.url);
    }
};

#endif  // MIDDLEWARE_H