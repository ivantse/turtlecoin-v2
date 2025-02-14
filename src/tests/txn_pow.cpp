// Copyright (c) 2021, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#include <benchmark.h>
#include <cli_helper.h>
#include <config.h>
#include <types.h>

#define POW_TEST_ITERATIONS 10

int main(int argc, char **argv)
{
    auto cli = std::make_shared<Utilities::CLIHelper>(argv);

    cli->parse(argc, argv);

    benchmark_header(40, 25);

    for (size_t i = 0; i < Configuration::Transaction::Fees::MAXIMUM_POW_ZEROS; ++i)
    {
        benchmark(
            [&i]()
            {
                Types::Blockchain::uncommited_normal_transaction_t tx;

                tx.public_key = Crypto::random_point();

                [[maybe_unused]] const auto success = tx.mine(i);
            },
            "Searching for " + std::to_string(i) + " leading zeros",
            POW_TEST_ITERATIONS,
            40,
            25);
    }

    return 0;
}
