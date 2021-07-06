// Copyright (c) 2021, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#include "logger.h"

#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>

logger Logger::create_logger(const std::string &path, const logging_level &level, size_t flush_interval)
{
    // setup logger thread pool
    spdlog::init_thread_pool(8192, 1);

    // set the logs to force a flush ever interval seconds
    spdlog::flush_every(std::chrono::seconds(flush_interval));

    // setup the console output
    auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();

    std::vector<spdlog::sink_ptr> sinks {console_sink};

    if (!path.empty())
    {
        // setup the file log output
        auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(path);

        sinks.push_back(file_sink);
    }

    // create the multisink logger
    auto logger = std::make_shared<spdlog::async_logger>(
        "multi_sink", sinks.begin(), sinks.end(), spdlog::thread_pool(), spdlog::async_overflow_policy::block);

    // set the pattern to what we want it to be
    logger->set_pattern("[%Y-%m-%d %H:%M:%S.%e UTC] [%^%l%$] %v", spdlog::pattern_time_type::utc);

    // set the default logger level as specified
    logger->set_level(level);

    // register the logger with spdlog
    spdlog::register_logger(logger);

    return logger;
}

logging_level Logger::get_log_level(size_t level)
{
    switch (level)
    {
        case 0:
            return spdlog::level::off;
        case 1:
            return spdlog::level::critical;
        case 2:
            return spdlog::level::err;
        case 3:
            return spdlog::level::warn;
        case 4:
            return spdlog::level::info;
        case 5:
            return spdlog::level::debug;
        case 6:
            return spdlog::level::trace;
        default:
            throw std::invalid_argument("Unknown log level supplied");
    }
}

logging_level Logger::get_log_level(const std::string &level)
{
    size_t temp = 0;

    try
    {
        temp = std::stoi(level);
    }
    catch (...)
    {
        throw std::invalid_argument("Logging level supplied is not a number");
    }

    return get_log_level(temp);
}
