#include <deque>

#include <glib.h>

#include <libwebsockets.h>

#include <CxxPtr/CPtr.h>
#include "CxxPtr/libconfigDestroy.h"

#include "Helpers/ConfigHelpers.h"
#include "Helpers/LwsLog.h"

#include "RtStreaming/GstRtStreaming/LibGst.h"
#include "Client/Log.h"

#include "Log.h"
#include "Config.h"
#include "Streamer.h"


static const auto Log = RecordStreamerLog;

static bool LoadConfig(Config* config)
{
    const std::deque<std::string> configDirs = ::ConfigDirs();
    if(configDirs.empty())
        return false;

    Config loadedConfig = *config;

    for(const std::string& configDir: configDirs) {
        const std::string configFile = configDir + "/record-streamer.conf";
        if(!g_file_test(configFile.c_str(), G_FILE_TEST_IS_REGULAR)) {
            Log()->info("Config \"{}\" not found", configFile);
            continue;
        }

        config_t config;
        config_init(&config);
        ConfigDestroy ConfigDestroy(&config);

        Log()->info("Loading config \"{}\"", configFile);
        if(!config_read_file(&config, configFile.c_str())) {
            Log()->error("Fail load config. {}. {}:{}",
                config_error_text(&config),
                configFile,
                config_error_line(&config));
            return false;
        }

        config_setting_t* targetConfig = config_lookup(&config, "target");
        if(targetConfig && CONFIG_TRUE == config_setting_is_group(targetConfig)) {
            const char* host = nullptr;
            if(CONFIG_TRUE == config_setting_lookup_string(targetConfig, "host", &host)) {
                loadedConfig.server = host;
            }

            int port= 0;
            if(CONFIG_TRUE == config_setting_lookup_int(targetConfig, "port", &port)) { // for backward compatibility
                loadedConfig.serverPort = static_cast<unsigned short>(port);
            }

            const char* uri = nullptr;
            if(CONFIG_TRUE == config_setting_lookup_string(targetConfig, "uri", &uri)) {
                loadedConfig.uri = uri;
            }

            const char* token = nullptr;
            if(CONFIG_TRUE == config_setting_lookup_string(targetConfig, "token", &token)) {
                loadedConfig.token = token;
            }
        }

        config_setting_t* streamerConfig = config_lookup(&config, "streamer");
        if(streamerConfig && CONFIG_TRUE == config_setting_is_group(streamerConfig)) {
            const char* test = nullptr;
            if(CONFIG_TRUE == config_setting_lookup_string(streamerConfig, "test", &test)) {
                loadedConfig.streamer.type = StreamerConfig::Type::Test;
                loadedConfig.streamer.source = test;
            }

            const char* url = nullptr;
            if(CONFIG_TRUE == config_setting_lookup_string(streamerConfig, "url", &url)) {
                loadedConfig.streamer.type = StreamerConfig::Type::ReStreamer;
                loadedConfig.streamer.source = url;
            }
        }

        config_setting_t* debugConfig = config_lookup(&config, "debug");
        if(debugConfig && CONFIG_TRUE == config_setting_is_group(debugConfig)) {
            int logLevel = 0;
            if(CONFIG_TRUE == config_setting_lookup_int(debugConfig, "log-level", &logLevel)) {
                if(logLevel > 0) {
                    loadedConfig.logLevel =
                        static_cast<spdlog::level::level_enum>(
                            spdlog::level::critical - std::min<int>(logLevel, spdlog::level::critical));
                }
            }
            int lwsLogLevel = 0;
            if(CONFIG_TRUE == config_setting_lookup_int(debugConfig, "lws-log-level", &lwsLogLevel)) {
                if(lwsLogLevel > 0) {
                    loadedConfig.lwsLogLevel =
                        static_cast<spdlog::level::level_enum>(
                            spdlog::level::critical - std::min<int>(lwsLogLevel, spdlog::level::critical));
                }
            }
        }

        const char* stunServer = nullptr;
        if(CONFIG_TRUE == config_lookup_string(&config, "stun-server", &stunServer)) {
            if(0 == g_ascii_strncasecmp(stunServer, "stun://", 7)) {
                loadedConfig.iceServers.emplace_back(stunServer);
            } else {
                Log()->error("STUN server URL should start with \"stun://\"");
            }
        }

        const char* turnServer = nullptr;
        if(CONFIG_TRUE == config_lookup_string(&config, "turn-server", &turnServer)) {
           if(0 == g_ascii_strncasecmp(turnServer, "turn://", 7)) {
                loadedConfig.iceServers.emplace_back(turnServer);
            } else {
                Log()->error("TURN server URL should start with \"turn://\"");
           }
        }
    }

    bool success = true;

    if(loadedConfig.server.empty()) {
        Log()->error("target host is requred");
        success = false;
    }

    if(!loadedConfig.serverPort) {
        Log()->error("target port is requred");
        success = false;
    }

    if(success) {
        *config = loadedConfig;
    }

    return success;
}

int main(int argc, char *argv[])
{
    Config config {};
    if(!LoadConfig(&config))
        return -1;

    InitLwsLogger(config.lwsLogLevel);
    InitWsClientLogger(config.logLevel);
    InitClientSessionLogger(config.logLevel);
    InitStreamerLogger(config.logLevel);

    LibGst libGst;

    return StreamerMain(config);
}
