#ifndef LITHIUM_CONFIG_JSON5_HPP
#define LITHIUM_CONFIG_JSON5_HPP

#include <stdexcept>
#include <string>

namespace lithium {
namespace internal {

class JSON5ParseError : public std::runtime_error {
public:
  explicit JSON5ParseError(const std::string &msg) : std::runtime_error(msg) {}
};

inline auto removeComments(const std::string &json5) -> std::string {
  if (json5.empty()) {
    return json5;
  }

  std::string result;
  result.reserve(json5.size());
  bool inSingleLineComment = false;
  bool inMultiLineComment = false;
  bool inString = false;

  try {
    for (size_t i = 0; i < json5.size(); ++i) {
      // 字符串处理
      if (!inSingleLineComment && !inMultiLineComment && json5[i] == '"') {
        inString = !inString;
        result += json5[i];
        continue;
      }

      if (inString) {
        result += json5[i];
        continue;
      }

      // 注释处理
      if (!inMultiLineComment && !inSingleLineComment && i + 1 < json5.size()) {
        if (json5[i] == '/' && json5[i + 1] == '/') {
          inSingleLineComment = true;
          ++i;
          continue;
        }
        if (json5[i] == '/' && json5[i + 1] == '*') {
          inMultiLineComment = true;
          ++i;
          continue;
        }
      }

      if (inSingleLineComment) {
        if (json5[i] == '\n') {
          inSingleLineComment = false;
          result += '\n';
        }
        continue;
      }

      if (inMultiLineComment) {
        if (i + 1 < json5.size() && json5[i] == '*' && json5[i + 1] == '/') {
          inMultiLineComment = false;
          ++i;
        }
        continue;
      }

      result += json5[i];
    }

    if (inString) {
      throw JSON5ParseError("Unterminated string");
    }
    if (inMultiLineComment) {
      throw JSON5ParseError("Unterminated multi-line comment");
    }

    return result;

  } catch (const std::exception &e) {
    throw JSON5ParseError(std::string("JSON5 parse error: ") + e.what());
  }
}

inline auto convertJSON5toJSON(const std::string &json5) -> std::string {
  try {
    std::string json = removeComments(json5);
    if (json.empty()) {
      return json;
    }

    std::string result;
    result.reserve(json.size() * 1.2); // 预分配空间
    bool inString = false;

    for (size_t i = 0; i < json.size(); ++i) {
      if (json[i] == '"') {
        inString = !inString;
        result += json[i];
        continue;
      }

      if (inString) {
        result += json[i];
        continue;
      }

      // 处理未加引号的键名
      if (!inString && std::isalpha(json[i]) || json[i] == '_') {
        size_t start = i;
        while (i < json.size() &&
               (std::isalnum(json[i]) || json[i] == '_' || json[i] == '-')) {
          ++i;
        }
        result += '"' + json.substr(start, i - start) + '"';
        --i;
        continue;
      }

      result += json[i];
    }

    if (inString) {
      throw JSON5ParseError("Unterminated string in JSON5");
    }

    return result;

  } catch (const std::exception &e) {
    throw JSON5ParseError(std::string("JSON5 to JSON conversion error: ") +
                          e.what());
  }
}

} // namespace internal
} // namespace lithium

#endif