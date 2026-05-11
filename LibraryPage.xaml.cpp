#include "pch.h"
#include "LibraryPage.xaml.h"
#include "LoginPage.xaml.h"
#include "Services/PlaybackService.h"
#include "Services/SettingsService.h"
#include "Services/NavidromeService.h"
#include "ViewModels/LibraryViewModel.h"
#include "ViewModels/LyricsViewModel.h"
#include "ViewModels/PlaylistsViewModel.h"
#include "GenresPage.xaml.h"
#include "AlbumsPage.xaml.h"
#include <Windows.UI.Xaml.Media.h>
#include "Services/LyricsService.h"
#include "Services/CastingService.h"
#include "Services/DebugLogger.h"
#include "UI/Controls/ThumbnailView.xaml.h"
#include "ViewModels/GenreViewModel.h"
#include <sstream>
#include <string>
#include <random>
#include <algorithm>
#include <Windows.UI.Xaml.Media.Animation.h>

using namespace Opal;
using namespace Platform;
using namespace concurrency;
using namespace Windows::Foundation;
using namespace Windows::UI::Xaml;
using namespace Windows::UI::Xaml::Controls;
using namespace Windows::UI::Xaml::Media;
using namespace Windows::UI::Xaml::Media::Animation;
using namespace Windows::UI::Xaml::Navigation;
using namespace Windows::Media::Playback;
using namespace Windows::UI::Xaml::Media::Imaging;
using namespace Windows::Data::Json;
using namespace Windows::ApplicationModel::Core;
using namespace Windows::UI::Core;
using namespace Windows::Media;
using namespace Windows::UI::Xaml::Interop;
using namespace Windows::UI::Xaml::Data;
using namespace Windows::UI::Xaml::Input;
using namespace Windows::Foundation;
using namespace Windows::UI::Xaml::Controls::Primitives;
using namespace Windows::UI::Xaml::Media;
using namespace Windows::System::Profile;
using namespace Opal::Services;
using namespace Opal::Models;

ScrollViewer^ FindScrollViewerChild(DependencyObject^ depObj)
{
    if (depObj == nullptr) return nullptr;
    if (dynamic_cast<ScrollViewer^>(depObj) != nullptr) return static_cast<ScrollViewer^>(depObj);

    for (int i = 0; i < VisualTreeHelper::GetChildrenCount(depObj); i++)
    {
        auto child = VisualTreeHelper::GetChild(depObj, i);
        auto result = FindScrollViewerChild(child);
        if (result != nullptr) return result;
    }
    return nullptr;
}

LibraryPage::LibraryPage()
{
    _artists = ref new Platform::Collections::Vector<ArtistModel^>();
    _albums = ref new Platform::Collections::Vector<AlbumID3^>();
    _albumSongs = ref new Platform::Collections::Vector<Song^>();
    _albumDiscGroups = ref new Platform::Collections::Vector<DiscGroup^>();
    _upcomingQueue = ref new Platform::Collections::Vector<Song^>();

    InitializeComponent();
    
    _lastLyricIndex = -1;
    _shouldShowPlayerOnLoad = false;

    this->Loaded += ref new RoutedEventHandler(this, &LibraryPage::OnPageLoaded);

    auto page = this;
    PlaybackService::Instance->SongChanged += ref new SongChangedHandler([page](PlaybackService^ sender, Song^ song) {
        page->Dispatcher->RunAsync(CoreDispatcherPriority::Normal, ref new DispatchedHandler([page, song]() {
            page->NowPlayingImage->SourceUrl = song->CoverUrl;
            page->LoadLyrics(song);
            page->UpdateUpcomingQueue();
        }));
    });

    auto lyricTimer = ref new DispatcherTimer();
    TimeSpan lts; lts.Duration = 100 * 10000LL;
    lyricTimer->Interval = lts;
    lyricTimer->Tick += ref new Windows::Foundation::EventHandler<Object^>([page](Object^ sender, Object^ args) {
        page->UpdateLyricsHighlight();
    });
    lyricTimer->Start();

    FeaturedGenresGrid->ItemsSource = ViewModels::GenreViewModel::Instance->Genres;

    CastingService::Instance->QueueSynced += ref new Windows::Foundation::EventHandler<Object^>([page](Object^ sender, Object^ args) {
        page->Dispatcher->RunAsync(CoreDispatcherPriority::Normal, ref new DispatchedHandler([page]() {
            page->UpdateUpcomingQueue();
        }));
    });

    auto carouselTimer = ref new DispatcherTimer();
    TimeSpan cts; cts.Duration = 5000 * 10000LL; // 5 seconds
    carouselTimer->Interval = cts;
    carouselTimer->Tick += ref new Windows::Foundation::EventHandler<Object^>([page](Object^ sender, Object^ args) {
        if (page->SpotlightCarousel->Visibility == Windows::UI::Xaml::Visibility::Visible) {
            page->OnCarouselNext(nullptr, nullptr);
        }
    });
    carouselTimer->Start();
}

void LibraryPage::OnNavigatedTo(Windows::UI::Xaml::Navigation::NavigationEventArgs^ e)
{
    // 2.1: Register BackRequested here so it is paired with OnNavigatedFrom unregistration
    auto self = this;
    _backRequestedToken = SystemNavigationManager::GetForCurrentView()->BackRequested +=
        ref new EventHandler<BackRequestedEventArgs^>([self](Object^ sender, BackRequestedEventArgs^ args) {
            if (self->FullPlayerGrid->Visibility == Windows::UI::Xaml::Visibility::Visible) {
                self->FullPlayerGrid->Visibility = Windows::UI::Xaml::Visibility::Collapsed;
                self->BrowseGrid->Visibility = Windows::UI::Xaml::Visibility::Visible;
                args->Handled = true;
            }
            else if (self->AlbumGrid->Visibility == Windows::UI::Xaml::Visibility::Visible) {
                if (self->ArtistDetailName->Text->Length() > 0) {
                    self->AlbumGrid->Visibility = Windows::UI::Xaml::Visibility::Collapsed;
                    self->ArtistGrid->Visibility = Windows::UI::Xaml::Visibility::Visible;
                } else self->LoadHomePage();
                args->Handled = true;
            }
            else if (self->HomeGrid->Visibility == Windows::UI::Xaml::Visibility::Collapsed) {
                self->LoadHomePage();
                args->Handled = true;
            }
            else args->Handled = true;
        });

    auto paramStr = dynamic_cast<Platform::String^>(e->Parameter);
    if (paramStr != nullptr && paramStr->Length() > 6 && std::wstring(paramStr->Data()).substr(0, 6) == L"GENRE:")
    {
        auto genre = ref new Platform::String(paramStr->Data() + 6);
        LoadGenreAlbums(genre);
    }
    else if (paramStr != nullptr && paramStr->Length() > 0)
    {
        LoadArtistPage(paramStr);
    }
    else
    {
        auto am = dynamic_cast<AlbumID3^>(e->Parameter);
        if (am != nullptr)
        {
            LoadAlbumPage(am->Id);

            auto animation = ConnectedAnimationService::GetForCurrentView()->GetAnimation("ForwardConnectedAnimation");
            if (animation != nullptr)
            {
                AlbumGrid->UpdateLayout();
                animation->TryStart(AlbumDetailImageBorder);
            }
        }
        else
        {
            auto s = dynamic_cast<Song^>(e->Parameter);
            if (s != nullptr)
            {
                PlaybackService::Instance->PlaySong(s);
                _shouldShowPlayerOnLoad = true;
            }
        }
    }
}

