#include "pch.h"
#include "LyricsService.h"
#include <regex>
#include <algorithm>
#include <vector>
#include <string>
#include <sstream>
#include <ppltasks.h>
#include <cstring>
#include "DebugLogger.h"

using namespace Opal::Models;
using namespace Opal::Services;
using namespace Platform;
using namespace concurrency;
using namespace Platform::Collections;
using namespace Windows::Foundation::Collections;
using namespace Windows::Data::Json;
using namespace Windows::Foundation;

LyricsResult^ LyricsService::ParseLrc(String^ rawLrc) {
    LyricsResult^ finalResult = ref new LyricsResult();
    finalResult->IsTimed = false;
    finalResult->Lines = ref new Vector<LyricLine^>();

    if (rawLrc == nullptr || rawLrc->IsEmpty()) return finalResult;

    std::wstring lyricsW = rawLrc->Data();
    NativeLyricsResult nativeResult = LyricsParser::ParseLrc(lyricsW, [](const std::string& component, const std::wstring& message) {
        DebugLogger::Instance->Log(ref new String(std::wstring(component.begin(), component.end()).c_str()), ref new String(message.c_str()));
    });

    Vector<LyricLine^>^ result = ref new Vector<LyricLine^>();
    for (const auto& nativeLine : nativeResult.Lines) {
        LyricLine^ line = ref new LyricLine();
        line->TimestampMs = nativeLine.TimestampMs;
        line->Text = ref new String(nativeLine.Text.c_str());
        result->Append(line);
    }

    finalResult->Lines = result;
    finalResult->IsTimed = nativeResult.IsTimed;
    return finalResult;
}

NativeLyricsResult LyricsParser::ParseLrc(const std::wstring& lyricsW, ErrorLogger logger) {
    NativeLyricsResult result;
    result.IsTimed = false;

    if (lyricsW.empty()) return result;

    std::wregex lrcRegex(L"\\[(\\d{2,}):(\\d{2})(?:\\.(\\d{2,3}))?\\]([^\\n\\r]*)");
    
    std::vector<NativeLyricLine> internalLines;

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
            
            internalLines.push_back({ totalMs, textW });
        } catch (const std::exception& ex) {
            if (logger) {
                logger("ParseLrcNative", L"Error parsing line: " + std::wstring(ex.what(), ex.what() + strlen(ex.what())));
            }
        } catch (...) {
            if (logger) {
                logger("ParseLrcNative", L"Unknown error parsing line");
            }
        }
    }

    if (!foundTimed) {
        std::wstringstream ss(lyricsW);
        std::wstring lineW;
        while (std::getline(ss, lineW)) {
            if (lineW.empty()) continue;
            if (lineW.back() == L'\r') lineW.pop_back();
            if (lineW.empty()) continue;
            internalLines.push_back({ 0, lineW });
        }
    }

    std::sort(internalLines.begin(), internalLines.end(), [](const NativeLyricLine& a, const NativeLyricLine& b) {
        return a.TimestampMs < b.TimestampMs;
    });

    result.Lines = internalLines;
    result.IsTimed = foundTimed;
    return result;
}

LyricsResult^ LyricsService::ParseOpenSubsonicJson(String^ jsonStr) {
    LyricsResult^ finalResult = ref new LyricsResult();
    finalResult->IsTimed = false;
    finalResult->Lines = ref new Vector<LyricLine^>();

    if (jsonStr == nullptr || jsonStr->IsEmpty()) return finalResult;

    try {
        JsonObject^ root = JsonObject::Parse(jsonStr);
        JsonObject^ response = root->HasKey("subsonic-response") ? root->GetNamedObject("subsonic-response") : root;

        IVector<LyricLine^>^ lines = ref new Vector<LyricLine^>();
        JsonObject^ targetLyricsObj = nullptr;

        // 1. Resolve structured lyrics if available
        if (response->HasKey("lyricsList")) {
            auto listVal = response->Lookup("lyricsList");
            if (listVal->ValueType == JsonValueType::Object) {
                auto listObj = listVal->GetObject();
                if (listObj->HasKey("structuredLyrics")) {
                    auto structArr = listObj->GetNamedArray("structuredLyrics");
                    for (unsigned int i = 0; i < structArr->Size; i++) {
                        auto cand = structArr->GetObjectAt(i);
                        // Prefer kind="main", but fallback to first timed one
                        if (cand->HasKey("kind") && cand->GetNamedString("kind") == "main") {
                            targetLyricsObj = cand;
                            break;
                        }
                    }
                    if (targetLyricsObj == nullptr && structArr->Size > 0) targetLyricsObj = structArr->GetObjectAt(0);
                }
            } else if (listVal->ValueType == JsonValueType::Array) {
                // Older Subsonic might return array
                auto arr = listVal->GetArray();
                if (arr->Size > 0) targetLyricsObj = arr->GetObjectAt(0);
            }
        } else if (response->HasKey("structuredLyrics")) {
             auto structArr = response->GetNamedArray("structuredLyrics");
             if (structArr->Size > 0) targetLyricsObj = structArr->GetObjectAt(0);
        }

        // 2. Parse from resolved object (OpenSubsonic Structured format)
        if (targetLyricsObj != nullptr) {
            finalResult->IsTimed = targetLyricsObj->HasKey("synced") ? targetLyricsObj->GetNamedBoolean("synced") : false;
            if (targetLyricsObj->HasKey("line")) {
                auto lineVal = targetLyricsObj->Lookup("line");
                if (lineVal->ValueType == JsonValueType::Array) {
                    auto arr = lineVal->GetArray();
                    for (unsigned int i = 0; i < arr->Size; i++) {
                        auto obj = arr->GetObjectAt(i);
                        auto l = ref new LyricLine();
                        l->Text = obj->HasKey("value") ? obj->GetNamedString("value") : "";
                        l->TimestampMs = obj->HasKey("start") ? (uint32)obj->GetNamedNumber("start") : 0;
                        lines->Append(l);
                    }
                }
            } else if (targetLyricsObj->HasKey("content")) {
                // Fallback to plain text / LRC if structured fails
                return ParseLrc(targetLyricsObj->GetNamedString("content"));
            }
        }
        // 3. Last fallback: Old Subsonic plain lyrics
        else if (response->HasKey("lyrics")) {
             auto val = response->Lookup("lyrics");
             String^ raw = "";
             if (val->ValueType == JsonValueType::String) raw = val->GetString();
             else if (val->ValueType == JsonValueType::Object) {
                 auto obj = val->GetObject();
                 raw = obj->HasKey("content") ? obj->GetNamedString("content") : (obj->HasKey("text") ? obj->GetNamedString("text") : "");
             }
             if (!raw->IsEmpty()) return ParseLrc(raw);
        }

        finalResult->Lines = lines;
        if (lines->Size > 0 && !finalResult->IsTimed) {
            // Heuristic check: if first line has timestamp > 0, assume timed
            if (lines->GetAt(0)->TimestampMs > 0) finalResult->IsTimed = true;
        }
    } catch (Platform::Exception^ ex) { DebugLogger::Instance->LogException("ParseOpenSubsonicJson", ex); }
    return finalResult;
}
