#define LITHIUM_GET_REQUIRED_PTR(var, type, id, ...)                         \
    auto var##Opt = GetPtr<type>(id);                                        \
    if (!var##Opt) {                                                         \
        LOG_ERROR( "{}: Failed to get " #type " pointer", ATOM_FUNC_NAME); \
        return __VA_ARGS__;                                                  \
    }                                                                        \
    auto var = var##Opt.value();
