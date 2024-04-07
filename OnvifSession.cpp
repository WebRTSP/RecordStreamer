#include "pch.h"

#include "OnvifSession.h"

#include <gsoap/plugin/wsseapi.h>

#include "ONVIF/DeviceBinding.nsmap"
#include "ONVIF/soapDeviceBindingProxy.h"
#include "ONVIF/soapMediaBindingProxy.h"
#include "ONVIF/soapPullPointSubscriptionBindingProxy.h"

#include "CxxPtr/GlibPtr.h"
#include "CxxPtr/GioPtr.h"

#include "Log.h"


namespace {

struct RequestMotionEventTaskData
{
    const Config *const config;
    const std::string mediaEndpoint;
};

void AddAuth(
    struct soap* soap,
    const std::optional<std::string>& username,
    const std::optional<std::string>& password) noexcept
{
    if(!username && !password) return;

    soap_wsse_add_UsernameTokenDigest(
        soap,
        nullptr,
        username ? username->c_str() : "",
        password ? password->c_str() : "");
}

}

enum {
    MOTION_EVENT_REQUEST_TIMEOUT = 1,
};

struct OnvifSession::Private
{
    static GQuark SoapDomain;
    static GQuark Domain;

    static void requestMediaUrisTaskFunc(
        GTask* task,
        gpointer sourceObject,
        gpointer taskData,
        GCancellable* cancellable);

    static void requestMotionEventTaskFunc(
        GTask* task,
        gpointer sourceObject,
        gpointer taskData,
        GCancellable* cancellable);

    struct MediaUris {
        std::string mediaEndpointUri;
        std::string streamUri;
    };

    Private(OnvifSession* owner, const Config& config);

    GSourcePtr timeoutAddSeconds(
        guint interval,
        GSourceFunc callback,
        gpointer data);


    void requestMediaUris() noexcept;
    void onMediaUris(std::unique_ptr<MediaUris>&) noexcept;

    void startMotionEventRequestTimeout() noexcept;

    void requestMotionEvent() noexcept;
    void onMotionEvent(gboolean isMotion) noexcept;

    void startRecordStopTimeout() noexcept;

    std::shared_ptr<spdlog::logger> log;

    OnvifSession *const owner;

    const Config& config;

    GCancellablePtr mediaUrlRequestTaskCancellablePtr;
    GTaskPtr mediaUrlRequestTaskPtr;

    std::unique_ptr<MediaUris> mediaUris;

    GSourcePtr moitionEventRequestTimeoutSource;

    GCancellablePtr motionEventRequestTaskCancellablePtr;
    GTaskPtr motionEventRequestTaskPtr;

    GSourcePtr recordStopTimeoutSource;
};

GQuark OnvifSession::Private::SoapDomain = g_quark_from_static_string("OnvifSession::SOAP");
GQuark OnvifSession::Private::Domain = g_quark_from_static_string("OnvifSession");

OnvifSession::Private::Private(OnvifSession* owner, const Config& config) :
    log(RecordStreamerLog()), owner(owner), config(config)
{
}

GSourcePtr OnvifSession::Private::timeoutAddSeconds(
    guint interval,
    GSourceFunc callback,
    gpointer data)
{
    GSource* source = g_timeout_source_new_seconds(interval);
    g_source_set_priority(source, G_PRIORITY_DEFAULT);
    g_source_set_callback(source, callback, data, NULL);

    GMainContext* mainContext = g_main_context_default();
    GMainContext* threadContext = g_main_context_get_thread_default();
    g_source_attach(source, threadContext ? threadContext : mainContext);

    return GSourcePtr(source);
}

