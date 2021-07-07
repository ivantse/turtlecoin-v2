// Copyright (c) 2021, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#ifndef TURTLECOIN_CORE_TRANSACTION_VALIDATOR_H
#define TURTLECOIN_CORE_TRANSACTION_VALIDATOR_H

#include "blockchain_storage.h"
#include "staking_engine.h"

#include <errors.h>
#include <logger.h>
#include <types.h>

using namespace Types::Blockchain;

namespace Core
{
    class TransactionValidator
    {
      public:
        TransactionValidator(std::shared_ptr<BlockchainStorage> &db, std::shared_ptr<StakingEngine> &se);

        /**
         * Performs basic construction checks of the provided transaction
         *
         * @param transaction
         * @return
         */
        [[nodiscard]] Error check(const transaction_t &transaction) const;

        /**
         * Performs basic construction checks of the provided transaction
         *
         * @param transaction
         * @return
         */
        [[nodiscard]] Error check(const uncommitted_transaction_t &transaction) const;

        /**
         * Performs full validation of the transaction
         *
         * @param transaction
         * @return
         */
        [[nodiscard]] Error validate(const transaction_t &transaction) const;

        /**
         * Performs full validation of the transaction
         *
         * @param transaction
         * @return
         */
        [[nodiscard]] Error validate(const uncommitted_transaction_t &transaction) const;

      private:
        std::shared_ptr<BlockchainStorage> m_blockchain_storage;

        std::shared_ptr<StakingEngine> m_staking_engine;
    };
} // namespace Core

#endif
