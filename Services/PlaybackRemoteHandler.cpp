#include "pch.h"
#include "PlaybackRemoteHandler.h"
#include "Services/PlaybackService.h"
#include "Services/DebugLogger.h"
#include <ppltasks.h>

using namespace Opal;
using namespace Platform;
using namespace Windows::Foundation;
using namespace Windows::Foundation::Collections;
using namespace Windows::ApplicationModel::AppService;
using namespace Windows::Data::Json;
using namespace Windows::UI::Core;
using namespace Windows::ApplicationModel::Core;
using namespace concurrency;

PlaybackRemoteHandler::PlaybackRemoteHandler() {
}

void PlaybackRemoteHandler::HandleIncomingRequest(AppServiceRequestReceivedEventArgs^ args) {
    // Retain deferral to keep the connection alive during processing
    auto deferral = args->GetDeferral();
    auto message = args->Request->Message;

    // Use thread-safe lock for parsing and state evaluation
    std::lock_guard<std::mutex> lock(_handlerLock);

    if (message->HasKey("Command")) {
        String^ command = safe_cast<String^>(message->Lookup("Command"));
        String^ jsonData = message->HasKey("Data") ? safe_cast<String^>(message->Lookup("Data")) : nullptr;

        DebugLogger::Instance->Log("PlaybackRemoteHandler", "Incoming Remote Command: " + command);
        ExecuteCommand(command, jsonData);
    }

    // Send acknowledgement
    auto response = ref new ValueSet();
    response->Insert("Status", "Accepted");
    
    create_task(args->Request->SendResponseAsync(response)).then([deferral](AppServiceResponseStatus status) {
        deferral->Complete();
    });
}

void PlaybackRemoteHandler::ExecuteCommand(String^ command, String^ jsonData) {
    if (command == "SyncState") {
        if (jsonData != nullptr) {
            try {
                JsonObject^ json = JsonObject::Parse(jsonData);
                SyncTrackState(json);
            } catch (Exception^ ex) {
                DebugLogger::Instance->Log("PlaybackRemoteHandler", "JSON Parse Error: " + ex->Message);
            }
        }
    } else if (command == "Play") {
        DispatchToUI(ref new DispatchedHandler([]() {
            PlaybackService::Instance->Player->Play();
        }));
    } else if (command == "Pause") {
        DispatchToUI(ref new DispatchedHandler([]() {
            PlaybackService::Instance->Player->Pause();
        }));
    } else if (command == "Next") {
        DispatchToUI(ref new DispatchedHandler([]() {
            PlaybackService::Instance->NextSong();
        }));
    } else if (command == "Previous") {
        DispatchToUI(ref new DispatchedHandler([]() {
            PlaybackService::Instance->PreviousSong();
        }));
    }
}

void PlaybackRemoteHandler::SyncTrackState(JsonObject^ trackData) {
    // Extract metadata using Windows::Data::Json as per constraints
    String^ trackId = trackData->HasKey("Id") ? trackData->GetNamedString("Id") : "";
    
    DispatchToUI(ref new DispatchedHandler([trackId, trackData]() {
        auto current = PlaybackService::Instance->CurrentSong;
        
        // Only sync if track ID differs to avoid playback loops
        if (current == nullptr || current->Id != trackId) {
            DebugLogger::Instance->Log("PlaybackRemoteHandler", "Syncing remote track ID: " + trackId);
            
            // Reconstruct song model from JSON
            auto s = ref new Song();
            s->Id = trackId;
            s->Title = trackData->HasKey("Title") ? trackData->GetNamedString("Title") : "Unknown";
            s->Artist = trackData->HasKey("Artist") ? trackData->GetNamedString("Artist") : "Unknown Artist";
            s->Album = trackData->HasKey("Album") ? trackData->GetNamedString("Album") : "";
            s->StreamUrl = trackData->HasKey("StreamUrl") ? trackData->GetNamedString("StreamUrl") : "";
            s->CoverUrl = trackData->HasKey("CoverUrl") ? trackData->GetNamedString("CoverUrl") : "";
            
            PlaybackService::Instance->PlaySong(s);
        }
    }));
}

void PlaybackRemoteHandler::DispatchToUI(DispatchedHandler^ handler) {
    if (handler == nullptr) return;

    // Ensure UI updates are dispatched to the CoreWindow dispatcher (Constraint requirement)
    Windows::UI::Core::CoreDispatcher^ dispatcher = nullptr;

    try {
        if (CoreApplication::MainView != nullptr) {
            dispatcher = CoreApplication::MainView->Dispatcher;
        }

        if (dispatcher == nullptr && Window::Current != nullptr && Window::Current->Dispatcher != nullptr) {
            dispatcher = Window::Current->Dispatcher;
        }
    } catch (...) {}

    if (dispatcher != nullptr) {
        dispatcher->RunAsync(CoreDispatcherPriority::Normal, handler);
    } else {
        // Fallback for background scenarios where UI might not be active but logic must run
        handler->Invoke();
    }
}
