// Copyright (c) 2021, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#include "aes_helper.h"

#include <aes.h>
#include <algparam.h>
#include <filters.h>
#include <modes.h>
#include <pwdbased.h>
#include <random.h>
#include <serializer.h>
#include <sha.h>

namespace Utilities::AES
{
    std::tuple<Error, std::string> decrypt(const std::string &input, const std::string &password, size_t iterations)
    {
        // converted into a reader to make it easier to work with
        auto reader = deserializer_t(std::vector<uint8_t>(input.begin(), input.end()));

        CryptoPP::byte key[16] = {0}, salt[16] = {0};

        if (reader.size() < sizeof(salt))
        {
            return {
                MAKE_ERROR_MSG(AES_DECRYPTION_ERROR, "Ciphertext does not contain enough data to include the salt"),
                std::string()};
        }

        // pull out the salt
        {
            const auto bytes = reader.bytes(sizeof(salt));

            std::copy(bytes.begin(), bytes.end(), salt);
        }

        CryptoPP::PKCS5_PBKDF2_HMAC<CryptoPP::SHA512> pbkdf2;

        // derive the AES key from the password and salt
        pbkdf2.DeriveKey(
            key,
            sizeof(key),
            0,
            reinterpret_cast<const CryptoPP::byte *>(password.c_str()),
            password.size(),
            salt,
            sizeof(salt),
            iterations);

        CryptoPP::CBC_Mode<CryptoPP::AES>::Decryption cbc_decryption;

        cbc_decryption.SetKeyWithIV(key, sizeof(key), salt);

        std::string decrypted;

        const auto buffer = reader.unread_data();

        try
        {
            CryptoPP::StringSource(
                reinterpret_cast<const CryptoPP::byte *>(buffer.data()),
                buffer.size(),
                true,
                new CryptoPP::StreamTransformationFilter(cbc_decryption, new CryptoPP::StringSink(decrypted)));
        }
        catch (const CryptoPP::Exception &)
        {
            return {MAKE_ERROR_MSG(AES_WRONG_PASSWORD, "Wrong password supplied for decryption"), std::string()};
        }

        return {MAKE_ERROR(SUCCESS), decrypted};
    }

    std::string encrypt(const std::string &input, const std::string &password, size_t iterations)
    {
        CryptoPP::byte key[16] = {0}, salt[16] = {0};

        // generate a random salt
        random_bytes(sizeof(salt), salt);

        CryptoPP::PKCS5_PBKDF2_HMAC<CryptoPP::SHA512> pbkdf2;

        // derive the AES key from the password and salt
        pbkdf2.DeriveKey(
            key,
            sizeof(key),
            0,
            reinterpret_cast<const CryptoPP::byte *>(password.c_str()),
            password.size(),
            salt,
            sizeof(salt),
            iterations);

        CryptoPP::CBC_Mode<CryptoPP::AES>::Encryption cbc_encryption;

        cbc_encryption.SetKeyWithIV(key, sizeof(key), salt);

        std::vector<CryptoPP::byte> encrypted;

        CryptoPP::StringSource(
            input, true, new CryptoPP::StreamTransformationFilter(cbc_encryption, new CryptoPP::VectorSink(encrypted)));

        auto writer = serializer_t();

        // pack the salt on to the front
        writer.bytes(salt, sizeof(salt));

        // append the encrypted data
        writer.bytes(encrypted.data(), encrypted.size());

        const auto bytes = writer.vector();

        // spit it all back as a string
        return std::string(bytes.begin(), bytes.end());
    }
} // namespace Utilities::AES
