#include "pch.h"
#include "CastingService.h"
#include "PlaybackService.h"
#include <windows.data.json.h>
#include <ppltasks.h>
#include <collection.h>
#include "DebugLogger.h"

using namespace Opal;
using namespace Platform;
using namespace Windows::Networking::Sockets;
using namespace Windows::Storage::Streams;
using namespace Windows::Data::Json;
using namespace Windows::Foundation::Collections;
using namespace concurrency;

static String^ GetCachedLocalIp() {
    static String^ cachedIp = nullptr;
    if (cachedIp != nullptr) return cachedIp;
    
    cachedIp = "127.0.0.1";
    try {
        auto hostnames = Windows::Networking::Connectivity::NetworkInformation::GetHostNames();
        for (auto h : hostnames) {
            if (h->Type == Windows::Networking::HostNameType::Ipv4) {
                cachedIp = h->CanonicalName;
                break; 
            }
        }
    } catch (...) {}
    return cachedIp;
}

CastingService^ CastingService::_instance = nullptr;

CastingService^ CastingService::Instance::get() {
    if (_instance == nullptr) {
        _instance = ref new CastingService();
    }
    return _instance;
}

CastingService::CastingService() {
    IsCastingActive = false;
    IsRemoteControlled = false;
    TargetDeviceIp = nullptr;
    _isListening = false;
    _isBinding = false;
    _discoveredDevices = ref new Platform::Collections::Vector<RemoteDevice^>();
}

IObservableVector<RemoteDevice^>^ CastingService::DiscoveredDevices::get() {
    return _discoveredDevices;
}

void CastingService::StartListening() {}
void CastingService::StopListening() {}

void CastingService::StartDiscovery() {
    if (_udpSocket != nullptr) return;

    _udpSocket = ref new DatagramSocket();
    _udpSocket->MessageReceived += ref new Windows::Foundation::TypedEventHandler<DatagramSocket^, DatagramSocketMessageReceivedEventArgs^>(this, &CastingService::OnUdpMessageReceived);

    create_task(_udpSocket->BindServiceNameAsync("9888")).then([this](task<void> t) {
        try {
            t.get();
            _udpSocket->JoinMulticastGroup(ref new Windows::Networking::HostName("239.0.0.222"));
            
            _discoveryTimer = ref new Windows::UI::Xaml::DispatcherTimer();
            Windows::Foundation::TimeSpan ts; ts.Duration = 5 * 10000000LL; // 5 seconds
            _discoveryTimer->Interval = ts;
            auto self = this;
            _discoveryTimer->Tick += ref new Windows::Foundation::EventHandler<Object^>([self](Object^, Object^) {
                self->SendDiscoveryBeacon();
            });
            _discoveryTimer->Start();
            
            DebugLogger::Instance->Log("CastingService", "Discovery started on UDP 9888");
            SendDiscoveryBeacon();
        }
        catch (Exception^ ex) {
            DebugLogger::Instance->LogException("StartDiscovery", ex);
        }
    });
}

void CastingService::StopDiscovery() {
    if (_discoveryTimer != nullptr) {
        _discoveryTimer->Stop();
        _discoveryTimer = nullptr;
    }
    if (_udpSocket != nullptr) {
        delete _udpSocket;
        _udpSocket = nullptr;
    }
}

void CastingService::SendDiscoveryBeacon() {
    if (_udpSocket == nullptr) return;

    auto deviceFamily = Windows::System::Profile::AnalyticsInfo::VersionInfo->DeviceFamily;
    String^ deviceName = deviceFamily == "Windows.Xbox" ? "Opal-Xbox" : "Opal-PC";
    
    String^ localIp = GetCachedLocalIp();

    String^ payload = "OPAL_BEACON|" + deviceName + "|" + localIp;
    
    create_task(_udpSocket->GetOutputStreamAsync(ref new Windows::Networking::HostName("239.0.0.222"), "9888")).then([this, payload](IOutputStream^ stream) {
        auto writer = ref new DataWriter(stream);
        writer->WriteString(payload);
        create_task(writer->StoreAsync()).then([writer, stream](unsigned int) {
            delete writer;
        });
    });
}

