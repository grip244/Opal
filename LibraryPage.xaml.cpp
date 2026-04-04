#include "pch.h"
#include "LibraryPage.xaml.h"
#include "LoginPage.xaml.h"
#include "Services/PlaybackService.h"
#include "Services/LyricsService.h"
#include <sstream>
#include <string>

using namespace Opal;
using namespace Platform;
using namespace concurrency;
using namespace Windows::Foundation;
using namespace Windows::UI::Xaml;
using namespace Windows::UI::Xaml::Controls;
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
    _albums = ref new Platform::Collections::Vector<AlbumModel^>();
    _albumSongs = ref new Platform::Collections::Vector<Song^>();
    _albumDiscGroups = ref new Platform::Collections::Vector<DiscGroup^>();
    _upcomingQueue = ref new Platform::Collections::Vector<Song^>();

    InitializeComponent();
    
    _lastLyricIndex = -1;

    this->Loaded += ref new RoutedEventHandler(this, &LibraryPage::OnPageLoaded);

    auto page = this;
    PlaybackService::Instance->SongChanged += ref new SongChangedHandler([page](PlaybackService^ sender, Song^ song) {
        page->Dispatcher->RunAsync(CoreDispatcherPriority::Normal, ref new DispatchedHandler([page, song]() {
            page->NowPlayingImage->Source = song->CoverArt;
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
}

void LibraryPage::OnNavigatedTo(Windows::UI::Xaml::Navigation::NavigationEventArgs^ e)
{
    auto artistId = dynamic_cast<Platform::String^>(e->Parameter);
    if (artistId != nullptr && artistId->Length() > 0)
    {
        LoadArtistPage(artistId);
    }
    else
    {
        auto am = dynamic_cast<AlbumModel^>(e->Parameter);
        if (am != nullptr)
        {
            LoadAlbumPage(am->Id);
        }
    }
}

void LibraryPage::OnPageLoaded(Object^ sender, RoutedEventArgs^ e)
{
    if (AnalyticsInfo::VersionInfo->DeviceFamily == "Windows.Xbox")
    {
        VisualStateManager::GoToState(this, "XboxState", false);
    }
    if (HomeGrid->Visibility == Windows::UI::Xaml::Visibility::Collapsed &&
        ArtistGrid->Visibility == Windows::UI::Xaml::Visibility::Collapsed &&
        AlbumGrid->Visibility == Windows::UI::Xaml::Visibility::Collapsed)
    {
        LoadHomePage();
    }

    auto self = this;
    SystemNavigationManager::GetForCurrentView()->BackRequested += ref new EventHandler<BackRequestedEventArgs^>(
        [self](Object^ sender, BackRequestedEventArgs^ args) {
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
}

void LibraryPage::LoadHomePage()
{
    BrowseGrid->Visibility = Windows::UI::Xaml::Visibility::Visible;
    HomeGrid->Visibility = Windows::UI::Xaml::Visibility::Visible;
    ArtistGrid->Visibility = Windows::UI::Xaml::Visibility::Collapsed;
    AlbumGrid->Visibility = Windows::UI::Xaml::Visibility::Collapsed;
    FullPlayerGrid->Visibility = Windows::UI::Xaml::Visibility::Collapsed;

    LibraryVM->LoadAllCategories();
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
                        Platform::String^ coverUrlStr = NavidromeService::Instance->GetCoverArtUrl(artistObj->GetNamedString("id", ""), 500);
                        try { self->ArtistDetailImageBrush->ImageSource = ref new BitmapImage(ref new Uri(coverUrlStr)); } catch (...) {}

                        if (artistObj->HasKey("album")) {
                            JsonArray^ albumsArray = artistObj->GetNamedArray("album");
                            self->_albums->Clear();
                            for (unsigned int i = 0; i < albumsArray->Size; i++) {
                                JsonObject^ albObj = albumsArray->GetObjectAt(i);
                                auto am = ref new AlbumModel();
                                am->Id = albObj->GetNamedString("id", "");
                                am->Title = albObj->HasKey("title") ? albObj->GetNamedString("title") : albObj->GetNamedString("name", "Unknown Album");
                                if (albObj->HasKey("year")) {
                                    try { am->Year = albObj->GetNamedNumber("year").ToString(); } catch (...) {
                                        am->Year = albObj->GetNamedValue("year")->Stringify();
                                    }
                                }
                                Platform::String^ cover = albObj->GetNamedString("coverArt", "");
                                Platform::String^ url = NavidromeService::Instance->GetCoverArtUrl(cover, 500);
                                try { am->CoverArt = ref new BitmapImage(ref new Uri(url)); } catch (...) {}
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

void LibraryPage::LoadAlbumPage(Platform::String^ albumId)
{
    HomeGrid->Visibility = Windows::UI::Xaml::Visibility::Collapsed;
    ArtistGrid->Visibility = Windows::UI::Xaml::Visibility::Collapsed;
    AlbumGrid->Visibility = Windows::UI::Xaml::Visibility::Visible;

    auto self = this;
    create_task(NavidromeService::Instance->GetAlbumAsync(albumId)).then([self](Platform::String^ jsonStr) {
        if (jsonStr != nullptr) {
            self->Dispatcher->RunAsync(CoreDispatcherPriority::Normal, ref new DispatchedHandler([self, jsonStr]() {
                try {
                    JsonObject^ rootRaw = JsonObject::Parse(jsonStr);
                    JsonObject^ root = rootRaw->GetNamedObject("subsonic-response", nullptr);
                    if (root != nullptr && root->HasKey("album")) {
                        JsonObject^ albumObj = root->GetNamedObject("album");
                        self->AlbumDetailTitle->Text = albumObj->HasKey("name") ? albumObj->GetNamedString("name") : albumObj->GetNamedString("title", "Unknown");
                        self->AlbumDetailArtist->Text = albumObj->GetNamedString("artist", "");
                        Platform::String^ coverUrlStr = NavidromeService::Instance->GetCoverArtUrl(albumObj->GetNamedString("coverArt", ""), 500);
                        try { self->AlbumDetailImage->Source = ref new BitmapImage(ref new Uri(coverUrlStr)); } catch (...) {}

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
                                    try { song->DiscNumber = (int)songObj->GetNamedNumber("discNumber"); } catch (...) {}
                                }
                                
                                if (songObj->HasKey("duration")) {
                                    int totalSeconds = (int)songObj->GetNamedNumber("duration");
                                    song->DurationInSeconds = totalSeconds;
                                    wchar_t buf[16]; swprintf_s(buf, L"%d:%02d", totalSeconds / 60, totalSeconds % 60);
                                    song->Duration = ref new Platform::String(buf);
                                } else { song->Duration = "--:--"; song->DurationInSeconds = 0; }

                                song->StreamUrl = NavidromeService::Instance->GetStreamUrl(song->Id);
                                Platform::String^ coverArtId = songObj->GetNamedString("coverArt", "");
                                song->CoverUrl = NavidromeService::Instance->GetCoverArtUrl(coverArtId, 500);
                                song->CoverArt = self->AlbumDetailImage->Source;
                                
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

    // Stage 1: LRCLib (Modern, high quality)
    create_task(LyricsService::FetchFromLrcLibAsync(song->Artist, song->Title, song->Album, (double)song->DurationInSeconds)).then([self, song](LyricsResult^ res) {
        if (res != nullptr && res->Lines->Size > 0) {
            self->Dispatcher->RunAsync(CoreDispatcherPriority::High, ref new DispatchedHandler([self, res]() {
                self->LyricsVM->Lines->Clear();
                self->LyricsVM->ActiveIndex = -1;
                for (auto l : res->Lines) self->LyricsVM->Lines->Append(l);
                self->LyricsVM->IsTimed = res->IsTimed;
            }));
            return;
        }

        // Stage 2: Navidrome OpenSubsonic (Server-side lyrics)
        create_task(NavidromeService::Instance->GetLyricsBySongIdAsync(song->Id)).then([self, song](Platform::String^ osJson) {
            if (osJson != nullptr) {
                auto osResult = LyricsService::ParseOpenSubsonicJson(osJson);
                if (osResult != nullptr && osResult->Lines->Size > 0) {
                    self->Dispatcher->RunAsync(CoreDispatcherPriority::High, ref new DispatchedHandler([self, osResult]() {
                        self->LyricsVM->Lines->Clear();
                        self->LyricsVM->ActiveIndex = -1;
                        for (auto l : osResult->Lines) self->LyricsVM->Lines->Append(l);
                        self->LyricsVM->IsTimed = osResult->IsTimed;
                    }));
                    return;
                }
            }

            // Stage 3: Netease (Crowdsourced fallback)
            create_task(LyricsService::FetchFromNeteaseAsync(song->Artist, song->Title)).then([self, song](LyricsResult^ nResult) {
                if (nResult != nullptr && nResult->Lines->Size > 0) {
                    self->Dispatcher->RunAsync(CoreDispatcherPriority::High, ref new DispatchedHandler([self, nResult]() {
                        self->LyricsVM->Lines->Clear();
                        self->LyricsVM->ActiveIndex = -1;
                        for (auto l : nResult->Lines) self->LyricsVM->Lines->Append(l);
                        self->LyricsVM->IsTimed = nResult->IsTimed;
                    }));
                    return;
                }

                // Stage 4: Legacy Subsonic (Last resort)
                create_task(NavidromeService::Instance->GetLyricsAsync(song->Artist, song->Title)).then([self](Platform::String^ legacyJson) {
                    self->Dispatcher->RunAsync(CoreDispatcherPriority::High, ref new DispatchedHandler([self, legacyJson]() {
                        if (legacyJson != nullptr) {
                            try {
                                JsonObject^ resp = JsonObject::Parse(legacyJson)->GetNamedObject("subsonic-response");
                                JsonObject^ lrc = resp->GetNamedObject("lyrics");
                                String^ c = lrc->HasKey("content") ? lrc->GetNamedString("content") : lrc->GetNamedString("text", "");
                                self->LyricsVM->SetLyrics(c);
                            } catch (...) { self->LyricsVM->SetLyrics(""); }
                        } else self->LyricsVM->SetLyrics("");
                    }));
                });
            });
        });
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
    create_task(NavidromeService::Instance->SearchAsync(query, 20)).then([self](Platform::String^ jsonStr) {
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

void LibraryPage::OnArtistClicked(Object^ sender, ItemClickEventArgs^ e) { auto am = dynamic_cast<ArtistModel^>(e->ClickedItem); if (am != nullptr) LoadArtistPage(am->Id); }
void LibraryPage::OnAlbumClicked(Object^ sender, ItemClickEventArgs^ e) { auto am = dynamic_cast<AlbumModel^>(e->ClickedItem); if (am != nullptr) LoadAlbumPage(am->Id); }
void LibraryPage::OnGenericAlbumClicked(Object^ sender, ItemClickEventArgs^ e)
{
    auto am = dynamic_cast<AlbumModel^>(e->ClickedItem);
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
                            s->Album = songObj->GetNamedString("album", "Unknown");
                            s->StreamUrl = NavidromeService::Instance->GetStreamUrl(s->Id);
                            s->CoverUrl = NavidromeService::Instance->GetCoverArtUrl(songObj->GetNamedString("coverArt"), 500);
                            s->CoverArt = ref new BitmapImage(ref new Uri(s->CoverUrl));
                            albumQueue->Append(s);
                        }
                        if (albumQueue->Size > 0) {
                            PlaybackService::Instance->PlayQueue(albumQueue, 0);
                            self->FullPlayerGrid->Visibility = Windows::UI::Xaml::Visibility::Visible;
                            self->BrowseGrid->Visibility = Windows::UI::Xaml::Visibility::Collapsed;
                        }
                    } catch (...) {}
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

bool LibraryPage::IsFullPlayerActive::get() {
    return FullPlayerGrid != nullptr && FullPlayerGrid->Visibility == Windows::UI::Xaml::Visibility::Visible;
}
void LibraryPage::OnPlayAlbumClicked(Object^ sender, RoutedEventArgs^ e) { if (_albumSongs->Size > 0) { PlaybackService::Instance->PlayQueue(_albumSongs, 0); } }
void LibraryPage::OnQueueItemClick(Object^ sender, ItemClickEventArgs^ e) { auto song = dynamic_cast<Song^>(e->ClickedItem); if (song != nullptr) { auto pb = PlaybackService::Instance; for (unsigned int i = 0; i < pb->Queue->Size; i++) if (pb->Queue->GetAt(i) == song) { PlaybackService::Instance->PlayQueue(pb->Queue, i); break; } } }
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
void LibraryPage::OnDisconnectClicked(Object^ sender, RoutedEventArgs^ e) {
    auto playback = PlaybackService::Instance;
    playback->Player->Pause();
    playback->Player->Source = nullptr;
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
            pb->Queue->Clear();
            pb->Queue->Append(song);
            pb->PlaySong(song);
            
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

void LibraryPage::OnCarouselNext(Object^ sender, RoutedEventArgs^ e)
{
    if (SpotlightCarousel->SelectedIndex < (int)SpotlightCarousel->Items->Size - 1)
        SpotlightCarousel->SelectedIndex++;
}

void LibraryPage::UpdateUpcomingQueue()
{
    auto pb = PlaybackService::Instance;
    auto fullQueue = pb->Queue;
    if (_upcomingQueue == nullptr || fullQueue == nullptr || pb->CurrentSong == nullptr) return;
    
    _upcomingQueue->Clear();
    
    // Find current index
    int currentIndex = -1;
    for (unsigned int i = 0; i < fullQueue->Size; i++) {
        if (fullQueue->GetAt(i)->Id == pb->CurrentSong->Id) {
            currentIndex = (int)i;
            break;
        }
    }
    
    // Append items AFTER the current song
    if (currentIndex != -1) {
        for (unsigned int i = (unsigned int)(currentIndex + 1); i < fullQueue->Size; i++) {
            _upcomingQueue->Append(fullQueue->GetAt(i));
        }
    }
}