void OnvifSession::Private::requestMediaUrisTaskFunc(
    GTask* task,
    gpointer sourceObject,
    gpointer taskData,
    GCancellable* cancellable)
{
    const Config& config = *static_cast<const Config*>(taskData);

    soap_status status;


    DeviceBindingProxy deviceProxy(config.streamer.source.c_str());

    _tds__GetCapabilities getCapabilities;
    _tds__GetCapabilitiesResponse getCapabilitiesResponse;
    AddAuth(deviceProxy.soap, config.streamer.username, config.streamer.password);
    status = deviceProxy.GetCapabilities(&getCapabilities, getCapabilitiesResponse);
    if(status != SOAP_OK) {
        const char* faultString = soap_fault_string(deviceProxy.soap);
        GError* error = g_error_new_literal(SoapDomain, status, faultString ? faultString : "GetCapabilities failed");
        g_task_return_error(task, error);
        return;
    }

    const std::string& mediaEndpoint = getCapabilitiesResponse.Capabilities->Media->XAddr;


    MediaBindingProxy mediaProxy(mediaEndpoint.c_str());

    _trt__GetProfiles getProfiles;
    _trt__GetProfilesResponse getProfilesResponse;
    AddAuth(mediaProxy.soap, config.streamer.username, config.streamer.password);
    status = mediaProxy.GetProfiles(&getProfiles, getProfilesResponse);
    if(status != SOAP_OK) {
        const char* faultString = soap_fault_string(deviceProxy.soap);
        GError* error = g_error_new_literal(SoapDomain, status, faultString ? faultString : "GetProfiles failed");
        g_task_return_error(task, error);
        return;
    }

    if(getProfilesResponse.Profiles.size() == 0) {
        GError* error =
            g_error_new_literal(
                Domain,
                DEVICE_MEDIA_HAS_NO_PROFILES,
                "Device Media has no profiles");
        g_task_return_error(task, error);
        return;
    }

    const tt__Profile *const mediaProfile = getProfilesResponse.Profiles[0];

    _trt__GetStreamUri getStreamUri;
    _trt__GetStreamUriResponse getStreamUriResponse;
    getStreamUri.ProfileToken = mediaProfile->token;

    tt__StreamSetup streamSetup;

    tt__Transport transport;
    transport.Protocol = tt__TransportProtocol::RTSP;

    streamSetup.Transport = &transport;

    getStreamUri.StreamSetup = &streamSetup;

    AddAuth(mediaProxy.soap, config.streamer.username, config.streamer.password);
    status = mediaProxy.GetStreamUri(&getStreamUri, getStreamUriResponse);
    if(status != SOAP_OK) {
        const char* faultString = soap_fault_string(deviceProxy.soap);
        GError* error = g_error_new_literal(SoapDomain, status, faultString ? faultString : "GetStreamUri failed");
        g_task_return_error(task, error);
        return;
    }

    const tt__MediaUri *const mediaUri = getStreamUriResponse.MediaUri;
    if(!mediaUri || mediaUri->Uri.empty()) {
        GError* error =
            g_error_new_literal(
                Domain,
                DEVICE_MEDIA_PROFILE_HAS_NO_STREAM_URI,
                "Device Media Profile has no stream uri");
        g_task_return_error(task, error);
        return;
    }

    GCharPtr uriStringPtr;
    if(config.streamer.username || config.streamer.password) {
        GUriPtr uriPtr(g_uri_parse(mediaUri->Uri.c_str(), G_URI_FLAGS_ENCODED, nullptr));
        GUri* uri = uriPtr.get();
        if(!g_uri_get_user(uri) && !g_uri_get_password(uri)) {
            GCharPtr userPtr(
                config.streamer.username ?
                    g_uri_escape_string(
                        config.streamer.username->c_str(),
                        G_URI_RESERVED_CHARS_SUBCOMPONENT_DELIMITERS,
                        false) :
                        nullptr);
            GCharPtr passwordPtr(
                config.streamer.password ?
                    g_uri_escape_string(
                        config.streamer.password->c_str(),
                        G_URI_RESERVED_CHARS_SUBCOMPONENT_DELIMITERS,
                        false) :
                        nullptr);
            uriStringPtr.reset(
                g_uri_join_with_user(
                    G_URI_FLAGS_ENCODED,
                    g_uri_get_scheme(uri),
                    userPtr.get(),
                    passwordPtr.get(),
                    g_uri_get_auth_params(uri),
                    g_uri_get_host(uri),
                    g_uri_get_port(uri),
                    g_uri_get_path(uri),
                    g_uri_get_query(uri),
                    g_uri_get_fragment(uri)));
        }
    }
    const std::string& mediaUriUri = uriStringPtr ? std::string(uriStringPtr.get()) : mediaUri->Uri;

    g_task_return_pointer(
        task,
        new MediaUris { mediaEndpoint, mediaUriUri },
        [] (gpointer mediaUris) { delete(static_cast<MediaUris*>(mediaUris)); });
}

