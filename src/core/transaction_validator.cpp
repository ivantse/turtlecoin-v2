// Copyright (c) 2021, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#include "transaction_validator.h"

#include <address.h>
#include <config.h>
#include <crypto.h>
#include <network_fees.h>

static inline Error check_pow_zeros(size_t pow_zeros)
{
    if (pow_zeros < Configuration::Transaction::Fees::MINIMUM_POW_ZEROS)
    {
        return MAKE_ERROR(TX_MINIMUM_POW);
    }

    return MAKE_ERROR(SUCCESS);
}

static inline uint64_t calculate_required_fee(size_t tx_size, size_t pow_zeros)
{
    return Common::NetworkFees::calculate_transaction_fee(tx_size, pow_zeros);
}

namespace Core
{
    TransactionValidator::TransactionValidator(
        std::shared_ptr<BlockchainStorage> &db,
        std::shared_ptr<StakingEngine> &se):
        m_blockchain_storage(db), m_staking_engine(se)
    {
    }

    Error TransactionValidator::check(const uncommitted_transaction_t &transaction) const
    {
        auto error = std::visit([&](auto &&tx) { return tx.check_construction(); }, transaction);

        if (error)
        {
            return error;
        }

        return std::visit(
            [&](auto &&tx)
            {
                const auto pow_zeros = tx.pow_hash().leading_zeros();

                auto error = check_pow_zeros(pow_zeros);

                if (error)
                {
                    return error;
                }

                if (tx.fee < calculate_required_fee(tx.size(), pow_zeros))
                {
                    return MAKE_ERROR(TX_LOW_FEE);
                }

                return MAKE_ERROR(SUCCESS);
            },
            transaction);
    }

    Error TransactionValidator::check(const transaction_t &transaction) const
    {
        auto error = std::visit([&](auto &&tx) { return tx.check_construction(); }, transaction);

        if (error)
        {
            return error;
        }

        return std::visit(
            [&](auto &&tx)
            {
                USEVARIANT(T, tx);

                if COMMITED_USER_TX_VARIANT (T)
                {
                    const auto pow_zeros = tx.pow_hash().leading_zeros();

                    auto error = check_pow_zeros(pow_zeros);

                    if (error)
                    {
                        return error;
                    }

                    if (tx.fee < calculate_required_fee(tx.size(), pow_zeros))
                    {
                        return MAKE_ERROR(TX_LOW_FEE);
                    }
                }
                /**
                 * Verifies that the genesis transaction provided goes to the wallet
                 * address specified in the configuration for the amounts specified
                 */
                else if VARIANT (T, genesis_transaction_t)
                {
                    if (tx.secret_key != Configuration::Transaction::Genesis::TX_PRIVATE_KEY)
                    {
                        return MAKE_ERROR(TX_SECRET_KEY);
                    }

                    auto [error, public_spend, public_view] =
                        Common::Address::decode(Configuration::Transaction::Genesis::DESTINATION_WALLET);

                    if (error)
                    {
                        return error;
                    }

                    const auto key_derivation = Crypto::generate_key_derivation(public_view, tx.secret_key);

                    for (size_t i = 0; i < tx.outputs.size(); ++i)
                    {
                        const auto &output = tx.outputs[i];

                        const auto key_derivation_scalar = Crypto::derivation_to_scalar(key_derivation, i);

                        const auto blinding_factor =
                            Crypto::RingCT::generate_commitment_blinding_factor(key_derivation_scalar);

                        const auto amount_mask = Crypto::RingCT::generate_amount_mask(key_derivation_scalar);

                        if (Crypto::derive_public_key(key_derivation_scalar, public_spend) != output.public_ephemeral)
                        {
                            return MAKE_ERROR(TX_OUTPUT_PUBLIC_EPHEMERAL);
                        }

                        if (Crypto::RingCT::toggle_masked_amount(amount_mask, output.amount)
                            != Configuration::Transaction::Genesis::OUTPUT_AMOUNT)
                        {
                            return MAKE_ERROR(TX_OUTPUT_AMOUNT);
                        }

                        if (Crypto::RingCT::generate_pedersen_commitment(
                                blinding_factor, Configuration::Transaction::Genesis::OUTPUT_AMOUNT)
                            != output.commitment)
                        {
                            return MAKE_ERROR(TX_OUTPUT_COMMITMENT);
                        }
                    }
                }
                else if VARIANT (T, staker_reward_transaction_t)
                {
                    // TODO: something
                }
                else if VARIANT (T, stake_refund_transaction_t)
                {
                    // TODO: something
                }

                return MAKE_ERROR(SUCCESS);
            },
            transaction);
    }

