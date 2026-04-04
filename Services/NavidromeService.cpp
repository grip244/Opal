#include "pch.h"
#include "NavidromeService.h"

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
}

bool NavidromeService::IsAuthenticated()
{
    return _serverUrl != nullptr && _username != nullptr && _password != nullptr;
}

void NavidromeService::SetCredentials(String^ serverUrl, String^ username, String^ password)
{
    _serverUrl = serverUrl;
    _username = username;
    _password = password;
}

String^ NavidromeService::NormalizeUrl(String^ url)
{
    std::wstring s(url->Data());
    auto schemePos = s.find(L"://");
    if (schemePos == std::wstring::npos) {
        s = L"http://" + s;
        schemePos = 4;
    }
    size_t hostStart = schemePos + 3;
    size_t slashPos = s.find(L'/', hostStart);
    std::wstring hostPort = (slashPos == std::wstring::npos) ? s.substr(hostStart) : s.substr(hostStart, slashPos - hostStart);
    if (hostPort.find(L':') == std::wstring::npos) {
        s.insert(hostStart + hostPort.size(), L":4533");
    }
    return ref new String(s.c_str());
}

String^ NavidromeService::GenerateSalt()
{
    if (_sessionSalt != nullptr) return _sessionSalt;
    auto buf = CryptographicBuffer::GenerateRandom(8);
    _sessionSalt = CryptographicBuffer::EncodeToHexString(buf);
    return _sessionSalt;
}

String^ NavidromeService::ComputeMd5Token(String^ input)
{
    auto alg = HashAlgorithmProvider::OpenAlgorithm("MD5");
    auto buffer = CryptographicBuffer::ConvertStringToBinary(input, BinaryStringEncoding::Utf8);
    auto hash = alg->HashData(buffer);
    return CryptographicBuffer::EncodeToHexString(hash);
}

IAsyncOperation<bool>^ NavidromeService::LoginAsync(String^ serverUrl, String^ username, String^ password)
{
    return create_async([=]() -> bool {
        try
        {
            auto salt = GenerateSalt();
            auto token = ComputeMd5Token(password + salt);
            auto normalizedServerUrl = NormalizeUrl(serverUrl);

            std::wstring rel = L"rest/ping.view?u=" + std::wstring(username->Data()) +
                L"&t=" + std::wstring(token->Data()) +
                L"&s=" + std::wstring(salt->Data()) +
                L"&v=1.16.1&c=Opal&f=json";

            std::wstring fullUrl = std::wstring(normalizedServerUrl->Data());
            if (fullUrl.back() != L'/') fullUrl += L'/';
            fullUrl += rel;

            HttpClient http;
            auto response = create_task(http.GetAsync(ref new Uri(ref new String(fullUrl.c_str())))).get();
            if (!response->IsSuccessStatusCode) return false;
            
            auto str = create_task(response->Content->ReadAsStringAsync()).get();
            std::wstring ws(str->Data());
            if (ws.find(L"ok") != std::wstring::npos || ws.find(L"status\":\"ok\"") != std::wstring::npos)
            {
                SetCredentials(serverUrl, username, password);
                return true;
            }
            return false;
        }
        catch (...)
        {
            return false;
        }
    });
}

IAsyncOperation<String^>^ NavidromeService::GetAlbumListAsync(String^ type, int size, int offset)
{
    return create_async([=]() -> String^ {
        try
        {
            auto salt = GenerateSalt();
            auto token = ComputeMd5Token(_password + salt);
            auto normalizedServerUrl = NormalizeUrl(_serverUrl);

            std::wstring rel = L"rest/getAlbumList2.view?u=" + std::wstring(_username->Data()) +
                L"&t=" + std::wstring(token->Data()) +
                L"&s=" + std::wstring(salt->Data()) +
                L"&v=1.16.1&c=Opal&f=json" +
                L"&type=" + std::wstring(type->Data()) +
                L"&size=" + std::to_wstring(size) +
                L"&offset=" + std::to_wstring(offset);

            std::wstring fullUrl = std::wstring(normalizedServerUrl->Data());
            if (fullUrl.back() != L'/') fullUrl += L'/';
            fullUrl += rel;

            HttpClient http;
            auto response = create_task(http.GetAsync(ref new Uri(ref new String(fullUrl.c_str())))).get();
            if (!response->IsSuccessStatusCode) return nullptr;
            return create_task(response->Content->ReadAsStringAsync()).get();
        }
        catch (...) { return nullptr; }
    });
}

