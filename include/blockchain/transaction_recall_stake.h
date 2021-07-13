// Copyright (c) 2021, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#ifndef TURTLECOIN_TRANSACTION_RECALL_STAKE_H
#define TURTLECOIN_TRANSACTION_RECALL_STAKE_H

#include "base_types.h"

namespace Types::Blockchain
{
    struct committed_recall_stake_transaction_t :
        BaseTypes::TransactionPrefix,
        BaseTypes::TransactionUserBody,
        BaseTypes::RecallStakeTransactionData,
        BaseTypes::CommittedTransactionSuffix,
        virtual BaseTypes::ITransaction
    {
        committed_recall_stake_transaction_t()
        {
            l_type = BaseTypes::TransactionType::RECALL_STAKE;
        }

        committed_recall_stake_transaction_t(deserializer_t &reader)
        {
            deserialize(reader);
        }

        committed_recall_stake_transaction_t(std::initializer_list<uint8_t> input)
        {
            std::vector<uint8_t> data(input);

            auto reader = deserializer_t(data);

            deserialize(reader);
        }

        committed_recall_stake_transaction_t(const std::vector<uint8_t> &data)
        {
            deserializer_t reader(data);

            deserialize(reader);
        }

        committed_recall_stake_transaction_t(const std::string &hex)
        {
            deserializer_t reader(hex);

            deserialize(reader);
        }

        JSON_OBJECT_CONSTRUCTORS(committed_recall_stake_transaction_t, fromJSON)

        void deserialize(deserializer_t &reader) override
        {
            deserialize_prefix(reader);

            deserialize_body(reader);

            deserialize_data(reader);

            deserialize_suffix(reader);
        }

        JSON_FROM_FUNC(fromJSON) override
        {
            JSON_OBJECT_OR_THROW();

            prefix_fromJSON(j);

            body_fromJSON(j);

            data_fromJSON(j);

            suffix_fromJSON(j);
        }

        [[nodiscard]] Error check_construction() const override
        {
            if (version != 1 & version != 2)
            {
                return MAKE_ERROR(TX_INVALID_VERSION);
            }

            if (public_key.empty())
            {
                return MAKE_ERROR(TX_PUBLIC_KEY);
            }

            if (fee == 0)
            {
                return MAKE_ERROR(TX_MISSING_FEE);
            }

            // check to verify that the transaction contains the proper number of inputs
            if (key_images.empty() || key_images.size() > Configuration::Transaction::MAXIMUM_INPUTS)
            {
                return MAKE_ERROR(TX_INVALID_INPUT_COUNT);
            }

            // verify that all key images are in the proper subgroup
            for (const auto &key_image : key_images)
            {
                if (!key_image.check_subgroup())
                {
                    return MAKE_ERROR(TX_INVALID_KEY_IMAGE);
                }
            }

            /**
             * Check for duplicate key images by de-duplicating them and comparing
             * the resulting vector size(s)
             */
            if (crypto_point_vector_t(key_images).dedupe_sort().size() != key_images.size())
            {
                return MAKE_ERROR(TX_DUPLICATE_KEY_IMAGE);
            }

            // check to verify that the transaction contains the proper number of outputs
            if (outputs.size() < Configuration::Transaction::MINIMUM_OUTPUTS
                || outputs.size() > Configuration::Transaction::MAXIMUM_OUTPUTS)
            {
                return MAKE_ERROR(TX_INVALID_OUTPUT_COUNT);
            }

            // check all of the output constructions
            for (const auto &output : outputs)
            {
                auto error = output.check_construction();

                if (error)
                {
                    return error;
                }
            }

            if (stake_amount == 0)
            {
                return MAKE_ERROR(TX_STAKE_NO_AMOUNT);
            }

            if (candidate_public_key.empty())
            {
                return MAKE_ERROR(STAKING_CANDIDATE_NOT_FOUND);
            }

            if (staker_id.empty())
            {
                return MAKE_ERROR(TX_STAKER_ID);
            }

            if (view_signature.empty())
            {
                return MAKE_ERROR(TX_RECALL_VIEW_SIGNATURE);
            }

            if (spend_signature.empty())
            {
                return MAKE_ERROR(TX_RECALL_SPEND_SIGNATURE);
            }

            return MAKE_ERROR(SUCCESS);
        }

        [[nodiscard]] crypto_hash_t digest() const
        {
            const auto data = serialize_digest();

            return Crypto::Hashing::sha3(data.data(), data.size());
        }

        [[nodiscard]] crypto_hash_t hash() const override
        {
            serializer_t writer;

            writer.key(digest());

            /**
             * To make sure that both an uncommitted and committed transaction have the same
             * hash, we incorporate the signature_hash and range_proof_hash into the hash of
             * the uncommitted transaction here to make sure that we get the same result
             */

            writer.key(signature_hash);

            writer.key(range_proof_hash);

            return Crypto::Hashing::sha3(writer.data(), writer.size());
        }

        [[nodiscard]] crypto_hash_t pow_hash() const
        {
            serializer_t writer;

            writer.bytes(serialize_digest());

            writer.key(range_proof_hash);

            const auto data = Crypto::Hashing::sha3(writer.data(), writer.size());

            return Crypto::Hashing::argon2id(
                data,
                Configuration::Transaction::ProofOfWork::ITERATIONS,
                Configuration::Transaction::ProofOfWork::MEMORY,
                Configuration::Transaction::ProofOfWork::THREADS);
        }

        [[nodiscard]] bool pow_verify(const uint8_t zeros = 0) const
        {
            return pow_hash().leading_zeros() >= zeros;
        }

        void serialize(serializer_t &writer) const override
        {
            serialize_prefix(writer);

            serialize_body(writer);

            serialize_data(writer);

            serialize_suffix(writer);
        }

        [[nodiscard]] std::vector<uint8_t> serialize() const override
        {
            serializer_t writer;

            serialize(writer);

            return writer.vector();
        }

        [[nodiscard]] std::vector<uint8_t> serialize_digest() const
        {
            serializer_t writer;

            serialize_prefix(writer);

            serialize_body(writer);

            serialize_data(writer);

            return writer.vector();
        }

        [[nodiscard]] size_t size() const override
        {
            const auto bytes = serialize();

            return bytes.size();
        }

        JSON_TO_FUNC(toJSON) override
        {
            writer.StartObject();
            {
                prefix_toJSON(writer);

                body_toJSON(writer);

                data_toJSON(writer);

                suffix_toJSON(writer);
            }
            writer.EndObject();
        }

        [[nodiscard]] std::string to_string() const override
        {
            const auto bytes = serialize();

            return Crypto::StringTools::to_hex(bytes.data(), bytes.size());
        }
    };

