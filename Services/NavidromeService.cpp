#include "pch.h"
#include "NavidromeService.h"
#include "DebugLogger.h"

using namespace Opal;
using namespace Platform;
using namespace concurrency;
using namespace Windows::Storage::Streams;
using namespace Windows::Security::Cryptography;
using namespace Windows::Security::Cryptography::Core;
using namespace Windows::Web::Http;
using namespace Windows::Foundation;

NavidromeService^ NavidromeService::_instance = nullptr;

NavidromeService^ NavidromeService::Instance::get()
{
    if (_instance == nullptr)
    {
        _instance = ref new NavidromeService();
    }
    return _instance;
}

NavidromeService::NavidromeService()
{
    _filter = ref new Windows::Web::Http::Filters::HttpBaseProtocolFilter();
    _httpClient = ref new HttpClient(_filter);
    _httpClient->DefaultRequestHeaders->UserAgent->TryParseAdd("Opal/1.0 (Windows/Xbox)");
}

// Creates a per-request HttpClient to avoid ChangedStateException when
// the shared HttpBaseProtocolFilter is accessed concurrently.
HttpClient^ NavidromeService::CreateRequestClient()
{
    auto filter = ref new Windows::Web::Http::Filters::HttpBaseProtocolFilter();
    auto client = ref new HttpClient(filter);
    client->DefaultRequestHeaders->UserAgent->TryParseAdd("Opal/1.0 (Windows/Xbox)");
    return client;
}

bool NavidromeService::IsAuthenticated()
{
    return _serverUrl != nullptr && _username != nullptr && _password != nullptr;
}

void NavidromeService::SetCredentials(String^ serverUrl, String^ username, String^ password)
{
    std::lock_guard<std::recursive_mutex> lock(_sessionMutex);
    _serverUrl = serverUrl;
    _username = username;
    _password = password;
    _sessionSalt = nullptr;
    _sessionToken = nullptr;
}

String^ NavidromeService::NormalizeUrl(String^ url)
{
    if (url == nullptr || url->IsEmpty()) return "";
    std::wstring result = NormalizeUrlNative(std::wstring(url->Data()));
    return ref new String(result.c_str());
}

std::wstring NavidromeService::NormalizeUrlNative(const std::wstring& url)
{
    if (url.empty()) return L"";
    std::wstring s = url;
    auto schemePos = s.find(L"://");
    if (schemePos == std::wstring::npos) {
        s = L"https://" + s;
        schemePos = 5;
    }
    size_t hostStart = schemePos + 3;
    size_t slashPos = s.find(L'/', hostStart);
    std::wstring hostPort = (slashPos == std::wstring::npos) ? s.substr(hostStart) : s.substr(hostStart, slashPos - hostStart);
    if (hostPort.find(L':') == std::wstring::npos) {
        s.insert(hostStart + hostPort.size(), L":4533");
    }
    return s;
}

String^ NavidromeService::GenerateSalt()
{
    if (_sessionSalt != nullptr) return _sessionSalt;
    auto buf = CryptographicBuffer::GenerateRandom(8);
    _sessionSalt = CryptographicBuffer::EncodeToHexString(buf);
    return _sessionSalt;
}

String^ NavidromeService::GetSessionToken(String^ password)
{
    std::lock_guard<std::recursive_mutex> lock(_sessionMutex);
    if (_sessionToken != nullptr) return _sessionToken;
    auto salt = GenerateSalt();
    _sessionToken = ComputeMd5Token(password + salt);
    return _sessionToken;
}

String^ NavidromeService::ComputeMd5Token(String^ input)
{
    // NOTE: MD5 is used here specifically because it is required by the Subsonic API protocol (v1.16.1)
    // for token-based authentication (t= and s= parameters). Use of stronger algorithms like SHA256 
    // would result in authentication failure with all Subsonic-compliant servers (like Navidrome).
    auto alg = HashAlgorithmProvider::OpenAlgorithm("MD5");
    auto buffer = CryptographicBuffer::ConvertStringToBinary(input, BinaryStringEncoding::Utf8);
    auto hash = alg->HashData(buffer);
    return CryptographicBuffer::EncodeToHexString(hash);
}