IAsyncOperation<String^>^ NavidromeService::GetAlbumListByYearAsync(int fromYear, int toYear, int size, int offset)
{
    return create_async([=]() -> String^ {
        try
        {
            auto salt = GenerateSalt();
            auto token = ComputeMd5Token(_password + salt);
            auto normalizedServerUrl = NormalizeUrl(_serverUrl);

            std::wstring rel = L"rest/getAlbumList2.view?u=" + std::wstring(_username->Data()) +
                L"&t=" + std::wstring(token->Data()) +
                L"&s=" + std::wstring(salt->Data()) +
                L"&v=1.16.1&c=Opal&f=json" +
                L"&type=byYear&fromYear=" + std::to_wstring(fromYear) + L"&toYear=" + std::to_wstring(toYear) +
                L"&size=" + std::to_wstring(size) +
                L"&offset=" + std::to_wstring(offset);

            std::wstring fullUrl = std::wstring(normalizedServerUrl->Data());
            if (fullUrl.back() != L'/') fullUrl += L'/';
            fullUrl += rel;

            HttpClient http;
            auto response = create_task(http.GetAsync(ref new Uri(ref new String(fullUrl.c_str())))).get();
            if (!response->IsSuccessStatusCode) return nullptr;
            return create_task(response->Content->ReadAsStringAsync()).get();
        }
        catch (...) { return nullptr; }
    });
}

String^ NavidromeService::GetCoverArtUrl(String^ id, int size)
{
    auto salt = GenerateSalt();
    auto token = ComputeMd5Token(_password + salt);
    auto normalizedServerUrl = NormalizeUrl(_serverUrl);

    std::wstring rel = L"rest/getCoverArt.view?u=" + std::wstring(_username->Data()) +
        L"&t=" + std::wstring(token->Data()) +
        L"&s=" + std::wstring(salt->Data()) +
        L"&v=1.16.1&c=Opal&id=" + std::wstring(id->Data()) +
        L"&size=" + std::to_wstring(size);

    std::wstring fullUrl = std::wstring(normalizedServerUrl->Data());
    if (fullUrl.back() != L'/') fullUrl += L'/';
    fullUrl += rel;

    return ref new String(fullUrl.c_str());
}

String^ NavidromeService::GetStreamUrl(String^ id)
{
    auto salt = GenerateSalt();
    auto token = ComputeMd5Token(_password + salt);
    auto normalizedServerUrl = NormalizeUrl(_serverUrl);

    std::wstring rel = L"rest/stream.view?u=" + std::wstring(_username->Data()) +
        L"&t=" + std::wstring(token->Data()) +
        L"&s=" + std::wstring(salt->Data()) +
        L"&v=1.16.1&c=Opal&id=" + std::wstring(id->Data());

    std::wstring fullUrl = std::wstring(normalizedServerUrl->Data());
    if (fullUrl.back() != L'/') fullUrl += L'/';
    fullUrl += rel;

    return ref new String(fullUrl.c_str());
}

