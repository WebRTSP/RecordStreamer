#pragma once

#include <optional>
#include <deque>

#include <spdlog/common.h>

#include "Client/Config.h"


struct StreamerConfig
{
    enum class Type {
        Test,
        ReStreamer,
        OnvifReStreamer
    };

    Type type = Type::Test;
    std::string source;
    std::optional<std::string> username;
    std::optional<std::string> password;
    bool recordOnMotion = false;
    std::chrono::seconds motionRecordDuration = std::chrono::seconds(10);
};

struct Config : public client::Config
{
    spdlog::level::level_enum logLevel = spdlog::level::info;
    spdlog::level::level_enum lwsLogLevel = spdlog::level::warn;

    std::string targetUri;
    std::string recordToken;

    typedef std::deque<std::string> IceServers;
    IceServers iceServers;

    StreamerConfig streamer;
};