IAsyncOperation<String^>^ NavidromeService::LoginAsync(String^ serverUrl, String^ username, String^ password)
{
    if (serverUrl == nullptr || username == nullptr || password == nullptr) return create_async([]() -> String^ { return "ERR_EMPTY_CREDENTIALS"; });

    return create_async([=]() -> String^ {
        try
        {
            auto salt = GenerateSalt();
            auto token = ComputeMd5Token(password + salt);
            auto normalizedServerUrl = NormalizeUrl(serverUrl);

            std::wstring rel = L"rest/ping.view?u=" + std::wstring(Uri::EscapeComponent(username)->Data()) +
                L"&t=" + std::wstring(token->Data()) +
                L"&s=" + std::wstring(salt->Data()) +
                L"&v=1.16.1&c=Opal[Xbox/Windows]&f=json";

            std::wstring fullUrl = std::wstring(normalizedServerUrl->Data());
            if (fullUrl.back() != L'/') fullUrl += L'/';
            fullUrl += rel;

            auto uri = ref new Uri(ref new String(fullUrl.c_str()));
            auto client = CreateRequestClient();
            auto response = create_task(client->GetAsync(uri)).get();
            
            if (response != nullptr && response->Content != nullptr)
            {
                auto str = create_task(response->Content->ReadAsStringAsync()).get();
                if (str != nullptr) {
                    std::wstring ws(str->Data());
                    
                    // Specific check for success status
                    if (ws.find(L"\"status\":\"ok\"") != std::wstring::npos || ws.find(L"\"status\": \"ok\"") != std::wstring::npos)
                    {
                        SetCredentials(serverUrl, username, password);
                        return "SUCCESS";
                    }
                    
                    // Check for specific Subsonic error codes
                    if (ws.find(L"\"code\":40") != std::wstring::npos) return "ERR_INVALID_CREDENTIALS";
                    if (ws.find(L"\"code\":50") != std::wstring::npos || response->StatusCode == Windows::Web::Http::HttpStatusCode::TooManyRequests) return "ERR_RATE_LIMIT";
                }
            }
            
            if (response != nullptr && response->StatusCode == Windows::Web::Http::HttpStatusCode::Unauthorized)
                return "ERR_UNAUTHORIZED";

            return "ERR_SERVER_CONNECTION";
        }
        catch (Exception^ ex)
        {
            DebugLogger::Instance->LogException("LoginAsync", ex);
            return "ERR_NETWORK_FAILURE";
        }
    });
}

IAsyncOperation<String^>^ NavidromeService::GetAlbumListAsync(String^ type, int size, int offset)
{
    if (!IsAuthenticated()) return create_async([]() -> String^ { return nullptr; });
    return create_async([=]() -> String^ {
        try
        {
            auto token = GetSessionToken(_password);
            auto salt = GenerateSalt();
            auto normalizedServerUrl = NormalizeUrl(_serverUrl);

            std::wstring rel = L"rest/getAlbumList2.view?u=" + std::wstring(_username->Data()) +
                L"&t=" + std::wstring(token->Data()) +
                L"&s=" + std::wstring(salt->Data()) +
                L"&v=1.16.1&c=Opal[Xbox/Windows]&f=json" +
                L"&type=" + std::wstring(Uri::EscapeComponent(type)->Data()) +
                L"&size=" + std::to_wstring(size) +
                L"&offset=" + std::to_wstring(offset);

            std::wstring fullUrl = std::wstring(normalizedServerUrl->Data());
            if (fullUrl.back() != L'/') fullUrl += L'/';
            fullUrl += rel;

            auto client = CreateRequestClient();
            auto response = create_task(client->GetAsync(ref new Uri(ref new String(fullUrl.c_str())))).get();
            if (!response->IsSuccessStatusCode) return nullptr;
            return create_task(response->Content->ReadAsStringAsync()).get();
        }
        catch (...) { return nullptr; }
    });
}

IAsyncOperation<String^>^ NavidromeService::GetAlbumListByGenreAsync(String^ genre, int size, int offset)
{
    if (!IsAuthenticated()) return create_async([]() -> String^ { return nullptr; });
    return create_async([=]() -> String^ {
        try
        {
            auto token = GetSessionToken(_password);
            auto salt = GenerateSalt();
            auto normalizedServerUrl = NormalizeUrl(_serverUrl);

            std::wstring rel = L"rest/getAlbumList2.view?u=" + std::wstring(_username->Data()) +
                L"&t=" + std::wstring(token->Data()) +
                L"&s=" + std::wstring(salt->Data()) +
                L"&v=1.16.1&c=Opal[Xbox/Windows]&f=json" +
                L"&type=byGenre&genre=" + std::wstring(Windows::Foundation::Uri::EscapeComponent(genre)->Data()) +
                L"&size=" + std::to_wstring(size) +
                L"&offset=" + std::to_wstring(offset);

            std::wstring fullUrl = std::wstring(normalizedServerUrl->Data());
            if (fullUrl.back() != L'/') fullUrl += L'/';
            fullUrl += rel;

            auto client = CreateRequestClient();
            auto response = create_task(client->GetAsync(ref new Uri(ref new String(fullUrl.c_str())))).get();
            if (!response->IsSuccessStatusCode) return nullptr;
            return create_task(response->Content->ReadAsStringAsync()).get();
        }
        catch (...) { return nullptr; }
    });
}

