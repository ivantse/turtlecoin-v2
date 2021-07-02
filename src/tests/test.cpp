// Copyright (c) 2021, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#include <cli_helper.h>
#include <network_fees.h>
#include <types.h>

int main(int argc, char **argv)
{
    auto cli = std::make_shared<Utilities::CLIHelper>(argv);

    cli->parse(argc, argv);

    Types::Blockchain::committed_stake_transaction_t a;

    a.version = 1;

    a.unlock_block = 0;

    a.fee = 1;

    a.key_images.resize(1);

    for (size_t i = 0; i < 2; ++i)
    {
        auto output = Types::Blockchain::transaction_output_t();

        output.amount = 0xFFFFFFFFFFFFFFFF;

        a.outputs.push_back(output);
    }

    a.pruning_hash = Crypto::random_hash();

    a.nonce = 0xFFFFFFFFFFFFFFFF;


    std::cout << a << std::endl;

    std::cout << Common::NetworkFees::calculate_base_transaction_fee(a.size()) << std::endl;

    for (size_t i = 0; i < Configuration::Transaction::Fees::MAXIMUM_POW_ZEROS; ++i)
    {
        std::cout << Common::NetworkFees::calculate_transaction_fee(a.size(), i) << std::endl;
    }

    return 0;
}
