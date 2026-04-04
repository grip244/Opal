#include "pch.h"
#include "LyricsService.h"
#include <regex>
#include <algorithm>
#include <vector>
#include <string>
#include <sstream>
#include <ppltasks.h>

using namespace Opal::Models;
using namespace Opal::Services;
using namespace Platform;
using namespace concurrency;
using namespace Platform::Collections;
using namespace Windows::Foundation::Collections;
using namespace Windows::Data::Json;
using namespace Windows::Foundation;

struct InternalLyricLine {
    uint32_t TimestampMs;
    String^ Text;
};

LyricsResult^ LyricsService::ParseLrc(String^ rawLrc) {
    LyricsResult^ finalResult = ref new LyricsResult();
    finalResult->IsTimed = false;
    finalResult->Lines = ref new Vector<LyricLine^>();

    if (rawLrc == nullptr || rawLrc->IsEmpty()) return finalResult;

    std::wstring lyricsW = rawLrc->Data();
    std::wregex lrcRegex(L"\\[(\\d{2,}):(\\d{2})(?:\\.(\\d{2,3}))?\\]([^\\n\\r]*)");
    
    std::vector<InternalLyricLine> internalLines;

    auto words_begin = std::wsregex_iterator(lyricsW.begin(), lyricsW.end(), lrcRegex);
    auto words_end = std::wsregex_iterator();

    bool foundTimed = false;
    for (std::wsregex_iterator i = words_begin; i != words_end; ++i) {
        std::wsmatch match = *i;
        foundTimed = true;
        
        try {
            uint32_t minutes = std::stoul(match[1].str());
            uint32_t seconds = std::stoul(match[2].str());
            uint32_t ms = 0;

            if (match[3].matched) {
                std::wstring msStr = match[3].str();
                ms = std::stoul(msStr);
                if (msStr.length() == 2) {
                    ms *= 10;
                }
            }

            uint32_t totalMs = (minutes * 60 + seconds) * 1000 + ms;
            std::wstring textW = match[4].str();
            
            size_t first = textW.find_first_not_of(L" \t");
            if (std::wstring::npos != first) {
                size_t last = textW.find_last_not_of(L" \t\r\n");
                textW = textW.substr(first, (last - first + 1));
            }
            
            String^ text = ref new String(textW.c_str());
            internalLines.push_back({ totalMs, text });
        } catch (...) {}
    }

    if (!foundTimed) {
        std::wstringstream ss(lyricsW);
        std::wstring lineW;
        while (std::getline(ss, lineW)) {
            if (lineW.empty()) continue;
            if (lineW.back() == L'\r') lineW.pop_back();
            if (lineW.empty()) continue;
            internalLines.push_back({ 0, ref new String(lineW.c_str()) });
        }
    }

    std::sort(internalLines.begin(), internalLines.end(), [](const InternalLyricLine& a, const InternalLyricLine& b) {
        return a.TimestampMs < b.TimestampMs;
    });

    Vector<LyricLine^>^ result = ref new Vector<LyricLine^>();
    for (const auto& internalLine : internalLines) {
        LyricLine^ line = ref new LyricLine();
        line->TimestampMs = internalLine.TimestampMs;
        line->Text = internalLine.Text;
        result->Append(line);
    }

    finalResult->Lines = result;
    finalResult->IsTimed = foundTimed;
    return finalResult;
}

