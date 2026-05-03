#include "pch.h"
#include "MainPage.xaml.h"
#include "LoginPage.xaml.h"
#include "SettingsPage.xaml.h"
#include "LibraryPage.xaml.h"
#include "ArtistsPage.xaml.h"
#include "AlbumsPage.xaml.h"
#include "TracksPage.xaml.h"
#include "GenresPage.xaml.h"
#include "PlaylistsPage.xaml.h"
#include "PlaylistDetailsPage.xaml.h"
#include "UI/Controls/ThumbnailView.xaml.h"
#include "ViewModels/LibraryViewModel.h"
#include "ViewModels/PlaylistsViewModel.h"
#include "Services/NavidromeService.h"
#include "Services/PlaybackService.h"
#include "Services/CastingService.h"
#include "Services/DebugLogger.h"
#include "Models/SharedModels.h"
#include <ppltasks.h>

using namespace Opal;
using namespace Platform;
using namespace Windows::UI::Xaml;
using namespace Windows::UI::Xaml::Controls;
using namespace Windows::UI::Xaml::Navigation;
using namespace Windows::UI::Xaml::Interop;
using namespace Windows::Foundation::Collections;
using namespace Microsoft::UI::Xaml::Controls;
using namespace Windows::Foundation;
using namespace Windows::UI::Core;
using namespace Windows::System::Profile;
using namespace Windows::Data::Json;
using namespace Windows::UI::Xaml::Media::Imaging;
using namespace concurrency;

Windows::UI::Xaml::Controls::Frame^ MainPage::GetNavigationFrame()
{
    return ContentFrame;
}

MainPage::MainPage()
{
    InitializeComponent();
    this->Loaded += ref new RoutedEventHandler(this, &MainPage::OnPageLoaded);

    auto self = this;
    PlaybackService::Instance->SongChanged += ref new SongChangedHandler([self](PlaybackService^ sender, Song^ song) {
        self->Dispatcher->RunAsync(CoreDispatcherPriority::Normal, ref new DispatchedHandler([self, song]() {
            self->NowPlayingTitle->Text = song->Title;
            self->PlayerExplicitBadge->Visibility = song->ExplicitStatus == "explicit" ? Windows::UI::Xaml::Visibility::Visible : Windows::UI::Xaml::Visibility::Collapsed;
            self->NowPlayingArtist->Text = song->Artist;
            self->MiniThumbnail->SourceUrl = song->CoverUrl;
            self->GlobalPlayerBar->Visibility = Windows::UI::Xaml::Visibility::Visible;
            self->FavoriteBtn->IsChecked = song->IsFavorite;
            self->FavoriteIcon->Glyph = song->IsFavorite ? ref new Platform::String(L"\uEB52") : ref new Platform::String(L"\uEB51");
            self->GlobalRatingControl->Value = song->Rating;
        }));
    });

    CastingService::Instance->TrackReceived += ref new Windows::Foundation::EventHandler<TrackData^>([self](Object^ sender, TrackData^ track) {
        self->Dispatcher->RunAsync(CoreDispatcherPriority::Normal, ref new DispatchedHandler([self, track]() {
            auto song = ref new Song();
            song->Id = track->Id;
            song->Title = track->Title;
            song->Artist = track->Artist;
            song->Album = track->Album;
            song->StreamUrl = track->StreamUrl;
            song->CoverUrl = track->CoverUrl;
            song->ExplicitStatus = track->ExplicitStatus;
            
            // Wake Up Call: Ensure player bar is visible even if nothing has played locally
            self->GlobalPlayerBar->Visibility = Windows::UI::Xaml::Visibility::Visible;
            
            self->NowPlayingTitle->Text = track->Title;
            self->NowPlayingArtist->Text = track->Artist;
            self->PlayerExplicitBadge->Visibility = track->ExplicitStatus == "explicit" ? Windows::UI::Xaml::Visibility::Visible : Windows::UI::Xaml::Visibility::Collapsed;
            
            if (track->CoverUrl != nullptr && !track->CoverUrl->IsEmpty()) {
                DebugLogger::Instance->Log("CastingService", "Loading Remote Thumbnail: " + track->CoverUrl);
                self->MiniThumbnail->SourceUrl = track->CoverUrl;
            }
            
            // Remote Playback Logic: Start playing the stream if received
            if (track->StreamUrl != nullptr && track->StreamUrl->Length() > 0) {
                PlaybackService::Instance->PlaySong(song);
            }
            
            // On Xbox, switch to full player mode and hide navigation
            if (AnalyticsInfo::VersionInfo->DeviceFamily == "Windows.Xbox") {
                self->UpdateMenuVisibility();
                
                auto targetType = Windows::UI::Xaml::Interop::TypeName(LibraryPage::typeid);
                if (self->ContentFrame->CurrentSourcePageType.Name != targetType.Name) {
                    self->ContentFrame->Navigate(targetType);
                }
                
                auto page = dynamic_cast<LibraryPage^>(self->ContentFrame->Content);
                if (page != nullptr) {
                    // Navigate to the full player UI specifically
                    page->ShowPlayer(false); 
                }
            }
            
            DebugLogger::Instance->Log("CastingService", "Remote track received and playing - " + track->Title);
        }));
    });

    CastingService::Instance->DevicesChanged += ref new Windows::Foundation::EventHandler<Object^>([self](Object^ sender, Object^ args) {
        self->Dispatcher->RunAsync(CoreDispatcherPriority::Normal, ref new DispatchedHandler([self]() {
            self->DiscoveredDevicesList->ItemsSource = CastingService::Instance->DiscoveredDevices;
        }));
    });

    auto timer = ref new DispatcherTimer();
    TimeSpan ts = { 500 * 10000LL };
    timer->Interval = ts;
    timer->Tick += ref new Windows::Foundation::EventHandler<Object^>([self](Object^ sender, Object^ args) {
        auto pb = PlaybackService::Instance;
        auto session = pb->Player->PlaybackSession;
        if (pb->CurrentSong != nullptr && session->PlaybackState != Windows::Media::Playback::MediaPlaybackState::None) {
            self->GlobalPlayerBar->Visibility = Windows::UI::Xaml::Visibility::Visible;
            self->ProgressSlider->Maximum = (double)session->NaturalDuration.Duration;
            self->ProgressSlider->Value = (double)session->Position.Duration;
            
            int curSecs = (int)(session->Position.Duration / 10000000LL);
            int durSecs = (int)(session->NaturalDuration.Duration / 10000000LL);
            wchar_t cBuf[16], dBuf[16];
            swprintf_s(cBuf, L"%d:%02d", curSecs / 60, curSecs % 60);
            swprintf_s(dBuf, L"%d:%02d", durSecs / 60, durSecs % 60);
            self->CurrentTimeText->Text = ref new String(cBuf);
            self->TotalTimeText->Text = ref new String(dBuf);
            
            auto song = pb->CurrentSong;
            if (song != nullptr) {
                self->NowPlayingTitle->Text = song->Title;
                self->PlayerExplicitBadge->Visibility = song->ExplicitStatus == "explicit" ? Windows::UI::Xaml::Visibility::Visible : Windows::UI::Xaml::Visibility::Collapsed;
                self->NowPlayingArtist->Text = song->Artist;
                if (self->MiniThumbnail->SourceUrl != song->CoverUrl) self->MiniThumbnail->SourceUrl = song->CoverUrl;
            }
            
            if (session->PlaybackState == Windows::Media::Playback::MediaPlaybackState::Playing) self->PlayPauseIcon->Symbol = Symbol::Pause;
            else self->PlayPauseIcon->Symbol = Symbol::Play;
        }
    });
    timer->Start();
}

