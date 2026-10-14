#include "storage/quack_transaction_manager.hpp"
#include "storage/quack_catalog.hpp"

namespace duckdb {

QuackTransactionManager::QuackTransactionManager(AttachedDatabase &db_p, QuackCatalog &quack_catalog_p)
    : TransactionManager(db_p), quack_catalog(quack_catalog_p) {
}

Transaction &QuackTransactionManager::StartTransaction(ClientContext &context) {
	auto transaction = make_uniq<QuackTransaction>(quack_catalog, *this, context);
	transaction->Start();
	auto &result = *transaction;
	lock_guard<mutex> l(transaction_lock);
	transactions[result] = std::move(transaction);
	return result;
}
ErrorData QuackTransactionManager::CommitTransaction(ClientContext &context, Transaction &transaction) {
	auto &quack_transaction = transaction.Cast<QuackTransaction>();
	try {
		quack_transaction.Commit();
	} catch (...) {
		quack_catalog.InvalidateIfChangedSince(quack_transaction.StartVersion());
		throw;
	}
	lock_guard<mutex> l(transaction_lock);
	transactions.erase(transaction);
	return ErrorData();
}

void QuackTransactionManager::RollbackTransaction(Transaction &transaction) {
	auto &quack_transaction = transaction.Cast<QuackTransaction>();
	try {
		quack_transaction.Rollback();
	} catch (...) {
		quack_catalog.InvalidateIfChangedSince(quack_transaction.StartVersion());
		throw;
	}
	quack_catalog.InvalidateIfChangedSince(quack_transaction.StartVersion());
	lock_guard<mutex> l(transaction_lock);
	transactions.erase(transaction);
}

void QuackTransactionManager::Checkpoint(ClientContext &context, bool force) {
	throw NotImplementedException("Checkpoint not implemented yet");
}

} // namespace duckdb
