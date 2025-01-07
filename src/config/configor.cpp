/*
 * configor.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2023-4-30

Description: Configor

**************************************************/

#include "configor.hpp"
#include "json5.hpp"

#include <fstream>
#include <mutex>
#include <ranges>
#include <shared_mutex>

#include "atom/function/global_ptr.hpp"
#include "atom/io/io.hpp"
#include "atom/log/loguru.hpp"
#include "atom/system/env.hpp"
#include "atom/type/json.hpp"

#include "constant/constant.hpp"

using json = nlohmann::json;

namespace lithium {

class ConfigManagerImpl {
public:
  template <typename ValueType>
  auto setOrAppendImpl(const std::string &key_path, ValueType &&value,
                       bool append) -> bool {
    std::unique_lock lock(rwMutex);

    // Check if the key_path is "/" and set the root value directly
    if (key_path == "/") {
      if (append) {
        if (!config.is_array()) {
          config = json::array();
        }
        config.push_back(std::forward<ValueType>(value));
      } else {
        config = std::forward<ValueType>(value);
      }
      LOG_F(INFO, "Set root config: {}", config.dump());
      return true;
    }

    json *p = &config;
    auto keys = key_path | std::views::split('/');

    for (auto it = keys.begin(); it != keys.end(); ++it) {
      std::string keyStr = std::string((*it).begin(), (*it).end());
      LOG_F(INFO, "Set config: {}", keyStr);

      if (std::next(it) == keys.end()) { // If this is the last key
        if (append) {
          if (!p->contains(keyStr)) {
            (*p)[keyStr] = json::array();
          }
          if (!(*p)[keyStr].is_array()) {
            LOG_F(ERROR, "Target key is not an array");
            return false;
          }
          (*p)[keyStr].push_back(std::forward<ValueType>(value));
        } else {
          (*p)[keyStr] = std::forward<ValueType>(value);
        }
        LOG_F(INFO, "Final config: {}", config.dump());
        return true;
      }

      if (!p->contains(keyStr) || !(*p)[keyStr].is_object()) {
        (*p)[keyStr] = json::object();
      }
      p = &(*p)[keyStr];
    }
    return false;
  }

