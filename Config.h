#pragma once

#include <deque>

#include <spdlog/common.h>

#include "Client/Config.h"


struct StreamerConfig
{
    enum class Type {
        Test,
        ReStreamer,
    };

    Type type = Type::Test;
    std::string source;
};

struct Config : public client::Config
{
    spdlog::level::level_enum logLevel = spdlog::level::info;
    spdlog::level::level_enum lwsLogLevel = spdlog::level::warn;

    std::string uri;
    std::string token;

    typedef std::deque<std::string> IceServers;
    IceServers iceServers;

    StreamerConfig streamer;
};