    typedef struct uncommitted_recall_stake_transaction_t :
        BaseTypes::TransactionPrefix,
        BaseTypes::TransactionUserBody,
        BaseTypes::RecallStakeTransactionData,
        BaseTypes::UncommittedTransactionSuffix,
        virtual BaseTypes::ITransaction
    {
        uncommitted_recall_stake_transaction_t()
        {
            l_type = BaseTypes::TransactionType::RECALL_STAKE;
        }

        uncommitted_recall_stake_transaction_t(deserializer_t &reader)
        {
            deserialize(reader);
        }

        uncommitted_recall_stake_transaction_t(std::initializer_list<uint8_t> input)
        {
            std::vector<uint8_t> data(input);

            auto reader = deserializer_t(data);

            deserialize(reader);
        }

        uncommitted_recall_stake_transaction_t(const std::vector<uint8_t> &data)
        {
            deserializer_t reader(data);

            deserialize(reader);
        }

        uncommitted_recall_stake_transaction_t(const std::string &hex)
        {
            deserializer_t reader(hex);

            deserialize(reader);
        }

        JSON_OBJECT_CONSTRUCTORS(uncommitted_recall_stake_transaction_t, fromJSON)

        [[nodiscard]] Error check_construction() const override
        {
            if (version != 1 && version != 2)
            {
                return MAKE_ERROR(TX_INVALID_VERSION);
            }

            if (public_key.empty())
            {
                return MAKE_ERROR(TX_PUBLIC_KEY);
            }

            if (fee == 0)
            {
                return MAKE_ERROR(TX_MISSING_FEE);
            }

            // check to verify that the transaction contains the proper number of inputs
            if (key_images.empty() || key_images.size() > Configuration::Transaction::MAXIMUM_INPUTS)
            {
                return MAKE_ERROR(TX_INVALID_INPUT_COUNT);
            }

            // verify that all key images are in the proper subgroup
            for (const auto &key_image : key_images)
            {
                if (!key_image.check_subgroup())
                {
                    return MAKE_ERROR(TX_INVALID_KEY_IMAGE);
                }
            }

            /**
             * Check for duplicate key images by de-duplicating them and comparing
             * the resulting vector size(s)
             */
            if (crypto_point_vector_t(key_images).dedupe_sort().size() != key_images.size())
            {
                return MAKE_ERROR(TX_DUPLICATE_KEY_IMAGE);
            }

            // check to verify that the transaction contains the proper number of outputs
            if (outputs.size() < Configuration::Transaction::MINIMUM_OUTPUTS
                || outputs.size() > Configuration::Transaction::MAXIMUM_OUTPUTS)
            {
                return MAKE_ERROR(TX_INVALID_OUTPUT_COUNT);
            }

            std::vector<crypto_pedersen_commitment_t> commitments;

            // check all of the output constructions
            for (const auto &output : outputs)
            {
                auto error = output.check_construction();

                if (error)
                {
                    return error;
                }

                commitments.push_back(output.commitment);
            }

            if (stake_amount == 0)
            {
                return MAKE_ERROR(TX_STAKE_NO_AMOUNT);
            }

            if (candidate_public_key.empty())
            {
                return MAKE_ERROR(STAKING_CANDIDATE_NOT_FOUND);
            }

            if (staker_id.empty())
            {
                return MAKE_ERROR(TX_STAKER_ID);
            }

            if (view_signature.empty())
            {
                return MAKE_ERROR(TX_RECALL_VIEW_SIGNATURE);
            }

            if (spend_signature.empty())
            {
                return MAKE_ERROR(TX_RECALL_SPEND_SIGNATURE);
            }

            // check that the transaction is in balance
            if (key_images.size() != pseudo_commitments.size())
            {
                return MAKE_ERROR(TX_INVALID_PSEUDO_COMMITMENT_COUNT);
            }

            if (!Crypto::RingCT::check_commitments_parity(pseudo_commitments, commitments, fee))
            {
                return MAKE_ERROR(TX_COMMITMENTS_DO_NOT_BALANCE);
            }

            // check the range proof is constructed
            if (!range_proof.check_construction())
            {
                return MAKE_ERROR(TX_INVALID_RANGE_PROOF);
            }

            /**
             * check to make sure that we have the same number of signatures
             * as the number of inputs
             */
            if (key_images.size() != signatures.size())
            {
                return MAKE_ERROR(TX_SIG_SIZE_MISMATCH);
            }

            // check the signature constructions
            for (const auto &signature : signatures)
            {
                if (!signature.check_construction(Configuration::Transaction::RING_SIZE))
                {
                    return MAKE_ERROR(TX_INVALID_SIGNATURE);
                }
            }

            return MAKE_ERROR(SUCCESS);
        }

        void deserialize(deserializer_t &reader) override
        {
            deserialize_prefix(reader);

            deserialize_body(reader);

            deserialize_data(reader);

            deserialize_suffix(reader);
        }

        JSON_FROM_FUNC(fromJSON) override
        {
            JSON_OBJECT_OR_THROW();

            prefix_fromJSON(j);

            body_fromJSON(j);

            data_fromJSON(j);

            suffix_fromJSON(j);
        }

        [[nodiscard]] crypto_hash_t digest() const
        {
            const auto data = serialize_digest();

            return Crypto::Hashing::sha3(data.data(), data.size());
        }

        [[nodiscard]] size_t committed_size() const
        {
            const auto data = serialize_digest();

            return data.size() + sizeof(crypto_hash_t) + sizeof(crypto_hash_t);
        }

        [[nodiscard]] crypto_hash_t hash() const override
        {
            serializer_t writer;

            writer.key(digest());

            /**
             * To make sure that both an uncommitted and committed transaction have the same
             * hash, we incorporate the signature_hash and range_proof_hash into the hash of
             * the uncommitted transaction here to make sure that we get the same result
             */

            writer.key(signature_hash());

            writer.key(range_proof_hash());

            return Crypto::Hashing::sha3(writer.data(), writer.size());
        }

        [[nodiscard]] bool mine(const uint8_t zeros = 0)
        {
            auto hash = pow_hash();

            if (hash.leading_zeros() >= zeros)
            {
                return true;
            }

            nonce = 0;

            while (hash.leading_zeros() < zeros && nonce != UINT64_MAX)
            {
                nonce++;

                hash = pow_hash();
            }

            return hash.leading_zeros() >= zeros;
        }

        [[nodiscard]] crypto_hash_t pow_hash() const
        {
            serializer_t writer;

            writer.bytes(serialize_digest());

            writer.key(range_proof_hash());

            const auto data = Crypto::Hashing::sha3(writer.data(), writer.size());

            return Crypto::Hashing::argon2id(
                data,
                Configuration::Transaction::ProofOfWork::ITERATIONS,
                Configuration::Transaction::ProofOfWork::MEMORY,
                Configuration::Transaction::ProofOfWork::THREADS);
        }

        [[nodiscard]] bool pow_verify(const uint8_t zeros = 0) const
        {
            return pow_hash().leading_zeros() >= zeros;
        }

        void serialize(serializer_t &writer) const override
        {
            serialize_prefix(writer);

            serialize_body(writer);

            serialize_data(writer);

            serialize_suffix(writer);
        }

        [[nodiscard]] std::vector<uint8_t> serialize() const override
        {
            serializer_t writer;

            serialize(writer);

            return writer.vector();
        }

        [[nodiscard]] std::vector<uint8_t> serialize_digest() const
        {
            serializer_t writer;

            serialize_prefix(writer);

            serialize_body(writer);

            serialize_data(writer);

            return writer.vector();
        }

        [[nodiscard]] size_t size() const override
        {
            const auto bytes = serialize();

            return bytes.size();
        }

        [[nodiscard]] committed_recall_stake_transaction_t to_committed() const
        {
            committed_recall_stake_transaction_t tx;

            tx.version = version;

            tx.unlock_block = unlock_block;

            tx.public_key = public_key;

            tx.nonce = nonce;

            tx.fee = fee;

            tx.key_images = key_images;

            tx.outputs = outputs;

            tx.staker_id = staker_id;

            tx.candidate_public_key = candidate_public_key;

            tx.stake_amount = stake_amount;

            tx.view_signature = view_signature;

            tx.spend_signature = spend_signature;

            tx.signature_hash = signature_hash();

            tx.range_proof_hash = range_proof_hash();

            return tx;
        }

        JSON_TO_FUNC(toJSON) override
        {
            writer.StartObject();
            {
                prefix_toJSON(writer);

                body_toJSON(writer);

                data_toJSON(writer);

                suffix_toJSON(writer);
            }
            writer.EndObject();
        }

        [[nodiscard]] std::string to_string() const override
        {
            const auto bytes = serialize();

            return Crypto::StringTools::to_hex(bytes.data(), bytes.size());
        }
    } uncommitted_recall_stake_transaction_t;
} // namespace Types::Blockchain