IAsyncOperation<String^>^ NavidromeService::GetAlbumListByYearAsync(int fromYear, int toYear, int size, int offset)
{
    if (!IsAuthenticated()) return create_async([]() -> String^ { return nullptr; });
    return create_async([=]() -> String^ {
        try
        {
            auto token = GetSessionToken(_password);
            auto salt = GenerateSalt();
            auto normalizedServerUrl = NormalizeUrl(_serverUrl);

            std::wstring rel = L"rest/getAlbumList2.view?u=" + std::wstring(_username->Data()) +
                L"&t=" + std::wstring(token->Data()) +
                L"&s=" + std::wstring(salt->Data()) +
                L"&v=1.16.1&c=Opal[Xbox/Windows]&f=json" +
                L"&type=byYear&fromYear=" + std::to_wstring(fromYear) + L"&toYear=" + std::to_wstring(toYear) +
                L"&size=" + std::to_wstring(size) +
                L"&offset=" + std::to_wstring(offset);

            std::wstring fullUrl = std::wstring(normalizedServerUrl->Data());
            if (fullUrl.back() != L'/') fullUrl += L'/';
            fullUrl += rel;

            auto client = CreateRequestClient();
            auto response = create_task(client->GetAsync(ref new Uri(ref new String(fullUrl.c_str())))).get();
            if (!response->IsSuccessStatusCode) return nullptr;
            return create_task(response->Content->ReadAsStringAsync()).get();
        }
        catch (...) { return nullptr; }
    });
}

String^ NavidromeService::GetCoverArtUrl(String^ id, int size)
{
    if (!IsAuthenticated()) return "";
    auto token = GetSessionToken(_password);
    auto salt = GenerateSalt();
    auto normalizedServerUrl = NormalizeUrl(_serverUrl);

    std::wstring rel = L"rest/getCoverArt.view?u=" + std::wstring(_username->Data()) +
        L"&t=" + std::wstring(token->Data()) +
        L"&s=" + std::wstring(salt->Data()) +
        L"&v=1.16.1&c=Opal[Xbox/Windows]&id=" + std::wstring(Uri::EscapeComponent(id)->Data()) +
        L"&size=" + std::to_wstring(size) + L"&c=true";

    std::wstring fullUrl = std::wstring(normalizedServerUrl->Data());
    if (fullUrl.back() != L'/') fullUrl += L'/';
    fullUrl += rel;

    return ref new String(fullUrl.c_str());
}

String^ NavidromeService::GetStreamUrl(String^ id)
{
    if (!IsAuthenticated()) return "";
    auto token = GetSessionToken(_password);
    auto salt = GenerateSalt();
    auto normalizedServerUrl = NormalizeUrl(_serverUrl);

    std::wstring rel = L"rest/stream.view?u=" + std::wstring(_username->Data()) +
        L"&t=" + std::wstring(token->Data()) +
        L"&s=" + std::wstring(salt->Data()) +
        L"&v=1.16.1&c=Opal[Xbox/Windows]&id=" + std::wstring(Uri::EscapeComponent(id)->Data());

    std::wstring fullUrl = std::wstring(normalizedServerUrl->Data());
    if (fullUrl.back() != L'/') fullUrl += L'/';
    fullUrl += rel;

    return ref new String(fullUrl.c_str());
}

IAsyncOperation<String^>^ NavidromeService::GetSongListAsync(String^ endpoint, int size, int offset)
{
    if (!IsAuthenticated()) return create_async([]() -> String^ { return nullptr; });
    return create_async([=]() -> String^ {
        try
        {
            auto token = GetSessionToken(_password);
            auto salt = GenerateSalt();
            auto normalizedServerUrl = NormalizeUrl(_serverUrl);

            std::wstring rel = L"rest/" + std::wstring(endpoint->Data()) + L".view?u=" + std::wstring(_username->Data()) +
                L"&t=" + std::wstring(token->Data()) +
                L"&s=" + std::wstring(salt->Data()) +
                L"&v=1.16.1&c=Opal[Xbox/Windows]&f=json";
            
            if (size > 0) rel += L"&size=" + std::to_wstring(size);
            if (offset > 0) rel += L"&offset=" + std::to_wstring(offset);

            std::wstring fullUrl = std::wstring(normalizedServerUrl->Data());
            if (fullUrl.back() != L'/') fullUrl += L'/';
            fullUrl += rel;

            auto client = CreateRequestClient();
            auto response = create_task(client->GetAsync(ref new Uri(ref new String(fullUrl.c_str())))).get();
            if (!response->IsSuccessStatusCode) return nullptr;
            return create_task(response->Content->ReadAsStringAsync()).get();
        }
        catch (...) { return nullptr; }
    });
}