void OnvifSession::Private::requestMotionEventTaskFunc(
    GTask* task,
    gpointer sourceObject,
    gpointer taskData,
    GCancellable* cancellable)
{
    soap_status status;

    const RequestMotionEventTaskData& data = *static_cast<RequestMotionEventTaskData*>(taskData);

    PullPointSubscriptionBindingProxy pullProxy(data.mediaEndpoint.c_str());

    _tev__PullMessages pullMessages;
    _tev__PullMessagesResponse pullMessagesResponse;
    AddAuth(pullProxy.soap, data.config->streamer.username, data.config->streamer.password);
    status = pullProxy.PullMessages(&pullMessages, pullMessagesResponse);
    if(status != SOAP_OK) {
        const char* faultString = soap_fault_string(pullProxy.soap);
        GError* error = g_error_new_literal(SoapDomain, status, faultString ? faultString : "PullMessages failed");
        g_task_return_error(task, error);
        return;
    }

    for(const wsnt__NotificationMessageHolderType* messageHolder: pullMessagesResponse.wsnt__NotificationMessage) {
        const soap_dom_element& message = messageHolder->Message.__any;
        soap_dom_element* data = message.elt_get("tt:Data");
        if(!data) {
            GError* error =
                g_error_new_literal(
                    Domain,
                    NOTIFICATION_MESSAGE_HAS_NO_DATA_ELEMENT,
                    "Notification message has no data element");
            g_task_return_error(task, error);
            return;
        }

        soap_dom_element* simpleItem = data->elt_get("tt:SimpleItem");
        for(;simpleItem; simpleItem = simpleItem->get_next()) {
            const soap_dom_attribute* name = simpleItem->att_get("Name");
            if(!name || !name->get_text()) continue;
            if(name->get_text() != std::string("IsMotion")) continue;

            const soap_dom_attribute* value = simpleItem->att_get("Value");
            if(!value || !value->get_text()) continue;

            gboolean isMotion = value->is_true();

            g_task_return_boolean(task, isMotion);
            return;
        }
    }

    GError* error =
        g_error_new_literal(
            Domain,
            NOTIFICATION_MESSAGE_DOES_NOT_CONTAIN_MOTION_EVENT,
            "Notification message doesn't contain motion event");
    g_task_return_error(task, error);
}

void OnvifSession::Private::requestMediaUris() noexcept
{
    auto readyCallback =
        [] (GObject* sourceObject, GAsyncResult* result, gpointer userData) {
            g_return_if_fail(g_task_is_valid(result, sourceObject));

            GError* error = nullptr;
            MediaUris* mediaUris =
                reinterpret_cast<MediaUris*>(g_task_propagate_pointer(G_TASK(result), &error));
            GErrorPtr errorPtr(error);
            std::unique_ptr<MediaUris> mediaUrisPtr(mediaUris);
            if(errorPtr) {
                RecordStreamerLog()->error(
                    "[{}] {}",
                    g_quark_to_string(errorPtr->domain),
                    errorPtr->message);

                if(errorPtr->code != G_IO_ERROR_CANCELLED) {
                    // has error but not cancelled (i.e. owner is still available)
                    OnvifSession::Private* self =
                        reinterpret_cast<OnvifSession::Private*>(userData);
                    self->owner->disconnect();
                }
            }

            OnvifSession::Private* self =
                reinterpret_cast<OnvifSession::Private*>(userData);

            if(mediaUrisPtr) {
                // no error and not cancelled yet
                self->onMediaUris(mediaUrisPtr);
            } else {
                self->owner->disconnect();
            }
        };

    GCancellable* cancellable = g_cancellable_new();
    GTask* task = g_task_new(nullptr, cancellable, readyCallback, this);
    mediaUrlRequestTaskCancellablePtr.reset(cancellable);
    mediaUrlRequestTaskPtr.reset(task);

    g_task_set_return_on_cancel(task, true);
    g_task_set_task_data(task, const_cast<Config*>(&config), nullptr);

    g_task_run_in_thread(task, requestMediaUrisTaskFunc);
}

void OnvifSession::Private::onMediaUris(std::unique_ptr<MediaUris>& mediaUris) noexcept
{
    log->info("Media endpoint uri discovered: {}", mediaUris->mediaEndpointUri);
    log->info("Media stream uri discovered: {}", mediaUris->streamUri);

    this->mediaUris.swap(mediaUris);

    if(config.streamer.recordOnMotion) {
        startMotionEventRequestTimeout();
    } else {
        owner->startRecord(this->mediaUris->streamUri);
    }
}