LyricsResult^ LyricsService::ParseOpenSubsonicJson(String^ jsonStr) {
    LyricsResult^ finalResult = ref new LyricsResult();
    finalResult->IsTimed = false;
    finalResult->Lines = ref new Vector<LyricLine^>();

    if (jsonStr == nullptr || jsonStr->IsEmpty()) return finalResult;

    try {
        JsonObject^ response = JsonObject::Parse(jsonStr)->GetNamedObject("subsonic-response", nullptr);
        if (response == nullptr) return finalResult;

        IVector<LyricLine^>^ lines = ref new Vector<LyricLine^>();
        JsonArray^ lyricsList = nullptr;
        
        if (response->HasKey("lyricsList")) {
            auto val = response->Lookup("lyricsList");
            if (val->ValueType == JsonValueType::Array) lyricsList = val->GetArray();
        } else if (response->HasKey("lyrics")) {
             auto val = response->Lookup("lyrics");
             if (val->ValueType == JsonValueType::Array) lyricsList = val->GetArray();
             else if (val->ValueType == JsonValueType::Object) { lyricsList = ref new JsonArray(); lyricsList->Append(val); }
        } else if (response->HasKey("structuredLyrics")) {
             auto val = response->Lookup("structuredLyrics");
             if (val->ValueType == JsonValueType::Object) { lyricsList = ref new JsonArray(); lyricsList->Append(val); }
        }

        if (lyricsList != nullptr && lyricsList->Size > 0) {
            JsonObject^ lyricsObj = nullptr;
            for (unsigned int i = 0; i < lyricsList->Size; i++) {
                auto cand = lyricsList->GetObjectAt(i);
                if (cand->HasKey("synced") && cand->GetNamedBoolean("synced")) { lyricsObj = cand; break; }
            }
            if (lyricsObj == nullptr) lyricsObj = lyricsList->GetObjectAt(0);

            if (lyricsObj != nullptr) {
                if (lyricsObj->HasKey("synced")) finalResult->IsTimed = lyricsObj->GetNamedBoolean("synced");
                if (lyricsObj->HasKey("line")) {
                    auto val = lyricsObj->Lookup("line");
                    if (val->ValueType == JsonValueType::Array) {
                        auto arr = val->GetArray();
                        for (unsigned int i = 0; i < arr->Size; i++) {
                            auto obj = arr->GetObjectAt(i);
                            auto l = ref new LyricLine();
                            l->Text = obj->HasKey("value") ? obj->GetNamedString("value") : "";
                            l->TimestampMs = obj->HasKey("start") ? (uint32)obj->GetNamedNumber("start") : 0;
                            if (obj->HasKey("start")) finalResult->IsTimed = true;
                            lines->Append(l);
                        }
                    } else if (val->ValueType == JsonValueType::Object) {
                        auto obj = val->GetObject();
                        auto l = ref new LyricLine();
                        l->Text = obj->HasKey("value") ? obj->GetNamedString("value") : "";
                        l->TimestampMs = obj->HasKey("start") ? (uint32)obj->GetNamedNumber("start") : 0;
                        if (obj->HasKey("start")) finalResult->IsTimed = true;
                        lines->Append(l);
                    }
                } else if (lyricsObj->HasKey("content") || lyricsObj->HasKey("text")) {
                    String^ raw = lyricsObj->HasKey("content") ? lyricsObj->GetNamedString("content") : lyricsObj->GetNamedString("text");
                    return ParseLrc(raw);
                }
            }
        }
        finalResult->Lines = lines;
    } catch (...) {}
    return finalResult;
}

IAsyncOperation<LyricsResult^>^ LyricsService::FetchFromLrcLibAsync(String^ artist, String^ title, String^ album, double durationSeconds) {
    std::wstring wArtist = artist != nullptr ? artist->Data() : L"";
    std::wstring wTitle = title != nullptr ? title->Data() : L"";
    std::wstring wAlbum = album != nullptr ? album->Data() : L"";

    auto trim = [](std::wstring& s) {
        size_t first = s.find_first_not_of(L" \t\r\n");
        if (std::wstring::npos == first) { s = L""; return; }
        size_t last = s.find_last_not_of(L" \t\r\n");
        s = s.substr(first, (last - first + 1));
    };
    trim(wArtist); trim(wTitle); trim(wAlbum);

    String^ cleanArtist = ref new String(wArtist.c_str());
    String^ cleanTitle = ref new String(wTitle.c_str());
    String^ cleanAlbum = ref new String(wAlbum.c_str());

    return create_async([=]() -> LyricsResult^ {
        LyricsResult^ emptyResult = ref new LyricsResult();
        emptyResult->IsTimed = false;
        emptyResult->Lines = ref new Vector<LyricLine^>();

        if (cleanArtist->IsEmpty() || cleanTitle->IsEmpty()) return emptyResult;

        try {
            // First attempt: Exact match via /api/get
            auto url = L"https://lrclib.net/api/get?artist=" + std::wstring(Uri::EscapeComponent(cleanArtist)->Data()) +
                       L"&track=" + std::wstring(Uri::EscapeComponent(cleanTitle)->Data());
            
            // Be careful with album/duration as they can cause mismatches if not perfect
            if (!cleanAlbum->IsEmpty() && cleanAlbum != "Unknown Album") {
                url += L"&album=" + std::wstring(Uri::EscapeComponent(cleanAlbum)->Data());
            }
            if (durationSeconds > 0) {
                url += L"&duration=" + std::to_wstring((int)durationSeconds);
            }

            Windows::Web::Http::HttpClient^ http = ref new Windows::Web::Http::HttpClient();
            http->DefaultRequestHeaders->UserAgent->Append(ref new Windows::Web::Http::Headers::HttpProductInfoHeaderValue("Opal", "1.0"));

            auto resp = create_task(http->GetAsync(ref new Uri(ref new String(url.c_str())))).get();
            if (resp->IsSuccessStatusCode) {
                auto jsonStr = create_task(resp->Content->ReadAsStringAsync()).get();
                auto root = JsonObject::Parse(jsonStr);
                String^ s = root->HasKey("syncedLyrics") ? root->GetNamedString("syncedLyrics") : nullptr;
                String^ p = root->HasKey("plainLyrics") ? root->GetNamedString("plainLyrics") : nullptr;
                if (s != nullptr && s->Length() > 0) return ParseLrc(s);
                if (p != nullptr && p->Length() > 0) return ParseLrc(p);
            }

            // Second attempt: Search API if /api/get failed (more lenient)
            auto searchUrl = L"https://lrclib.net/api/search?artist=" + std::wstring(Uri::EscapeComponent(cleanArtist)->Data()) +
                             L"&track=" + std::wstring(Uri::EscapeComponent(cleanTitle)->Data());
            
            auto sResp = create_task(http->GetAsync(ref new Uri(ref new String(searchUrl.c_str())))).get();
            if (sResp->IsSuccessStatusCode) {
                auto sJsonStr = create_task(sResp->Content->ReadAsStringAsync()).get();
                auto results = JsonArray::Parse(sJsonStr);
                if (results->Size > 0) {
                    auto best = results->GetObjectAt(0);
                    String^ s = best->HasKey("syncedLyrics") ? best->GetNamedString("syncedLyrics") : nullptr;
                    String^ p = best->HasKey("plainLyrics") ? best->GetNamedString("plainLyrics") : nullptr;
                    if (s != nullptr && s->Length() > 0) return ParseLrc(s);
                    if (p != nullptr && p->Length() > 0) return ParseLrc(p);
                }
            }
        } catch (...) {}
        return emptyResult;
    });
}