void LibraryPage::OnPageLoaded(Object^ sender, RoutedEventArgs^ e)
{
    if (AnalyticsInfo::VersionInfo->DeviceFamily == "Windows.Xbox")
    {
        VisualStateManager::GoToState(this, "XboxState", false);
    }
    if (_shouldShowPlayerOnLoad) {
        ShowPlayer(false);
        _shouldShowPlayerOnLoad = false;
    }
    else if (HomeGrid->Visibility == Windows::UI::Xaml::Visibility::Collapsed &&
        ArtistGrid->Visibility == Windows::UI::Xaml::Visibility::Collapsed &&
        AlbumGrid->Visibility == Windows::UI::Xaml::Visibility::Collapsed &&
        FullPlayerGrid->Visibility == Windows::UI::Xaml::Visibility::Collapsed)
    {
        LoadHomePage();
    }

    // Initialize UI with current song if something is already playing
    auto currentSong = PlaybackService::Instance->CurrentSong;
    if (currentSong != nullptr) {
        NowPlayingImage->SourceUrl = currentSong->CoverUrl;
        LoadLyrics(currentSong);
        UpdateUpcomingQueue();
    }

    // 2.1: BackRequested registration moved to OnNavigatedTo/OnNavigatedFrom
    // to prevent handler accumulation on each page load
}

void LibraryPage::LoadHomePage()
{
    BrowseGrid->Visibility = Windows::UI::Xaml::Visibility::Visible;
    HomeGrid->Visibility = Windows::UI::Xaml::Visibility::Visible;
    ArtistGrid->Visibility = Windows::UI::Xaml::Visibility::Collapsed;
    AlbumGrid->Visibility = Windows::UI::Xaml::Visibility::Collapsed;
    FullPlayerGrid->Visibility = Windows::UI::Xaml::Visibility::Collapsed;
    
    ViewModels::GenreViewModel::Instance->LoadGenresAsync();
    LibraryVM->LoadAllCategories();
}

void LibraryPage::LoadGenreAlbums(Platform::String^ genre)
{
    BrowseGrid->Visibility = Windows::UI::Xaml::Visibility::Visible;
    HomeGrid->Visibility = Windows::UI::Xaml::Visibility::Collapsed;
    ArtistGrid->Visibility = Windows::UI::Xaml::Visibility::Visible;
    AlbumGrid->Visibility = Windows::UI::Xaml::Visibility::Collapsed;
    FullPlayerGrid->Visibility = Windows::UI::Xaml::Visibility::Collapsed;

    ArtistDetailName->Text = genre;
    ArtistDetailType->Text = "GENRE";
    ArtistDetailImage->Visibility = Windows::UI::Xaml::Visibility::Collapsed;
    ArtistDetailImage->SourceUrl = nullptr;

    auto self = this;
    create_task(NavidromeService::Instance->GetAlbumListByGenreAsync(genre, 50, 0)).then([self](Platform::String^ jsonStr) {
        if (jsonStr != nullptr) {
            self->Dispatcher->RunAsync(CoreDispatcherPriority::Normal, ref new DispatchedHandler([self, jsonStr]() {
                try {
                    JsonObject^ rootRaw = JsonObject::Parse(jsonStr);
                    JsonObject^ root = rootRaw->GetNamedObject("subsonic-response", nullptr);
                    if (root != nullptr) {
                        JsonObject^ listObj = root->HasKey("albumList2") ? root->GetNamedObject("albumList2") : (root->HasKey("albumList") ? root->GetNamedObject("albumList") : nullptr);
                        if (listObj != nullptr && listObj->HasKey("album")) {
                            JsonArray^ albumsArray = listObj->GetNamedArray("album");
                            self->_albums->Clear();
                            for (unsigned int i = 0; i < albumsArray->Size; i++) {
                                JsonObject^ albObj = albumsArray->GetObjectAt(i);
                                auto am = ref new AlbumID3();
                                am->Id = albObj->GetNamedString("id", "");
                                am->Title = albObj->HasKey("title") ? albObj->GetNamedString("title") : albObj->GetNamedString("name", "Unknown Album");
                                am->Artist = albObj->GetNamedString("artist", "");
                                
                                if (albObj->HasKey("year")) {
                                    try { am->Year = albObj->GetNamedNumber("year").ToString(); } catch (...) {
                                        try { am->Year = albObj->GetNamedValue("year")->Stringify(); } catch (...) { am->Year = ""; }
                                    }
                                }

                                auto coverId = albObj->HasKey("coverArt") ? albObj->GetNamedString("coverArt") : albObj->GetNamedString("id", "");
                                am->CoverUrl = NavidromeService::Instance->GetCoverArtUrl(coverId, 500);
                                
                                self->_albums->Append(am);
                            }
                            self->AlbumsGridView->ItemsSource = self->_albums;
                        }
                    }
                } catch (...) {}
            }));
        }
    });
}