IAsyncOperation<String^>^ NavidromeService::GetSongListAsync(String^ endpoint, int size)
{
    return create_async([=]() -> String^ {
        try
        {
            auto salt = GenerateSalt();
            auto token = ComputeMd5Token(_password + salt);
            auto normalizedServerUrl = NormalizeUrl(_serverUrl);

            std::wstring rel = L"rest/" + std::wstring(endpoint->Data()) + L".view?u=" + std::wstring(_username->Data()) +
                L"&t=" + std::wstring(token->Data()) +
                L"&s=" + std::wstring(salt->Data()) +
                L"&v=1.16.1&c=Opal&f=json" +
                L"&size=" + std::to_wstring(size);

            std::wstring fullUrl = std::wstring(normalizedServerUrl->Data());
            if (fullUrl.back() != L'/') fullUrl += L'/';
            fullUrl += rel;

            HttpClient http;
            auto response = create_task(http.GetAsync(ref new Uri(ref new String(fullUrl.c_str())))).get();
            if (!response->IsSuccessStatusCode) return nullptr;
            return create_task(response->Content->ReadAsStringAsync()).get();
        }
        catch (...) { return nullptr; }
    });
}

IAsyncOperation<String^>^ NavidromeService::GetLyricsAsync(String^ artist, String^ title)
{
    return create_async([=]() -> String^ {
        try
        {
            auto salt = GenerateSalt();
            auto token = ComputeMd5Token(_password + salt);
            auto normalizedServerUrl = NormalizeUrl(_serverUrl);

            std::wstring rel = L"rest/getLyrics.view?u=" + std::wstring(_username->Data()) +
                L"&t=" + std::wstring(token->Data()) +
                L"&s=" + std::wstring(salt->Data()) +
                L"&v=1.16.1&c=Opal&f=json" +
                L"&artist=" + std::wstring(Uri::EscapeComponent(artist)->Data()) +
                L"&title=" + std::wstring(Uri::EscapeComponent(title)->Data());

            std::wstring fullUrl = std::wstring(normalizedServerUrl->Data());
            if (fullUrl.back() != L'/') fullUrl += L'/';
            fullUrl += rel;

            HttpClient http;
            auto response = create_task(http.GetAsync(ref new Uri(ref new String(fullUrl.c_str())))).get();
            if (!response->IsSuccessStatusCode) return nullptr;
            return create_task(response->Content->ReadAsStringAsync()).get();
        }
        catch (...) { return nullptr; }
    });
}

IAsyncOperation<String^>^ NavidromeService::GetLyricsBySongIdAsync(String^ songId)
{
    return create_async([=]() -> String^ {
        try
        {
            auto salt = GenerateSalt();
            auto token = ComputeMd5Token(_password + salt);
            auto normalizedServerUrl = NormalizeUrl(_serverUrl);

            std::wstring rel = L"rest/getLyricsBySongId.view?u=" + std::wstring(_username->Data()) +
                L"&t=" + std::wstring(token->Data()) +
                L"&s=" + std::wstring(salt->Data()) +
                L"&v=1.16.1&c=Opal&f=json" +
                L"&id=" + std::wstring(songId->Data());

            std::wstring fullUrl = std::wstring(normalizedServerUrl->Data());
            if (fullUrl.back() != L'/') fullUrl += L'/';
            fullUrl += rel;

            HttpClient http;
            auto response = create_task(http.GetAsync(ref new Uri(ref new String(fullUrl.c_str())))).get();
            if (!response->IsSuccessStatusCode) return nullptr;
            return create_task(response->Content->ReadAsStringAsync()).get();
        }
        catch (...) { return nullptr; }
    });
}

IAsyncOperation<String^>^ NavidromeService::SearchAsync(String^ query, int size)
{
    return create_async([=]() -> String^ {
        try
        {
            auto salt = GenerateSalt();
            auto token = ComputeMd5Token(_password + salt);
            auto normalizedServerUrl = NormalizeUrl(_serverUrl);

            std::wstring rel = L"rest/search3.view?u=" + std::wstring(_username->Data()) +
                L"&t=" + std::wstring(token->Data()) +
                L"&s=" + std::wstring(salt->Data()) +
                L"&v=1.16.1&c=Opal&f=json" +
                L"&query=" + std::wstring(Uri::EscapeComponent(query)->Data()) +
                L"&songCount=" + std::to_wstring(size);

            std::wstring fullUrl = std::wstring(normalizedServerUrl->Data());
            if (fullUrl.back() != L'/') fullUrl += L'/';
            fullUrl += rel;

            HttpClient http;
            auto response = create_task(http.GetAsync(ref new Uri(ref new String(fullUrl.c_str())))).get();
            if (!response->IsSuccessStatusCode) return nullptr;
            return create_task(response->Content->ReadAsStringAsync()).get();
        }
        catch (...) { return nullptr; }
    });
}

