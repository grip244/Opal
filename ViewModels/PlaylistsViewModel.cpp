#include "pch.h"
#include "PlaylistsViewModel.h"
#include "Services/NavidromeService.h"
#include "Services/DebugLogger.h"

using namespace Opal;
using namespace Opal::ViewModels;
using namespace Platform;
using namespace Platform::Collections;
using namespace Windows::Data::Json;
using namespace concurrency;
using namespace Windows::Foundation::Collections;
using namespace Windows::Storage::Streams;

PlaylistsViewModel^ PlaylistsViewModel::_instance = nullptr;

PlaylistsViewModel::PlaylistsViewModel() : _isLoading(false)
{
    _playlists = ref new Platform::Collections::Vector<PlaylistModel^>();
}

void PlaylistsViewModel::LoadPlaylistsAsync()
{
    if (IsLoading) return;
    IsLoading = true;

    auto self = this;
    create_task(NavidromeService::Instance->GetPlaylistsAsync()).then([self](String^ jsonStr) {
        if (jsonStr != nullptr) {
            try {
                JsonObject^ rootRaw = JsonObject::Parse(jsonStr);
                JsonObject^ root = rootRaw->GetNamedObject("subsonic-response", nullptr);
                if (root != nullptr && root->HasKey("playlists")) {
                    JsonObject^ playlistsObj = root->GetNamedObject("playlists");
                    auto newPlaylists = ref new Vector<PlaylistModel^>();
                    if (playlistsObj->HasKey("playlist")) {
                        JsonArray^ playlistsArray = playlistsObj->GetNamedArray("playlist");
                        for (unsigned int i = 0; i < playlistsArray->Size; i++) {
                            JsonObject^ pObj = playlistsArray->GetObjectAt(i);
                            auto pm = ref new PlaylistModel();
                            pm->Id = pObj->GetNamedString("id", "");
                            pm->Name = pObj->GetNamedString("name", "Unknown Playlist");
                            pm->Owner = pObj->GetNamedString("owner", "Unknown");
                            pm->SongCount = (int)pObj->GetNamedNumber("songCount", 0);
                            pm->Comment = pObj->HasKey("comment") ? pObj->GetNamedString("comment", "") : "";
                            pm->IsPublic = pObj->HasKey("public") ? pObj->GetNamedBoolean("public", false) : false;
                            
                            int duration = (int)pObj->GetNamedNumber("duration", 0);
                            wchar_t buf[32];
                            swprintf_s(buf, L"%d songs \x00b7 %d:%02d", pm->SongCount, duration / 60, duration % 60);
                            pm->Duration = ref new String(buf);

                            String^ coverId = pObj->HasKey("coverArt") ? pObj->GetNamedString("coverArt") : "";
                            if (coverId->Length() > 0) {
                                String^ url = NavidromeService::Instance->GetCoverArtUrl(coverId, 300);
                                pm->CoverArt = ref new Windows::UI::Xaml::Media::Imaging::BitmapImage(ref new Windows::Foundation::Uri(url));
                            }

                            newPlaylists->Append(pm);
                        }
                    }

                    auto disp = App::MainDispatcher;
                    if (disp != nullptr) {
                        disp->RunAsync(Windows::UI::Core::CoreDispatcherPriority::Normal, ref new Windows::UI::Core::DispatchedHandler([self, newPlaylists]() {
                            self->_playlists->Clear();
                            for (auto p : newPlaylists) self->_playlists->Append(p);
                            self->NotifyPropertyChanged("Playlists");
                            self->IsLoading = false;
                        }));
                    }
                    return;
                }
            }
            catch (Exception^ ex) {
                DebugLogger::Instance->LogException("PlaylistsViewModel::LoadPlaylistsAsync", ex);
            }
        }
        self->IsLoading = false;
    });
}

void PlaylistsViewModel::NotifyPropertyChanged(String^ prop)
{
    PropertyChanged(this, ref new Windows::UI::Xaml::Data::PropertyChangedEventArgs(prop));
}

void PlaylistsViewModel::CreatePlaylist(String^ name)
{
    auto self = this;
    create_task(NavidromeService::Instance->CreatePlaylistAsync(name)).then([self](String^ json) {
        self->LoadPlaylistsAsync();
    });
}

void PlaylistsViewModel::DeletePlaylist(String^ id)
{
    auto self = this;
    create_task(NavidromeService::Instance->DeletePlaylistAsync(id)).then([self]() {
        self->LoadPlaylistsAsync();
    });
}

void PlaylistsViewModel::AddSongToPlaylist(String^ playlistId, String^ songId)
{
    auto self = this;
    create_task(NavidromeService::Instance->AddSongToPlaylistAsync(playlistId, songId)).then([self]() {
        self->LoadPlaylistsAsync();
    });
}

void PlaylistsViewModel::RemoveSongFromPlaylist(String^ playlistId, int songIndex)
{
    auto self = this;
    create_task(NavidromeService::Instance->RemoveSongFromPlaylistAsync(playlistId, songIndex)).then([self]() {
        self->LoadPlaylistsAsync();
    });
}

void PlaylistsViewModel::UpdatePlaylistMetadata(String^ playlistId, String^ name, String^ comment, bool isPublic)
{
    auto self = this;
    create_task(NavidromeService::Instance->UpdatePlaylistMetadataAsync(playlistId, name, comment, isPublic)).then([self]() {
        self->LoadPlaylistsAsync();
    });
}

void PlaylistsViewModel::ReorderPlaylist(String^ playlistId, Windows::Foundation::Collections::IVector<String^>^ songIds)
{
    auto self = this;
    create_task(NavidromeService::Instance->ReorderPlaylistAsync(playlistId, songIds)).then([self]() {
        self->LoadPlaylistsAsync();
    });
}

void PlaylistsViewModel::UploadPlaylistImage(String^ playlistId, IBuffer^ imageData, String^ mimeType)
{
    auto self = this;
    create_task(NavidromeService::Instance->UploadPlaylistImageAsync(playlistId, imageData, mimeType)).then([self]() {
        self->LoadPlaylistsAsync();
    });
}
