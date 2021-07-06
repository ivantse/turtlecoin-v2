// Copyright (c) 2021, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#ifndef TURTLECOIN_LOGGER_H
#define TURTLECOIN_LOGGER_H

#include <spdlog/async.h>
#include <spdlog/spdlog.h>

typedef spdlog::level::level_enum logging_level;

typedef std::shared_ptr<spdlog::async_logger> logger;

class Logger
{
  public:
    /**
     * Creates a new instance of a logger with the given path and level
     *
     * @param path
     * @param level
     * @param flush_interval
     * @return
     */
    static logger create_logger(
        const std::string &path = std::string(),
        const logging_level &level = logging_level::info,
        size_t flush_interval = 1);

    /**
     * Returns the log level based upon the value found in the string
     *
     * @param level
     * @return
     */
    static logging_level get_log_level(const std::string &level);

    /**
     * Returns the log level based upon the value found in the integer
     *
     * @param level
     * @return
     */
    static logging_level get_log_level(size_t level);
};

#endif // TURTLECOIN_LOGGER_H