namespace std
{
    inline ostream &operator<<(ostream &os, const Types::Blockchain::committed_recall_stake_transaction_t &value)
    {
        const auto pow_hash = value.pow_hash();

        os << "Committed Recall Stake Transaction [" << value.size() << " bytes]" << std::endl
           << "\tHash: " << value.hash() << std::endl
           << "\tDigest: " << value.digest() << std::endl
           << "\tSignature Hash: " << value.signature_hash << std::endl
           << "\tRange Proof Hash: " << value.range_proof_hash << std::endl
           << "\tPoW Hash: " << pow_hash << std::endl
           << "\tPoW Zeros: " << pow_hash.leading_zeros() << std::endl
           << "\tVersion: " << value.version << std::endl
           << "\tUnlock Block: " << value.unlock_block << std::endl
           << "\tPublic Key: " << value.public_key << std::endl
           << "\tNonce: " << value.nonce << std::endl
           << "\tFee: " << value.fee << std::endl
           << "\tInput Offsets:" << std::endl

           << "\tInput Key Images:" << std::endl;

        for (const auto &key_image : value.key_images)
        {
            os << "\t\t" << key_image << std::endl;
        }

        os << std::endl << "\tOutputs:" << std::endl;

        for (const auto &output : value.outputs)
        {
            os << output << std::endl;
        }

        os << "\tStaker Recall Amount: " << std::to_string(value.stake_amount) << std::endl
           << "\tCandidate Public Key: " << value.candidate_public_key << std::endl
           << "\tStaker ID: " << value.staker_id << std::endl
           << "\tStaker View Signature: " << value.view_signature << std::endl
           << "\tStaker Spend Signature: " << value.spend_signature << std::endl
           << std::endl;

        return os;
    }