void MainPage::OnPageLoaded(Object^ sender, RoutedEventArgs^ e)
{
    DebugLogger::Instance->Log("MainPage", "Page Loaded");
    UpdateSidebarPlaylists();

    auto timer = ref new DispatcherTimer();
    TimeSpan delay; delay.Duration = 1500 * 10000; // 1.5 seconds in 100ns ticks
    timer->Interval = delay;
    auto self = this;
    timer->Tick += ref new EventHandler<Object^>([this, timer](Object^, Object^) {
        timer->Stop();
        this->UpdateSidebarPlaylists();
    });
    timer->Start();

    CastingService::Instance->Dispatcher = this->Dispatcher;

    // Remove redundant auto self = this;
    PlaybackService::Instance->SongChanged += ref new SongChangedHandler([self](PlaybackService^ sender, Song^ song) {
         if (CastingService::Instance->IsCastingActive && !AnalyticsInfo::VersionInfo->DeviceFamily->Equals("Windows.Xbox")) {
             DebugLogger::Instance->Log("MainPage", "Handover - Auto-casting next song: " + song->Title);
             
             auto track = ref new TrackData();
             track->Id = song->Id;
             track->Title = song->Title;
             track->Artist = song->Artist;
             track->Album = song->Album;
             track->CoverUrl = song->CoverUrl;
             track->StreamUrl = song->StreamUrl;
             track->ExplicitStatus = song->ExplicitStatus;
             
             CastingService::Instance->CastTrack(CastingService::Instance->TargetDeviceIp, track);
             
             // Sync the new queue window too
             self->SyncQueue();
         }
    });

    if (AnalyticsInfo::VersionInfo->DeviceFamily == "Windows.Xbox")
    {
        VisualStateManager::GoToState(this, "XboxState", false);
    }
    
    // Casting service is already initialized in App::OnLaunched

    // Initial routing logic
    if (NavidromeService::Instance->IsAuthenticated())
    {

        ContentFrame->Navigate(Windows::UI::Xaml::Interop::TypeName(LibraryPage::typeid));
    }
    else
    {
        ContentFrame->Navigate(Windows::UI::Xaml::Interop::TypeName(LoginPage::typeid));
    }

    UpdateMenuVisibility();
    
    auto vm = PlaylistsVM;
    vm->LoadPlaylistsAsync();
    vm->Playlists->VectorChanged += ref new VectorChangedEventHandler<PlaylistModel^>([this](IObservableVector<PlaylistModel^>^ sender, IVectorChangedEventArgs^ args) {
        auto self = this;
        this->Dispatcher->RunAsync(CoreDispatcherPriority::Normal, ref new DispatchedHandler([self]() {
            self->UpdateSidebarPlaylists();
        }));
    });
}