void OnvifSession::Private::startMotionEventRequestTimeout() noexcept
{
    assert(!moitionEventRequestTimeoutSource);

    auto timeoutFunc =
        [] (gpointer userData) -> gboolean {
            Private* p = static_cast<Private*>(userData);

            p->moitionEventRequestTimeoutSource = 0;
            p->requestMotionEvent();

            return FALSE;
        };

    moitionEventRequestTimeoutSource =
        timeoutAddSeconds(
            MOTION_EVENT_REQUEST_TIMEOUT,
            timeoutFunc,
            this);
}

void OnvifSession::Private::requestMotionEvent() noexcept
{
    auto readyCallback =
        [] (GObject* sourceObject, GAsyncResult* result, gpointer userData) {
            g_return_if_fail(g_task_is_valid(result, sourceObject));

            GError* error = nullptr;
            gboolean isMotion = g_task_propagate_boolean(G_TASK(result), &error);
            GErrorPtr errorPtr(error);
            if(errorPtr) {
                RecordStreamerLog()->error(
                    "[{}] {}",
                    g_quark_to_string(errorPtr->domain),
                    errorPtr->message);

                if(errorPtr->code != G_IO_ERROR_CANCELLED) {
                    // has error but not cancelled (i.e. owner is still available)
                    OnvifSession::Private* self =
                        reinterpret_cast<OnvifSession::Private*>(userData);
                    self->owner->disconnect();
                }
            } else {
                // no error and not cancelled yet
                OnvifSession::Private* self =
                    reinterpret_cast<OnvifSession::Private*>(userData);
                self->onMotionEvent(isMotion);
            }
        };

    GCancellable* cancellable = g_cancellable_new();
    GTask* task = g_task_new(nullptr, cancellable, readyCallback, this);
    motionEventRequestTaskCancellablePtr.reset(cancellable);
    motionEventRequestTaskPtr.reset(task);

    g_task_set_return_on_cancel(task, true);
    g_task_set_task_data(
        task,
        new RequestMotionEventTaskData { &config, mediaUris->mediaEndpointUri },
        [] (gpointer userData) { delete static_cast<RequestMotionEventTaskData*>(userData); });

    g_task_run_in_thread(task, requestMotionEventTaskFunc);
}

void OnvifSession::Private::onMotionEvent(gboolean isMotion) noexcept
{
    if(isMotion) {
        log->info("Motion detected!");

        if(!owner->isStreaming()) {
            owner->startRecord(mediaUris->streamUri);
        }

        startRecordStopTimeout();
    }

    startMotionEventRequestTimeout();
}

void OnvifSession::Private::startRecordStopTimeout() noexcept
{
    if(recordStopTimeoutSource) {
        g_source_destroy(recordStopTimeoutSource.get());
        recordStopTimeoutSource.reset();
    }

    auto timeoutFunc =
        [] (gpointer userData) -> gboolean {
            Private* p = static_cast<Private*>(userData);

            RecordStreamerLog()->info("Stopping record by timeout...");

            p->owner->stopRecord();

            p->recordStopTimeoutSource = 0;
            return FALSE;
        };

    recordStopTimeoutSource =
        timeoutAddSeconds(
            config.streamer.motionRecordDuration.count(),
            timeoutFunc,
            this);
}


OnvifSession::OnvifSession(
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
    _p(std::make_unique<OnvifSession::Private>(this, std::ref(config)))
{
}

OnvifSession::~OnvifSession()
{
    g_cancellable_cancel(_p->mediaUrlRequestTaskCancellablePtr.get());

    g_cancellable_cancel(_p->motionEventRequestTaskCancellablePtr.get());

    if(_p->moitionEventRequestTimeoutSource) {
        g_source_destroy(_p->moitionEventRequestTimeoutSource.get());
    }

    if(_p->recordStopTimeoutSource) {
        g_source_destroy(_p->recordStopTimeoutSource.get());
    }
}

bool OnvifSession::onOptionsResponse(
    const rtsp::Request& request,
    const rtsp::Response& response) noexcept
{
    if(!ClientRecordSession::onOptionsResponse(request, response))
        return false;

    _p->requestMediaUris();

    return true;
}