void LibraryPage::LoadArtistPage(Platform::String^ artistId)
{
    HomeGrid->Visibility = Windows::UI::Xaml::Visibility::Collapsed;
    ArtistGrid->Visibility = Windows::UI::Xaml::Visibility::Visible;
    AlbumGrid->Visibility = Windows::UI::Xaml::Visibility::Collapsed;

    auto self = this;
    create_task(NavidromeService::Instance->GetArtistAsync(artistId)).then([self](Platform::String^ jsonStr) {
        if (jsonStr != nullptr) {
            self->Dispatcher->RunAsync(CoreDispatcherPriority::Normal, ref new DispatchedHandler([self, jsonStr]() {
                try {
                    JsonObject^ rootRaw = JsonObject::Parse(jsonStr);
                    JsonObject^ root = rootRaw->GetNamedObject("subsonic-response", nullptr);
                    if (root != nullptr && root->HasKey("artist")) {
                        JsonObject^ artistObj = root->GetNamedObject("artist");
                        self->ArtistDetailName->Text = artistObj->GetNamedString("name", "");
                        self->ArtistDetailType->Text = "ARTIST";
                        self->ArtistDetailImage->Visibility = Windows::UI::Xaml::Visibility::Visible;
                        self->ArtistDetailImage->SourceUrl = NavidromeService::Instance->GetCoverArtUrl(artistObj->GetNamedString("id", ""), 500);

                        if (artistObj->HasKey("album")) {
                            JsonArray^ albumsArray = artistObj->GetNamedArray("album");
                            self->_albums->Clear();
                            for (unsigned int i = 0; i < albumsArray->Size; i++) {
                                JsonObject^ albObj = albumsArray->GetObjectAt(i);
                                auto am = ref new AlbumID3();
                                am->Id = albObj->GetNamedString("id", "");
                                am->Title = albObj->HasKey("title") ? albObj->GetNamedString("title") : albObj->GetNamedString("name", "Unknown Album");
                                
                                if (albObj->HasKey("explicitStatus")) {
                                    auto val = albObj->GetNamedValue("explicitStatus");
                                    if (val != nullptr && val->ValueType == JsonValueType::String) {
                                        am->ExplicitStatus = val->GetString();
                                    } else { am->ExplicitStatus = L""; }
                                } else { am->ExplicitStatus = L""; }
                                
                                if (albObj->HasKey("version")) {
                                    auto val = albObj->GetNamedValue("version");
                                    if (val != nullptr && val->ValueType == JsonValueType::String) am->Version = val->GetString();
                                }
                                if (albObj->HasKey("sortName")) {
                                    auto val = albObj->GetNamedValue("sortName");
                                    if (val != nullptr && val->ValueType == JsonValueType::String) am->SortName = val->GetString();
                                }

                                if (albObj->HasKey("year")) {
                                    try { am->Year = albObj->GetNamedNumber("year").ToString(); } catch (Exception^ ex) {
                                        am->Year = albObj->GetNamedValue("year")->Stringify();
                                        DebugLogger::Instance->LogException("LoadArtistPage (AlbumYear)", ex);
                                    }
                                }
                                Platform::String^ cover = albObj->HasKey("coverArt") ? albObj->GetNamedString("coverArt") : albObj->GetNamedString("id", "");
                                am->CoverUrl = NavidromeService::Instance->GetCoverArtUrl(cover, 500);
                                self->_albums->Append(am);
                            }
                            auto disp = App::MainDispatcher;
                            if (disp != nullptr) {
                                disp->RunAsync(Windows::UI::Core::CoreDispatcherPriority::Normal, ref new Windows::UI::Core::DispatchedHandler([self]() {
                                    self->AlbumsGridView->ItemsSource = self->_albums;
                                }));
                            }
                        }
                    }
                } catch (...) {}
            }));
        }
    });
}

void LibraryPage::LoadAlbumPage(Platform::String^ albumId)
{
    HomeGrid->Visibility = Windows::UI::Xaml::Visibility::Collapsed;
    ArtistGrid->Visibility = Windows::UI::Xaml::Visibility::Collapsed;
    AlbumGrid->Visibility = Windows::UI::Xaml::Visibility::Visible;
    TracksListView->CanReorderItems = false;
    TracksListView->CanDragItems = false;

    auto self = this;
    create_task(NavidromeService::Instance->GetAlbumAsync(albumId)).then([self](Platform::String^ jsonStr) {
        if (jsonStr != nullptr) {
            self->Dispatcher->RunAsync(CoreDispatcherPriority::Normal, ref new DispatchedHandler([self, jsonStr]() {
                try {
                    JsonObject^ rootRaw = JsonObject::Parse(jsonStr);
                    JsonObject^ root = rootRaw->GetNamedObject("subsonic-response", nullptr);
                    if (root != nullptr && root->HasKey("album")) {
                        JsonObject^ albumObj = root->GetNamedObject("album");
                        Platform::String^ parsedTitle = albumObj->HasKey("name") ? albumObj->GetNamedString("name") : albumObj->GetNamedString("title", "Unknown");
                        
                        bool isExplicit = false;
                        if (albumObj->HasKey("explicitStatus")) {
                            auto val = albumObj->GetNamedValue("explicitStatus");
                            if (val != nullptr && val->ValueType == JsonValueType::String) {
                                isExplicit = (val->GetString() == L"explicit");
                            }
                        }
                        
                        self->AlbumDetailTitle->Text = parsedTitle;
                        self->AlbumExplicitBadge->Visibility = isExplicit ? Windows::UI::Xaml::Visibility::Visible : Windows::UI::Xaml::Visibility::Collapsed;
                        self->AlbumDetailArtist->Text = albumObj->GetNamedString("artist", "");
                        self->AlbumDetailImage->SourceUrl = NavidromeService::Instance->GetCoverArtUrl(albumObj->HasKey("coverArt") ? albumObj->GetNamedString("coverArt") : albumObj->GetNamedString("id", ""), 500);

                        if (albumObj->HasKey("song")) {
                            JsonArray^ songsArray = albumObj->GetNamedArray("song");
                            self->_albumSongs->Clear();
                            self->_albumDiscGroups->Clear();
                            DiscGroup^ currentDisc = nullptr;
                            int lastDiscNum = -1;

                            for (unsigned int i = 0; i < songsArray->Size; i++) {
                                JsonObject^ songObj = songsArray->GetObjectAt(i);
                                auto song = ref new Song();
                                song->Id = songObj->GetNamedString("id", "");
                                song->Title = songObj->GetNamedString("title", "Unknown Track");
                                song->Artist = songObj->GetNamedString("artist", "Unknown Artist");
                                
                                if (songObj->HasKey("explicitStatus")) {
                                    auto val = songObj->GetNamedValue("explicitStatus");
                                    if (val != nullptr && val->ValueType == JsonValueType::String) {
                                        song->ExplicitStatus = val->GetString();
                                    } else { song->ExplicitStatus = L""; }
                                } else {
                                    if (albumObj->HasKey("explicitStatus")) {
                                        auto val = albumObj->GetNamedValue("explicitStatus");
                                        if (val != nullptr && val->ValueType == JsonValueType::String) {
                                            song->ExplicitStatus = val->GetString();
                                        } else { song->ExplicitStatus = L""; }
                                    } else { song->ExplicitStatus = L""; }
                                }
                                
                                song->Album = songObj->GetNamedString("album", "Unknown Album");

                                if (songObj->HasKey("track")) {
                                    auto trackVal = songObj->GetNamedValue("track");
                                    if (trackVal->ValueType == JsonValueType::String) song->TrackNumber = trackVal->GetString();
                                    else if (trackVal->ValueType == JsonValueType::Number) {
                                        wchar_t tBuf[16];
                                        swprintf_s(tBuf, L"%d", (int)trackVal->GetNumber());
                                        song->TrackNumber = ref new Platform::String(tBuf);
                                    }
                                }
                                
                                song->DiscNumber = 1;
                                if (songObj->HasKey("discNumber")) {
                                    try { song->DiscNumber = (int)songObj->GetNamedNumber("discNumber"); } catch (Exception^ ex) { DebugLogger::Instance->LogException("LoadAlbumPage (DiscNumber)", ex); }
                                }
                                
                                if (songObj->HasKey("duration")) {
                                    int totalSeconds = (int)songObj->GetNamedNumber("duration");
                                    song->DurationInSeconds = totalSeconds;
                                    wchar_t buf[16]; swprintf_s(buf, L"%d:%02d", totalSeconds / 60, totalSeconds % 60);
                                    song->Duration = ref new Platform::String(buf);
                                } else { song->Duration = "--:--"; song->DurationInSeconds = 0; }

                                if (songObj->HasKey("year")) {
                                    auto y = songObj->GetNamedValue("year");
                                    if (y->ValueType == JsonValueType::Number) {
                                        wchar_t yBuf[16]; swprintf_s(yBuf, L"%d", (int)y->GetNumber());
                                        song->Year = ref new Platform::String(yBuf);
                                    } else song->Year = y->Stringify();
                                }

                                song->StreamUrl = NavidromeService::Instance->GetStreamUrl(song->Id);
                                Platform::String^ coverArtId = songObj->HasKey("coverArt") ? songObj->GetNamedString("coverArt") : song->Id;
                                song->CoverUrl = NavidromeService::Instance->GetCoverArtUrl(coverArtId, 500);
                                
                                if (songObj->HasKey("starred")) song->IsFavorite = true;
                                else song->IsFavorite = false;
                                
                                if (songObj->HasKey("userRating")) song->Rating = songObj->GetNamedNumber("userRating");
                                else song->Rating = 0.0;

                                
                                self->_albumSongs->Append(song);
                                
                                if (song->DiscNumber != lastDiscNum) {
                                    lastDiscNum = song->DiscNumber;
                                    currentDisc = ref new DiscGroup();
                                    wchar_t dBuf[32]; swprintf_s(dBuf, L"Disc %d", lastDiscNum);
                                    currentDisc->Title = ref new Platform::String(dBuf);
                                    self->_albumDiscGroups->Append(currentDisc);
                                }
                                if (currentDisc != nullptr) currentDisc->Items->Append(song);
                            }
                            
                            auto cvs = dynamic_cast<Windows::UI::Xaml::Data::CollectionViewSource^>(self->Resources->Lookup("AlbumSongsSource"));
                            if (cvs != nullptr) cvs->Source = self->_albumDiscGroups;
                        }
                    }
                } catch (...) {}
            }));
        }
    });
}