void MainPage::OnMenuItemInvoked(Microsoft::UI::Xaml::Controls::NavigationView^ sender, Microsoft::UI::Xaml::Controls::NavigationViewItemInvokedEventArgs^ args)
{
    if (args->IsSettingsInvoked)
    {
        ContentFrame->Navigate(Windows::UI::Xaml::Interop::TypeName(SettingsPage::typeid));
    }
    else
    {
        auto tag = args->InvokedItemContainer->Tag->ToString();
        if (tag == "Library")
        {
            auto currentType = ContentFrame->CurrentSourcePageType;
            auto targetType = Windows::UI::Xaml::Interop::TypeName(LibraryPage::typeid);
            if (currentType.Name != targetType.Name)
            {
                ContentFrame->Navigate(targetType);
            }
            else
            {
                auto page = dynamic_cast<LibraryPage^>(ContentFrame->Content);
                if (page != nullptr) page->LoadHomePage();
            }
        }
        else if (tag == "Artists")
        {
            ContentFrame->Navigate(Windows::UI::Xaml::Interop::TypeName(ArtistsPage::typeid));
        }
        else if (tag == "Albums")
        {
            ContentFrame->Navigate(Windows::UI::Xaml::Interop::TypeName(AlbumsPage::typeid));
        }
        else if (tag == "Tracks")
        {
            ContentFrame->Navigate(Windows::UI::Xaml::Interop::TypeName(TracksPage::typeid));
        }
        else if (tag == "Genres")
        {
            ContentFrame->Navigate(Windows::UI::Xaml::Interop::TypeName(GenresPage::typeid));
        }
        else if (tag == "Playlists")
        {
            ContentFrame->Navigate(Windows::UI::Xaml::Interop::TypeName(PlaylistsPage::typeid));
        }
        else if (tag == "NewPlaylist")
        {
            OnNewPlaylistClick(nullptr, nullptr);
        }
        else
        {
            // Dynamic playlist ID
            ContentFrame->Navigate(Windows::UI::Xaml::Interop::TypeName(PlaylistDetailsPage::typeid), tag);
        }
    }
}

void MainPage::OnPlayPauseClicked(Object^ sender, RoutedEventArgs^ e) {
    auto pb = PlaybackService::Instance;
    if (pb->Player->PlaybackSession->PlaybackState == Windows::Media::Playback::MediaPlaybackState::Playing) {
        pb->Player->Pause();
        if (CastingService::Instance->IsCastingActive) CastingService::Instance->SendCommand("PAUSE");
    } else {
        pb->Player->Play();
        if (CastingService::Instance->IsCastingActive) CastingService::Instance->SendCommand("PLAY");
    }
}

void MainPage::OnNextClicked(Object^ sender, RoutedEventArgs^ e) { 
    PlaybackService::Instance->NextSong(); 
}
void MainPage::OnPreviousClicked(Object^ sender, RoutedEventArgs^ e) { 
    PlaybackService::Instance->PreviousSong(); 
}


void MainPage::OnVolumeChanged(Object^ sender, Windows::UI::Xaml::Controls::Primitives::RangeBaseValueChangedEventArgs^ e) { 
    PlaybackService::Instance->Player->Volume = e->NewValue / 100.0; 
}
void MainPage::OnLyricsToggleClicked(Object^ sender, RoutedEventArgs^ e) { 
    auto targetType = Windows::UI::Xaml::Interop::TypeName(LibraryPage::typeid);
    if (ContentFrame->CurrentSourcePageType.Name != targetType.Name) {
        ContentFrame->Navigate(targetType);
    }
    
    auto page = dynamic_cast<LibraryPage^>(ContentFrame->Content);
    if (page != nullptr) {
        page->ShowPlayer(true);
    }
}

