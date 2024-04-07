#include "Session.h"


Session::Session(
    const Config& config,
    const CreatePeer& createPeer,
    const SendRequest& sendRequest,
    const SendResponse& sendResponse) noexcept :
    ClientRecordSession(
        config.targetUri,
        config.recordToken,
        config.webRTCConfig,
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

    startRecord(_config.streamer.source);

    return true;
}