void CastingService::OnUdpMessageReceived(DatagramSocket^ sender, DatagramSocketMessageReceivedEventArgs^ args) {
    auto reader = args->GetDataReader();
    if (reader == nullptr) return;
    String^ message = reader->ReadString(reader->UnconsumedBufferLength);
    if (message == nullptr) return;
    
    std::wstring msg = message->Data();

    if (msg.find(L"OPAL_BEACON|") == 0) {
        size_t firstPipe = msg.find(L"|");
        size_t secondPipe = msg.find(L"|", firstPipe + 1);
        
        if (firstPipe != std::wstring::npos && secondPipe != std::wstring::npos) {
            String^ name = ref new String(msg.substr(firstPipe + 1, secondPipe - firstPipe - 1).c_str());
            String^ ip = ref new String(msg.substr(secondPipe + 1).c_str());
            
            if (GetCachedLocalIp() == ip) return;

            bool exists = false;
            for (auto d : _discoveredDevices) {
                if (d->Ip == ip) { exists = true; break; }
            }

            if (!exists) {
                _discoveredDevices->Append(ref new RemoteDevice(name, ip));
                DevicesChanged(this, nullptr);
                DebugLogger::Instance->Log("CastingService", "Discovered: " + name + " @ " + ip);
            }
        }
    }
    else if (msg.find(L"OPAL_CAST|") == 0) {
        String^ rawJson = ref new String(msg.substr(10).c_str());
        DebugLogger::Instance->Log("CastingService", "Inbound UDP Cast Received!");
        if (Dispatcher != nullptr) {
            Dispatcher->RunAsync(Windows::UI::Core::CoreDispatcherPriority::Normal, ref new Windows::UI::Core::DispatchedHandler([this, rawJson]() {
                ProcessMessage(rawJson);
            }));
        }
    }
    else if (msg.find(L"OPAL_CMD|") == 0) {
        String^ cmd = ref new String(msg.substr(9).c_str());
        DebugLogger::Instance->Log("CastingService", "Inbound Remote Command: " + cmd);
        if (Dispatcher != nullptr) {
            Dispatcher->RunAsync(Windows::UI::Core::CoreDispatcherPriority::Normal, ref new Windows::UI::Core::DispatchedHandler([this, cmd]() {
                auto pb = PlaybackService::Instance;
                if (cmd == "PLAY") pb->Player->Play();
                else if (cmd == "PAUSE") pb->Player->Pause();
                else if (std::wstring(cmd->Data()).find(L"SEEK|") == 0) {
                    std::wstring seekVal = cmd->Data();
                    seekVal = seekVal.substr(5);
                    long long ticks = std::stoll(seekVal);
                    Windows::Foundation::TimeSpan ts; ts.Duration = ticks;
                    pb->Player->PlaybackSession->Position = ts;
                }
                else if (std::wstring(cmd->Data()).find(L"QUEUE|") == 0) {
                    try {
                        String^ rawJson = ref new String(cmd->Data() + 6);
                        auto array = JsonArray::Parse(rawJson);
                        auto songs = ref new Platform::Collections::Vector<Song^>();
                        for (unsigned int i = 0; i < array->Size; i++) {
                            auto obj = array->GetObjectAt(i);
                            auto song = ref new Song();
                            song->Id = obj->GetNamedString("Id", "");
                            song->Title = obj->GetNamedString("Title", "");
                            song->Artist = obj->GetNamedString("Artist", "");
                            song->Album = obj->GetNamedString("Album", "");
                            song->CoverUrl = obj->GetNamedString("CoverUrl", "");
                            if (song->CoverUrl != nullptr && song->CoverUrl->Length() > 0) {
                                try { song->CoverArt = ref new Windows::UI::Xaml::Media::Imaging::BitmapImage(ref new Windows::Foundation::Uri(song->CoverUrl)); } catch(...) {}
                            }
                            song->StreamUrl = obj->GetNamedString("StreamUrl", "");
                            songs->Append(song);
                        }
                        pb->SetQueue(songs);
                        DebugLogger::Instance->Log("CastingService", "Inbound Queue Sync Received via CMD!");
                        QueueSynced(this, nullptr);
                    } catch (...) {}
                }
                else if (cmd == "DISCONNECT") {
                    CastingService::Instance->IsRemoteControlled = false;
                    DebugLogger::Instance->Log("CastingService", "Remote Control Disconnected.");
                }
            }));
        }
    }
    else if (msg.find(L"OPAL_STATE|") == 0) {
        // ... (Existing State Logic)
        String^ posStr = ref new String(msg.substr(11).c_str());
        long long ticks = std::stoll(posStr->Data());
        if (Dispatcher != nullptr) {
            Dispatcher->RunAsync(Windows::UI::Core::CoreDispatcherPriority::Normal, ref new Windows::UI::Core::DispatchedHandler([ticks]() {
                Windows::Foundation::TimeSpan ts; ts.Duration = ticks;
                PlaybackService::Instance->Player->PlaybackSession->Position = ts;
            }));
        }
    }
    else if (msg.find(L"OPAL_QUEUE|") == 0) {
        // (Keep this just in case, but usually hits OPAL_CMD|)
        String^ rawJson = ref new String(msg.substr(11).c_str());
        DebugLogger::Instance->Log("CastingService", "Inbound Queue Sync Received!");
        
        if (Dispatcher != nullptr) {
            Dispatcher->RunAsync(Windows::UI::Core::CoreDispatcherPriority::Normal, ref new Windows::UI::Core::DispatchedHandler([this, rawJson]() {
                try {
                    auto array = JsonArray::Parse(rawJson);
                    auto pb = PlaybackService::Instance;
                    auto songs = ref new Platform::Collections::Vector<Song^>();
                    for (unsigned int i = 0; i < array->Size; i++) {
                        auto obj = array->GetObjectAt(i);
                        auto song = ref new Song();
                        song->Id = obj->GetNamedString("Id", "");
                        song->Title = obj->GetNamedString("Title", "");
                        song->Artist = obj->GetNamedString("Artist", "");
                        song->Album = obj->GetNamedString("Album", "");
                        song->CoverUrl = obj->GetNamedString("CoverUrl", "");
                        if (song->CoverUrl != nullptr && song->CoverUrl->Length() > 0) {
                            try { song->CoverArt = ref new Windows::UI::Xaml::Media::Imaging::BitmapImage(ref new Windows::Foundation::Uri(song->CoverUrl)); } catch (...) {}
                        }
                        song->StreamUrl = obj->GetNamedString("StreamUrl", "");
                        songs->Append(song);
                    }
                    pb->SetQueue(songs);
                    QueueSynced(this, nullptr);
                } catch(...) {}
            }));
        }
    }
}

