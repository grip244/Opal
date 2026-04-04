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
    _exploreAlbums = ref new Vector<AlbumModel^>();
    _newestAlbums = ref new Vector<AlbumModel^>();
    _recentReleasedAlbums = ref new Vector<AlbumModel^>();
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
                        CoreApplication::MainView->CoreWindow->Dispatcher->RunAsync(CoreDispatcherPriority::Normal, ref new DispatchedHandler([this, albums]() {
                            this->_exploreAlbums->Clear();
                            for (unsigned int i = 0; i < albums->Size; i++) {
                                JsonObject^ alb = albums->GetObjectAt(i);
                                auto am = ref new AlbumModel();
                                am->Id = alb->GetNamedString("id", "");
                                am->Title = alb->HasKey("title") ? alb->GetNamedString("title") : alb->GetNamedString("name", "Unknown");
                                am->Artist = alb->GetNamedString("artist", "Unknown");
                                if (alb->HasKey("year")) am->Year = alb->GetNamedValue("year")->Stringify();
                                am->CoverArt = ref new BitmapImage(ref new Uri(NavidromeService::Instance->GetCoverArtUrl(alb->GetNamedString("coverArt", ""), 500)));
                                this->_exploreAlbums->Append(am);
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
                        CoreApplication::MainView->CoreWindow->Dispatcher->RunAsync(CoreDispatcherPriority::Normal, ref new DispatchedHandler([this, albums]() {
                            this->_newestAlbums->Clear();
                            for (unsigned int i = 0; i < albums->Size; i++) {
                                JsonObject^ alb = albums->GetObjectAt(i);
                                auto am = ref new AlbumModel();
                                am->Id = alb->GetNamedString("id", "");
                                am->Title = alb->HasKey("title") ? alb->GetNamedString("title") : alb->GetNamedString("name", "Unknown");
                                am->Artist = alb->GetNamedString("artist", "Unknown");
                                am->CoverArt = ref new BitmapImage(ref new Uri(NavidromeService::Instance->GetCoverArtUrl(alb->GetNamedString("coverArt", ""), 500)));
                                this->_newestAlbums->Append(am);
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
                        CoreApplication::MainView->CoreWindow->Dispatcher->RunAsync(CoreDispatcherPriority::Normal, ref new DispatchedHandler([this, albums]() {
                            this->_recentReleasedAlbums->Clear();
                            for (unsigned int i = 0; i < albums->Size; i++) {
                                JsonObject^ alb = albums->GetObjectAt(i);
                                auto am = ref new AlbumModel();
                                am->Id = alb->GetNamedString("id", "");
                                am->Title = alb->HasKey("title") ? alb->GetNamedString("title") : alb->GetNamedString("name", "Unknown");
                                am->Artist = alb->GetNamedString("artist", "Unknown");
                                if (alb->HasKey("year")) am->Year = alb->GetNamedValue("year")->Stringify();
                                am->CoverArt = ref new BitmapImage(ref new Uri(NavidromeService::Instance->GetCoverArtUrl(alb->GetNamedString("coverArt", ""), 500)));
                                this->_recentReleasedAlbums->Append(am);
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
                        CoreApplication::MainView->CoreWindow->Dispatcher->RunAsync(CoreDispatcherPriority::Normal, ref new DispatchedHandler([this, songs]() {
                            this->_spotlightSongs->Clear();
                            for (unsigned int i = 0; i < songs->Size; i++) {
                                JsonObject^ s = songs->GetObjectAt(i);
                                auto sm = ref new Song();
                                sm->Id = s->GetNamedString("id", "");
                                sm->Title = s->GetNamedString("title", "Unknown");
                                sm->Artist = s->GetNamedString("artist", "Unknown");
                                sm->Album = s->GetNamedString("album", "");
                                auto coverId = s->GetNamedString("coverArt", "");
                                sm->CoverArt = ref new BitmapImage(ref new Uri(NavidromeService::Instance->GetCoverArtUrl(coverId, 800)));
                                this->_spotlightSongs->Append(sm);
                            }
                        }));
                    }
                }
            } catch (...) {}
        }
    });

}