void LibraryPage::PlaySong(unsigned int index)
{
    PlaybackService::Instance->PlayQueue(PlaybackService::Instance->Queue, index);
}

void LibraryPage::LoadLyrics(Song^ song)
{
    auto self = this;
    
    // Set a loading state
    self->LyricsVM->Lines->Clear();
    auto loadingLine = ref new LyricLine();
    loadingLine->Text = "Loading lyrics...";
    loadingLine->TimestampMs = 0;
    self->LyricsVM->Lines->Append(loadingLine);
    self->LyricsVM->IsTimed = false;
    self->LyricsVM->ActiveIndex = -1;

    // Use OpenSubsonic (Modern, server-side) as the primary and only source
    create_task(NavidromeService::Instance->GetLyricsBySongIdAsync(song->Id)).then([self](Platform::String^ json) {
        if (json != nullptr) {
            auto result = LyricsService::ParseOpenSubsonicJson(json);
            self->Dispatcher->RunAsync(CoreDispatcherPriority::High, ref new DispatchedHandler([self, result]() {
                self->LyricsVM->Lines->Clear();
                self->LyricsVM->ActiveIndex = -1;
                
                if (result != nullptr && result->Lines->Size > 0) {
                    for (auto l : result->Lines) self->LyricsVM->Lines->Append(l);
                    self->LyricsVM->IsTimed = result->IsTimed;
                } else {
                    self->LyricsVM->SetLyrics(""); // No lyrics found fallback
                }
            }));
        } else {
            self->Dispatcher->RunAsync(CoreDispatcherPriority::High, ref new DispatchedHandler([self]() {
                self->LyricsVM->SetLyrics("");
            }));
        }
    });
}


void LibraryPage::UpdateLyricsHighlight()
{
    auto player = PlaybackService::Instance->Player;
    if (player == nullptr || LyricsVM == nullptr || LyricsVM->Lines->Size == 0) return;
    auto self = this;
    this->Dispatcher->RunAsync(CoreDispatcherPriority::High, ref new DispatchedHandler([self, player]() {
        if (player == nullptr) return;
        auto position = player->PlaybackSession->Position;
        uint32_t curMs = (uint32_t)(position.Duration / 10000);
        int prevIndex = self->LyricsVM->ActiveIndex;
        self->LyricsVM->UpdatePosition(curMs);
        int newIndex = self->LyricsVM->ActiveIndex;
        if (newIndex != -1 && newIndex != prevIndex) {
            self->LyricsListView->UpdateLayout();
            self->LyricsListView->ScrollIntoView(self->LyricsVM->Lines->GetAt(newIndex), ScrollIntoViewAlignment::Leading);
        }
    }));
}


void LibraryPage::Search(String^ query)
{
    auto self = this;
    create_task(NavidromeService::Instance->SearchAsync(query, 20, 0)).then([self](Platform::String^ jsonStr) {
        if (jsonStr != nullptr) {
            self->Dispatcher->RunAsync(CoreDispatcherPriority::Normal, ref new DispatchedHandler([self, jsonStr]() {
                try {
                    JsonObject^ rootRaw = JsonObject::Parse(jsonStr);
                    JsonObject^ root = rootRaw->GetNamedObject("subsonic-response", nullptr);
                    if (root != nullptr) {
                        JsonObject^ result3 = root->GetNamedObject("searchResult3", nullptr);
                        if (result3->HasKey("artist")) {
                            JsonArray^ artistsArray = result3->GetNamedArray("artist", nullptr);
                            if (artistsArray->Size > 0) self->LoadArtistPage(artistsArray->GetObjectAt(0)->GetNamedString("id", ""));
                        } else if (result3->HasKey("album")) {
                            JsonArray^ albumsArray = result3->GetNamedArray("album", nullptr);
                            if (albumsArray->Size > 0) self->LoadAlbumPage(albumsArray->GetObjectAt(0)->GetNamedString("id", ""));
                        }
                    }
                } catch (...) {}
            }));
        }
    });
}

