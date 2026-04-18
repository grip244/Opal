#pragma once

#include <mutex>
#include <windows.data.json.h>

namespace Opal {
    /// <summary>
    /// Thread-safe handler for Project Rome remote playback commands.
    /// Parses incoming ValueSet messages and synchronizes state with the local PlaybackService.
    /// </summary>
    ref class PlaybackRemoteHandler sealed {
    public:
        PlaybackRemoteHandler();

        /// <summary>
        /// Processes an incoming AppService request.
        /// </summary>
        void HandleIncomingRequest(Windows::ApplicationModel::AppService::AppServiceRequestReceivedEventArgs^ args);

    private:
        std::mutex _handlerLock;

        void ExecuteCommand(Platform::String^ command, Platform::String^ jsonData);
        void SyncTrackState(Windows::Data::Json::JsonObject^ trackData);
        
        void DispatchToUI(Windows::UI::Core::DispatchedHandler^ handler);
    };
}
