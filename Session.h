#pragma once

#include "WebRTSP/Client/ClientRecordSession.h"

#include "Config.h"


class Session : public ClientRecordSession
{
public:
    Session(
        const Config&,
        const CreatePeer& createPeer,
        const std::function<void (const rtsp::Request*) noexcept>& sendRequest,
        const std::function<void (const rtsp::Response*) noexcept>& sendResponse) noexcept;

    bool onOptionsResponse(
        const rtsp::Request&,
        const rtsp::Response&) noexcept override;

private:
    const Config&  _config;
};