IAsyncOperation<String^>^ NavidromeService::GetLyricsBySongIdAsync(String^ songId)
{
    if (!IsAuthenticated()) return create_async([]() -> String^ { return nullptr; });
    return create_async([=]() -> String^ {
        try
        {
            auto token = GetSessionToken(_password);
            auto salt = GenerateSalt();
            auto normalizedServerUrl = NormalizeUrl(_serverUrl);

            std::wstring rel = L"rest/getLyricsBySongId.view?u=" + std::wstring(_username->Data()) +
                L"&t=" + std::wstring(token->Data()) +
                L"&s=" + std::wstring(salt->Data()) +
                L"&v=1.16.1&c=Opal[Xbox/Windows]&f=json" +
                L"&enhanced=true&id=" + std::wstring(Uri::EscapeComponent(songId)->Data());

            std::wstring fullUrl = std::wstring(normalizedServerUrl->Data());
            if (fullUrl.back() != L'/') fullUrl += L'/';
            fullUrl += rel;

            auto client = CreateRequestClient();
            auto response = create_task(client->GetAsync(ref new Uri(ref new String(fullUrl.c_str())))).get();
            if (!response->IsSuccessStatusCode) return nullptr;
            return create_task(response->Content->ReadAsStringAsync()).get();
        }
        catch (...) { return nullptr; }
    });
}

IAsyncOperation<String^>^ NavidromeService::SearchAsync(String^ query, int size, int offset)
{
    if (!IsAuthenticated()) return create_async([]() -> String^ { return nullptr; });
    return create_async([=]() -> String^ {
        try
        {
            auto token = GetSessionToken(_password);
            auto salt = GenerateSalt();
            auto normalizedServerUrl = NormalizeUrl(_serverUrl);

            std::wstring rel = L"rest/search3.view?u=" + std::wstring(_username->Data()) +
                L"&t=" + std::wstring(token->Data()) +
                L"&s=" + std::wstring(salt->Data()) +
                L"&v=1.16.1&c=Opal[Xbox/Windows]&f=json" +
                L"&query=" + std::wstring(Uri::EscapeComponent(query)->Data()) +
                L"&songCount=" + std::to_wstring(size) +
                L"&songOffset=" + std::to_wstring(offset);

            std::wstring fullUrl = std::wstring(normalizedServerUrl->Data());
            if (fullUrl.back() != L'/') fullUrl += L'/';
            fullUrl += rel;

            auto client = CreateRequestClient();
            auto response = create_task(client->GetAsync(ref new Uri(ref new String(fullUrl.c_str())))).get();
            if (!response->IsSuccessStatusCode) return nullptr;
            return create_task(response->Content->ReadAsStringAsync()).get();
        }
        catch (...) { return nullptr; }
    });
}

IAsyncOperation<String^>^ NavidromeService::GetArtistsAsync()
{
    return GetSongListAsync("getArtists", 0, 0);
}

IAsyncOperation<String^>^ NavidromeService::GetGenresAsync()
{
    return GetSongListAsync("getGenres", 0, 0);
}

IAsyncOperation<String^>^ NavidromeService::GetArtistAsync(String^ id)
{
    if (!IsAuthenticated() || _serverUrl == nullptr || _username == nullptr) return create_async([]() -> String^ { return nullptr; });
    return create_async([=]() -> String^ {
        auto token = GetSessionToken(_password);
        auto salt = GenerateSalt();
        auto normalizedServerUrl = NormalizeUrl(_serverUrl);
        std::wstring rel = L"rest/getArtist.view?u=" + std::wstring(_username->Data()) + L"&t=" + std::wstring(token->Data()) + L"&s=" + std::wstring(salt->Data()) + L"&v=1.16.1&c=Opal[Xbox/Windows]&f=json&id=" + std::wstring(Uri::EscapeComponent(id)->Data());
        std::wstring fullUrl = std::wstring(normalizedServerUrl->Data());
        if (fullUrl.back() != L'/') fullUrl += L'/';
        return create_task(CreateRequestClient()->GetAsync(ref new Uri(ref new String((fullUrl + rel).c_str())))).then([](HttpResponseMessage^ resp) {
            return resp->IsSuccessStatusCode ? create_task(resp->Content->ReadAsStringAsync()).get() : nullptr;
        }).get();
    });
}