// Legacy casting handlers removed

void MainPage::OnThumbnailClicked(Object^ sender, Windows::UI::Xaml::Input::TappedRoutedEventArgs^ e) {
    auto targetType = Windows::UI::Xaml::Interop::TypeName(LibraryPage::typeid);
    if (ContentFrame->CurrentSourcePageType.Name != targetType.Name) {
        ContentFrame->Navigate(targetType);
    }
    
    auto page = dynamic_cast<LibraryPage^>(ContentFrame->Content);
    if (page != nullptr) {
        page->ShowPlayer(false);
        ExpandThumbnailButton->Visibility = Windows::UI::Xaml::Visibility::Collapsed;
    }
}
void MainPage::OnThumbnailPointerEntered(Object^ sender, Windows::UI::Xaml::Input::PointerRoutedEventArgs^ e) {
    auto page = dynamic_cast<LibraryPage^>(ContentFrame->Content);
    bool expanded = (page != nullptr && page->IsFullPlayerActive);
    if (!expanded) ExpandThumbnailButton->Visibility = Windows::UI::Xaml::Visibility::Visible;
}
void MainPage::OnThumbnailPointerExited(Object^ sender, Windows::UI::Xaml::Input::PointerRoutedEventArgs^ e) {
    ExpandThumbnailButton->Visibility = Windows::UI::Xaml::Visibility::Collapsed;
}
void MainPage::OnProgressSeek(Object^ sender, Windows::UI::Xaml::Input::PointerRoutedEventArgs^ e) {
    auto pb = PlaybackService::Instance;
    long long ticks = (long long)ProgressSlider->Value;
    TimeSpan ts; ts.Duration = ticks; 
    pb->Player->PlaybackSession->Position = ts; 
    
    if (CastingService::Instance->IsCastingActive) {
        CastingService::Instance->SendCommand("SEEK|" + ticks.ToString());
    }
}

void MainPage::OnCastFlyoutOpened(Object^ sender, Object^ e) {
    // Sync UI with actual service state
    bool isActive = CastingService::Instance->IsCastingActive;
    DisconnectButton->Visibility = isActive ? Windows::UI::Xaml::Visibility::Visible : Windows::UI::Xaml::Visibility::Collapsed;
    
    // Clear list selection so re-selecting the same device triggers the event
    DiscoveredDevicesList->SelectedIndex = -1;
}

void MainPage::OnCastNowClicked(Object^ sender, RoutedEventArgs^ e) {
    String^ ip = TargetIPBox->Text;
    if (ip->Length() < 7) return;

    auto pb = PlaybackService::Instance;
    auto song = pb->CurrentSong;
    if (song != nullptr) {
        auto track = ref new TrackData();
        track->Id = song->Id;
        track->Title = song->Title;
        track->Artist = song->Artist;
        track->Album = song->Album;
        track->CoverUrl = song->CoverUrl;
        track->StreamUrl = song->StreamUrl;
        track->ExplicitStatus = song->ExplicitStatus;
        
        CastingService::Instance->CastTrack(ip, track);
        CastingService::Instance->IsCastingActive = true;
        CastingService::Instance->TargetDeviceIp = ip;
        
        // Illuminate the button using system accent color
        auto accentBrush = dynamic_cast<Windows::UI::Xaml::Media::Brush^>(Windows::UI::Xaml::Application::Current->Resources->Lookup("SystemControlHighlightAccentBrush"));
        auto icon = dynamic_cast<Windows::UI::Xaml::Controls::FontIcon^>(CastButton->Content);
        if (icon != nullptr && accentBrush != nullptr) icon->Foreground = accentBrush;
        
        DisconnectButton->Visibility = Windows::UI::Xaml::Visibility::Visible;
        CastButton->Flyout->Hide();
    }
}

void MainPage::OnDisconnectClicked(Object^ sender, RoutedEventArgs^ e) {
    if (CastingService::Instance->IsCastingActive) {
        CastingService::Instance->SendCommand("DISCONNECT");
    }
    CastingService::Instance->IsCastingActive = false;
    CastingService::Instance->TargetDeviceIp = nullptr;
    
    PlaybackService::Instance->Player->IsMuted = false;

    // Reset icon color to white
    auto icon = dynamic_cast<Windows::UI::Xaml::Controls::FontIcon^>(CastButton->Content);
    if (icon != nullptr) icon->Foreground = ref new Windows::UI::Xaml::Media::SolidColorBrush(Windows::UI::Colors::White);
    
    DisconnectButton->Visibility = Windows::UI::Xaml::Visibility::Collapsed;
    DiscoveredDevicesList->SelectedIndex = -1;
    TargetIPBox->Text = "";
    CastButton->Flyout->Hide();
}

