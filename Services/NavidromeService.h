#pragma once

#include <ppltasks.h>
#include <string>

namespace Opal
{
    public ref class NavidromeService sealed
    {
    public:
        static property NavidromeService^ Instance
        {
            NavidromeService^ get();
        }

        bool IsAuthenticated();
        void SetCredentials(Platform::String^ serverUrl, Platform::String^ username, Platform::String^ password);
        
        Platform::String^ GetServerUrl() { return _serverUrl; }
        Platform::String^ GetUsername() { return _username; }
        Platform::String^ GetPassword() { return _password; }

        Windows::Foundation::IAsyncOperation<Platform::String^>^ LoginAsync(Platform::String^ serverUrl, Platform::String^ username, Platform::String^ password);

        // API Methods (using stored credentials)
        Windows::Foundation::IAsyncOperation<Platform::String^>^ GetAlbumListAsync(Platform::String^ type, int size, int offset);
        Windows::Foundation::IAsyncOperation<Platform::String^>^ GetAlbumListByGenreAsync(Platform::String^ genre, int size, int offset);
        Windows::Foundation::IAsyncOperation<Platform::String^>^ GetAlbumListByYearAsync(int fromYear, int toYear, int size, int offset);
        Platform::String^ GetCoverArtUrl(Platform::String^ id, int size);
        Platform::String^ GetStreamUrl(Platform::String^ id);
        Windows::Foundation::IAsyncOperation<Platform::String^>^ GetSongListAsync(Platform::String^ endpoint, int size);
        Windows::Foundation::IAsyncOperation<Platform::String^>^ GetLyricsBySongIdAsync(Platform::String^ songId);
        Windows::Foundation::IAsyncOperation<Platform::String^>^ SearchAsync(Platform::String^ query, int size);
        Windows::Foundation::IAsyncOperation<Platform::String^>^ GetArtistsAsync();
        Windows::Foundation::IAsyncOperation<Platform::String^>^ GetGenresAsync();
        Windows::Foundation::IAsyncOperation<Platform::String^>^ GetArtistAsync(Platform::String^ id);
        Windows::Foundation::IAsyncOperation<Platform::String^>^ GetAlbumAsync(Platform::String^ id);
        Windows::Foundation::IAsyncOperation<Platform::String^>^ GetSongAsync(Platform::String^ id);
        Windows::Foundation::IAsyncAction^ ScrobbleAsync(Platform::String^ id, bool submission, long long time);
        Windows::Foundation::IAsyncAction^ ToggleFavoriteAsync(Platform::String^ id, bool isFavorite);
        Windows::Foundation::IAsyncAction^ SetRatingAsync(Platform::String^ id, int rating);
        Windows::Foundation::IAsyncOperation<Platform::String^>^ GetPlaylistsAsync();
        Windows::Foundation::IAsyncOperation<Platform::String^>^ GetPlaylistAsync(Platform::String^ id);
        Windows::Foundation::IAsyncOperation<Platform::String^>^ CreatePlaylistAsync(Platform::String^ name);
        Windows::Foundation::IAsyncAction^ DeletePlaylistAsync(Platform::String^ id);
        Windows::Foundation::IAsyncAction^ AddSongToPlaylistAsync(Platform::String^ playlistId, Platform::String^ songId);
        Windows::Foundation::IAsyncAction^ UpdatePlaylistNameAsync(Platform::String^ playlistId, Platform::String^ newName);
        Windows::Foundation::IAsyncAction^ UpdatePlaylistMetadataAsync(Platform::String^ playlistId, Platform::String^ name, Platform::String^ comment, bool isPublic);
        Windows::Foundation::IAsyncAction^ ReorderPlaylistAsync(Platform::String^ playlistId, Windows::Foundation::Collections::IVector<Platform::String^>^ songIds);
        Windows::Foundation::IAsyncAction^ RemoveSongFromPlaylistAsync(Platform::String^ playlistId, int songIndex);
        Windows::Foundation::IAsyncAction^ UploadPlaylistImageAsync(Platform::String^ playlistId, Windows::Storage::Streams::IBuffer^ imageData, Platform::String^ mimeType);


    private:
        NavidromeService();
        static NavidromeService^ _instance;

        Windows::Web::Http::HttpClient^ _httpClient;
        Windows::Web::Http::Filters::HttpBaseProtocolFilter^ _filter;

        Platform::String^ _serverUrl;
        Platform::String^ _username;
        Platform::String^ _password;

        Platform::String^ _sessionSalt;
        Platform::String^ GenerateSalt();
        Platform::String^ ComputeMd5Token(Platform::String^ input);
        Platform::String^ NormalizeUrl(Platform::String^ url);

    internal:
        static std::wstring NormalizeUrlNative(const std::wstring& url);
    };
}