    inline ostream &operator<<(ostream &os, const Types::Blockchain::uncommitted_recall_stake_transaction_t &value)
    {
        const auto pow_hash = value.pow_hash();

        os << "Uncommitted Recall Stake Transaction [" << value.size() << " bytes]" << std::endl
           << "\tCommitted Size: " << value.committed_size() << " bytes" << std::endl
           << "\tHash: " << value.hash() << std::endl
           << "\tDigest: " << value.digest() << std::endl
           << "\tSignature Hash: " << value.signature_hash() << std::endl
           << "\tRange Proof Hash: " << value.range_proof_hash() << std::endl
           << "\tPoW Hash: " << pow_hash << std::endl
           << "\tPoW Zeros: " << pow_hash.leading_zeros() << std::endl
           << "\tVersion: " << value.version << std::endl
           << "\tUnlock Block: " << value.unlock_block << std::endl
           << "\tPublic Key: " << value.public_key << std::endl
           << "\tNonce: " << value.nonce << std::endl
           << "\tFee: " << value.fee << std::endl;

        os << "\tInput Key Images:" << std::endl;

        for (const auto &key_image : value.key_images)
        {
            os << "\t\t" << key_image << std::endl;
        }

        os << std::endl << "\tOutputs:" << std::endl;

        for (const auto &output : value.outputs)
        {
            os << output << std::endl;
        }

        os << "\tStaker Recall Amount: " << std::to_string(value.stake_amount) << std::endl
           << "\tCandidate Public Key: " << value.candidate_public_key << std::endl
           << "\tStaker ID: " << value.staker_id << std::endl
           << "\tStaker View Signature: " << value.view_signature << std::endl
           << "\tStaker Spend Signature: " << value.spend_signature << std::endl
           << std::endl;

        os << "\tRing Participants:" << std::endl;
        for (const auto &elem : value.ring_participants)
        {
            os << "\t\t" << elem << std::endl;
        }

        for (const auto &signature : value.signatures)
        {
            os << signature << std::endl;
        }

        os << value.range_proof << std::endl;

        return os;
    }
} // namespace std

#endif // TURTLECOIN_TRANSACTION_RECALL_STAKE_H
