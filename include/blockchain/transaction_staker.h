// Copyright (c) 2021, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#ifndef TURTLECOIN_TRANSACTION_STAKER_H
#define TURTLECOIN_TRANSACTION_STAKER_H

#include "base_types.h"

namespace Types::Blockchain
{
    struct staker_transaction_t : BaseTypes::TransactionHeader, virtual BaseTypes::ITransaction
    {
        staker_transaction_t()
        {
            l_type = BaseTypes::TransactionType::STAKER;
        }

        staker_transaction_t(deserializer_t &reader)
        {
            deserialize(reader);
        }

        staker_transaction_t(const std::vector<uint8_t> &data)
        {
            deserializer_t reader(data);

            deserialize(reader);
        }

        staker_transaction_t(std::initializer_list<uint8_t> input)
        {
            std::vector<uint8_t> data(input);

            auto reader = deserializer_t(data);

            deserialize(reader);
        }

        staker_transaction_t(const std::string &hex)
        {
            deserializer_t reader(hex);

            deserialize(reader);
        }

        JSON_OBJECT_CONSTRUCTORS(staker_transaction_t, fromJSON)

        [[nodiscard]] Error check_construction() const override
        {
            if (version != 1)
            {
                return MAKE_ERROR(TX_INVALID_VERSION);
            }

            for (const auto &output : staker_outputs)
            {
                auto error = output.check_construction();

                if (error)
                {
                    return error;
                }
            }

            for (const auto &output : staker_penalties)
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
            deserialize_header(reader);

            // Staker Outputs
            {
                const auto count = reader.varint<uint64_t>();

                staker_outputs.clear();

                for (size_t i = 0; i < count; ++i)
                {
                    staker_outputs.emplace_back(reader);
                }
            }

            // Staker Penalties
            {
                const auto count = reader.varint<uint64_t>();

                staker_penalties.clear();

                for (size_t i = 0; i < count; ++i)
                {
                    staker_penalties.emplace_back(reader);
                }
            }
        }

        JSON_FROM_FUNC(fromJSON) override
        {
            JSON_OBJECT_OR_THROW();

            header_fromJSON(j);

            JSON_MEMBER_OR_THROW("staker_outputs");

            staker_outputs.clear();

            for (const auto &elem : get_json_array(j, "staker_outputs"))
            {
                staker_outputs.emplace_back(elem);
            }

            JSON_MEMBER_OR_THROW("staker_penalties");

            staker_penalties.clear();

            for (const auto &elem : get_json_array(j, "staker_penalties"))
            {
                staker_penalties.emplace_back(elem);
            }
        }

        [[nodiscard]] crypto_hash_t hash() const override
        {
            const auto bytes = serialize();

            return Crypto::Hashing::sha3(bytes.data(), bytes.size());
        }

        void serialize(serializer_t &writer) const override
        {
            serialize_header(writer);

            writer.varint(staker_outputs.size());

            for (const auto &staker_output : staker_outputs)
            {
                staker_output.serialize(writer);
            }

            writer.varint(staker_penalties.size());

            for (const auto &staker_output : staker_penalties)
            {
                staker_output.serialize(writer);
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
                header_toJSON(writer);

                writer.Key("staker_outputs");
                writer.StartArray();
                {
                    for (const auto &staker_output : staker_outputs)
                    {
                        staker_output.toJSON(writer);
                    }
                }
                writer.EndArray();

                writer.Key("staker_penalties");
                writer.StartArray();
                {
                    for (const auto &staker_output : staker_penalties)
                    {
                        staker_output.toJSON(writer);
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

        std::vector<staker_output_t> staker_outputs;

        std::vector<staker_output_t> staker_penalties;
    };
} // namespace Types::Blockchain

namespace std
{
    inline ostream &operator<<(ostream &os, const Types::Blockchain::staker_transaction_t &value)
    {
        os << "Staker Transaction [" << value.size() << " bytes]" << std::endl
           << "\tHash: " << value.hash() << std::endl
           << "\tVersion: " << value.version << std::endl
           << "\tOutputs:" << std::endl;

        for (const auto &output : value.staker_outputs)
        {
            os << output << std::endl;
        }

        os << "\tPenalties:" << std::endl;

        for (const auto &output : value.staker_penalties)
        {
            os << output << std::endl;
        }

        return os;
    }
} // namespace std

#endif
