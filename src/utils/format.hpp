#include <crow/json.h>
#include <format>
#include <optional>

namespace std {
template <typename T>
struct formatter<std::optional<T>> {
    // 设置默认的格式
    char presentation = 'g';  // 'g' 是默认的输出格式，可以根据需求更改

    // 解析格式字符串
    constexpr auto parse(format_parse_context& ctx) {
        auto it = ctx.begin(), end = ctx.end();
        if (it != end && (*it == 'n')) {
            presentation = 'n';  // 如果遇到 'n'，则输出 "none" 表示空值
            ++it;
        }
        return it;
    }

    // 格式化输出
    template <typename FormatContext>
    auto format(const std::optional<T>& opt, FormatContext& ctx) const {
        if (opt) {
            // 如果 optional 有值，则格式化其内部值
            return format_to(ctx.out(), "{}", *opt);
        } else {
            // 如果 optional 没有值，输出 "none" 或 "-"，取决于 format
            // 解析的情况
            if (presentation == 'n') {
                return format_to(ctx.out(), "none");
            } else {
                return format_to(ctx.out(), "-");
            }
        }
    }
};
}  // namespace std

namespace std {
template <>
struct formatter<crow::json::detail::r_string> : formatter<std::string> {
    constexpr auto parse(format_parse_context& ctx) -> decltype(ctx.begin()) {
        return ctx.begin();
    }

    template <typename FormatContext>
    auto format(const crow::json::detail::r_string& r,
                FormatContext& ctx) const {
        return formatter<std::string>::format(static_cast<std::string>(r), ctx);
    }
};
}  // namespace std