IAsyncOperation<String^>^ NavidromeService::GetAlbumAsync(String^ id)
{
    if (!IsAuthenticated() || _serverUrl == nullptr || _username == nullptr) return create_async([]() -> String^ { return nullptr; });
    return create_async([=]() -> String^ {
        auto token = GetSessionToken(_password);
        auto salt = GenerateSalt();
        auto normalizedServerUrl = NormalizeUrl(_serverUrl);
        std::wstring rel = L"rest/getAlbum.view?u=" + std::wstring(_username->Data()) + L"&t=" + std::wstring(token->Data()) + L"&s=" + std::wstring(salt->Data()) + L"&v=1.16.1&c=Opal[Xbox/Windows]&f=json&id=" + std::wstring(Uri::EscapeComponent(id)->Data());
        std::wstring fullUrl = std::wstring(normalizedServerUrl->Data());
        if (fullUrl.back() != L'/') fullUrl += L'/';
        return create_task(CreateRequestClient()->GetAsync(ref new Uri(ref new String((fullUrl + rel).c_str())))).then([](HttpResponseMessage^ resp) {
            return resp->IsSuccessStatusCode ? create_task(resp->Content->ReadAsStringAsync()).get() : nullptr;
        }).get();
    });
}

IAsyncOperation<String^>^ NavidromeService::GetSongAsync(String^ id)
{
    if (!IsAuthenticated() || _serverUrl == nullptr || _username == nullptr) return create_async([]() -> String^ { return nullptr; });
    return create_async([=]() -> String^ {
        auto token = GetSessionToken(_password);
        auto salt = GenerateSalt();
        auto normalizedServerUrl = NormalizeUrl(_serverUrl);
        std::wstring rel = L"rest/getSong.view?u=" + std::wstring(_username->Data()) + L"&t=" + std::wstring(token->Data()) + L"&s=" + std::wstring(salt->Data()) + L"&v=1.16.1&c=Opal[Xbox/Windows]&f=json&id=" + std::wstring(Uri::EscapeComponent(id)->Data());
        std::wstring fullUrl = std::wstring(normalizedServerUrl->Data());
        if (fullUrl.back() != L'/') fullUrl += L'/';
        return create_task(CreateRequestClient()->GetAsync(ref new Uri(ref new String((fullUrl + rel).c_str())))).then([](HttpResponseMessage^ resp) {
            return resp->IsSuccessStatusCode ? create_task(resp->Content->ReadAsStringAsync()).get() : nullptr;
        }).get();
    });
}

Windows::Foundation::IAsyncAction^ NavidromeService::ScrobbleAsync(String^ id, bool submission, long long time)
{
    if (!IsAuthenticated()) return create_async([]() {});
    return create_async([=]() {
        try
        {
            auto token = GetSessionToken(_password);
            auto salt = GenerateSalt();
            auto normalizedServerUrl = NormalizeUrl(_serverUrl);

            std::wstring rel = L"rest/scrobble.view?u=" + std::wstring(_username->Data()) +
                L"&t=" + std::wstring(token->Data()) +
                L"&s=" + std::wstring(salt->Data()) +
                L"&v=1.16.1&c=Opal[Xbox/Windows]&f=json" +
                L"&id=" + std::wstring(Uri::EscapeComponent(id)->Data()) +
                L"&submission=" + (submission ? L"true" : L"false");

            if (time > 0) {
                rel += L"&time=" + std::to_wstring(time);
            }

            std::wstring fullUrl = std::wstring(normalizedServerUrl->Data());
            if (fullUrl.back() != L'/') fullUrl += L'/';
            fullUrl += rel;

            create_task(CreateRequestClient()->GetAsync(ref new Uri(ref new String(fullUrl.c_str())))).get();
        }
        catch (...) {}
    });
}

Windows::Foundation::IAsyncAction^ NavidromeService::ToggleFavoriteAsync(String^ id, bool isFavorite)
{
    if (!IsAuthenticated()) return create_async([]() {});
    return create_async([=]() {
        try
        {
            auto token = GetSessionToken(_password);
            auto salt = GenerateSalt();
            auto normalizedServerUrl = NormalizeUrl(_serverUrl);

            std::wstring endpoint = isFavorite ? L"star" : L"unstar";
            std::wstring rel = L"rest/" + endpoint + L".view?u=" + std::wstring(_username->Data()) +
                L"&t=" + std::wstring(token->Data()) +
                L"&s=" + std::wstring(salt->Data()) +
                L"&v=1.16.1&c=Opal[Xbox/Windows]&f=json" +
                L"&id=" + std::wstring(Uri::EscapeComponent(id)->Data());

            std::wstring fullUrl = std::wstring(normalizedServerUrl->Data());
            if (fullUrl.back() != L'/') fullUrl += L'/';
            fullUrl += rel;

            create_task(CreateRequestClient()->GetAsync(ref new Uri(ref new String(fullUrl.c_str())))).get();
        }
        catch (...) {}
    });
}