void CastingService::ProcessMessage(String^ rawJson) {
    try {
        auto json = JsonObject::Parse(rawJson);
        auto track = ref new TrackData();
        
        if (json->HasKey("Id")) track->Id = json->GetNamedString("Id");
        if (json->HasKey("Title")) track->Title = json->GetNamedString("Title");
        if (json->HasKey("Artist")) track->Artist = json->GetNamedString("Artist");
        if (json->HasKey("Album")) track->Album = json->GetNamedString("Album");
        if (json->HasKey("CoverUrl")) track->CoverUrl = json->GetNamedString("CoverUrl");
        if (json->HasKey("StreamUrl")) track->StreamUrl = json->GetNamedString("StreamUrl");
        if (json->HasKey("ExplicitStatus")) track->ExplicitStatus = json->GetNamedString("ExplicitStatus");

        DebugLogger::Instance->Log("CastingService", "Casting successful, playing: " + track->Title);
        IsRemoteControlled = true;
        TrackReceived(this, track);
        
        // Start state reporting on Xbox
        static Windows::UI::Xaml::DispatcherTimer^ stateTimer = nullptr;
        if (stateTimer == nullptr) {
            stateTimer = ref new Windows::UI::Xaml::DispatcherTimer();
            Windows::Foundation::TimeSpan ts; ts.Duration = 10000000LL; // 1 second
            stateTimer->Interval = ts;
            auto self = this;
            stateTimer->Tick += ref new Windows::Foundation::EventHandler<Object^>([self](Object^, Object^) {
                if (PlaybackService::Instance->Player->PlaybackSession->PlaybackState == Windows::Media::Playback::MediaPlaybackState::Playing) {
                    long long ticks = PlaybackService::Instance->Player->PlaybackSession->Position.Duration;
                    String^ payload = "OPAL_STATE|" + ticks.ToString();
                    if (self->_udpSocket != nullptr) {
                        create_task(self->_udpSocket->GetOutputStreamAsync(ref new Windows::Networking::HostName("239.0.0.222"), "9888")).then([payload](IOutputStream^ stream) {
                            auto writer = ref new DataWriter(stream);
                            writer->WriteString(payload);
                            create_task(writer->StoreAsync()).then([writer](unsigned int) { delete writer; });
                        });
                    }
                }
            });
            stateTimer->Start();
        }
    }
    catch (Exception^ ex) {
        DebugLogger::Instance->LogException("CastingService (ProcessMessage)", ex);
    }
}