void LibraryPage::NextSong() { PlaybackService::Instance->NextSong(); }
void LibraryPage::PreviousSong() { PlaybackService::Instance->PreviousSong(); }
void LibraryPage::ShuffleQueue() { PlaybackService::Instance->Shuffle(); }

void LibraryPage::OnHomeClicked(Object^ sender, RoutedEventArgs^ e) { LoadHomePage(); }
void LibraryPage::OnRandomTrackClicked(Object^ sender, RoutedEventArgs^ e) { 
    PlaybackService::Instance->StartAutoPlayback(); 
    BrowseGrid->Visibility = Windows::UI::Xaml::Visibility::Collapsed; 
    FullPlayerGrid->Visibility = Windows::UI::Xaml::Visibility::Visible; 
}
void LibraryPage::OnRandomAlbumClicked(Object^ sender, RoutedEventArgs^ e) {
    auto self = this;
    create_task(NavidromeService::Instance->GetAlbumListAsync("random", 20, 0)).then([self](Platform::String^ jsonStr) {
        if (jsonStr != nullptr) {
            self->Dispatcher->RunAsync(CoreDispatcherPriority::Normal, ref new DispatchedHandler([self, jsonStr]() {
                try {
                    JsonObject^ rootRaw = JsonObject::Parse(jsonStr);
                    JsonObject^ root = rootRaw->GetNamedObject("subsonic-response", nullptr);
                    if (root != nullptr) {
                        JsonObject^ albumsObj = root->GetNamedObject("albumList2", nullptr);
                        JsonArray^ albumsArray = albumsObj->GetNamedArray("album", nullptr);
                        if (albumsArray->Size > 0) self->LoadAlbumPage(albumsArray->GetObjectAt(0)->GetNamedString("id", ""));
                    }
                } catch (...) {}
            }));
        }
    });
}

void LibraryPage::OnSeeAllGenres(Object^ sender, RoutedEventArgs^ e)
{
    this->Frame->Navigate(TypeName(GenresPage::typeid));
}

void LibraryPage::OnSeeAllExplore(Object^ sender, RoutedEventArgs^ e)
{
    this->Frame->Navigate(TypeName(AlbumsPage::typeid));
}

void LibraryPage::OnSeeAllRecentlyPlayed(Object^ sender, RoutedEventArgs^ e)
{
    this->Frame->Navigate(TypeName(AlbumsPage::typeid));
}

void LibraryPage::OnSeeAllNewest(Object^ sender, RoutedEventArgs^ e)
{
    this->Frame->Navigate(TypeName(AlbumsPage::typeid));
}

void LibraryPage::OnSeeAllRecentReleases(Object^ sender, RoutedEventArgs^ e)
{
    this->Frame->Navigate(TypeName(AlbumsPage::typeid));
}

void LibraryPage::OnArtistClicked(Object^ sender, ItemClickEventArgs^ e) { auto am = dynamic_cast<ArtistModel^>(e->ClickedItem); if (am != nullptr) LoadArtistPage(am->Id); }
void LibraryPage::OnAlbumClicked(Object^ sender, ItemClickEventArgs^ e) { auto am = dynamic_cast<AlbumID3^>(e->ClickedItem); if (am != nullptr) LoadAlbumPage(am->Id); }
void LibraryPage::OnGenericAlbumClicked(Object^ sender, ItemClickEventArgs^ e)
{
    auto am = dynamic_cast<AlbumID3^>(e->ClickedItem);
    if (am != nullptr) {
        auto self = this;
        create_task(NavidromeService::Instance->GetAlbumAsync(am->Id)).then([self](Platform::String^ jsonStr) {
            if (jsonStr != nullptr) {
                self->Dispatcher->RunAsync(CoreDispatcherPriority::Normal, ref new DispatchedHandler([self, jsonStr]() {
                    try {
                        JsonObject^ root = JsonObject::Parse(jsonStr)->GetNamedObject("subsonic-response");
                        JsonArray^ songsArray = root->GetNamedObject("album")->GetNamedArray("song");
                        auto albumQueue = ref new Platform::Collections::Vector<Song^>();
                        for (unsigned int i = 0; i < songsArray->Size; i++) {
                            JsonObject^ songObj = songsArray->GetObjectAt(i);
                            auto s = ref new Song();
                            s->Id = songObj->GetNamedString("id");
                            s->Title = songObj->GetNamedString("title", "Unknown");
                            s->Artist = songObj->GetNamedString("artist", "Unknown");
                            
                            if (songObj->HasKey("explicitStatus")) {
                                auto val = songObj->GetNamedValue("explicitStatus");
                                if (val != nullptr && val->ValueType == JsonValueType::String) {
                                    s->ExplicitStatus = val->GetString();
                                } else { s->ExplicitStatus = L""; }
                            } else {
                                auto albumNode = root->GetNamedObject("album");
                                if (albumNode->HasKey("explicitStatus")) {
                                    auto val = albumNode->GetNamedValue("explicitStatus");
                                    if (val != nullptr && val->ValueType == JsonValueType::String) {
                                        s->ExplicitStatus = val->GetString();
                                    } else { s->ExplicitStatus = L""; }
                                } else { s->ExplicitStatus = L""; }
                            }
                            
                            s->Album = songObj->GetNamedString("album", "Unknown");

                            if (songObj->HasKey("year")) {
                                auto y = songObj->GetNamedValue("year");
                                if (y->ValueType == JsonValueType::Number) {
                                    wchar_t yBuf[16]; swprintf_s(yBuf, L"%d", (int)y->GetNumber());
                                    s->Year = ref new Platform::String(yBuf);
                                } else s->Year = y->Stringify();
                            }
                            s->StreamUrl = NavidromeService::Instance->GetStreamUrl(s->Id);
                            Platform::String^ coverArtId = songObj->HasKey("coverArt") ? songObj->GetNamedString("coverArt") : s->Id;
                            s->CoverUrl = NavidromeService::Instance->GetCoverArtUrl(coverArtId, 500);
                            
                            if (songObj->HasKey("starred")) s->IsFavorite = true;
                            else s->IsFavorite = false;
                            
                            if (songObj->HasKey("userRating")) s->Rating = songObj->GetNamedNumber("userRating");
                            else s->Rating = 0.0;
                            albumQueue->Append(s);
                        }
                        if (albumQueue->Size > 0) {
                            PlaybackService::Instance->PlayQueue(albumQueue, 0);
                            self->FullPlayerGrid->Visibility = Windows::UI::Xaml::Visibility::Visible;
                            self->BrowseGrid->Visibility = Windows::UI::Xaml::Visibility::Collapsed;
                        }
                    } catch (Exception^ ex) { DebugLogger::Instance->LogException("OnGenericAlbumClicked (JSON)", ex); }
                }));
            }
        });
    }
}

