// Copyright (c) 2021, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#ifndef TURTLECOIN_IP_ADDRESS_H
#define TURTLECOIN_IP_ADDRESS_H

#include <ipv6.h>
#include <serializable.h>

/**
 * This structure is here to wrap the IPv6 address type that is provided
 * via the external package as it allows to easily parse v4/v6 addresses
 */
struct ip_address_t : virtual BaseTypes::IStorable
{
  public:
    ip_address_t() {}

    ip_address_t(const std::string &address)
    {
        const auto [error, addr] = IPParser::str_to_ip(address);

        if (error)
        {
            throw std::invalid_argument("Could not parse IP address from string");
        }

        m_address = addr;

        normalize_v4_embedded();
    }

    ip_address_t(deserializer_t &reader)
    {
        deserialize(reader);

        normalize_v4_embedded();
    }

    ip_address_t(const std::vector<uint8_t> &data)
    {
        deserializer_t reader(data);

        deserialize(reader);

        normalize_v4_embedded();
    }

    JSON_OBJECT_CONSTRUCTORS(ip_address_t, fromJSON);

    void deserialize(deserializer_t &reader) override
    {
        m_address = {};

        for (size_t i = 0; i < IPV6_NUM_COMPONENTS; ++i)
        {
            m_address.address.components[i] = reader.varint<uint16_t>();
        }

        m_address.flags = reader.varint<uint32_t>();

        normalize_v4_embedded();
    }

    JSON_FROM_FUNC(fromJSON) override
    {
        JSON_OBJECT_OR_THROW();

        JSON_MEMBER_OR_THROW("address");

        const auto str = get_json_string(j, "address");

        const auto [error, address] = IPParser::str_to_ip(str);

        if (error)
        {
            throw std::invalid_argument("Could not parse IP address from JSON");
        }

        m_address = address;

        normalize_v4_embedded();
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

    /**
     * Returns if the IP address is a v4 address
     *
     * @return
     */
    [[nodiscard]] bool is_v4() const
    {
        bool result = true;

        for (size_t i = 4; i < IPV6_NUM_COMPONENTS; ++i)
        {
            if (m_address.address.components[i] != 0)
            {
                result = false;
            }
        }

        return result;
    }

    /**
     * Returns if the IP address is a v6 address
     *
     * @return
     */
    [[nodiscard]] bool is_v6() const
    {
        return !is_v4();
    }

    [[nodiscard]] uint16_t port() const
    {
        return m_address.port;
    }

    void serialize(serializer_t &writer) const override
    {
        for (size_t i = 0; i < IPV6_NUM_COMPONENTS; ++i)
        {
            const auto &x = m_address.address.components[i];

            writer.varint(x);
        }

        writer.varint(m_address.flags);
    }

    [[nodiscard]] std::vector<uint8_t> serialize() const override
    {
        auto writer = serializer_t();

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
            writer.Key("address");
            writer.String(to_string());
        }
        writer.EndObject();
    }

    [[nodiscard]] std::string to_string() const override
    {
        const auto [error, str] = IPParser::ip_to_str(m_address);

        if (error)
        {
            return std::string();
        }

        return "[" + str + "]";
    }

    [[nodiscard]] uint64_t type() const override
    {
        return 0;
    }

  private:
    void normalize_v4_embedded()
    {
        /**
         * If we read a v4 address and it reads it as compatible (xxx.xxx.xxx.xxx)
         * we need to convert it to embedded (::ffff:xxx.xxx.xxx.xxx) for our own
         * sanity
         */
        if (m_address.flags & IPParser::ipv6_flag_t::IPV6_FLAG_IPV4_COMPAT)
        {
            m_address.flags |= IPParser::ipv6_flag_t::IPV6_FLAG_IPV4_EMBED;

            m_address.flags &= ~(IPParser::ipv6_flag_t::IPV6_FLAG_IPV4_COMPAT);

            auto &comp = m_address.address.components;

            // shift the byes to the end
            for (size_t i = 0, k = IPV4_EMBED_INDEX; i < IPV4_NUM_COMPONENTS; ++i, ++k)
            {
                comp[k] = comp[i];

                comp[i] = 0;
            }

            // set the preceding byte to max value
            comp[IPV4_EMBED_INDEX - 1] = 0xffff;
        }
    }

    IPParser::ipv6_address_full_t m_address = {};
};

namespace std
{
    inline ostream &operator<<(ostream &os, const ip_address_t &value)
    {
        os << value.to_string();

        return os;
    }
} // namespace std

#endif