IAsyncOperation<LyricsResult^>^ LyricsService::FetchFromNeteaseAsync(String^ artist, String^ title) {
    std::wstring wArtist = artist != nullptr ? artist->Data() : L"";
    std::wstring wTitle = title != nullptr ? title->Data() : L"";

    auto trim = [](std::wstring& s) {
        size_t first = s.find_first_not_of(L" \t\r\n");
        if (std::wstring::npos == first) { s = L""; return; }
        size_t last = s.find_last_not_of(L" \t\r\n");
        s = s.substr(first, (last - first + 1));
    };
    trim(wArtist); trim(wTitle);

    String^ cleanArtist = ref new String(wArtist.c_str());
    String^ cleanTitle = ref new String(wTitle.c_str());

    return create_async([=]() -> LyricsResult^ {
        LyricsResult^ emptyResult = ref new LyricsResult();
        emptyResult->IsTimed = false;
        emptyResult->Lines = ref new Vector<LyricLine^>();

        if (cleanArtist->IsEmpty() || cleanTitle->IsEmpty()) return emptyResult;

        try {
            auto query = std::wstring(cleanTitle->Data()) + L" " + std::wstring(cleanArtist->Data());
            auto searchUrl = L"https://music.163.com/api/search/get/web?s=" + std::wstring(Uri::EscapeComponent(ref new String(query.c_str()))->Data()) + L"&type=1&offset=0&total=true&limit=5";
            
            Windows::Web::Http::HttpClient^ http = ref new Windows::Web::Http::HttpClient();
            http->DefaultRequestHeaders->UserAgent->Append(ref new Windows::Web::Http::Headers::HttpProductInfoHeaderValue("Mozilla", "5.0"));
            http->DefaultRequestHeaders->Referer = ref new Uri("https://music.163.com/");

            auto searchResp = create_task(http->GetAsync(ref new Uri(ref new String(searchUrl.c_str())))).get();
            if (!searchResp->IsSuccessStatusCode) return emptyResult;
            
            auto searchJson = create_task(searchResp->Content->ReadAsStringAsync()).get();
            auto root = JsonObject::Parse(searchJson);
            if (!root->HasKey("result")) return emptyResult;
            
            auto resultVal = root->Lookup("result");
            if (resultVal->ValueType != JsonValueType::Object) return emptyResult;
            
            auto resultObj = resultVal->GetObject();
            if (!resultObj->HasKey("songs")) return emptyResult;
            
            auto songsArray = resultObj->GetNamedArray("songs");
            if (songsArray->Size == 0) return emptyResult;
            
            int songId = (int)songsArray->GetObjectAt(0)->GetNamedNumber("id");
            auto lyricsUrl = L"https://music.163.com/api/song/lyric?os=pc&id=" + std::to_wstring(songId) + L"&lv=-1&kv=-1&tv=-1";
            
            auto lyricsResp = create_task(http->GetAsync(ref new Uri(ref new String(lyricsUrl.c_str())))).get();
            if (!lyricsResp->IsSuccessStatusCode) return emptyResult;
            
            auto lyricsJson = create_task(lyricsResp->Content->ReadAsStringAsync()).get();
            auto lyricsRoot = JsonObject::Parse(lyricsJson);
            if (lyricsRoot->HasKey("lrc")) {
                auto lrcObj = lyricsRoot->GetNamedObject("lrc");
                if (lrcObj->HasKey("lyric")) return ParseLrc(lrcObj->GetNamedString("lyric"));
            }
        } catch (...) {}
        return emptyResult;
    });
}


