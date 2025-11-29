#ifndef LITHIUM_DATABASE_CORE_TRANSACTION_HPP
#define LITHIUM_DATABASE_CORE_TRANSACTION_HPP

#include "types.hpp"

namespace lithium::database::core {

// Forward declaration
class Database;

class Transaction {
public:
    /**
     * @brief Constructs a Transaction object.
     *
     * @param db Reference to the database connection.
     * @throws TransactionError if transaction cannot be started
     */
    explicit Transaction(Database& db);

    /**
     * @brief Destructor that rolls back the transaction if not committed.
     */
    ~Transaction();

    // Non-copyable
    Transaction(const Transaction&) = delete;
    Transaction& operator=(const Transaction&) = delete;

    /**
     * @brief Commits the transaction.
     *
     * @throws TransactionError if commit fails
     */
    void commit();

    /**
     * @brief Rolls back the transaction.
     *
     * @throws TransactionError if rollback fails
     */
    void rollback();

private:
    Database& db;
    bool committed = false;
    bool rolledBack = false;
};

}  // namespace lithium::database::core

#endif  // LITHIUM_DATABASE_CORE_TRANSACTION_HPP
