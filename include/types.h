// Copyright (c) 2021, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#ifndef TURTLECOIN_TYPES_H
#define TURTLECOIN_TYPES_H

#include <variant>

// Block
#include "blockchain/block.h"

// Network Transactions
#include "blockchain/transaction_genesis.h"
#include "blockchain/transaction_stake_refund.h"
#include "blockchain/transaction_staker.h"

// User Generated Transactions
#include "blockchain/transaction_normal.h"
#include "blockchain/transaction_recall_stake.h"
#include "blockchain/transaction_stake.h"

// Network Packets
#include "network/packet_data.h"
#include "network/packet_handshake.h"
#include "network/packet_keepalive.h"
#include "network/packet_peer_exchange.h"

// Staking Types
#include "staking/candidate.h"
#include "staking/stake.h"

#define USEVARIANT(id, obj) using id = std::decay_t<decltype(obj)>
#define VARIANTC(left, right) std::is_same_v<left, right>
#define VARIANT(left, right) constexpr(VARIANTC(left, right))
#define COMMITED_USER_TX_VARIANT(left)                                                                  \
    constexpr(                                                                                          \
        VARIANTC(left, committed_normal_transaction_t) || VARIANTC(left, committed_stake_transaction_t) \
        || VARIANTC(left, committed_recall_stake_transaction_t))

namespace Types
{
    namespace Blockchain
    {
        typedef std::variant<
            genesis_transaction_t,
            staker_transaction_t,
            committed_normal_transaction_t,
            committed_recall_stake_transaction_t,
            committed_stake_transaction_t,
            stake_refund_transaction_t>
            transaction_t;

        typedef std::variant<
            uncommited_normal_transaction_t,
            uncommitted_stake_transaction_t,
            uncommitted_recall_stake_transaction_t>
            uncommitted_transaction_t;

        typedef std::
            variant<committed_normal_transaction_t, committed_recall_stake_transaction_t, committed_stake_transaction_t>
                user_transaction_t;

        typedef BaseTypes::TransactionType TransactionType;
    } // namespace Blockchain

    namespace Network
    {
        typedef std::variant<packet_handshake_t, packet_peer_exchange_t, packet_keepalive_t, packet_data_t>
            network_packet_t;
    } // namespace Network
} // namespace Types

#endif // TURTLECOIN_TYPES_H