void LibraryPage::OnTrackClicked(Object^ sender, ItemClickEventArgs^ e) {
    auto song = dynamic_cast<Song^>(e->ClickedItem);
    if (song != nullptr) {
        PlaybackService::Instance->PlaySong(song);
    }
}

void LibraryPage::OnFavoriteIconLoaded(Object^ sender, RoutedEventArgs^ e) {
    auto icon = dynamic_cast<FontIcon^>(sender);
    if (!icon) return;
    auto song = dynamic_cast<Song^>(icon->Tag);
    if (song) {
        icon->Glyph = song->IsFavorite ? ref new Platform::String(L"\uEB52") : ref new Platform::String(L"\uEB51");
    }
}

void LibraryPage::OnTrackFavoriteClicked(Object^ sender, RoutedEventArgs^ e) {
    auto btn = dynamic_cast<Windows::UI::Xaml::Controls::Primitives::ToggleButton^>(sender);
    if (!btn) return;
    auto song = dynamic_cast<Song^>(btn->Tag);
    if (song != nullptr) {
        bool isFav = false;
        if (btn->IsChecked != nullptr) isFav = btn->IsChecked->Value;
        else isFav = !song->IsFavorite;
        
        song->IsFavorite = isFav;
        auto icon = dynamic_cast<FontIcon^>(btn->Content);
        if (icon) icon->Glyph = isFav ? ref new Platform::String(L"\uEB52") : ref new Platform::String(L"\uEB51");
        
        NavidromeService::Instance->ToggleFavoriteAsync(song->Id, isFav);
    }
}

void LibraryPage::OnTrackRatingChanged(Microsoft::UI::Xaml::Controls::RatingControl^ sender, Object^ args) {
    auto song = dynamic_cast<Song^>(sender->Tag);
    if (song != nullptr) {
        double newVar = sender->Value;
        if (song->Rating != newVar && newVar >= 0) {
            song->Rating = newVar;
            NavidromeService::Instance->SetRatingAsync(song->Id, (int)newVar);
        }
    }
}


void LibraryPage::OnQueueDragItemsCompleted(ListViewBase^ sender, DragItemsCompletedEventArgs^ args) {
    if (args->DropResult == Windows::ApplicationModel::DataTransfer::DataPackageOperation::Move) {
        auto pb = PlaybackService::Instance;
        
        // Find the current song's index to ensure we sync the correct part of the queue
        int currentIndex = (int)pb->CurrentIndex;
        if (currentIndex >= (int)pb->Queue->Size || (pb->CurrentSong != nullptr && pb->Queue->GetAt(currentIndex)->Id != pb->CurrentSong->Id)) {
            currentIndex = -1;
            for (unsigned int i = 0; i < pb->Queue->Size; i++) {
                if (pb->Queue->GetAt(i)->Id == pb->CurrentSong->Id) {
                    currentIndex = (int)i;
                    break;
                }
            }
        }

        if (currentIndex != -1) {
            unsigned int masterStartIndex = (unsigned int)(currentIndex + 1);
            
            // Capture the new order into a temporary list first to avoid dependency issues
            // (Clearing the ItemsSource would otherwise clear sender->Items during iteration)
            auto newOrder = ref new Platform::Collections::Vector<Song^>();
            for (unsigned int i = 0; i < sender->Items->Size; i++) {
                auto s = dynamic_cast<Song^>(sender->Items->GetAt(i));
                if (s != nullptr) newOrder->Append(s);
            }

            // Sync master queue with the captured new order
            while (pb->Queue->Size > masterStartIndex) {
                pb->RemoveFromQueue(masterStartIndex);
            }
            
            // Also sync our backing vector
            _upcomingQueue->Clear();

            for (unsigned int i = 0; i < newOrder->Size; i++) {
                auto s = newOrder->GetAt(i);
                pb->AddToQueue(s);
                _upcomingQueue->Append(s);
            }
            
            // Restore current index
            pb->CurrentIndex = (unsigned int)currentIndex;
            
            SyncQueueToCastingTarget();
        }
    }
}

void LibraryPage::OnRemoveFromQueueClick(Object^ sender, RoutedEventArgs^ e) {
    auto btn = dynamic_cast<Button^>(sender);
    if (btn != nullptr) {
        auto song = dynamic_cast<Song^>(btn->Tag);
        if (song != nullptr) {
            auto pb = PlaybackService::Instance;
            for (unsigned int i = 0; i < pb->Queue->Size; i++) {
                if (pb->Queue->GetAt(i) == song) {
                    pb->RemoveFromQueue(i);
                    break;
                }
            }
            UpdateUpcomingQueue();
            SyncQueueToCastingTarget();
        }
    }
}

void LibraryPage::OnClearQueueClick(Object^ sender, RoutedEventArgs^ e) {
    auto pb = PlaybackService::Instance;
    unsigned int masterStartIndex = pb->CurrentIndex + 1;
    while (pb->Queue->Size > masterStartIndex) {
        pb->RemoveFromQueue(masterStartIndex);
    }
    UpdateUpcomingQueue();
    SyncQueueToCastingTarget();
}


void LibraryPage::SyncQueueToCastingTarget() {
    if (CastingService::Instance->IsCastingActive && CastingService::Instance->TargetDeviceIp != nullptr) {
        auto pb = PlaybackService::Instance;
        JsonArray^ array = ref new JsonArray();
        for (unsigned int i = 0; i < pb->Queue->Size; i++) {
            auto s = pb->Queue->GetAt(i);
            JsonObject^ obj = ref new JsonObject();
            obj->Insert("Id", JsonValue::CreateStringValue(s->Id));
            obj->Insert("Title", JsonValue::CreateStringValue(s->Title != nullptr ? s->Title : ""));
            obj->Insert("Artist", JsonValue::CreateStringValue(s->Artist != nullptr ? s->Artist : ""));
            obj->Insert("Album", JsonValue::CreateStringValue(s->Album != nullptr ? s->Album : ""));
            obj->Insert("CoverUrl", JsonValue::CreateStringValue(s->CoverUrl != nullptr ? s->CoverUrl : ""));
            obj->Insert("StreamUrl", JsonValue::CreateStringValue(s->StreamUrl != nullptr ? s->StreamUrl : ""));
            array->Append(obj);
        }
        CastingService::Instance->SendCommand("QUEUE|" + array->Stringify());
    }
}