void MainPage::OnDeviceSelected(Object^ sender, SelectionChangedEventArgs^ e) {
    auto selection = DiscoveredDevicesList->SelectedItem;
    if (selection != nullptr) {
        auto device = dynamic_cast<RemoteDevice^>(selection);
        if (device != nullptr) {
            DebugLogger::Instance->Log("MainPage", "Direct casting initiated to: " + device->Name + " (" + device->Ip + ")");
            
            auto song = PlaybackService::Instance->CurrentSong;
            if (song != nullptr) {
                auto track = ref new TrackData();
                track->Id = song->Id;
                track->Title = song->Title;
                track->Artist = song->Artist;
                track->Album = song->Album;
                track->CoverUrl = song->CoverUrl;
                track->StreamUrl = song->StreamUrl;
                track->ExplicitStatus = song->ExplicitStatus;
                
                CastingService::Instance->TargetDeviceIp = device->Ip;
                CastingService::Instance->IsCastingActive = true;
                CastingService::Instance->CastTrack(device->Ip, track);
                
                PlaybackService::Instance->Player->IsMuted = true;
        
                // Illuminate button
                auto icon = dynamic_cast<Windows::UI::Xaml::Controls::FontIcon^>(CastButton->Content);
                if (icon != nullptr) icon->Foreground = dynamic_cast<Windows::UI::Xaml::Media::Brush^>(Windows::UI::Xaml::Application::Current->Resources->Lookup("SystemControlHighlightAccentBrush"));
                DisconnectButton->Visibility = Windows::UI::Xaml::Visibility::Visible;
            }
            
            CastFlyout->Hide();
        }
    }
}

void MainPage::OnNavigated(Object^ sender, NavigationEventArgs^ e)
{
    // Update NavigationView selection based on current page
    auto name = e->SourcePageType.Name;
    if (name == Windows::UI::Xaml::Interop::TypeName(LibraryPage::typeid).Name)
    {
        MainNavigationView->SelectedItem = MainNavigationView->MenuItems->GetAt(0);
    }
    else if (name == Windows::UI::Xaml::Interop::TypeName(ArtistsPage::typeid).Name)
    {
        MainNavigationView->SelectedItem = MainNavigationView->MenuItems->GetAt(1);
    }
    else if (name == Windows::UI::Xaml::Interop::TypeName(AlbumsPage::typeid).Name)
    {
        MainNavigationView->SelectedItem = MainNavigationView->MenuItems->GetAt(2);
    }
    else if (name == Windows::UI::Xaml::Interop::TypeName(TracksPage::typeid).Name)
    {
        MainNavigationView->SelectedItem = MainNavigationView->MenuItems->GetAt(3);
    }
    else if (name == Windows::UI::Xaml::Interop::TypeName(GenresPage::typeid).Name)
    {
        MainNavigationView->SelectedItem = MainNavigationView->MenuItems->GetAt(4);
    }
    else if (name == Windows::UI::Xaml::Interop::TypeName(PlaylistDetailsPage::typeid).Name)
    {
        auto param = dynamic_cast<Platform::String^>(e->Parameter);
        if (param != nullptr) {
            for (unsigned int i = 0; i < MainNavigationView->MenuItems->Size; i++) {
                auto item = dynamic_cast<Microsoft::UI::Xaml::Controls::NavigationViewItem^>(MainNavigationView->MenuItems->GetAt(i));
                if (item != nullptr && item->Tag != nullptr && item->Tag->ToString() == param) {
                    MainNavigationView->SelectedItem = item;
                    break;
                }
            }
        }
    }
    else if (name == Windows::UI::Xaml::Interop::TypeName(PlaylistsPage::typeid).Name)
    {
        for (unsigned int i = 0; i < MainNavigationView->MenuItems->Size; i++) {
            auto item = dynamic_cast<Microsoft::UI::Xaml::Controls::NavigationViewItem^>(MainNavigationView->MenuItems->GetAt(i));
            if (item != nullptr && item->Tag != nullptr && item->Tag->ToString() == "Playlists") {
                MainNavigationView->SelectedItem = item;
                break;
            }
        }
    }

    UpdateMenuVisibility();
}