Windows::Foundation::IAsyncAction^ NavidromeService::SetRatingAsync(String^ id, int rating)
{
    if (!IsAuthenticated()) return create_async([]() {});
    return create_async([=]() {
        try
        {
            auto token = GetSessionToken(_password);
            auto salt = GenerateSalt();
            auto normalizedServerUrl = NormalizeUrl(_serverUrl);

            std::wstring rel = L"rest/setRating.view?u=" + std::wstring(_username->Data()) +
                L"&t=" + std::wstring(token->Data()) +
                L"&s=" + std::wstring(salt->Data()) +
                L"&v=1.16.1&c=Opal[Xbox/Windows]&f=json" +
                L"&id=" + std::wstring(Uri::EscapeComponent(id)->Data()) +
                L"&rating=" + std::to_wstring(rating);

            std::wstring fullUrl = std::wstring(normalizedServerUrl->Data());
            if (fullUrl.back() != L'/') fullUrl += L'/';
            fullUrl += rel;

            create_task(CreateRequestClient()->GetAsync(ref new Uri(ref new String(fullUrl.c_str())))).get();
        }
        catch (...) {}
    });
}

IAsyncOperation<String^>^ NavidromeService::GetPlaylistsAsync()
{
    return GetSongListAsync("getPlaylists", 0, 0);
}

IAsyncOperation<String^>^ NavidromeService::GetPlaylistAsync(String^ id)
{
    if (!IsAuthenticated() || _serverUrl == nullptr || _username == nullptr) return create_async([]() -> String^ { return nullptr; });
    return create_async([=]() -> String^ {
        auto token = GetSessionToken(_password);
        auto salt = GenerateSalt();
        auto normalizedServerUrl = NormalizeUrl(_serverUrl);
        std::wstring rel = L"rest/getPlaylist.view?u=" + std::wstring(_username->Data()) + L"&t=" + std::wstring(token->Data()) + L"&s=" + std::wstring(salt->Data()) + L"&v=1.16.1&c=Opal[Xbox/Windows]&f=json&id=" + std::wstring(Uri::EscapeComponent(id)->Data());
        std::wstring fullUrl = std::wstring(normalizedServerUrl->Data());
        if (fullUrl.back() != L'/') fullUrl += L'/';
        return create_task(CreateRequestClient()->GetAsync(ref new Uri(ref new String((fullUrl + rel).c_str())))).then([](HttpResponseMessage^ resp) {
            return resp->IsSuccessStatusCode ? create_task(resp->Content->ReadAsStringAsync()).get() : nullptr;
        }).get();
    });
}

IAsyncOperation<String^>^ NavidromeService::CreatePlaylistAsync(String^ name)
{
    if (!IsAuthenticated()) return create_async([]() -> String^ { return nullptr; });
    return create_async([=]() -> String^ {
        try {
            auto token = GetSessionToken(_password);
            auto salt = GenerateSalt();
            auto normalizedServerUrl = NormalizeUrl(_serverUrl);
            auto escapedName = Windows::Foundation::Uri::EscapeComponent(name);
            std::wstring rel = L"rest/createPlaylist.view?u=" + std::wstring(_username->Data()) + L"&t=" + std::wstring(token->Data()) + L"&s=" + std::wstring(salt->Data()) + L"&v=1.16.1&c=Opal[Xbox/Windows]&f=json&name=" + std::wstring(escapedName->Data());
            std::wstring fullUrl = std::wstring(normalizedServerUrl->Data());
            if (fullUrl.back() != L'/') fullUrl += L'/';
            auto response = create_task(CreateRequestClient()->GetAsync(ref new Uri(ref new String((fullUrl + rel).c_str())))).get();
            if (response->IsSuccessStatusCode) {
                return create_task(response->Content->ReadAsStringAsync()).get();
            }
        } catch (...) {}
        return nullptr;
    });
}

