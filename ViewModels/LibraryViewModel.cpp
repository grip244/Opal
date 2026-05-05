#include "pch.h"
#include "LibraryViewModel.h"
#include "Services/NavidromeService.h"
#include <ppltasks.h>
#include <ctime>

using namespace Opal;
using namespace Opal::ViewModels;
using namespace Platform;
using namespace Platform::Collections;
using namespace Windows::Data::Json;
using namespace Windows::UI::Xaml::Media::Imaging;
using namespace Windows::Foundation;
using namespace Windows::UI::Core;
using namespace Windows::ApplicationModel::Core;
using namespace concurrency;

LibraryViewModel^ LibraryViewModel::_instance = nullptr;

LibraryViewModel^ LibraryViewModel::Instance::get()
{
    if (_instance == nullptr)
    {
        _instance = ref new LibraryViewModel();
    }
    return _instance;
}

LibraryViewModel::LibraryViewModel()
{
    _exploreAlbums = ref new Vector<AlbumID3^>();
    _newestAlbums = ref new Vector<AlbumID3^>();
    _recentReleasedAlbums = ref new Vector<AlbumID3^>();
    _recentPlayedAlbums = ref new Vector<AlbumID3^>();
    _spotlightSongs = ref new Vector<Song^>();
}

void LibraryViewModel::LoadAllCategories()
{
    ClearAll();
    auto navService = NavidromeService::Instance;
    if (!navService->IsAuthenticated()) return;


    // Load spotlight first (hero panel), then stagger the rest
    create_task(navService->GetSongListAsync("getRandomSongs", 5, 0)).then([this](Platform::String^ jsonStr) {
        UpdateSongCollection(_spotlightSongs, jsonStr);
        return create_task(NavidromeService::Instance->GetAlbumListAsync("random", 20, 0));
    }).then([this](Platform::String^ jsonStr) {
        UpdateAlbumCollection(_exploreAlbums, jsonStr);
        return create_task(NavidromeService::Instance->GetAlbumListAsync("newest", 20, 0));
    }).then([this](Platform::String^ jsonStr) {
        UpdateAlbumCollection(_newestAlbums, jsonStr);
        return create_task(NavidromeService::Instance->GetAlbumListAsync("recent", 20, 0));
    }).then([this](Platform::String^ jsonStr) {
        UpdateAlbumCollection(_recentPlayedAlbums, jsonStr);
        auto cal = ref new Windows::Globalization::Calendar();
        cal->SetToNow();
        return create_task(NavidromeService::Instance->GetAlbumListByYearAsync(cal->Year, 1900, 20, 0));
    }).then([this](Platform::String^ jsonStr) {
        UpdateAlbumCollection(_recentReleasedAlbums, jsonStr);
    });
}

void LibraryViewModel::UpdateAlbumCollection(Platform::Collections::Vector<AlbumID3^>^ targetCollection, Platform::String^ jsonStr)
{
    if (jsonStr == nullptr) return;
    try {
        JsonObject^ root = JsonObject::Parse(jsonStr)->GetNamedObject("subsonic-response", nullptr);
        if (root != nullptr) {
            JsonObject^ list = root->HasKey("albumList2") ? root->GetNamedObject("albumList2") : (root->HasKey("albumList") ? root->GetNamedObject("albumList") : nullptr);
            if (list != nullptr && list->HasKey("album")) {
                JsonArray^ albums = list->GetNamedArray("album");
                auto newAlbs = ref new Vector<AlbumID3^>();
                for (unsigned int i = 0; i < albums->Size; i++) {
                    JsonObject^ alb = albums->GetObjectAt(i);
                    auto am = ref new AlbumID3();
                    am->Id = alb->GetNamedString("id", "");
                    am->Title = alb->HasKey("title") ? alb->GetNamedString("title") : alb->GetNamedString("name", "Unknown");
                    am->Artist = alb->GetNamedString("artist", "Unknown");

                    if (alb->HasKey("explicitStatus")) {
                        auto val = alb->GetNamedValue("explicitStatus");
                        if (val != nullptr && val->ValueType == JsonValueType::String) {
                            am->ExplicitStatus = val->GetString();
                        } else { am->ExplicitStatus = L""; }
                    } else { am->ExplicitStatus = L""; }

                    if (alb->HasKey("version")) {
                        auto val = alb->GetNamedValue("version");
                        if (val != nullptr && val->ValueType == JsonValueType::String) am->Version = val->GetString();
                    }
                    if (alb->HasKey("sortName")) {
                        auto val = alb->GetNamedValue("sortName");
                        if (val != nullptr && val->ValueType == JsonValueType::String) am->SortName = val->GetString();
                    }

                    if (alb->HasKey("year")) {
                        try { am->Year = alb->GetNamedNumber("year").ToString(); } catch (...) {
                            try { am->Year = alb->GetNamedValue("year")->Stringify(); } catch (...) { am->Year = ""; }
                        }
                    }

                    auto coverId = alb->HasKey("coverArt") ? alb->GetNamedString("coverArt") : alb->GetNamedString("id", "");
                    auto url = NavidromeService::Instance->GetCoverArtUrl(coverId, 500);
                    am->CoverUrl = url;
                    am->PopulateSearchTerms();
                    newAlbs->Append(am);
                }

                App::MainDispatcher->RunAsync(CoreDispatcherPriority::Normal, ref new DispatchedHandler([targetCollection, newAlbs]() {
                    targetCollection->Clear();
                    for (unsigned int i = 0; i < newAlbs->Size; i++) {
                        targetCollection->Append(newAlbs->GetAt(i));
                    }
                }));
            }
        }
    } catch (...) {}
}

