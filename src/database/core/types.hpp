#ifndef LITHIUM_DATABASE_CORE_TYPES_HPP
#define LITHIUM_DATABASE_CORE_TYPES_HPP

#include "atom/error/exception.hpp"

namespace lithium::database::core {

// Exception classes - all follow the XxxError naming convention
class DatabaseOpenError : public atom::error::Exception {
    using Exception::Exception;
};

class SqlExecutionError : public atom::error::Exception {
    using Exception::Exception;
};

class StatementPrepareError : public atom::error::Exception {
    using Exception::Exception;
};

class TransactionError : public atom::error::Exception {
    using Exception::Exception;
};

class ValidationError : public atom::error::Exception {
    using Exception::Exception;
};

// Backward compatibility aliases
using FailedToOpenDatabase = DatabaseOpenError;
using SQLExecutionError = SqlExecutionError;
using PrepareStatementError = StatementPrepareError;

// Exception macros - standardized naming
#define THROW_DATABASE_OPEN_ERROR(...)                \
    throw lithium::database::core::DatabaseOpenError( \
        ATOM_FILE_NAME, ATOM_FILE_LINE, ATOM_FUNC_NAME, __VA_ARGS__)

#define THROW_SQL_EXECUTION_ERROR(...)                \
    throw lithium::database::core::SqlExecutionError( \
        ATOM_FILE_NAME, ATOM_FILE_LINE, ATOM_FUNC_NAME, __VA_ARGS__)

#define THROW_STATEMENT_PREPARE_ERROR(...)                \
    throw lithium::database::core::StatementPrepareError( \
        ATOM_FILE_NAME, ATOM_FILE_LINE, ATOM_FUNC_NAME, __VA_ARGS__)

#define THROW_TRANSACTION_ERROR(...)                 \
    throw lithium::database::core::TransactionError( \
        ATOM_FILE_NAME, ATOM_FILE_LINE, ATOM_FUNC_NAME, __VA_ARGS__)

#define THROW_VALIDATION_ERROR(...)                 \
    throw lithium::database::core::ValidationError( \
        ATOM_FILE_NAME, ATOM_FILE_LINE, ATOM_FUNC_NAME, __VA_ARGS__)

// Backward compatibility macros
#define THROW_FAILED_TO_OPEN_DATABASE(...) \
    THROW_DATABASE_OPEN_ERROR(__VA_ARGS__)
#define THROW_PREPARE_STATEMENT_ERROR(...) \
    THROW_STATEMENT_PREPARE_ERROR(__VA_ARGS__)

}  // namespace lithium::database::core

#endif  // LITHIUM_DATABASE_CORE_TYPES_HPP