void MainPage::UpdateMenuVisibility()
{
    auto currentType = ContentFrame->CurrentSourcePageType;
    bool isLoginPage = (currentType.Name == Windows::UI::Xaml::Interop::TypeName(LoginPage::typeid).Name);

    if (isLoginPage)
    {
        MainNavigationView->PaneDisplayMode = Microsoft::UI::Xaml::Controls::NavigationViewPaneDisplayMode::LeftMinimal;
        MainNavigationView->IsPaneToggleButtonVisible = false;
        MainNavigationView->IsSettingsVisible = false;
    }
    else
    {
        MainNavigationView->IsSettingsVisible = true;
        
        bool isXbox = (AnalyticsInfo::VersionInfo->DeviceFamily == "Windows.Xbox");
        bool isRemote = CastingService::Instance->IsRemoteControlled;

        if (isXbox && isRemote)
        {
            // Lock into Kiosk mode when being cast to on Xbox
            MainNavigationView->PaneDisplayMode = Microsoft::UI::Xaml::Controls::NavigationViewPaneDisplayMode::LeftMinimal;
            MainNavigationView->IsPaneToggleButtonVisible = false;
            MainNavigationView->IsPaneOpen = false;
        }
        else if (isXbox)
        {
            MainNavigationView->PaneDisplayMode = Microsoft::UI::Xaml::Controls::NavigationViewPaneDisplayMode::LeftCompact;
            MainNavigationView->IsPaneToggleButtonVisible = true;
        }
        else
        {
            MainNavigationView->PaneDisplayMode = Microsoft::UI::Xaml::Controls::NavigationViewPaneDisplayMode::Left;
            MainNavigationView->IsPaneToggleButtonVisible = true;
        }
    }
}
void MainPage::OnSearchTextChanged(AutoSuggestBox^ sender, AutoSuggestBoxTextChangedEventArgs^ args)
{
    if (args->Reason == AutoSuggestionBoxTextChangeReason::UserInput)
    {
        String^ query = sender->Text;
        if (query->Length() < 2)
        {
            sender->ItemsSource = nullptr;
            return;
        }

    auto self = this;
    create_task(NavidromeService::Instance->SearchAsync(query, 10)).then([self, sender](String^ jsonStr) {
        if (jsonStr == nullptr) return;

        self->Dispatcher->RunAsync(CoreDispatcherPriority::Normal, ref new DispatchedHandler([self, sender, jsonStr]() {
                try {
                    JsonObject^ root = JsonObject::Parse(jsonStr)->GetNamedObject("subsonic-response");
                    JsonObject^ results = root->GetNamedObject("searchResult3", nullptr);
                    if (results == nullptr) return;

                    auto suggestions = ref new Platform::Collections::Vector<SearchResultModel^>();

                    // Add Artists
                    if (results->HasKey("artist")) {
                        JsonArray^ artists = results->GetNamedArray("artist");
                        for (unsigned int i = 0; i < artists->Size; i++) {
                            auto obj = artists->GetObjectAt(i);
                            auto model = ref new SearchResultModel();
                            model->Id = obj->GetNamedString("id");
                            model->Type = "Artist";
                            model->Title = obj->GetNamedString("name");
                            model->Subtitle = "Artist";
                            model->Icon = L"\uE716"; // Contact
                            model->ImageUrl = NavidromeService::Instance->GetCoverArtUrl(obj->GetNamedString("id"), 100);
                            suggestions->Append(model);
                        }
                    }

                    // Add Albums
                    if (results->HasKey("album")) {
                        JsonArray^ albums = results->GetNamedArray("album");
                        for (unsigned int i = 0; i < albums->Size; i++) {
                            auto obj = albums->GetObjectAt(i);
                            auto model = ref new SearchResultModel();
                            model->Id = obj->GetNamedString("id");
                            model->Type = "Album";
                            model->Icon = L"\uE179"; // AllApps
                            model->Title = obj->HasKey("name") ? obj->GetNamedString("name") : obj->GetNamedString("title", "Unknown Album");
                            model->Subtitle = obj->GetNamedString("artist", "Unknown Artist");
                            String^ coverId = obj->HasKey("coverArt") ? obj->GetNamedString("coverArt") : obj->GetNamedString("id");
                            model->ImageUrl = NavidromeService::Instance->GetCoverArtUrl(coverId, 100);
                            suggestions->Append(model);
                        }
                    }

                    // Add Songs
                    if (results->HasKey("song")) {
                        JsonArray^ songs = results->GetNamedArray("song");
                        for (unsigned int i = 0; i < songs->Size; i++) {
                            auto obj = songs->GetObjectAt(i);
                            auto model = ref new SearchResultModel();
                            model->Id = obj->GetNamedString("id");
                            model->Type = "Song";
                            model->Title = obj->GetNamedString("title");
                            model->Subtitle = obj->GetNamedString("artist") + " - " + obj->GetNamedString("album", "");
                            model->Icon = L"\uE8D6"; // Audio
                            String^ coverId = obj->HasKey("coverArt") ? obj->GetNamedString("coverArt") : "";
                            if (coverId->Length() > 0) {
                                model->ImageUrl = NavidromeService::Instance->GetCoverArtUrl(coverId, 100);
                            }
                            suggestions->Append(model);
                        }
                    }

                    sender->ItemsSource = suggestions;
                }
                catch (...) {}
            }));
        });
    }
}

