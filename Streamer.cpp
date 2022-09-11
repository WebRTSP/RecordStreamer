#include "Streamer.h"

#include <thread>
#include <memory>
#include <functional>

#include <CxxPtr/GlibPtr.h>

#include "RtStreaming/GstRtStreaming/LibGst.h"
#include "RtStreaming/GstRtStreaming/GstClient.h"

#include "WebRTSP/Client/Log.h"
#include "WebRTSP/Client/WsClient.h"

#include "RtStreaming/GstRtStreaming/GstTestStreamer.h"
#include "RtStreaming/GstRtStreaming/GstReStreamer.h"

#include "Session.h"
#include "OnvifSession.h"

enum {
    RECONNECT_TIMEOUT = 10,
};


static std::unique_ptr<WebRTCPeer> CreateClientPeer(const Config& config, const std::string& uri)
{
    switch(config.streamer.type) {
    case StreamerConfig::Type::Test:
        return std::make_unique<GstTestStreamer>(config.streamer.source);
    case StreamerConfig::Type::ReStreamer:
        return std::make_unique<GstReStreamer>(uri, std::string());
    case StreamerConfig::Type::OnvifReStreamer:
        return std::make_unique<GstReStreamer>(uri, std::string());
    default:
        return nullptr;
    }
}

static std::unique_ptr<rtsp::ClientSession> CreateClientSession (
    const Config& config,
    const std::function<void (const rtsp::Request*) noexcept>& sendRequest,
    const std::function<void (const rtsp::Response*) noexcept>& sendResponse) noexcept
{
    if(config.streamer.type == StreamerConfig::Type::OnvifReStreamer) {
        return
            std::make_unique<OnvifSession>(
                config,
                std::bind(CreateClientPeer, std::ref(config), std::placeholders::_1),
                sendRequest,
                sendResponse);
    } else {
        return
            std::make_unique<Session>(
                config,
                std::bind(CreateClientPeer, std::ref(config), std::placeholders::_1),
                sendRequest,
                sendResponse);
    }
}

static void ClientDisconnected(client::WsClient* client) noexcept
{
    GSourcePtr timeoutSourcePtr(g_timeout_source_new_seconds(RECONNECT_TIMEOUT));
    GSource* timeoutSource = timeoutSourcePtr.get();
    g_source_set_callback(timeoutSource,
        [] (gpointer userData) -> gboolean {
            static_cast<client::WsClient*>(userData)->connect();
            return false;
        }, client, nullptr);
    g_source_attach(timeoutSource, g_main_context_get_thread_default());
}

int StreamerMain(const Config& config)
{
    LibGst libGst;

    GMainContextPtr contextPtr(g_main_context_new());
    GMainContext* context = contextPtr.get();
    g_main_context_push_thread_default(context);

    GMainLoopPtr loopPtr(g_main_loop_new(context, FALSE));
    GMainLoop* loop = loopPtr.get();

    client::WsClient client(
        config,
        loop,
        std::bind(
            CreateClientSession,
            std::ref(config),
            std::placeholders::_1,
            std::placeholders::_2),
        std::bind(ClientDisconnected, &client));

    if(client.init()) {
        client.connect();
        g_main_loop_run(loop);
    }

    return 0;
}