IAsyncAction^ NavidromeService::DeletePlaylistAsync(String^ id)
{
    if (!IsAuthenticated()) return create_async([]() {});
    return create_async([=]() {
        try {
            auto token = GetSessionToken(_password);
            auto salt = GenerateSalt();
            auto normalizedServerUrl = NormalizeUrl(_serverUrl);
            auto escapedId = Windows::Foundation::Uri::EscapeComponent(id);
            std::wstring rel = L"rest/deletePlaylist.view?u=" + std::wstring(_username->Data()) + L"&t=" + std::wstring(token->Data()) + L"&s=" + std::wstring(salt->Data()) + L"&v=1.16.1&c=Opal[Xbox/Windows]&f=json&id=" + std::wstring(escapedId->Data());
            std::wstring fullUrl = std::wstring(normalizedServerUrl->Data());
            if (fullUrl.back() != L'/') fullUrl += L'/';
            create_task(CreateRequestClient()->GetAsync(ref new Uri(ref new String((fullUrl + rel).c_str())))).get();
        } catch (...) {}
    });
}

IAsyncAction^ NavidromeService::AddSongToPlaylistAsync(String^ playlistId, String^ songId)
{
    if (!IsAuthenticated()) return create_async([]() {});
    return create_async([=]() {
        try {
            auto token = GetSessionToken(_password);
            auto salt = GenerateSalt();
            auto normalizedServerUrl = NormalizeUrl(_serverUrl);
            auto escapedPid = Windows::Foundation::Uri::EscapeComponent(playlistId);
            auto escapedSid = Windows::Foundation::Uri::EscapeComponent(songId);
            // Pass both id and playlistId for maximum compatibility
            std::wstring rel = L"rest/updatePlaylist.view?u=" + std::wstring(_username->Data()) + L"&t=" + std::wstring(token->Data()) + L"&s=" + std::wstring(salt->Data()) + L"&v=1.16.1&c=Opal[Xbox/Windows]&f=json&playlistId=" + std::wstring(escapedPid->Data()) + L"&songIdToAdd=" + std::wstring(escapedSid->Data());
            std::wstring fullUrl = std::wstring(normalizedServerUrl->Data());
            if (fullUrl.back() != L'/') fullUrl += L'/';
            auto response = create_task(CreateRequestClient()->GetAsync(ref new Uri(ref new String((fullUrl + rel).c_str())))).get();
            DebugLogger::Instance->Log("NavidromeService", "AddSongToPlaylist Response: " + response->StatusCode.ToString());
        } catch (...) {}
    });
}

IAsyncAction^ NavidromeService::UpdatePlaylistNameAsync(String^ playlistId, String^ newName)
{
    return create_async([this, playlistId, newName]() {
        auto token = GetSessionToken(_password);
        auto salt = GenerateSalt();
        auto normalizedServerUrl = NormalizeUrl(_serverUrl);
        std::wstring rel = L"rest/updatePlaylist.view?u=" + std::wstring(_username->Data()) + L"&t=" + std::wstring(token->Data()) + L"&s=" + std::wstring(salt->Data()) + L"&v=1.16.1&c=Opal[Xbox/Windows]&f=json&playlistId=" + std::wstring(Uri::EscapeComponent(playlistId)->Data()) + L"&name=" + std::wstring(Uri::EscapeComponent(newName)->Data());
        std::wstring fullUrl = std::wstring(normalizedServerUrl->Data());
        if (fullUrl.back() != L'/') fullUrl += L'/';
        create_task(CreateRequestClient()->GetAsync(ref new Uri(ref new String((fullUrl + rel).c_str())))).get();
    });
}

IAsyncAction^ NavidromeService::RemoveSongFromPlaylistAsync(String^ playlistId, int songIndex)
{
    if (!IsAuthenticated()) return create_async([]() {});
    return create_async([=]() {
        try {
            auto token = GetSessionToken(_password);
            auto salt = GenerateSalt();
            auto normalizedServerUrl = NormalizeUrl(_serverUrl);
            auto escapedPid = Windows::Foundation::Uri::EscapeComponent(playlistId);
            // Pass both id and playlistId for maximum compatibility
            // Pass both playlistId and id for maximum compatibility
            std::wstring rel = L"rest/updatePlaylist.view?u=" + std::wstring(_username->Data()) + L"&t=" + std::wstring(token->Data()) + L"&s=" + std::wstring(salt->Data()) + L"&v=1.16.1&c=Opal[Xbox/Windows]&f=json&playlistId=" + std::wstring(escapedPid->Data()) + L"&id=" + std::wstring(escapedPid->Data()) + L"&songIndexToRemove=" + std::to_wstring(songIndex);
            std::wstring fullUrl = std::wstring(normalizedServerUrl->Data());
            if (fullUrl.back() != L'/') fullUrl += L'/';
            auto response = create_task(CreateRequestClient()->GetAsync(ref new Uri(ref new String((fullUrl + rel).c_str())))).get();
            DebugLogger::Instance->Log("NavidromeService", "RemoveSongFromPlaylist Response: " + response->StatusCode.ToString());
        } catch (...) {}
    });
}

