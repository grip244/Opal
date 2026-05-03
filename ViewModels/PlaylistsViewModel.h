#pragma once
#include <Windows.UI.Xaml.Data.h>
#include <collection.h>
#include "Models/SharedModels.h"

namespace Opal
{
    namespace ViewModels
    {
        [Windows::UI::Xaml::Data::Bindable]
        public ref class PlaylistsViewModel sealed : Windows::UI::Xaml::Data::INotifyPropertyChanged
        {
        private:
            static PlaylistsViewModel^ _instance;
            Windows::Foundation::Collections::IObservableVector<PlaylistModel^>^ _playlists;
            bool _isLoading;

        public:
            virtual event Windows::UI::Xaml::Data::PropertyChangedEventHandler^ PropertyChanged;

            static property PlaylistsViewModel^ Instance {
                PlaylistsViewModel^ get() {
                    if (_instance == nullptr) _instance = ref new PlaylistsViewModel();
                    return _instance;
                }
            }

            property Windows::Foundation::Collections::IObservableVector<PlaylistModel^>^ Playlists {
                Windows::Foundation::Collections::IObservableVector<PlaylistModel^>^ get() { return _playlists; }
            }

            property bool IsLoading {
                bool get() { return _isLoading; }
                void set(bool value) {
                    if (_isLoading != value) {
                        _isLoading = value;
                        NotifyPropertyChanged("IsLoading");
                    }
                }
            }

            void LoadPlaylistsAsync();
            void Clear();
            void CreatePlaylist(Platform::String^ name);
            void DeletePlaylist(Platform::String^ id);
            void AddSongToPlaylist(Platform::String^ playlistId, Platform::String^ songId);
            void RemoveSongFromPlaylist(Platform::String^ playlistId, int songIndex);
            void UpdatePlaylistMetadata(Platform::String^ playlistId, Platform::String^ name, Platform::String^ comment, bool isPublic);
            void ReorderPlaylist(Platform::String^ playlistId, Windows::Foundation::Collections::IVector<Platform::String^>^ songIds);
            void UploadPlaylistImage(Platform::String^ playlistId, Windows::Storage::Streams::IBuffer^ imageData, Platform::String^ mimeType);
            
            void NotifyPropertyChanged(Platform::String^ prop);

        private:
            PlaylistsViewModel();
        };
    }
}
