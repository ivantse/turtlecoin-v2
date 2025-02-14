// Copyright (c) 2021, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#ifndef TURTLECOIN_BLOCK_H
#define TURTLECOIN_BLOCK_H

#include "base_types.h"
#include "transaction_genesis.h"
#include "transaction_staker.h"

#include <map>
#include <signature.h>

namespace Types::Blockchain
{
    typedef std::variant<Types::Blockchain::genesis_transaction_t, Types::Blockchain::staker_transaction_t>
        block_transaction_t;

    enum block_digest_mode_t
    {
        BLOCK_DIGEST_FULL,
        BLOCK_DIGEST_PRODUCER,
        BLOCK_DIGEST_VALIDATOR
    };

    struct block_t
    {
        block_t() {}

        block_t(deserializer_t &reader)
        {
            deserialize(reader);
        }

        block_t(std::initializer_list<uint8_t> input)
        {
            std::vector<uint8_t> data(input);

            auto reader = deserializer_t(data);

            deserialize(reader);
        }

        block_t(const std::vector<uint8_t> &data)
        {
            deserializer_t reader(data);

            deserialize(reader);
        }

        block_t(const std::string &hex)
        {
            deserializer_t reader(hex);

            deserialize(reader);
        }

        JSON_OBJECT_CONSTRUCTORS(block_t, fromJSON);

        /**
         * Helper method to simply insert a transaction hash into the block
         * @param hash
         */
        void append_transaction_hash(const crypto_hash_t &hash)
        {
            if (transactions.find(hash) == transactions.end())
            {
                transactions.insert(hash);
            }
        }

        /**
         * Helper method to simply add a validator public key and signature into the block
         * @param public_key
         * @param signature
         */
        void append_validator_signature(const crypto_public_key_t &public_key, const crypto_signature_t &signature)
        {
            if (validator_signatures.find(public_key) == validator_signatures.end())
            {
                validator_signatures.insert({public_key, signature});
            }
        }

        /**
         * Deserializes the block from the reader provided
         * @param reader
         */
        void deserialize(deserializer_t &reader)
        {
            version = reader.varint<uint64_t>();

            previous_blockhash = reader.key<crypto_hash_t>();

            timestamp = reader.varint<uint64_t>();

            block_index = reader.varint<uint64_t>();

            {
                const auto type = reader.varint<uint64_t>(true);

                switch (type)
                {
                    case BaseTypes::TransactionType::GENESIS:
                        reward_tx = genesis_transaction_t(reader);
                        break;
                    case BaseTypes::TransactionType::STAKER:
                        reward_tx = staker_transaction_t(reader);
                        break;
                    default:
                        throw std::invalid_argument("Invalid reward_tx type");
                }
            }

            // transactions
            {
                const auto count = reader.varint<uint64_t>();

                transactions.clear();

                for (size_t i = 0; i < count; ++i)
                {
                    transactions.insert(reader.key<crypto_hash_t>());
                }
            }

            const auto has_producer = reader.boolean();

            if (has_producer)
            {
                producer_public_key = reader.key<crypto_public_key_t>();

                producer_signature = reader.key<crypto_signature_t>();
            }

            // validator keys & signatures
            {
                const auto count = reader.varint<uint64_t>();

                validator_signatures.clear();

                for (size_t i = 0; i < count; ++i)
                {
                    const auto public_key = reader.key<crypto_public_key_t>();

                    const auto signature = reader.key<crypto_signature_t>();

                    validator_signatures.insert({public_key, signature});
                }
            }
        }

