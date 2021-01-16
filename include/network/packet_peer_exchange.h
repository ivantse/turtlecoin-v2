// Copyright (c) 2021, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#ifndef TURTLECOIN_NETWORK_PACKET_PEER_EXCHANGE_H
#define TURTLECOIN_NETWORK_PACKET_PEER_EXCHANGE_H

#include "base_types.h"

namespace TurtleCoin::Types::Network
{
    struct packet_peer_exchange_t : TurtleCoin::BaseTypes::NetworkPacket
    {
        packet_peer_exchange_t()
        {
            type = 1200;
        }

        packet_peer_exchange_t(deserializer_t &reader)
        {
            deserialize(reader);
        }

        packet_peer_exchange_t(std::initializer_list<uint8_t> input)
        {
            std::vector<uint8_t> data(input);

            auto reader = deserializer_t(data);

            deserialize(reader);
        }

        packet_peer_exchange_t(const std::vector<uint8_t> &data)
        {
            deserializer_t reader(data);

            deserialize(reader);
        }

        void serialize(serializer_t &writer) const
        {
            writer.varint(type);

            writer.varint(version);

            writer.varint(peers.size());

            for (const auto &peer : peers)
            {
                peer.serialize(writer);
            }
        }

        [[nodiscard]] std::vector<uint8_t> serialize() const
        {
            auto writer = serializer_t();

            serialize(writer);

            return writer.vector();
        }

        [[nodiscard]] size_t size() const
        {
            return serialize().size();
        }

        [[nodiscard]] std::string to_string() const
        {
            const auto bytes = serialize();

            return Crypto::StringTools::to_hex(bytes.data(), bytes.size());
        }

        std::vector<network_peer_t> peers;

      private:
        void deserialize(deserializer_t &reader)
        {
            type = reader.varint<uint16_t>();

            version = reader.varint<uint16_t>();

            const auto peer_count = reader.varint<uint64_t>();

            peers.clear();

            for (size_t i = 0; i < peer_count; ++i)
            {
                const auto peer = network_peer_t(reader);

                peers.push_back(peer);
            }
        }
    };
} // namespace TurtleCoin::Types::Network

namespace std
{
    inline ostream &operator<<(ostream &os, const TurtleCoin::Types::Network::packet_peer_exchange_t &value)
    {
        os << "Handshake Packet [" << value.size() << " bytes]" << std::endl
           << "\tType: " << std::to_string(value.type) << std::endl
           << "\tVersion: " << std::to_string(value.version) << std::endl
           << "\tPeers: " << std::endl;

        for (const auto &peer : value.peers)
        {
            os << peer << std::endl;
        }

        return os;
    }
} // namespace std

#endif // TURTLECOIN_NETWORK_PACKET_PEER_EXCHANGE_H