void MainPage::OnSearchSuggestionChosen(AutoSuggestBox^ sender, AutoSuggestBoxSuggestionChosenEventArgs^ args)
{
    auto result = dynamic_cast<SearchResultModel^>(args->SelectedItem);
    if (result != nullptr)
    {
        sender->Text = result->Title;
    }
}

void MainPage::OnSearchEntered(AutoSuggestBox^ sender, AutoSuggestBoxQuerySubmittedEventArgs^ args)
{
    SearchResultModel^ selected = nullptr;
    if (args->ChosenSuggestion != nullptr)
    {
        selected = dynamic_cast<SearchResultModel^>(args->ChosenSuggestion);
    }

    if (selected != nullptr)
    {
        if (selected->Type == "Artist")
        {
            ContentFrame->Navigate(Windows::UI::Xaml::Interop::TypeName(LibraryPage::typeid), selected->Id);
        }
        else if (selected->Type == "Album")
        {
            auto am = ref new AlbumID3();
            am->Id = selected->Id;
            ContentFrame->Navigate(Windows::UI::Xaml::Interop::TypeName(LibraryPage::typeid), am);
        }
        else if (selected->Type == "Song")
        {
            // For songs, we might want to just play it or navigate to the album
            // Let's fetch the song and play it
            auto self = this;
            auto songId = selected->Id;
            create_task(NavidromeService::Instance->GetSongAsync(songId)).then([self, songId](String^ jsonStr) {
                if (jsonStr != nullptr) {
                    self->Dispatcher->RunAsync(CoreDispatcherPriority::Normal, ref new DispatchedHandler([self, jsonStr]() {
                        try {
                            JsonObject^ root = JsonObject::Parse(jsonStr)->GetNamedObject("subsonic-response");
                            JsonObject^ songObj = root->GetNamedObject("song");
                            auto s = ref new Song();
                            s->Id = songObj->GetNamedString("id");
                            s->Title = songObj->GetNamedString("title");
                            s->Artist = songObj->GetNamedString("artist");
                            s->Album = songObj->GetNamedString("album");
                            s->StreamUrl = NavidromeService::Instance->GetStreamUrl(s->Id);
                            s->CoverUrl = NavidromeService::Instance->GetCoverArtUrl(songObj->HasKey("coverArt") ? songObj->GetNamedString("coverArt") : s->Id, 500);
                            
                            PlaybackService::Instance->PlaySong(s);
                            
                            // Navigate to library player and pass the song
                            auto targetType = Windows::UI::Xaml::Interop::TypeName(LibraryPage::typeid);
                            if (self->ContentFrame->CurrentSourcePageType.Name != targetType.Name) {
                                self->ContentFrame->Navigate(targetType, s);
                            } else {
                                auto page = dynamic_cast<LibraryPage^>(self->ContentFrame->Content);
                                if (page != nullptr) page->ShowPlayer(false);
                            }
                        } catch (...) {}
                    }));
                }
            });
        }
    }
    else if (args->QueryText != nullptr && args->QueryText->Length() > 0)
    {
        auto targetType = Windows::UI::Xaml::Interop::TypeName(LibraryPage::typeid);
        if (ContentFrame->CurrentSourcePageType.Name != targetType.Name)
        {
            ContentFrame->Navigate(targetType);
        }

        auto page = dynamic_cast<LibraryPage^>(ContentFrame->Content);
        if (page != nullptr)
        {
            page->Search(args->QueryText);
        }
    }
}

void MainPage::OnFavoriteClicked(Object^ sender, RoutedEventArgs^ e) {
    auto pb = PlaybackService::Instance;
    auto song = pb->CurrentSong;
    if (song != nullptr) {
        bool isFav = false;
        auto btn = dynamic_cast<Windows::UI::Xaml::Controls::Primitives::ToggleButton^>(sender);
        if (btn != nullptr && btn->IsChecked != nullptr) {
            isFav = btn->IsChecked->Value;
        } else {
            isFav = !song->IsFavorite;
        }
        song->IsFavorite = isFav;
        FavoriteIcon->Glyph = isFav ? ref new Platform::String(L"\uEB52") : ref new Platform::String(L"\uEB51");
        NavidromeService::Instance->ToggleFavoriteAsync(song->Id, isFav);
    }
}