        /**
         * Deserializes the block from JSON encoded data
         * @param j
         */
        JSON_FROM_FUNC(fromJSON)
        {
            JSON_OBJECT_OR_THROW();

            LOAD_U64_FROM_JSON(version);

            LOAD_KEY_FROM_JSON(previous_blockhash);

            LOAD_U64_FROM_JSON(timestamp);

            LOAD_U64_FROM_JSON(block_index);

            JSON_MEMBER_OR_THROW("reward_tx");

            {
                const auto &elem = get_json_value(j, "reward_tx");

                const auto type = detect_json_type(elem);

                switch (type)
                {
                    case BaseTypes::TransactionType::GENESIS:
                        reward_tx = genesis_transaction_t(elem);
                        break;
                    case BaseTypes::TransactionType::STAKER:
                        reward_tx = staker_transaction_t(elem);
                        break;
                    default:
                        throw std::invalid_argument("Invalid reward_tx type");
                }
            }

            JSON_MEMBER_OR_THROW("transactions");

            transactions.clear();

            for (const auto &elem : get_json_array(j, "transactions"))
            {
                crypto_hash_t hash = get_json_string(elem);

                transactions.insert(hash);
            }

            if (has_member(j, "producer_signature"))
            {
                const auto &elem = get_json_value(j, "producer_signature");

                if (!has_member(elem, "public_key") || !has_member(elem, "signature"))
                {
                    throw std::invalid_argument("producer signature object does not contain proper JSON values");
                }

                producer_public_key = get_json_string(elem, "public_key");

                producer_signature = get_json_string(elem, "signature");
            }

            if (has_member(j, "validator_signatures"))
            {
                validator_signatures.clear();

                for (const auto &elem : get_json_array(j, "validator_signatures"))
                {
                    if (!has_member(elem, "public_key") || !has_member(elem, "signature"))
                    {
                        throw std::invalid_argument("validator signatures object does not contain proper JSON values");
                    }

                    crypto_public_key_t public_key = get_json_string(elem, "public_key");

                    crypto_signature_t signature = get_json_string(elem, "signature");

                    append_validator_signature(public_key, signature);
                }
            }
        }

        /**
         * Calculates the hash of the block
         * @return
         */
        [[nodiscard]] crypto_hash_t hash() const
        {
            return message_digest();
        }

        /**
         * Provides the index of the block
         * @return
         */
        [[nodiscard]] uint64_t index() const
        {
            return block_index;
        }

        /**
         * Tells us if the block is a genesis block (contains a genesis transaction)
         * @return
         */
        [[nodiscard]] bool is_genesis() const
        {
            return std::visit(
                [](auto &&arg)
                {
                    using T = std::decay_t<decltype(arg)>;

                    if constexpr (std::is_same_v<T, genesis_transaction_t>)
                    {
                        return true;
                    }
                    else
                    {
                        return false;
                    }
                },
                reward_tx);
        }

        /**
         * Calculates the message digest (multiple forms available) used for producer
         * and validator signing methods
         * @param mode
         * @return
         */
        [[nodiscard]] crypto_hash_t message_digest(const block_digest_mode_t mode = BLOCK_DIGEST_FULL) const
        {
            const auto bytes = serialize(mode);

            return Crypto::Hashing::sha3(bytes.data(), bytes.size());
        }

        /**
         * Adds the producer signature and public key to the block using the provided secret key
         * The public key and signature are also returned by the method
         * @param secret_key
         * @return
         */
        std::tuple<crypto_public_key_t, crypto_signature_t> producer_sign(const crypto_secret_key_t &secret_key)
        {
            producer_public_key = Crypto::secret_key_to_public_key(secret_key);

            const auto digest = message_digest(BLOCK_DIGEST_PRODUCER);

            producer_signature = Crypto::Signature::generate_signature(digest, secret_key);

            return {producer_public_key, producer_signature};
        }

        /**
         * Serializes the block using the provided writer
         * @param writer
         * @param mode
         */
        void serialize(serializer_t &writer, const block_digest_mode_t mode = BLOCK_DIGEST_FULL) const
        {
            writer.varint(version);

            writer.key(previous_blockhash);

            writer.varint(timestamp);

            writer.varint(block_index);

            std::visit([&writer](auto &&arg) { arg.serialize(writer); }, reward_tx);

            writer.varint(transactions.size());

            for (const auto &tx : transactions)
            {
                tx.serialize(writer);
            }

            if (mode == BLOCK_DIGEST_PRODUCER)
            {
                return;
            }

            const auto has_producer = (producer_public_key != Crypto::Z && !producer_signature.empty());

            writer.boolean(has_producer);

            if (has_producer)
            {
                producer_public_key.serialize(writer);

                producer_signature.serialize(writer);
            }
            else
            {
                if (mode == BLOCK_DIGEST_VALIDATOR)
                {
                    throw std::runtime_error("cannot create validator digest without producer signature");
                }
            }

            if (mode == BLOCK_DIGEST_VALIDATOR)
            {
                return;
            }

            writer.varint(validator_signatures.size());

            for (const auto &[public_key, signature] : validator_signatures)
            {
                writer.key(public_key);

                writer.key(signature);
            }
        }

