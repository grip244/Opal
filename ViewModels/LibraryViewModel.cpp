#include "pch.h"
#include "LibraryViewModel.h"
#include "Services/NavidromeService.h"
#include "Services/ImageCacheService.h"
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
    auto navService = NavidromeService::Instance;
    if (!navService->IsAuthenticated()) return;


    // Load Explore (random)
    create_task(navService->GetAlbumListAsync("random", 20, 0)).then([this](Platform::String^ jsonStr) {
        if (jsonStr != nullptr) {
            try {
                JsonObject^ root = JsonObject::Parse(jsonStr)->GetNamedObject("subsonic-response", nullptr);
                if (root != nullptr && root->HasKey("albumList2")) {
                    JsonObject^ list = root->GetNamedObject("albumList2");
                    if (list->HasKey("album")) {
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

                            if (alb->HasKey("year")) am->Year = alb->GetNamedValue("year")->Stringify();
                            
                            auto coverId = alb->HasKey("coverArt") ? alb->GetNamedString("coverArt") : alb->GetNamedString("id", "");
                            auto url = NavidromeService::Instance->GetCoverArtUrl(coverId, 500);
                            am->CoverUrl = url;

                            newAlbs->Append(am);
                        }

                        auto self = this;
                        App::MainDispatcher->RunAsync(CoreDispatcherPriority::Normal, ref new DispatchedHandler([self, newAlbs]() {
                            self->_exploreAlbums->Clear();
                            for (unsigned int i = 0; i < newAlbs->Size; i++) {
                                Opal::AlbumID3^ am = newAlbs->GetAt(i);
                                try {
                                    if (am->CoverUrl != nullptr && am->CoverUrl->Length() > 0) {
                                        auto bi = ref new BitmapImage(ref new Windows::Foundation::Uri(am->CoverUrl));
                                        bi->DecodePixelWidth = 200;
                                        am->CoverArt = bi;
                                    }
                                } catch (...) {}
                                self->_exploreAlbums->Append(am);
                            }
                        }));
                    }
                }
            } catch (...) {}
        }
    });

    // Load Newly Added (newest)
    create_task(navService->GetAlbumListAsync("newest", 20, 0)).then([this](Platform::String^ jsonStr) {
        if (jsonStr != nullptr) {
            try {
                JsonObject^ root = JsonObject::Parse(jsonStr)->GetNamedObject("subsonic-response", nullptr);
                if (root != nullptr && root->HasKey("albumList2")) {
                    JsonObject^ list = root->GetNamedObject("albumList2");
                    if (list->HasKey("album")) {
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

                            auto coverId = alb->HasKey("coverArt") ? alb->GetNamedString("coverArt") : alb->GetNamedString("id", "");
                            auto url = NavidromeService::Instance->GetCoverArtUrl(coverId, 500);
                            am->CoverUrl = url;
                            newAlbs->Append(am);
                        }

                        auto self = this;
                        App::MainDispatcher->RunAsync(CoreDispatcherPriority::Normal, ref new DispatchedHandler([self, newAlbs]() {
                            self->_newestAlbums->Clear();
                            for (unsigned int i = 0; i < newAlbs->Size; i++) {
                                Opal::AlbumID3^ am = newAlbs->GetAt(i);
                                try {
                                    if (am->CoverUrl != nullptr && am->CoverUrl->Length() > 0) {
                                        auto bi = ref new BitmapImage(ref new Windows::Foundation::Uri(am->CoverUrl));
                                        bi->DecodePixelWidth = 200;
                                        am->CoverArt = bi;
                                    }
                                } catch (...) {}
                                self->_newestAlbums->Append(am);
                            }
                        }));
                    }
                }
            } catch (...) {}
        }
    });
    
    // Load Recently Played (recent)
    create_task(navService->GetAlbumListAsync("recent", 20, 0)).then([this](Platform::String^ jsonStr) {
        if (jsonStr != nullptr) {
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
                            
                            if (alb->HasKey("year")) {
                                try { am->Year = alb->GetNamedNumber("year").ToString(); } catch (...) {
                                    try { am->Year = alb->GetNamedValue("year")->Stringify(); } catch (...) { am->Year = ""; }
                                }
                            }

                            auto coverId = alb->HasKey("coverArt") ? alb->GetNamedString("coverArt") : alb->GetNamedString("id", "");
                            auto url = NavidromeService::Instance->GetCoverArtUrl(coverId, 500);
                            am->CoverUrl = url;
                            newAlbs->Append(am);
                        }

                        auto self = this;
                        App::MainDispatcher->RunAsync(CoreDispatcherPriority::Normal, ref new DispatchedHandler([self, newAlbs]() {
                            self->_recentPlayedAlbums->Clear();
                            for (unsigned int i = 0; i < newAlbs->Size; i++) {
                                Opal::AlbumID3^ am = newAlbs->GetAt(i);
                                try {
                                    if (am->CoverUrl != nullptr && am->CoverUrl->Length() > 0) {
                                        auto bi = ref new BitmapImage(ref new Windows::Foundation::Uri(am->CoverUrl));
                                        bi->DecodePixelWidth = 200;
                                        am->CoverArt = bi;
                                    }
                                } catch (...) {}
                                self->_recentPlayedAlbums->Append(am);
                            }
                        }));
                    }
                }
            } catch (...) {}
        }
    });

    // Load Recently Released (byYear)
    auto cal = ref new Windows::Globalization::Calendar();
    cal->SetToNow();
    int currentYear = cal->Year;
    create_task(navService->GetAlbumListByYearAsync(currentYear, 1900, 20, 0)).then([this](Platform::String^ jsonStr) {
        if (jsonStr != nullptr) {
            try {
                JsonObject^ root = JsonObject::Parse(jsonStr)->GetNamedObject("subsonic-response", nullptr);
                if (root != nullptr && root->HasKey("albumList2")) {
                    JsonObject^ list = root->GetNamedObject("albumList2");
                    if (list->HasKey("album")) {
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

                            if (alb->HasKey("year")) am->Year = alb->GetNamedValue("year")->Stringify();
                            auto coverId = alb->HasKey("coverArt") ? alb->GetNamedString("coverArt") : alb->GetNamedString("id", "");
                            auto url = NavidromeService::Instance->GetCoverArtUrl(coverId, 500);
                            am->CoverUrl = url;
                            newAlbs->Append(am);
                        }

                        auto self = this;
                        App::MainDispatcher->RunAsync(CoreDispatcherPriority::Normal, ref new DispatchedHandler([self, newAlbs]() {
                            self->_recentReleasedAlbums->Clear();
                            for (unsigned int i = 0; i < newAlbs->Size; i++) {
                                Opal::AlbumID3^ am = newAlbs->GetAt(i);
                                try {
                                    if (am->CoverUrl != nullptr && am->CoverUrl->Length() > 0) {
                                        auto bi = ref new BitmapImage(ref new Windows::Foundation::Uri(am->CoverUrl));
                                        bi->DecodePixelWidth = 200;
                                        am->CoverArt = bi;
                                    }
                                } catch (...) {}
                                self->_recentReleasedAlbums->Append(am);
                            }
                        }));
                    }
                }
            } catch (...) {}
        }
    });
    
    // Load Spotlight (random songs)
    create_task(navService->GetSongListAsync("getRandomSongs", 5)).then([this](Platform::String^ jsonStr) {
        if (jsonStr != nullptr) {
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
                            sm->IsFavorite = s->HasKey("starred");
                            sm->Rating = s->HasKey("userRating") ? (int)s->GetNamedNumber("userRating") : 0;

                            newSongs->Append(sm);
                        }

                        App::MainDispatcher->RunAsync(CoreDispatcherPriority::Normal, ref new DispatchedHandler([this, newSongs]() {
                            this->_spotlightSongs->Clear();
                            for (auto sm : newSongs) {
                                try {
                                    if (sm->CoverUrl != nullptr && sm->CoverUrl->Length() > 0) {
                                        auto bi = ref new BitmapImage(ref new Uri(sm->CoverUrl));
                                        bi->DecodePixelWidth = 400; // Spotlight is larger
                                        sm->CoverArt = bi;
                                    }
                                } catch (...) {}
                                this->_spotlightSongs->Append(sm);
                            }
                        }));
                    }
                }
            } catch (...) {}
        }
    });

}