    Error TransactionValidator::validate(const uncommitted_transaction_t &transaction) const
    {
        // first thing first, check the construction of the transaction
        auto error = check(transaction);

        if (error)
        {
            return error;
        }

        return std::visit(
            [&](auto &&tx)
            {
                USEVARIANT(T, tx);

                const auto message_digest = tx.digest();

                // first we gather up the output commitments
                std::vector<crypto_pedersen_commitment_t> commitments;

                for (const auto &output : tx.outputs)
                {
                    commitments.push_back(output.commitment);
                }

                // verify the range proof
                if (!Crypto::RangeProofs::BulletproofsPlus::verify(tx.range_proof, commitments))
                {
                    return MAKE_ERROR(TX_INVALID_RANGE_PROOF);
                }

                // check for double spends via key image checks
                if (m_blockchain_storage->key_image_exists(tx.key_images))
                {
                    return MAKE_ERROR(TX_KEY_IMAGE_ALREADY_EXISTS);
                }

                // now go fetch the input public ephemerals and commitments from storage
                std::vector<crypto_public_key_t> public_keys;

                commitments.clear();

                {
                    auto [error, inputs] = m_blockchain_storage->get_transaction_output(tx.ring_participants);

                    if (error)
                    {
                        return error;
                    }

                    for (const auto &input : inputs)
                    {
                        public_keys.push_back(input.public_ephemeral);

                        commitments.push_back(input.commitment);
                    }
                }

                // now loop through the signatures and verify them
                for (size_t i = 0; i < tx.signatures.size(); ++i)
                {
                    const auto &signature = tx.signatures[i];

                    const auto &key_image = tx.key_images[i];

                    // if the signature is invalid, kick it back
                    if (!Crypto::RingSignature::CLSAG::check_ring_signature(
                            message_digest, key_image, public_keys, signature, commitments))
                    {
                        return MAKE_ERROR(TX_INVALID_RING_SIGNATURE);
                    }
                }

                if VARIANT (T, uncommitted_recall_stake_transaction_t)
                {
                    // TODO: do something
                }

                return MAKE_ERROR(SUCCESS);
            },
            transaction);
    }

    Error TransactionValidator::validate(const transaction_t &transaction) const
    {
        // first thing first, check the construction of the transaction
        auto error = check(transaction);

        if (error)
        {
            return error;
        }

        return std::visit(
            [&](auto &&tx)
            {
                USEVARIANT(T, tx);

                if COMMITED_USER_TX_VARIANT (T)
                {
                    // check for double spends via key image checks
                    if (m_blockchain_storage->key_image_exists(tx.key_images))
                    {
                        return MAKE_ERROR(TX_KEY_IMAGE_ALREADY_EXISTS);
                    }

                    if VARIANT (T, committed_recall_stake_transaction_t)
                    {
                        // TODO: do something
                    }
                }
                else if VARIANT (T, genesis_transaction_t)
                {
                    if (m_blockchain_storage->block_exists(0))
                    {
                        return MAKE_ERROR(TX_GENESIS_ALREADY_EXISTS);
                    }
                }
                else if VARIANT (T, staker_reward_transaction_t)
                {
                    // TODO: do something
                }
                else if VARIANT (T, stake_refund_transaction_t)
                {
                    // TODO: do something
                }

                return MAKE_ERROR(SUCCESS);
            },
            transaction);
    }
} // namespace Core