IAsyncAction^ NavidromeService::UpdatePlaylistMetadataAsync(String^ playlistId, String^ name, String^ comment, bool isPublic)
{
    if (!IsAuthenticated()) return create_async([]() {});
    return create_async([=]() {
        try {
            auto token = GetSessionToken(_password);
            auto salt = GenerateSalt();
            auto normalizedServerUrl = NormalizeUrl(_serverUrl);
            auto escapedPid = Windows::Foundation::Uri::EscapeComponent(playlistId);
            auto escapedName = Windows::Foundation::Uri::EscapeComponent(name);
            auto escapedComment = Windows::Foundation::Uri::EscapeComponent(comment);

            std::wstring rel = L"rest/updatePlaylist.view?u=" + std::wstring(_username->Data()) +
                L"&t=" + std::wstring(token->Data()) +
                L"&s=" + std::wstring(salt->Data()) +
                L"&v=1.16.1&c=Opal[Xbox/Windows]&f=json&playlistId=" + std::wstring(escapedPid->Data()) +
                L"&name=" + std::wstring(escapedName->Data()) +
                L"&comment=" + std::wstring(escapedComment->Data()) +
                L"&public=" + (isPublic ? L"true" : L"false");

            std::wstring fullUrl = std::wstring(normalizedServerUrl->Data());
            if (fullUrl.back() != L'/') fullUrl += L'/';
            create_task(CreateRequestClient()->GetAsync(ref new Uri(ref new String((fullUrl + rel).c_str())))).get();
        } catch (...) {}
    });
}

IAsyncAction^ NavidromeService::ReorderPlaylistAsync(String^ playlistId, Windows::Foundation::Collections::IVector<String^>^ songIds)
{
    if (!IsAuthenticated()) return create_async([]() {});
    return create_async([=]() {
        try {
            // Reordering in Subsonic is most reliably done by using createPlaylist with the existing playlistId
            // which overwrites the playlist tracklist.
            auto token = GetSessionToken(_password);
            auto salt = GenerateSalt();
            auto normalizedServerUrl = NormalizeUrl(_serverUrl);
            auto escapedPid = Windows::Foundation::Uri::EscapeComponent(playlistId);

            std::wstring rel = L"rest/createPlaylist.view?u=" + std::wstring(_username->Data()) +
                L"&t=" + std::wstring(token->Data()) +
                L"&s=" + std::wstring(salt->Data()) +
                L"&v=1.16.1&c=Opal[Xbox/Windows]&f=json&playlistId=" + std::wstring(escapedPid->Data());

            for (auto sid : songIds) {
                rel += L"&songId=" + std::wstring(Windows::Foundation::Uri::EscapeComponent(sid)->Data());
            }

            std::wstring fullUrl = std::wstring(normalizedServerUrl->Data());
            if (fullUrl.back() != L'/') fullUrl += L'/';
            create_task(CreateRequestClient()->GetAsync(ref new Uri(ref new String((fullUrl + rel).c_str())))).get();
        } catch (...) {}
    });
}

IAsyncAction^ NavidromeService::UploadPlaylistImageAsync(String^ playlistId, Windows::Storage::Streams::IBuffer^ imageData, String^ mimeType)
{
    if (!IsAuthenticated()) return create_async([]() {});
    return create_async([=]() {
        try {
            auto token = GetSessionToken(_password);
            auto salt = GenerateSalt();
            auto normalizedServerUrl = NormalizeUrl(_serverUrl);
            auto escapedPid = Windows::Foundation::Uri::EscapeComponent(playlistId);

            std::wstring rel = L"rest/updatePlaylist.view?u=" + std::wstring(_username->Data()) +
                L"&t=" + std::wstring(token->Data()) +
                L"&s=" + std::wstring(salt->Data()) +
                L"&v=1.16.1&c=Opal[Xbox/Windows]&f=json&playlistId=" + std::wstring(escapedPid->Data());

            std::wstring fullUrl = std::wstring(normalizedServerUrl->Data());
            if (fullUrl.back() != L'/') fullUrl += L'/';
            fullUrl += rel;

            auto content = ref new Windows::Web::Http::HttpMultipartFormDataContent();
            auto fileContent = ref new Windows::Web::Http::HttpBufferContent(imageData);
            fileContent->Headers->ContentType = ref new Windows::Web::Http::Headers::HttpMediaTypeHeaderValue(mimeType);
            content->Add(fileContent, "image", "cover.jpg");

            create_task(CreateRequestClient()->PostAsync(ref new Uri(ref new String(fullUrl.c_str())), content)).get();
        } catch (...) {}
    });
}