void LibraryViewModel::SyncLibraryThumbnails()
{
    create_task(Opal::Services::ImageCacheService::Instance->EnsureInitializedAsync()).then([this]() {
        SyncLibraryBatch(0);
    });
}

void LibraryViewModel::SyncLibraryBatch(int offset)
{
    if (offset >= 5000) return;

    auto navService = NavidromeService::Instance;
    if (!navService->IsAuthenticated()) return;

    create_task(navService->GetAlbumListAsync("newest", 500, offset)).then([this, offset](Platform::String^ jsonStr) {
        if (jsonStr == nullptr) return;
        try {
            JsonObject^ root = JsonObject::Parse(jsonStr)->GetNamedObject("subsonic-response", nullptr);
            if (root != nullptr && root->HasKey("albumList2")) {
                JsonObject^ listObj = root->GetNamedObject("albumList2");
                if (listObj->HasKey("album")) {
                    JsonArray^ albums = listObj->GetNamedArray("album");
                    for (unsigned int i = 0; i < albums->Size; i++) {
                        auto alb = albums->GetObjectAt(i);
                        auto url = NavidromeService::Instance->GetCoverArtUrl(alb->HasKey("coverArt") ? alb->GetNamedString("coverArt") : alb->GetNamedString("id", ""), 500);
                        Opal::Services::ImageCacheService::Instance->PreCacheImageAsync(url);
                    }
                }
            }
            
            // Proceed to next batch after a small delay to let I/O breathe
            SyncLibraryBatch(offset + 500);
        } catch (...) {}
    });
}
