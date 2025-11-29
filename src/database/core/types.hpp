#ifndef LITHIUM_DATABASE_CORE_TYPES_HPP
#define LITHIUM_DATABASE_CORE_TYPES_HPP

#include "atom/error/exception.hpp"

namespace lithium::database::core {

// Exception classes
class FailedToOpenDatabase : public atom::error::Exception {
    using Exception::Exception;
};

class SQLExecutionError : public atom::error::Exception {
    using Exception::Exception;
};

class PrepareStatementError : public atom::error::Exception {
    using Exception::Exception;
};

class TransactionError : public atom::error::Exception {
    using Exception::Exception;
};

class ValidationError : public atom::error::Exception {
    using Exception::Exception;
};

// Exception macros
#define THROW_FAILED_TO_OPEN_DATABASE(...)         \
    throw lithium::database::core::FailedToOpenDatabase( \
        ATOM_FILE_NAME, ATOM_FILE_LINE, ATOM_FUNC_NAME, __VA_ARGS__)

#define THROW_SQL_EXECUTION_ERROR(...)                                         \
    throw lithium::database::core::SQLExecutionError(ATOM_FILE_NAME, ATOM_FILE_LINE, \
                                               ATOM_FUNC_NAME, __VA_ARGS__)

#define THROW_PREPARE_STATEMENT_ERROR(...)          \
    throw lithium::database::core::PrepareStatementError( \
        ATOM_FILE_NAME, ATOM_FILE_LINE, ATOM_FUNC_NAME, __VA_ARGS__)

#define THROW_TRANSACTION_ERROR(...)                                          \
    throw lithium::database::core::TransactionError(ATOM_FILE_NAME, ATOM_FILE_LINE, \
                                              ATOM_FUNC_NAME, __VA_ARGS__)

#define THROW_VALIDATION_ERROR(...)                                          \
    throw lithium::database::core::ValidationError(ATOM_FILE_NAME, ATOM_FILE_LINE, \
                                             ATOM_FUNC_NAME, __VA_ARGS__)

}  // namespace lithium::database::core

#endif  // LITHIUM_DATABASE_CORE_TYPES_HPP
