// Copyright (c) 2021, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#include "utilities.h"

#include "colors.h"

#include <algorithm>
#include <iostream>
#include <network/ip_address.h>
#include <serializer.h>

namespace Utilities
{
    std::tuple<std::string, uint16_t, crypto_hash_t> normalize_host_port(std::string host)
    {
        uint16_t port = 0;

        /**
         * ZMQ likes to include <proto>:// at the front of all host strings
         * we need to remove it for our own sanity as we only support TCP today
         */
        {
            const auto token = std::string("//");

            const auto pos = host.find(token);

            if (pos != std::string::npos)
            {
                host = host.substr(pos + token.size());
            }
        }

        /**
         * If there is a semicolon in the host, then it could contain the port number
         */
        if (host.find(':') != std::string::npos)
        {
            // separate by the semicolon
            auto parts = str_split(host, ':');

            // grab the past one as a possible port
            const auto _port = parts.back();

            // does the "port" contain a .? If not, then it's likely a port
            if (_port.find('.') == std::string::npos)
            {
                // try to convert the string to a uint16_t
                try
                {
                    port = std::stoi(_port);

                    parts.pop_back();
                }
                catch (...)
                {
                }
            }

            // join the host back together
            host = str_join(parts, ':');
        }

        // load the host into the v6 parser to help normalize it
        {
            auto addr = ip_address_t(std::string(host));

            host = addr.to_string();
        }

        serializer_t writer;

        writer.bytes(host.data(), host.size());

        writer.varint(port);

        const auto hash = Crypto::Hashing::sha3(writer.data(), writer.size());

        return {host, port, hash};
    }

    std::tuple<std::string, uint16_t, crypto_hash_t> normalize_host_port(const std::string &host, uint16_t port)
    {
        const auto [_host, _port, _hash] = normalize_host_port(host);

        serializer_t writer;

        writer.bytes(_host.data(), _host.size());

        writer.varint(port);

        const auto hash = Crypto::Hashing::sha3(writer.data(), writer.size());

        return {_host, port, hash};
    }

    void print_table(std::vector<std::string> rows, bool has_header)
    {
        size_t long_left = 0;

        for (const auto &elem : rows)
        {
            long_left = std::max(long_left, elem.length());
        }

        size_t total_width = long_left + 4;

        std::cout << COLOR::white << std::string(total_width, '=') << COLOR::reset << std::endl;

        if (has_header)
        {
            const auto &elem = rows.front();

            std::cout << COLOR::white << "| " << COLOR::yellow << str_pad(elem, long_left) << COLOR::white << " |"
                      << std::endl;

            std::cout << COLOR::white << std::string(total_width, '=') << COLOR::reset << std::endl;

            rows.erase(rows.begin());
        }

        for (const auto &elem : rows)
        {
            std::cout << COLOR::white << "| " << COLOR::yellow << str_pad(elem, long_left) << COLOR::white << " |"
                      << std::endl;
        }

        std::cout << COLOR::white << std::string(total_width, '=') << COLOR::reset << std::endl << std::endl;
    }

    void print_table(std::vector<std::tuple<std::string, std::string>> rows, bool has_header)
    {
        size_t long_left = 0, long_right = 0;

        for (const auto &[left, right] : rows)
        {
            long_left = std::max(long_left, left.length());

            long_right = std::max(long_right, right.length());
        }

        size_t total_width = long_left + long_right + 7;

        std::cout << COLOR::white << std::string(total_width, '=') << COLOR::reset << std::endl;

        if (has_header)
        {
            const auto &[left, right] = rows.front();

            std::cout << COLOR::white << "| " << COLOR::yellow << str_pad(left, long_left) << COLOR::white << " | "
                      << COLOR::green << str_pad(right, long_right) << COLOR::white << " |" << std::endl;

            std::cout << COLOR::white << std::string(total_width, '=') << COLOR::reset << std::endl;

            rows.erase(rows.begin());
        }

        for (const auto &[left, right] : rows)
        {
            std::cout << COLOR::white << "| " << COLOR::yellow << str_pad(left, long_left) << COLOR::white << " | "
                      << COLOR::green << str_pad(right, long_right) << COLOR::white << " |" << std::endl;
        }

        std::cout << COLOR::white << std::string(total_width, '=') << COLOR::reset << std::endl << std::endl;
    }

    void print_table(std::vector<std::tuple<std::string, std::string, std::string>> rows, bool has_header)
    {
        size_t long_left = 0, long_middle = 0, long_right = 0;

        for (const auto &[left, middle, right] : rows)
        {
            long_left = std::max(long_left, left.length());

            long_middle = std::max(long_middle, middle.length());

            long_right = std::max(long_right, right.length());
        }

        size_t total_width = long_left + long_middle + long_right + 10;

        std::cout << COLOR::white << std::string(total_width, '=') << COLOR::reset << std::endl;

        if (has_header)
        {
            const auto &[left, middle, right] = rows.front();

            std::cout << COLOR::white << "| " << COLOR::yellow << str_pad(left, long_left) << COLOR::white << " | "
                      << COLOR::green << str_pad(middle, long_middle) << COLOR::white << " | " << COLOR::cyan
                      << str_pad(right, long_right) << COLOR::white << " |" << std::endl;

            std::cout << COLOR::white << std::string(total_width, '=') << COLOR::reset << std::endl;

            rows.erase(rows.begin());
        }

        for (const auto &[left, middle, right] : rows)
        {
            std::cout << COLOR::white << "| " << COLOR::yellow << str_pad(left, long_left) << COLOR::white << " | "
                      << COLOR::green << str_pad(middle, long_middle) << COLOR::white << " | " << COLOR::cyan
                      << str_pad(right, long_right) << COLOR::white << " |" << std::endl;
        }

        std::cout << COLOR::white << std::string(total_width, '=') << COLOR::reset << std::endl << std::endl;
    }

    std::string str_join(const std::vector<std::string> &input, const char &ch)
    {
        std::string result;

        for (const auto &part : input)
        {
            result += part + ch;
        }

        // trim the trailing character that we appended
        result = result.substr(0, result.size() - 1);

        return result;
    }

    std::string str_pad(std::string input, size_t length)
    {
        if (input.length() < length)
        {
            const auto delta = length - input.length();

            for (size_t i = 0; i < delta; ++i)
            {
                input += " ";
            }
        }

        return input;
    }

    std::vector<std::string> str_split(const std::string &input, const char &ch)
    {
        auto pos = input.find(ch);

        uint64_t initial_pos = 0;

        std::vector<std::string> result;

        while (pos != std::string::npos)
        {
            result.push_back(input.substr(initial_pos, pos - initial_pos));

            initial_pos = pos + 1;

            pos = input.find(ch, initial_pos);
        }

        result.push_back(input.substr(initial_pos, std::min(pos, input.size()) - initial_pos + 1));

        return result;
    }

    void str_trim(std::string &str, bool to_lowercase)
    {
        const auto whitespace = "\t\n\r\f\v";

        str.erase(str.find_last_not_of(whitespace) + 1);

        str.erase(0, str.find_first_not_of(whitespace));

        if (to_lowercase)
        {
            std::transform(str.begin(), str.end(), str.begin(), [](unsigned char c) { return std::tolower(c); });
        }
    }
} // namespace Utilities