void CastingService::CastTrack(String^ ipAddress, TrackData^ track) {
    if (_udpSocket == nullptr) return;
    
    // Find local IP to replace 'localhost' if necessary
    String^ localIp = GetCachedLocalIp();

    auto fixUrl = [localIp](String^ url) -> String^ {
        if (url == nullptr) return "";
        std::wstring u = url->Data();
        size_t pos = u.find(L"localhost");
        if (pos != std::wstring::npos) u.replace(pos, 9, localIp->Data());
        pos = u.find(L"127.0.0.1");
        if (pos != std::wstring::npos) u.replace(pos, 9, localIp->Data());
        return ref new String(u.c_str());
    };

    JsonObject^ json = ref new JsonObject();
    json->Insert("Id", JsonValue::CreateStringValue(track->Id));
    json->Insert("Title", JsonValue::CreateStringValue(track->Title));
    json->Insert("Artist", JsonValue::CreateStringValue(track->Artist));
    json->Insert("Album", JsonValue::CreateStringValue(track->Album));
    json->Insert("CoverUrl", JsonValue::CreateStringValue(fixUrl(track->CoverUrl)));
    json->Insert("StreamUrl", JsonValue::CreateStringValue(fixUrl(track->StreamUrl)));
    json->Insert("ExplicitStatus", JsonValue::CreateStringValue(track->ExplicitStatus));

    String^ payload = "OPAL_CAST|" + json->Stringify();
    DebugLogger::Instance->Log("CastingService", "Sending Cast Payload: " + payload);
    auto hostname = ref new Windows::Networking::HostName(ipAddress);

    create_task(_udpSocket->GetOutputStreamAsync(hostname, "9888")).then([this, payload](IOutputStream^ stream) {
        auto writer = ref new DataWriter(stream);
        writer->WriteString(payload);
        create_task(writer->StoreAsync()).then([writer, stream](unsigned int) {
            delete writer;
        });
    });
}

void CastingService::SendCommand(String^ command) {
    if (_udpSocket == nullptr || TargetDeviceIp == nullptr) return;

    String^ payload = "OPAL_CMD|" + command;
    auto hostname = ref new Windows::Networking::HostName(TargetDeviceIp);

    create_task(_udpSocket->GetOutputStreamAsync(hostname, "9888")).then([this, payload](IOutputStream^ stream) {
        auto writer = ref new DataWriter(stream);
        writer->WriteString(payload);
        create_task(writer->StoreAsync()).then([writer, stream](unsigned int) {
            delete writer;
        });
    });
}