Opal::ViewModels::LyricsViewModel^ LibraryPage::LyricsVM::get() {
    return Opal::ViewModels::LyricsViewModel::Instance;
}

Opal::ViewModels::LibraryViewModel^ LibraryPage::LibraryVM::get() {
    return Opal::ViewModels::LibraryViewModel::Instance;
}

Windows::Foundation::Collections::IVector<Song^>^ LibraryPage::PlaybackQueue::get() {
    return PlaybackService::Instance->Queue;
}

Windows::Foundation::Collections::IObservableVector<Song^>^ LibraryPage::UpcomingQueue::get() {
    return _upcomingQueue;
}

bool LibraryPage::IsFullPlayerActive::get() {
    return FullPlayerGrid != nullptr && FullPlayerGrid->Visibility == Windows::UI::Xaml::Visibility::Visible;
}
void LibraryPage::OnPlayAlbumClicked(Object^ sender, RoutedEventArgs^ e) { if (_albumSongs->Size > 0) { PlaybackService::Instance->PlayQueue(_albumSongs, 0); } }
void LibraryPage::OnShuffleAlbumClicked(Object^ sender, RoutedEventArgs^ e)
{
    if (_albumSongs->Size > 0)
    {
        std::vector<Song^> songs;
        for (auto s : _albumSongs) songs.push_back(s);
        
        std::random_device rd;
        std::mt19937 g(rd());
        std::shuffle(songs.begin(), songs.end(), g);
        
        auto output = ref new Platform::Collections::Vector<Song^>(std::move(songs));
        PlaybackService::Instance->PlayQueue(output, 0);
    }
}
void LibraryPage::OnQueueItemClick(Object^ sender, ItemClickEventArgs^ e) { 
    auto song = dynamic_cast<Song^>(e->ClickedItem); 
    if (song != nullptr) { 
        PlaybackService::Instance->PlaySong(song); 
    } 
}
void LibraryPage::ToggleFullPlayer() { 
    if (FullPlayerGrid->Visibility == Windows::UI::Xaml::Visibility::Collapsed) { 
        FullPlayerGrid->Visibility = Windows::UI::Xaml::Visibility::Visible; 
        BrowseGrid->Visibility = Windows::UI::Xaml::Visibility::Collapsed; 
    } else { 
        FullPlayerGrid->Visibility = Windows::UI::Xaml::Visibility::Collapsed; 
        BrowseGrid->Visibility = Windows::UI::Xaml::Visibility::Visible; 
    } 
}
void LibraryPage::ShowPlayer(bool showLyrics) {
    bool wasCollapsed = (FullPlayerGrid->Visibility == Windows::UI::Xaml::Visibility::Collapsed);
    FullPlayerGrid->Visibility = Windows::UI::Xaml::Visibility::Visible;
    BrowseGrid->Visibility = Windows::UI::Xaml::Visibility::Collapsed;
    
    if (showLyrics) {
        if (!wasCollapsed && LyricsOverlay->Visibility == Windows::UI::Xaml::Visibility::Visible) {
            // Toggle BACK to Art if already showing lyrics and player was already open
            LyricsOverlay->Visibility = Windows::UI::Xaml::Visibility::Collapsed;
            AlbumArtView->Visibility = Windows::UI::Xaml::Visibility::Visible;
        } else {
            LyricsOverlay->Visibility = Windows::UI::Xaml::Visibility::Visible;
            AlbumArtView->Visibility = Windows::UI::Xaml::Visibility::Collapsed;
        }
    }
}
void LibraryPage::OnLyricsToggleClicked(Object^ sender, RoutedEventArgs^ e) {
    if (LyricsOverlay->Visibility == Windows::UI::Xaml::Visibility::Collapsed) { 
        LyricsOverlay->Visibility = Windows::UI::Xaml::Visibility::Visible; 
        AlbumArtView->Visibility = Windows::UI::Xaml::Visibility::Collapsed; 
        LyricsListView->Focus(Windows::UI::Xaml::FocusState::Programmatic); 
    }
    else { 
        LyricsOverlay->Visibility = Windows::UI::Xaml::Visibility::Collapsed; 
        AlbumArtView->Visibility = Windows::UI::Xaml::Visibility::Visible; 
    }
}

void LibraryPage::OnNavigatedFrom(Windows::UI::Xaml::Navigation::NavigationEventArgs^ e)
{
    // 2.1: Unregister the BackRequested handler to prevent accumulation
    SystemNavigationManager::GetForCurrentView()->BackRequested -= _backRequestedToken;
}

void LibraryPage::OnDisconnectClicked(Object^ sender, RoutedEventArgs^ e) {
    // 1.1: Pause playback and clear all session state before navigating away
    auto playback = PlaybackService::Instance;
    playback->Player->Pause();
    playback->Player->Source = nullptr;

    NavidromeService::Instance->SetCredentials(nullptr, nullptr, nullptr);
    ViewModels::LibraryViewModel::Instance->ClearAll();
    ViewModels::PlaylistsViewModel::Instance->Clear();

    // Clear the back-stack so the user cannot navigate back from LoginPage
    this->Frame->BackStack->Clear();
    this->Frame->Navigate(Windows::UI::Xaml::Interop::TypeName(LoginPage::typeid));
}
void Opal::LibraryPage::OnCarouselScrollLeft(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e)
{
    auto btn = dynamic_cast<Windows::UI::Xaml::Controls::Button^>(sender);
    if (!btn || !btn->Tag) return;
    auto targetName = btn->Tag->ToString();
    auto targetGrid = dynamic_cast<Windows::UI::Xaml::Controls::GridView^>(this->FindName(targetName));
    if (targetGrid) {
        auto sv = FindScrollViewerChild(targetGrid);
        if (sv) {
            sv->ChangeView(sv->HorizontalOffset - 500, nullptr, nullptr);
        }
    }
}

void Opal::LibraryPage::OnCarouselScrollRight(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e)
{
    auto btn = dynamic_cast<Windows::UI::Xaml::Controls::Button^>(sender);
    if (!btn || !btn->Tag) return;
    auto targetName = btn->Tag->ToString();
    auto targetGrid = dynamic_cast<Windows::UI::Xaml::Controls::GridView^>(this->FindName(targetName));
    if (targetGrid) {
        auto sv = FindScrollViewerChild(targetGrid);
        if (sv) {
            sv->ChangeView(sv->HorizontalOffset + 500, nullptr, nullptr);
        }
    }
}
void LibraryPage::OnCollapseClicked(Object^ sender, RoutedEventArgs^ e)
{
    ToggleFullPlayer();
}

