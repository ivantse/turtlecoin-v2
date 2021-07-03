// Copyright (c) 2021, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#include <aes_helper.h>
#include <cli_helper.h>
#include <crypto_common.h>
#include <logger.h>

int main(int argc, char **argv)
{
    auto cli = std::make_shared<Utilities::CLIHelper>(argv);

    cli->parse(argc, argv);

    auto logger = Logger::create_logger("", cli->log_level());

    logger->warn("AES Encryption Check");

    const auto hash = Crypto::random_hash().to_string();

    logger->info("Cleartext: {0}", hash);

    const auto password = Crypto::random_hash().to_string();

    logger->info("Password: {0}", password);

    const auto encrypted = Utilities::AES::encrypt(hash, password);

    logger->info("Ciphertext: {0}", encrypted);

    {
        const auto [error, decrypted] = Utilities::AES::decrypt(encrypted, password);

        if (error)
        {
            logger->error("Could not decrypt ciphertext: {0}", error.to_string());

            return 1;
        }

        if (decrypted != hash)
        {
            logger->error("Decryption results in unexpected value: {0}", decrypted);

            return 1;
        }

        logger->info("Cleartext: {0}", decrypted);
    }

    {
        const auto wrong_password = Crypto::random_hash().to_string();

        logger->info("Wrong Password: {0}", wrong_password);

        const auto [error, decrypted] = Utilities::AES::decrypt(encrypted, wrong_password);

        if (!error)
        {
            logger->error("Incorrect password decrypted data: {0}", decrypted);

            return 1;
        }

        logger->info("Incorrect password does not decrypt data");
    }

    return 0;
}
