#define GetIntConfig(path)                                                     \
  GetPtr<ConfigManager>(Constatns::CONFIG_MANAGER)                             \
      .value()                                                                 \
      ->getValue(path)                                                         \
      .value()                                                                 \
      .get<int>()

#define GetFloatConfig(path)                                                   \
  GetPtr<ConfigManager>(Constatns::CONFIG_MANAGER)                             \
      .value()                                                                 \
      ->getValue(path)                                                         \
      .value()                                                                 \
      .get<float>()

#define GetBoolConfig(path)                                                    \
  GetPtr<ConfigManager>(Constatns::CONFIG_MANAGER)                             \
      .value()                                                                 \
      ->getValue(path)                                                         \
      .value()                                                                 \
      .get<bool>()

#define GetDoubleConfig(path)                                                  \
  GetPtr<ConfigManager>(Constatns::CONFIG_MANAGER)                             \
      .value()                                                                 \
      ->getValue(path)                                                         \
      .value()                                                                 \
      .get<double>()

#define GetStringConfig(path)                                                  \
  GetPtr<ConfigManager>(Constatns::CONFIG_MANAGER)                             \
      .value()                                                                 \
      ->getValue(path)                                                         \
      .value()                                                                 \
      .get<std::string>()

#define GET_CONFIG_VALUE(configManager, path, type, outputVar)                 \
  type outputVar;                                                              \
  do {                                                                         \
    auto opt = (configManager)->getValue(path);                                \
    if (opt.has_value()) {                                                     \
      try {                                                                    \
        (outputVar) = opt.value().get<type>();                                 \
      } catch (const std::bad_optional_access &e) {                            \
        LOG_F(ERROR, "Bad access to config value for {}: {}", path, e.what()); \
        THROW_BAD_CONFIG_EXCEPTION(e.what());                                  \
      } catch (const json::exception &e) {                                     \
        LOG_F(ERROR, "Invalid config value for {}: {}", path, e.what());       \
        THROW_INVALID_CONFIG_EXCEPTION(e.what());                              \
      } catch (const std::exception &e) {                                      \
        THROW_UNKOWN(e.what());                                                \
      }                                                                        \
    } else {                                                                   \
      LOG_F(WARNING, "Config value for {} not found", path);                   \
      THROW_OBJ_NOT_EXIST("Config value for", path);                           \
    }                                                                          \
  } while (0)