        /**
         * Serializes the block to a vector of bytes (uint8_t)
         * @param mode
         * @return
         */
        [[nodiscard]] std::vector<uint8_t> serialize(const block_digest_mode_t mode = BLOCK_DIGEST_FULL) const
        {
            serializer_t writer;

            serialize(writer, mode);

            return writer.vector();
        }

        /**
         * Provides the size of the block in bytes
         * @return
         */
        [[nodiscard]] size_t size() const
        {
            const auto bytes = serialize();

            return bytes.size();
        }

        /**
         * Serializes the block to JSON
         * @param writer
         */
        JSON_TO_FUNC(toJSON)
        {
            writer.StartObject();
            {
                U64_TO_JSON(version);

                KEY_TO_JSON(previous_blockhash);

                U64_TO_JSON(timestamp);

                U64_TO_JSON(block_index);

                writer.Key("reward_tx");
                std::visit([&writer](auto &&arg) { arg.toJSON(writer); }, reward_tx);

                writer.Key("transactions");
                writer.StartArray();
                {
                    for (const auto &tx : transactions)
                    {
                        tx.toJSON(writer);
                    }
                }
                writer.EndArray();

                const auto has_producer = (producer_public_key != Crypto::Z && !producer_signature.empty());

                if (has_producer)
                {
                    writer.Key("producer_signature");
                    writer.StartObject();
                    {
                        writer.Key("public_key");
                        producer_public_key.toJSON(writer);

                        writer.Key("signature");
                        producer_signature.toJSON(writer);
                    }
                    writer.EndObject();
                }

                if (!validator_signatures.empty())
                {
                    writer.Key("validator_signatures");
                    writer.StartArray();
                    {
                        for (const auto &[public_key, signature] : validator_signatures)
                        {
                            writer.StartObject();
                            {
                                writer.Key("public_key");
                                producer_public_key.toJSON(writer);

                                writer.Key("signature");
                                producer_signature.toJSON(writer);
                            }
                            writer.EndObject();
                        }
                    }
                    writer.EndArray();
                }
            }
            writer.EndObject();
        }

        /**
         * Serializes the block to a hexadecimal encoded string
         * @return
         */
        [[nodiscard]] std::string to_string() const
        {
            const auto bytes = serialize();

            return Crypto::StringTools::to_hex(bytes.data(), bytes.size());
        }

        /**
         * Quick and dirty checks to validate that the construction of the block is correct.
         * It does not; however, verify that the proper parties have signed the block or
         * that the resulting coinbase transaction was constructed correctly (correct recipients, etc).
         * @return
         */
        [[nodiscard]] bool validate_construction() const
        {
            if (!std::visit(
                    [](auto &&arg)
                    {
                        using T = std::decay_t<decltype(arg)>;

                        if constexpr (std::is_same_v<T, Blockchain::genesis_transaction_t>)
                        {
                            return !arg.outputs.empty();
                        }
                        else if constexpr (std::is_same_v<T, Blockchain::staker_transaction_t>)
                        {
                            return !arg.staker_outputs.empty();
                        }
                        else
                        {
                            return true;
                        }
                    },
                    reward_tx))
            {
                return false;
            }

            // producer may not validate their own blocks
            if (validator_signatures.find(producer_public_key) != validator_signatures.end())
            {
                return false;
            }

            // check the producer signature
            if (!validate_producer_signature())
            {
                return false;
            }

            // check the validator signatures
            return validate_validator_signatures();
        }

