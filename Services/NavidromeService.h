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

        Windows::Foundation::IAsyncOperation<bool>^ LoginAsync(Platform::String^ serverUrl, Platform::String^ username, Platform::String^ password);

        // API Methods (using stored credentials)
        Windows::Foundation::IAsyncOperation<Platform::String^>^ GetAlbumListAsync(Platform::String^ type, int size, int offset);
        Windows::Foundation::IAsyncOperation<Platform::String^>^ GetAlbumListByYearAsync(int fromYear, int toYear, int size, int offset);
        Platform::String^ GetCoverArtUrl(Platform::String^ id, int size);
        Platform::String^ GetStreamUrl(Platform::String^ id);
        Windows::Foundation::IAsyncOperation<Platform::String^>^ GetSongListAsync(Platform::String^ endpoint, int size);
        Windows::Foundation::IAsyncOperation<Platform::String^>^ GetLyricsAsync(Platform::String^ artist, Platform::String^ title);
        Windows::Foundation::IAsyncOperation<Platform::String^>^ GetLyricsBySongIdAsync(Platform::String^ songId);
        Windows::Foundation::IAsyncOperation<Platform::String^>^ SearchAsync(Platform::String^ query, int size);
        Windows::Foundation::IAsyncOperation<Platform::String^>^ GetArtistsAsync();
        Windows::Foundation::IAsyncOperation<Platform::String^>^ GetArtistAsync(Platform::String^ id);
        Windows::Foundation::IAsyncOperation<Platform::String^>^ GetAlbumAsync(Platform::String^ id);
        Windows::Foundation::IAsyncAction^ ScrobbleAsync(Platform::String^ id, bool submission);

    private:
        NavidromeService();
        static NavidromeService^ _instance;

        Platform::String^ _serverUrl;
        Platform::String^ _username;
        Platform::String^ _password;

        Platform::String^ _sessionSalt;
        Platform::String^ GenerateSalt();
        Platform::String^ ComputeMd5Token(Platform::String^ input);
        Platform::String^ NormalizeUrl(Platform::String^ url);
    };
}
