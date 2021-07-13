// Copyright (c) 2021, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#ifndef TURTLECOIN_TRANSACTION_STAKE_REFUND_H
#define TURTLECOIN_TRANSACTION_STAKE_REFUND_H

#include "base_types.h"

namespace Types::Blockchain
{
    struct stake_refund_transaction_t : BaseTypes::TransactionPrefix, virtual BaseTypes::ITransaction
    {
        stake_refund_transaction_t()
        {
            l_type = BaseTypes::TransactionType::STAKE_REFUND;
        }

        stake_refund_transaction_t(deserializer_t &reader)
        {
            deserialize(reader);
        }

        stake_refund_transaction_t(std::initializer_list<uint8_t> input)
        {
            std::vector<uint8_t> data(input);

            auto reader = deserializer_t(data);

            deserialize(reader);
        }

        stake_refund_transaction_t(const std::vector<uint8_t> &data)
        {
            deserializer_t reader(data);

            deserialize(reader);
        }

        stake_refund_transaction_t(const std::string &hex)
        {
            deserializer_t reader(hex);

            deserialize(reader);
        }

        JSON_OBJECT_CONSTRUCTORS(stake_refund_transaction_t, fromJSON)

        [[nodiscard]] Error check_construction() const override
        {
            if (version != 1)
            {
                return MAKE_ERROR(TX_INVALID_VERSION);
            }

            if (public_key.empty())
            {
                return MAKE_ERROR(TX_PUBLIC_KEY);
            }

            if (secret_key.empty())
            {
                return MAKE_ERROR(TX_SECRET_KEY);
            }

            if (Crypto::secret_key_to_public_key(secret_key) != public_key)
            {
                return MAKE_ERROR(TX_KEYPAIR_MISMATCH);
            }

            if (recall_stake_tx.empty())
            {
                return MAKE_ERROR(TX_RECALL_STAKE_TX_HASH);
            }

            if (outputs.size() != 1)
            {
                return MAKE_ERROR(TX_INVALID_OUTPUT_COUNT);
            }

            for (const auto &output : outputs)
            {
                auto error = output.check_construction();

                if (error)
                {
                    return error;
                }
            }

            return MAKE_ERROR(SUCCESS);
        }

        void deserialize(deserializer_t &reader) override
        {
            deserialize_prefix(reader);

            secret_key = reader.key<crypto_secret_key_t>();

            recall_stake_tx = reader.key<crypto_hash_t>();

            // outputs
            {
                const auto count = reader.varint<uint64_t>();

                outputs.clear();

                for (size_t i = 0; i < count; ++i)
                {
                    outputs.emplace_back(reader);
                }
            }
        }

        JSON_FROM_FUNC(fromJSON) override
        {
            JSON_OBJECT_OR_THROW();

            prefix_fromJSON(j);

            LOAD_KEY_FROM_JSON(secret_key);

            LOAD_KEY_FROM_JSON(recall_stake_tx);

            JSON_MEMBER_OR_THROW("outputs");

            outputs.clear();

            for (const auto &elem : get_json_array(j, "outputs"))
            {
                outputs.emplace_back(elem);
            }
        }

        [[nodiscard]] crypto_hash_t hash() const override
        {
            const auto bytes = serialize();

            return Crypto::Hashing::sha3(bytes.data(), bytes.size());
        }

        void serialize(serializer_t &writer) const override
        {
            serialize_prefix(writer);

            secret_key.serialize(writer);

            recall_stake_tx.serialize(writer);

            writer.varint(outputs.size());

            for (const auto &output : outputs)
            {
                output.serialize(writer);
            }
        }

        [[nodiscard]] std::vector<uint8_t> serialize() const override
        {
            serializer_t writer;

            serialize(writer);

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

                KEY_TO_JSON(secret_key);

                KEY_TO_JSON(recall_stake_tx);

                writer.Key("outputs");
                writer.StartArray();
                {
                    for (const auto &output : outputs)
                    {
                        output.toJSON(writer);
                    }
                }
                writer.EndArray();
            }
            writer.EndObject();
        }

        [[nodiscard]] std::string to_string() const override
        {
            const auto bytes = serialize();

            return Crypto::StringTools::to_hex(bytes.data(), bytes.size());
        }

        crypto_secret_key_t secret_key;
        crypto_hash_t recall_stake_tx;
        std::vector<transaction_output_t> outputs;
    };
} // namespace Types::Blockchain

namespace std
{
    inline ostream &operator<<(ostream &os, const Types::Blockchain::stake_refund_transaction_t &value)
    {
        os << "Stake Refund Transaction [" << value.size() << " bytes]" << std::endl
           << "\tHash: " << value.hash() << std::endl
           << "\tVersion: " << value.version << std::endl
           << "\tUnlock Block: " << value.unlock_block << std::endl
           << "\tPublic Key: " << value.public_key << std::endl
           << "\tSecret key: " << value.secret_key << std::endl
           << "\tRecall Stake Tx: " << value.recall_stake_tx << std::endl;

        os << std::endl << "\tOutputs:" << std::endl;

        for (const auto &output : value.outputs)
            os << output << std::endl;

        return os;
    }
} // namespace std

#endif // TURTLECOIN_TRANSACTION_STAKE_REFUND_H