IAsyncOperation<String^>^ NavidromeService::GetArtistsAsync()
{
    return GetSongListAsync("getArtists", 0);
}

IAsyncOperation<String^>^ NavidromeService::GetArtistAsync(String^ id)
{
    return create_async([=]() -> String^ {
        auto salt = GenerateSalt();
        auto token = ComputeMd5Token(_password + salt);
        auto normalizedServerUrl = NormalizeUrl(_serverUrl);
        std::wstring rel = L"rest/getArtist.view?u=" + std::wstring(_username->Data()) + L"&t=" + std::wstring(token->Data()) + L"&s=" + std::wstring(salt->Data()) + L"&v=1.16.1&c=Opal&f=json&id=" + std::wstring(id->Data());
        std::wstring fullUrl = std::wstring(normalizedServerUrl->Data());
        if (fullUrl.back() != L'/') fullUrl += L'/';
        return create_task(HttpClient().GetAsync(ref new Uri(ref new String((fullUrl + rel).c_str())))).then([](HttpResponseMessage^ resp) {
            return resp->IsSuccessStatusCode ? create_task(resp->Content->ReadAsStringAsync()).get() : nullptr;
        }).get();
    });
}

IAsyncOperation<String^>^ NavidromeService::GetAlbumAsync(String^ id)
{
    return create_async([=]() -> String^ {
        auto salt = GenerateSalt();
        auto token = ComputeMd5Token(_password + salt);
        auto normalizedServerUrl = NormalizeUrl(_serverUrl);
        std::wstring rel = L"rest/getAlbum.view?u=" + std::wstring(_username->Data()) + L"&t=" + std::wstring(token->Data()) + L"&s=" + std::wstring(salt->Data()) + L"&v=1.16.1&c=Opal&f=json&id=" + std::wstring(id->Data());
        std::wstring fullUrl = std::wstring(normalizedServerUrl->Data());
        if (fullUrl.back() != L'/') fullUrl += L'/';
        return create_task(HttpClient().GetAsync(ref new Uri(ref new String((fullUrl + rel).c_str())))).then([](HttpResponseMessage^ resp) {
            return resp->IsSuccessStatusCode ? create_task(resp->Content->ReadAsStringAsync()).get() : nullptr;
        }).get();
    });
}

Windows::Foundation::IAsyncAction^ NavidromeService::ScrobbleAsync(String^ id, bool submission)
{
    return create_async([=]() {
        try
        {
            auto salt = GenerateSalt();
            auto token = ComputeMd5Token(_password + salt);
            auto normalizedServerUrl = NormalizeUrl(_serverUrl);

            std::wstring rel = L"rest/scrobble.view?u=" + std::wstring(_username->Data()) +
                L"&t=" + std::wstring(token->Data()) +
                L"&s=" + std::wstring(salt->Data()) +
                L"&v=1.16.1&c=Opal&f=json" +
                L"&id=" + std::wstring(id->Data()) +
                L"&submission=" + (submission ? L"true" : L"false");

            std::wstring fullUrl = std::wstring(normalizedServerUrl->Data());
            if (fullUrl.back() != L'/') fullUrl += L'/';
            fullUrl += rel;

            HttpClient http;
            create_task(http.GetAsync(ref new Uri(ref new String(fullUrl.c_str())))).get();
        }
        catch (...) {}
    });
}