  mutable std::shared_mutex rwMutex;
  json config;
  std::thread ioThread;
};

ConfigManager::ConfigManager() : impl_(std::make_unique<ConfigManagerImpl>()) {
  LOG_F(INFO, "ConfigManager created.");
}

ConfigManager::~ConfigManager() {
  if (impl_->ioThread.joinable()) {
    impl_->ioThread.join();
  }
  if (saveAll("./")) {
    DLOG_F(INFO, "Config saved successfully.");
  } else {
    DLOG_F(ERROR, "Failed to save config on destruction.");
  }
}

auto ConfigManager::createShared() -> std::shared_ptr<ConfigManager> {
  static std::shared_ptr<ConfigManager> instance =
      std::make_shared<ConfigManager>();
  return instance;
}

auto ConfigManager::createUnique() -> std::unique_ptr<ConfigManager> {
  return std::make_unique<ConfigManager>();
}

auto ConfigManager::loadFromFile(const fs::path &path) -> bool {
  std::shared_lock lock(impl_->rwMutex);
  try {
    std::ifstream ifs(path);
    if (!ifs || ifs.peek() == std::ifstream::traits_type::eof()) {
      LOG_F(ERROR, "Failed to open file: {}", path.string());
      return false;
    }
    json j = json::parse(ifs);
    if (j.empty()) {
      LOG_F(WARNING, "Config file is empty: {}", path.string());
      return false;
    }
    std::string filename = path.stem().string();
    impl_->config[filename] = j;
    LOG_F(INFO, "Config loaded from file: {}", path.string());
    return true;
  } catch (const json::exception &e) {
    LOG_F(ERROR, "Failed to parse file: {}, error message: {}", path.string(),
          e.what());
  } catch (const std::exception &e) {
    LOG_F(ERROR, "Failed to load config file: {}, error message: {}",
          path.string(), e.what());
  }
  return false;
}

auto ConfigManager::loadFromDir(const fs::path &dir_path, bool recursive)
    -> bool {
  std::shared_lock lock(impl_->rwMutex);
  try {
    for (const auto &entry : fs::directory_iterator(dir_path)) {
      if (entry.is_regular_file()) {
        if (entry.path().extension() == ".json" ||
            entry.path().extension() == ".lithium") {
          if (!loadFromFile(entry.path())) {
            LOG_F(WARNING, "Failed to load config file: {}",
                  entry.path().string());
          }
        } else if (entry.path().extension() == ".json5" ||
                   entry.path().extension() == ".lithium5") {
          std::ifstream ifs(entry.path());
          if (!ifs || ifs.peek() == std::ifstream::traits_type::eof()) {
            LOG_F(ERROR, "Failed to open file: {}", entry.path().string());
            return false;
          }
          std::string json5((std::istreambuf_iterator<char>(ifs)),
                            std::istreambuf_iterator<char>());
          json j = json::parse(internal::convertJSON5toJSON(json5));
          if (j.empty()) {
            LOG_F(WARNING, "Config file is empty: {}", entry.path().string());
            return false;
          }
          std::string filename = entry.path().stem().string();
          impl_->config[filename] = j;
        }
      } else if (recursive && entry.is_directory()) {
        loadFromDir(entry.path(), true);
      }
    }
    LOG_F(INFO, "Config loaded from directory: {}", dir_path.string());
    return true;
  } catch (const std::exception &e) {
    LOG_F(ERROR, "Failed to load config file from: {}, error message: {}",
          dir_path.string(), e.what());
    return false;
  }
}

auto ConfigManager::save(const fs::path &file_path) const -> bool {
  std::unique_lock lock(impl_->rwMutex);
  std::ofstream ofs(file_path);
  if (!ofs) {
    LOG_F(ERROR, "Failed to open file: {}", file_path.string());
    return false;
  }
  try {
    std::string filename = file_path.stem().string();
    if (impl_->config.contains(filename)) {
      ofs << impl_->config[filename].dump(4);
      ofs.close();
      LOG_F(INFO, "Config saved to file: {}", file_path.string());
      return true;
    } else {
      LOG_F(ERROR, "Config for file: {} not found", file_path.string());
      return false;
    }
  } catch (const std::exception &e) {
    LOG_F(ERROR, "Failed to save config to file: {}, error message: {}",
          file_path.string(), e.what());
    return false;
  }
}

auto ConfigManager::saveAll(const fs::path &dir_path) const -> bool {
  std::unique_lock lock(impl_->rwMutex);
  try {
    for (const auto &[filename, config] : impl_->config.items()) {
      fs::path file_path = dir_path / (filename + ".json");
      std::ofstream ofs(file_path);
      if (!ofs) {
        LOG_F(ERROR, "Failed to open file: {}", file_path.string());
        return false;
      }
      ofs << config.dump(4);
      ofs.close();
      LOG_F(INFO, "Config saved to file: {}", file_path.string());
    }
    return true;
  } catch (const std::exception &e) {
    LOG_F(ERROR,
          "Failed to save all configs to directory: {}, error message: {}",
          dir_path.string(), e.what());
    return false;
  }
}

auto ConfigManager::get(const std::string &key_path) const
    -> std::optional<json> {
  std::shared_lock lock(impl_->rwMutex);
  const json *p = &impl_->config;
  for (const auto &key : key_path | std::views::split('/')) {
    std::string keyStr = std::string(key.begin(), key.end());
    if (p->is_object() && p->contains(keyStr)) {
      p = &(*p)[keyStr];
    } else {
      LOG_F(WARNING, "Key not found: {}", key_path);
      return std::nullopt;
    }
  }
  return *p;
}

auto ConfigManager::set(const std::string &key_path, const json &value)
    -> bool {
  return impl_->setOrAppendImpl(key_path, value, false);
}

auto ConfigManager::set(const std::string &key_path, json &&value) -> bool {
  return impl_->setOrAppendImpl(key_path, std::move(value), false);
}

auto ConfigManager::append(const std::string &key_path, const json &value)
    -> bool {
  return impl_->setOrAppendImpl(key_path, value, true);
}

auto ConfigManager::remove(const std::string &key_path) -> bool {
  std::unique_lock lock(impl_->rwMutex);
  json *p = &impl_->config;
  std::vector<std::string> keys;
  for (const auto &key : key_path | std::views::split('/')) {
    keys.emplace_back(key.begin(), key.end());
  }
  for (auto it = keys.begin(); it != keys.end(); ++it) {
    if (std::next(it) == keys.end()) {
      if (p->is_object() && p->contains(*it)) {
        p->erase(*it);
        LOG_F(INFO, "Deleted key: {}", key_path);
        return true;
      }
      LOG_F(WARNING, "Key not found for deletion: {}", key_path);
      return false;
    }
    if (!p->is_object() || !p->contains(*it)) {
      LOG_F(WARNING, "Key not found for deletion: {}", key_path);
      return false;
    }
    p = &(*p)[*it];
  }
  return false;
}

auto ConfigManager::has(const std::string &key_path) const -> bool {
  return get(key_path).has_value();
}

void ConfigManager::tidy() {
  std::unique_lock lock(impl_->rwMutex);
  json updatedConfig;
  for (const auto &[key, value] : impl_->config.items()) {
    json *p = &updatedConfig;
    for (const auto &subKey : key | std::views::split('/')) {
      std::string subKeyStr = std::string(subKey.begin(), subKey.end());
      if (!p->contains(subKeyStr)) {
        (*p)[subKeyStr] = json::object();
      }
      p = &(*p)[subKeyStr];
    }
    *p = value;
    LOG_F(INFO, "Tidied key: {} with value: {}", key, value.dump());
  }
  impl_->config = std::move(updatedConfig);
  LOG_F(INFO, "Config tidied.");
}

void ConfigManager::merge(const json &src, json &target) {
  for (auto it = src.begin(); it != src.end(); ++it) {
    LOG_F(INFO, "Merge config: {}", it.key());
    if (it->is_object() && target.contains(it.key()) &&
        target[it.key()].is_object()) {
      merge(*it, target[it.key()]);
    } else {
      target[it.key()] = *it;
    }
  }
}

void ConfigManager::merge(const json &src) {
  merge(src, impl_->config);
  LOG_F(INFO, "Config merged.");
}

void ConfigManager::clear() {
  std::unique_lock lock(impl_->rwMutex);
  impl_->config.clear();
  LOG_F(INFO, "Config cleared.");
}

auto ConfigManager::getKeys() const -> std::vector<std::string> {
  std::shared_lock lock(impl_->rwMutex);
  std::vector<std::string> paths;
  std::function<void(const json &, std::string)> listPaths =
      [&](const json &j, std::string path) {
        for (auto it = j.begin(); it != j.end(); ++it) {
          if (it.value().is_object()) {
            listPaths(it.value(), path + "/" + it.key());
          } else {
            paths.emplace_back(path + "/" + it.key());
          }
        }
      };
  listPaths(impl_->config, "");
  return paths;
}

auto ConfigManager::listPaths() const -> std::vector<std::string> {
  std::shared_lock lock(impl_->rwMutex);
  std::vector<std::string> paths;
  std::weak_ptr<atom::utils::Env> envPtr;
  GET_OR_CREATE_WEAK_PTR(envPtr, atom::utils::Env, Constants::ENVIRONMENT);
  auto env = envPtr.lock();
  if (!env) {
    LOG_F(ERROR, "Failed to get environment instance");
    return paths;
  }

  // Get the config directory from the command line arguments
  auto configDir = env->get("config");
  if (configDir.empty() || !atom::io::isFolderExists(configDir)) {
    // Get the config directory from the environment if not set or invalid
    configDir = env->getEnv("LITHIUM_CONFIG_DIR", "./config");
  }

  // Check for JSON files in the config directory
  return atom::io::checkFileTypeInFolder(configDir, {".json"},
                                         atom::io::FileOption::PATH);
}
} // namespace lithium