void LibraryPage::OnSpotlightTapped(Object^ sender, TappedRoutedEventArgs^ e)
{
    auto grid = dynamic_cast<Grid^>(sender);
    if (grid != nullptr) {
        auto song = dynamic_cast<Song^>(grid->Tag);
        if (song != nullptr) {
            auto pb = PlaybackService::Instance;
            auto songs = ref new Platform::Collections::Vector<Song^>();
            songs->Append(song);
            pb->PlayQueue(songs, 0);
            
            // Switch to full player view
            FullPlayerGrid->Visibility = Windows::UI::Xaml::Visibility::Visible;
            BrowseGrid->Visibility = Windows::UI::Xaml::Visibility::Collapsed;
        }
    }
}

void LibraryPage::OnCarouselPrev(Object^ sender, RoutedEventArgs^ e)
{
    if (SpotlightCarousel->SelectedIndex > 0)
        SpotlightCarousel->SelectedIndex--;
}

void LibraryPage::OnCarouselNext(Object^ sender, RoutedEventArgs^ e) {
    auto current = SpotlightCarousel->SelectedIndex;
    int size = (int)SpotlightCarousel->Items->Size;
    if (size > 0) {
        if (current < size - 1) SpotlightCarousel->SelectedIndex = current + 1;
        else SpotlightCarousel->SelectedIndex = 0;
    }
}

void LibraryPage::OnCarouselSelectionChanged(Object^ sender, SelectionChangedEventArgs^ e)
{
    if (SpotlightCarousel == nullptr || CarouselPrevBtn == nullptr || CarouselNextBtn == nullptr) return;

    auto current = SpotlightCarousel->SelectedIndex;
    auto size = (int)SpotlightCarousel->Items->Size;

    CarouselPrevBtn->Visibility = (current > 0) ? Windows::UI::Xaml::Visibility::Visible : Windows::UI::Xaml::Visibility::Collapsed;
    CarouselNextBtn->Visibility = (current < size - 1) ? Windows::UI::Xaml::Visibility::Visible : Windows::UI::Xaml::Visibility::Collapsed;
}

void LibraryPage::OnGenreClicked(Object^ sender, ItemClickEventArgs^ e) {
    auto genre = dynamic_cast<GenreModel^>(e->ClickedItem);
    if (genre != nullptr) {
        LoadGenreAlbums(genre->Name);
    }
}

void LibraryPage::OnBackClicked(Object^ sender, RoutedEventArgs^ e) {
    LoadHomePage();
}

void LibraryPage::UpdateUpcomingQueue()
{
    auto pb = PlaybackService::Instance;
    auto fullQueue = pb->Queue;
    if (_upcomingQueue == nullptr || fullQueue == nullptr || pb->CurrentSong == nullptr) return;
    
    _upcomingQueue->Clear();
    
    // Find current index
    int currentIndex = (int)pb->CurrentIndex;
    
    // Safety check: is the song at this index actually the current song?
    if (currentIndex >= (int)fullQueue->Size || fullQueue->GetAt(currentIndex)->Id != pb->CurrentSong->Id) {
        // Find it by ID instead
        currentIndex = -1;
        for (unsigned int i = 0; i < fullQueue->Size; i++) {
            if (fullQueue->GetAt(i)->Id == pb->CurrentSong->Id) {
                currentIndex = (int)i;
                break;
            }
        }
    }
    
    // Append items AFTER the current song
    if (currentIndex != -1) {
        for (unsigned int i = (unsigned int)(currentIndex + 1); i < fullQueue->Size; i++) {
            _upcomingQueue->Append(fullQueue->GetAt(i));
        }
    }
}

void LibraryPage::OnTrackContextOpening(Object^ sender, Object^ e)
{
    auto menu = dynamic_cast<MenuFlyout^>(sender);
    if (menu == nullptr) return;
    
    MenuFlyoutSubItem^ addToPlaylistSub = nullptr;
    for (unsigned int i = 0; i < menu->Items->Size; i++) {
        auto item = menu->Items->GetAt(i);
        auto frameworkItem = dynamic_cast<FrameworkElement^>(item);
        if (frameworkItem != nullptr) {
            if (frameworkItem->Name == "AddToPlaylistMenu") addToPlaylistSub = dynamic_cast<MenuFlyoutSubItem^>(item);
        }
    }
    
    if (addToPlaylistSub != nullptr) {
        addToPlaylistSub->Items->Clear();
        auto vm = ViewModels::PlaylistsViewModel::Instance;
        for (unsigned int i = 0; i < vm->Playlists->Size; i++) {
            auto p = vm->Playlists->GetAt(i);
            auto item = ref new MenuFlyoutItem();
            item->Text = p->Name;
            item->Tag = p;
            item->Click += ref new RoutedEventHandler([this, p, menu](Object^ s, RoutedEventArgs^ args) {
                auto grid = dynamic_cast<Grid^>(menu->Target);
                if (grid != nullptr) {
                    auto song = dynamic_cast<Song^>(grid->DataContext);
                    if (song != nullptr) {
                        ViewModels::PlaylistsViewModel::Instance->AddSongToPlaylist(p->Id, song->Id);
                    }
                }
            });
            addToPlaylistSub->Items->Append(item);
        }
    }
}

void LibraryPage::OnPlayMenuClicked(Object^ sender, RoutedEventArgs^ e) {
    auto item = dynamic_cast<MenuFlyoutItem^>(sender);
    auto grid = dynamic_cast<Grid^>(dynamic_cast<MenuFlyout^>(item->Parent)->Target);
    if (grid != nullptr) {
        auto song = dynamic_cast<Song^>(grid->DataContext);
        if (song) PlaybackService::Instance->PlaySong(song);
    }
}

void LibraryPage::OnAddQueueMenuClicked(Object^ sender, RoutedEventArgs^ e) {
    auto item = dynamic_cast<MenuFlyoutItem^>(sender);
    auto grid = dynamic_cast<Grid^>(dynamic_cast<MenuFlyout^>(item->Parent)->Target);
    if (grid != nullptr) {
        auto song = dynamic_cast<Song^>(grid->DataContext);
        if (song) PlaybackService::Instance->AddToQueue(song);
    }
}

// OnRemoveFromPlaylistClicked removed - migrated to PlaylistDetailsPage.xaml.cpp



void LibraryPage::OnMoreButtonClicked(Object^ sender, RoutedEventArgs^ e)
{
    auto btn = dynamic_cast<Button^>(sender);
    if (btn != nullptr) {
        auto grid = dynamic_cast<Grid^>(VisualTreeHelper::GetParent(btn));
        if (grid != nullptr) {
            auto flyout = grid->ContextFlyout;
            if (flyout != nullptr) {
                flyout->ShowAt(grid);
            }
        }
    }
}
