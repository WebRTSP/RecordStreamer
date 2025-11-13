#pragma once

#include "WebRTSP/Client/ClientRecordSession.h"

#include "Config.h"


class OnvifSession : public ClientRecordSession
{
public:
    enum Error: int32_t {
        DEVICE_MEDIA_HAS_NO_PROFILES = 1,
        DEVICE_MEDIA_PROFILE_HAS_NO_STREAM_URI = 2,
        NOTIFICATION_MESSAGE_HAS_NO_DATA_ELEMENT = 3,
        NOTIFICATION_MESSAGE_DOES_NOT_CONTAIN_MOTION_EVENT = 4,
    };

    OnvifSession(
        const Config&,
        const CreatePeer& createPeer,
        const SendRequest& sendRequest,
        const SendResponse& sendResponse) noexcept;
    ~OnvifSession();

    bool onOptionsResponse(
        const rtsp::Request&,
        const rtsp::Response&) noexcept override;

private:
    struct Private;
    std::unique_ptr<Private> _p;
};