void MainPage::OnGlobalRatingChanged(Microsoft::UI::Xaml::Controls::RatingControl^ sender, Object^ args) {
    auto pb = PlaybackService::Instance;
    auto song = pb->CurrentSong;
    if (song != nullptr) {
        double newVar = sender->Value;
        if (song->Rating != newVar && newVar >= 0) {
            song->Rating = newVar;
            NavidromeService::Instance->SetRatingAsync(song->Id, (int)newVar);
        }
    }
}

void MainPage::SyncQueue() {
    if (!CastingService::Instance->IsCastingActive || CastingService::Instance->TargetDeviceIp == nullptr) return;

    auto pb = PlaybackService::Instance;
    auto queue = pb->Queue;
    auto array = ref new JsonArray();
    
    unsigned int currentIndex = pb->CurrentIndex;
    unsigned int queueSize = queue->Size;
    
    // Start from current song if possible, or 0
    unsigned int start = (currentIndex < queueSize) ? currentIndex : 0;
    // Sync up to 20 songs from the current point to stay within reasonable network limits
    unsigned int end = (start + 20 < queueSize) ? start + 20 : queueSize;

    for (unsigned int i = start; i < end; i++) {
        auto song = queue->GetAt(i);
        auto obj = ref new JsonObject();
        obj->Insert("Title", JsonValue::CreateStringValue(song->Title != nullptr ? song->Title : ""));
        obj->Insert("Artist", JsonValue::CreateStringValue(song->Artist != nullptr ? song->Artist : ""));
        obj->Insert("Album", JsonValue::CreateStringValue(song->Album != nullptr ? song->Album : ""));
        obj->Insert("CoverUrl", JsonValue::CreateStringValue(song->CoverUrl != nullptr ? song->CoverUrl : ""));
        obj->Insert("StreamUrl", JsonValue::CreateStringValue(song->StreamUrl != nullptr ? song->StreamUrl : ""));
        array->Append(obj);
    }
    
    CastingService::Instance->SendCommand("QUEUE|" + array->Stringify());
}

ViewModels::PlaylistsViewModel^ MainPage::PlaylistsVM::get()
{
    return ViewModels::PlaylistsViewModel::Instance;
}

bool MainPage::IsFullPlayerActive::get()
{
    auto page = dynamic_cast<LibraryPage^>(ContentFrame->Content);
    if (page != nullptr)
    {
        return page->IsFullPlayerActive;
    }
    return false;
}

void MainPage::OnNewPlaylistClick(Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e)
{
    auto input = ref new TextBox();
    input->PlaceholderText = "Playlist Name";
    input->Margin = Thickness(0, 10, 0, 0);

    auto dialog = ref new ContentDialog();
    dialog->Title = "New Playlist";
    dialog->Content = input;
    dialog->PrimaryButtonText = "Create";
    dialog->CloseButtonText = "Cancel";
    dialog->DefaultButton = ContentDialogButton::Primary;

    create_task(dialog->ShowAsync()).then([this, input](ContentDialogResult result) {
        if (result == ContentDialogResult::Primary && !input->Text->IsEmpty()) {
            PlaylistsVM->CreatePlaylist(input->Text);
        }
    });
}

void MainPage::UpdateSidebarPlaylists()
{
    static bool _updateQueued = false;
    if (_updateQueued) return;
    _updateQueued = true;

    this->Dispatcher->RunAsync(CoreDispatcherPriority::Low, ref new DispatchedHandler([this]() {
        _updateQueued = false;
        // Find the "NewPlaylist" item to insert after
        int startIndex = -1;
        for (unsigned int i = 0; i < MainNavigationView->MenuItems->Size; i++) {
            auto item = dynamic_cast<Microsoft::UI::Xaml::Controls::NavigationViewItem^>(MainNavigationView->MenuItems->GetAt(i));
            if (item != nullptr && item->Tag != nullptr && item->Tag->ToString() == "NewPlaylist") {
                startIndex = (int)i;
                break;
            }
        }

        if (startIndex == -1) return;

        // Remove existing dynamic items (all items after "NewPlaylist")
        while (MainNavigationView->MenuItems->Size > (unsigned int)(startIndex + 1)) {
            MainNavigationView->MenuItems->RemoveAt(startIndex + 1);
        }

        auto vm = ViewModels::PlaylistsViewModel::Instance;
        for (unsigned int i = 0; i < vm->Playlists->Size; i++)
        {
            auto p = vm->Playlists->GetAt(i);
            auto item = ref new Microsoft::UI::Xaml::Controls::NavigationViewItem();
            item->Content = p->Name;
            item->Tag = p->Id;
            item->Icon = ref new SymbolIcon(Symbol::MusicInfo);
            item->Margin = Thickness(12, 0, 0, 0); // Indent to look like sub-items
            MainNavigationView->MenuItems->Append(item);
        }
    }));
}