void LibraryViewModel::UpdateSongCollection(Platform::Collections::Vector<Song^>^ targetCollection, Platform::String^ jsonStr)
{
    if (jsonStr == nullptr) return;
    try {
        JsonObject^ root = JsonObject::Parse(jsonStr)->GetNamedObject("subsonic-response", nullptr);
        if (root != nullptr && root->HasKey("randomSongs")) {
            JsonObject^ rs = root->GetNamedObject("randomSongs");
            if (rs->HasKey("song")) {
                JsonArray^ songs = rs->GetNamedArray("song");
                auto newSongs = ref new Vector<Song^>();
                for (unsigned int i = 0; i < songs->Size; i++) {
                    JsonObject^ s = songs->GetObjectAt(i);
                    auto sm = ref new Song();
                    sm->Id = s->GetNamedString("id", "");
                    sm->Title = s->GetNamedString("title", "Unknown");
                    sm->Artist = s->GetNamedString("artist", "Unknown");
                    sm->Album = s->GetNamedString("album", "");

                    if (s->HasKey("explicitStatus")) {
                        auto val = s->GetNamedValue("explicitStatus");
                        if (val != nullptr && val->ValueType == JsonValueType::String) {
                            sm->ExplicitStatus = val->GetString();
                        } else { sm->ExplicitStatus = L""; }
                    } else { sm->ExplicitStatus = L""; }

                    auto coverId = s->HasKey("coverArt") ? s->GetNamedString("coverArt") : s->GetNamedString("id", "");
                    sm->CoverUrl = NavidromeService::Instance->GetCoverArtUrl(coverId, 800);
                    sm->StreamUrl = NavidromeService::Instance->GetStreamUrl(sm->Id);
                    if (s->HasKey("duration")) {
                        sm->DurationInSeconds = (int)s->GetNamedNumber("duration");
                    }
                    if (s->HasKey("year")) {
                        auto y = s->GetNamedValue("year");
                        if (y->ValueType == JsonValueType::Number) {
                            wchar_t yBuf[16]; swprintf_s(yBuf, L"%d", (int)y->GetNumber());
                            sm->Year = ref new Platform::String(yBuf);
                        } else sm->Year = y->Stringify();
                    }
                    if (s->HasKey("genre")) {
                        sm->Genre = s->GetNamedString("genre", "");
                    }
                    sm->IsFavorite = s->HasKey("starred");
                    sm->Rating = s->HasKey("userRating") ? (int)s->GetNamedNumber("userRating") : 0;
                    sm->PopulateSearchTerms();
                    newSongs->Append(sm);
                }

                App::MainDispatcher->RunAsync(CoreDispatcherPriority::Normal, ref new DispatchedHandler([targetCollection, newSongs]() {
                    targetCollection->Clear();
                    for each (auto sm in newSongs) {
                        targetCollection->Append(sm);
                    }
                }));
            }
        }
    } catch (...) {}
}

void LibraryViewModel::ClearAll()
{
    _exploreAlbums->Clear();
    _newestAlbums->Clear();
    _recentReleasedAlbums->Clear();
    _recentPlayedAlbums->Clear();
    _spotlightSongs->Clear();
}
