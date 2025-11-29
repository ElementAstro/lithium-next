#include "transaction.hpp"

#include <spdlog/spdlog.h>

#include "database.hpp"

namespace lithium::database::core {

//------------------------------------------------------------------------------
// Transaction Implementation
//------------------------------------------------------------------------------

Transaction::Transaction(Database& db)
    : db(db), committed(false), rolledBack(false) {
    try {
        db.execute("BEGIN TRANSACTION;");
    } catch (const std::exception& e) {
        spdlog::error("Failed to begin transaction: {}", e.what());
        THROW_TRANSACTION_ERROR("Failed to begin transaction: " +
                                std::string(e.what()));
    }
}

Transaction::~Transaction() {
    // Auto-rollback if not committed or rolled back explicitly
    if (!committed && !rolledBack) {
        try {
            rollback();
        } catch (const std::exception& e) {
            spdlog::error(
                "Failed to auto-rollback transaction in destructor: {}",
                e.what());
        }
    }
}

void Transaction::commit() {
    if (committed || rolledBack) {
        THROW_TRANSACTION_ERROR("Transaction already committed or rolled back");
    }

    try {
        db.execute("COMMIT;");
        committed = true;
        spdlog::info("Transaction committed successfully");
    } catch (const std::exception& e) {
        spdlog::error("Failed to commit transaction: {}", e.what());
        THROW_TRANSACTION_ERROR("Failed to commit transaction: " +
                                std::string(e.what()));
    }
}

void Transaction::rollback() {
    if (committed || rolledBack) {
        THROW_TRANSACTION_ERROR("Transaction already committed or rolled back");
    }

    try {
        db.execute("ROLLBACK;");
        rolledBack = true;
        spdlog::info("Transaction rolled back successfully");
    } catch (const std::exception& e) {
        spdlog::error("Failed to rollback transaction: {}", e.what());
        THROW_TRANSACTION_ERROR("Failed to rollback transaction: " +
                                std::string(e.what()));
    }
}

}  // namespace lithium::database::core