        /**
         * Validates that the producer signature contained within the block is valid. It does not;
         * however, validate that producer was permitted to sign the block. That logic needs
         * handled elsewhere in the software.
         * @return
         */
        [[nodiscard]] bool validate_producer_signature() const
        {
            const auto digest = message_digest(BLOCK_DIGEST_PRODUCER);

            return Crypto::Signature::check_signature(digest, producer_public_key, producer_signature);
        }

        /**
         * Loops through all validator signatures contained within the block to verify
         * that the block has been properly signed. It does not; however, validate
         * that only the permitted validators have signed the block. That logic needs
         * handled elsewhere in the software.
         * @return
         */
        [[nodiscard]] bool validate_validator_signatures() const
        {
            if (validator_signatures.empty())
            {
                return false;
            }

            const auto digest = message_digest(BLOCK_DIGEST_VALIDATOR);

            for (const auto &[public_key, signature] : validator_signatures)
            {
                if (!Crypto::Signature::check_signature(digest, public_key, signature))
                {
                    return false;
                }
            }

            return true;
        }

        /**
         * Adds a validator signature and public key to the block using the provided secret key
         * The public key and signature are also returned by the method
         * @param secret_key
         * @return
         */
        std::tuple<crypto_public_key_t, crypto_signature_t> validator_sign(const crypto_secret_key_t &secret_key)
        {
            const auto public_key = Crypto::secret_key_to_public_key(secret_key);

            const auto digest = message_digest(BLOCK_DIGEST_VALIDATOR);

            const auto signature = Crypto::Signature::generate_signature(digest, secret_key);

            if (validator_signatures.find(public_key) == validator_signatures.end())
            {
                validator_signatures.insert({public_key, signature});
            }

            return {public_key, signature};
        }

        uint64_t version = 1, timestamp = 0, block_index = 0;
        crypto_hash_t previous_blockhash;

        // default to a staker reward transaction as there should only ever be one genesis transaction
        Blockchain::block_transaction_t reward_tx = staker_transaction_t();

        /**
         * Transaction hashes must be properly ordered in a block using standard sorting
         * to ensure consistency in calculating the block message digest
         */
        std::set<crypto_hash_t> transactions;
        crypto_public_key_t producer_public_key;
        crypto_signature_t producer_signature;

        /**
         * Validator signatures of the block must be properly ordered using standard sorting
         * to ensure consistency in the final block hash
         */
        std::map<crypto_public_key_t, crypto_signature_t> validator_signatures;

      private:
        uint64_t detect_json_type(const JSONValue &j)
        {
            JSON_OBJECT_OR_THROW();

            JSON_MEMBER_OR_THROW("type");

            return get_json_uint64_t(j, "type");
        }
    };
} // namespace Types::Blockchain

namespace std
{
    inline ostream &operator<<(ostream &os, const Types::Blockchain::block_t &value)
    {
        os << "Block [" << value.size() << " bytes]" << std::endl
           << "\tHash: " << value.hash() << std::endl
           << "\tVersion: " << value.version << std::endl
           << "\tPrevious Block Hash: " << value.previous_blockhash << std::endl
           << "\tTimestamp: " << value.timestamp << std::endl
           << "\tBlock Index: " << value.block_index << std::endl
           << std::endl;

        std::visit([&os](auto &&arg) { os << arg; }, value.reward_tx);

        os << std::endl << std::endl << "\tBlock Transactions:" << std::endl;

        for (const auto &tx : value.transactions)
        {
            os << "\t\t" << tx << std::endl;
        }

        os << "\tProducer Public Key: " << value.producer_public_key << std::endl
           << "\tProducer Signature: " << value.producer_signature << std::endl
           << "\tValidators:" << std::endl;

        for (const auto &[public_key, signature] : value.validator_signatures)
        {
            os << "\t\tPublic Key: " << public_key << std::endl
               << "\t\tSignature: " << signature << std::endl
               << std::endl;
        }

        return os;
    }
} // namespace std

#endif // TURTLECOIN_BLOCK_H
