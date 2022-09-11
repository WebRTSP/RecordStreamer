#include "Session.h"


Session::Session(
    const Config& config,
    const CreatePeer& createPeer,
    const std::function<void (const rtsp::Request*) noexcept>& sendRequest,
    const std::function<void (const rtsp::Response*) noexcept>& sendResponse) noexcept :
    ClientRecordSession(
        config.targetUri,
        config.recordToken,
        config.iceServers,
        createPeer,
        sendRequest,
        sendResponse),
    _config(config)
{
}

bool Session::onOptionsResponse(
    const rtsp::Request& request,
    const rtsp::Response& response) noexcept
{
    if(!ClientRecordSession::onOptionsResponse(request, response))
        return false;

    if(!isSupported(rtsp::Method::RECORD))
        return false;

    startRecord(_config.streamer.source);

    return true;
}
