// Copyright (c) 2021, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#ifndef TURTLECOIN_STAKE_H
#define TURTLECOIN_STAKE_H

#include <crypto_types.h>

namespace Types::Staking
{
    struct stake_t : virtual BaseTypes::IStorable
    {
        stake_t() {}

        stake_t(
            const crypto_public_key_t &public_view_key,
            const crypto_public_key_t &public_spend_key,
            uint64_t stake):
            public_view_key(public_view_key), public_spend_key(public_spend_key), stake(stake)
        {
        }

        stake_t(std::initializer_list<uint8_t> input)
        {
            std::vector<uint8_t> data(input);

            auto reader = deserializer_t(data);

            deserialize(reader);
        }

        stake_t(const std::vector<uint8_t> &data)
        {
            auto reader = deserializer_t(data);

            deserialize(reader);
        }

        stake_t(deserializer_t &reader)
        {
            deserialize(reader);
        }

        JSON_OBJECT_CONSTRUCTORS(stake_t, fromJSON);

        void deserialize(deserializer_t &reader) override
        {
            record_version = reader.varint<uint64_t>();

            candidate_public_key = reader.key<crypto_public_key_t>();

            public_view_key = reader.key<crypto_public_key_t>();

            public_spend_key = reader.key<crypto_public_key_t>();

            stake = reader.varint<uint64_t>();
        }

        JSON_FROM_FUNC(fromJSON) override
        {
            JSON_OBJECT_OR_THROW();

            LOAD_U64_FROM_JSON(record_version);

            LOAD_KEY_FROM_JSON(candidate_public_key);

            LOAD_KEY_FROM_JSON(public_view_key);

            LOAD_KEY_FROM_JSON(public_spend_key);

            LOAD_U64_FROM_JSON(stake);
        }

        /**
         * Calculates the hash of the structure
         * @return
         */
        [[nodiscard]] crypto_hash_t hash() const override
        {
            const auto data = serialize();

            return Crypto::Hashing::sha3(data.data(), data.size());
        }

        [[nodiscard]] crypto_hash_t id() const
        {
            serializer_t writer;

            public_view_key.serialize(writer);

            public_spend_key.serialize(writer);

            return Crypto::Hashing::sha3(writer.data(), writer.size());
        }

        void serialize(serializer_t &writer) const override
        {
            writer.varint(record_version);

            candidate_public_key.serialize(writer);

            public_view_key.serialize(writer);

            public_spend_key.serialize(writer);

            writer.varint(stake);
        }

        [[nodiscard]] std::vector<uint8_t> serialize() const override
        {
            serializer_t writer;

            serialize(writer);

            return writer.vector();
        }

        [[nodiscard]] size_t size() const override
        {
            return serialize().size();
        }

        JSON_TO_FUNC(toJSON) override
        {
            writer.StartObject();
            {
                U64_TO_JSON(record_version);

                KEY_TO_JSON(candidate_public_key);

                KEY_TO_JSON(public_view_key);

                KEY_TO_JSON(public_spend_key);

                U64_TO_JSON(stake);
            }
            writer.EndObject();
        }

        [[nodiscard]] std::string to_string() const override
        {
            const auto bytes = serialize();

            return Crypto::StringTools::to_hex(bytes.data(), bytes.size());
        }

        [[nodiscard]] uint64_t type() const override
        {
            return 0;
        }

        [[nodiscard]] uint64_t version() const
        {
            return record_version;
        }

        crypto_public_key_t candidate_public_key, public_spend_key, public_view_key;

        uint64_t stake = 0;

      private:
        /**
         * This allows us to signify updates to the record schema in the future
         */
        uint64_t record_version = Configuration::Staking::STAKE_RECORD_VERSION;
    };
} // namespace Types::Staking

namespace std
{
    inline ostream &operator<<(ostream &os, const Types::Staking::stake_t &value)
    {
        os << "Stake [v" << value.version() << "]" << std::endl
           << "Staker ID: " << value.id() << std::endl
           << "Candidate Public Key: " << value.candidate_public_key << std::endl
           << "Staker Public View Key: " << value.public_view_key << std::endl
           << "Staker Public Spend Key: " << value.public_spend_key << std::endl
           << "Stake Amount: " << value.stake << std::endl;

        return os;
    }
} // namespace std

#endif // TURTLECOIN_STAKE_H
