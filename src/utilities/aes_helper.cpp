// Copyright (c) 2021, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#include "aes_helper.h"

#include <crypto_common.h>

namespace Utilities::AES
{
    std::tuple<Error, std::string> decrypt(const std::string &input, const std::string &password, size_t iterations)
    {
        try
        {
            const auto decrypted = Crypto::AES::decrypt(input, password, iterations);

            return {MAKE_ERROR(SUCCESS), decrypted};
        }
        catch (const std::exception &e)
        {
            return {MAKE_ERROR_MSG(AES_DECRYPTION_ERROR, e.what()), std::string()};
        }
    }

    std::string encrypt(const std::string &input, const std::string &password, size_t iterations)
    {
        return Crypto::AES::encrypt(input, password, iterations);
    }
} // namespace Utilities::AES
