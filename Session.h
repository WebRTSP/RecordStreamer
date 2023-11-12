#pragma once

#include "WebRTSP/Client/ClientRecordSession.h"

#include "Config.h"


class Session : public ClientRecordSession
{
public:
    Session(
        const Config&,
        const CreatePeer& createPeer,
        const SendRequest& sendRequest,
        const SendResponse& sendResponse) noexcept;

    bool onOptionsResponse(
        const rtsp::Request&,
        const rtsp::Response&) noexcept override;

protected:
    bool playSupportRequired(const std::string& /*uri*/) noexcept override { return false; }
    bool recordSupportRequired(const std::string& /*uri*/) noexcept override { return true; }

private:
    const Config&  _config;
};
