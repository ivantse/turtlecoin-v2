// Copyright (c) 2021, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#ifndef TURTLECOIN_UTILITIES_AES_H
#define TURTLECOIN_UTILITIES_AES_H

#include <config.h>
#include <errors.h>
#include <string>
#include <tuple>

namespace Utilities::AES
{
    /**
     * Decrypts data from the provided encrypted string using the supplied password
     *
     * @param input
     * @param password
     * @param iterations
     * @return
     */
    std::tuple<Error, std::string> decrypt(
        const std::string &input,
        const std::string &password,
        size_t iterations = Configuration::Wallet::PBKDF2_ITERS);

    /**
     * Encrypts the provided string using the supplied password into an encrypted string
     * @param input
     * @param password
     * @param iterations
     * @return
     */
    std::string encrypt(
        const std::string &input,
        const std::string &password,
        size_t iterations = Configuration::Wallet::PBKDF2_ITERS);
}; // namespace Utilities::AES

#endif
