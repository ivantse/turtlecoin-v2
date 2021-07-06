// Copyright (c) 2021, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#include <cli_helper.h>
#include <console.h>
#include <cppfs/FileHandle.h>
#include <cppfs/fs.h>
#include <p2p_node.h>

int main(int argc, char **argv)
{
    auto console = std::make_shared<Utilities::ConsoleHandler>(Configuration::Version::PROJECT_NAME + " Seed Node");

    auto cli = std::make_shared<Utilities::CLIHelper>(argv);

    uint16_t server_port = Configuration::P2P::DEFAULT_BIND_PORT;

    auto seed_nodes = std::vector<std::string>();

    bool reset_db = false;

    const auto default_db_path = cli->get_default_db_directory();

    std::string db_path = default_db_path.toNative(), log_path;

    // clang-format off
    cli->add_options("Seed Node")
        ("d,db-path", "Specify the <path> to the database directory",
            cxxopts::value<std::string>(db_path)->default_value(db_path), "<path>")
        ("p,port", "The local port to bind the server to",
            cxxopts::value<uint16_t>(server_port)->default_value(std::to_string(server_port)), "#")
        ("reset", "Reset the peer database",
            cxxopts::value<bool>(reset_db)->implicit_value("true"))
        ("seed-node", "Additional seed nodes to attempt when bootstrapping",
            cxxopts::value<std::vector<std::string>>(seed_nodes), "<ip:port>");
    // clang-format on

    cli->parse(argc, argv);

    cli->argument_load("log-file", log_path);

    const auto database_path = cli->get_db_path(db_path, "peerlist");

    console->catch_abort();

    auto logger = Logger::create_logger(log_path, cli->log_level());

    if (reset_db)
    {
        try
        {
            auto file = cppfs::fs::open(database_path.toNative());

            file.remove();

            logger->info("Reset Peer Database");
        }
        catch (const std::exception &e)
        {
            logger->error("Could not reset Peer Database: {0}", e.what());

            exit(1);
        }
    }

    auto server = std::make_shared<P2P::Node>(logger, database_path.path(), server_port, true);

    console->register_command(
        "status",
        "Displays the current node status",
        [&]()
        {
            std::vector<std::tuple<std::string, std::string>> rows {
                {"Item", "Value"},
                {"Version", cli->get_version()},
                {"P2P Version", std::to_string(Configuration::P2P::VERSION)},
                {"Minimum P2P Version", std::to_string(Configuration::P2P::MINIMUM_VERSION)},
                {"Peer ID", server->peer_id().to_string()},
                {"Port", std::to_string(server->port())},
                {"Incoming Connections", std::to_string(server->incoming_connections())},
                {"Outgoing Connections", std::to_string(server->outgoing_connections())},
                {"Known Peers", std::to_string(server->peers()->count())}};

            if (!server->external_address().empty())
            {
                rows.emplace_back("External IP", server->external_address());
            }

            Utilities::print_table(rows, true);
        });

    console->register_command(
        "incoming_connections",
        "Displays the current incoming connections",
        [&]()
        {
            std::vector<std::string> rows {"Address"};

            for (const auto &address : server->incoming_connected())
            {
                rows.emplace_back(address);
            }

            Utilities::print_table(rows, true);
        });

    console->register_command(
        "outgoing_connections",
        "Displays the current outgoing connections",
        [&]()
        {
            std::vector<std::string> rows {"Address"};

            for (const auto &address : server->outgoing_connected())
            {
                rows.emplace_back(address);
            }

            Utilities::print_table(rows, true);
        });

    console->register_command(
        "peers",
        "Prints the full list of known peers",
        [&]()
        {
            const auto peers = server->peers()->peers();

            if (!peers.empty())
            {
                std::vector<std::tuple<std::string, std::string, std::string>> rows;

                rows.reserve(peers.size() + 1);

                rows.emplace_back("Peer ID", "IP:Port", "Last Seen");

                for (const auto &peer : peers)
                {
                    rows.emplace_back(
                        peer.peer_id.to_string(),
                        peer.address.to_string() + ":" + std::to_string(peer.port),
                        std::to_string(peer.last_seen));
                }

                Utilities::print_table(rows, true);
            }
            else
            {
                logger->info("Peer list is empty");
            }
        });

    console->register_command(
        "prune_peers",
        "Performs a pruning of our peer list",
        [&]()
        {
            server->peers()->prune();

            logger->info("Pruned peer list");
        });

    console->register_command(
        "set_log",
        "Sets the log level to <#>",
        [&](const std::vector<std::string> &args)
        {
            if (args.empty())
            {
                logger->error("Must supply a log level with this command [0-6]");

                return;
            }

            try
            {
                const auto new_level = Logger::get_log_level(args.front());

                logger->set_level(new_level);

                logger->info("Changed logging level to: {0}", args.front());
            }
            catch (const std::exception &e)
            {
                logger->error("Could not set new logging level: {0}", e.what());
            }
        });

    logger->info("Starting seed node...");

    const auto error = server->start(seed_nodes);

    if (error)
    {
        logger->error("Seed node could not start: {0}", error.to_string());

        exit(1);
    }

    logger->info("P2P Seed node started on *:{0}", server_port);

    console->run();

    logger->info("P2P Seed node shutting down");
}
