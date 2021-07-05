// Copyright (c) 2021, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#ifndef TURTLECOIN_UTILITIES_H
#define TURTLECOIN_UTILITIES_H

#include <hashing.h>
#include <string>
#include <vector>

namespace Utilities
{
    /**
     * Normalizes the given string based host (with or without port #)
     * into it's host portion, the port, and a hash of the combination of the two
     *
     * @param host
     * @return
     */
    std::tuple<std::string, uint16_t, crypto_hash_t> normalize_host_port(std::string host);

    /**
     * Normalizes the given string based host (with or without port #)
     * into it's host portion, the port, and a hash of the combination of the two
     *
     * @param host
     * @param port
     * @return
     */
    std::tuple<std::string, uint16_t, crypto_hash_t> normalize_host_port(const std::string &host, uint16_t port);

    /**
     * Prints the given vector of strings as a table
     *
     * @param rows
     * @param has_header
     */
    void print_table(std::vector<std::string> rows, bool has_header = false);

    /**
     * Prints the given tuple of left/right columns as a table
     *
     * @param rows
     * @param has_header
     */
    void print_table(std::vector<std::tuple<std::string, std::string>> rows, bool has_header = false);

    /**
     * Prints the given tuple of left/middle/right columns as a table
     *
     * @param rows
     * @param has_header
     */
    void print_table(std::vector<std::tuple<std::string, std::string, std::string>> rows, bool has_header = false);

    /**
     * Joins a vector of strings together using the specified character as the delimiter
     *
     * @param input
     * @param ch
     * @return
     */
    std::string str_join(const std::vector<std::string> &input, const char &ch = ' ');

    /**
     * Pads a string with blank spaces up to the specified length
     *
     * @param input
     * @param length
     * @return
     */
    std::string str_pad(std::string input, size_t length = 0);

    /**
     * Splits a string into a vector of strings using the specified character as a delimiter
     *
     * @param input
     * @param ch
     * @return
     */
    std::vector<std::string> str_split(const std::string &input, const char &ch = ' ');

    /**
     * Trims any whitespace from both the start and end of the given string
     *
     * @param str
     * @param to_lowercase
     */
    void str_trim(std::string &str, bool to_lowercase = false);
} // namespace Utilities

#endif // TURTLECOIN_UTILITIES_H
