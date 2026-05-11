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
#include "Services/GamepadService.h"
#include <Windows.UI.Composition.h>
#include <Windows.UI.Xaml.Hosting.h>
#include <Windows.Foundation.Numerics.h>
#include <Windows.UI.Xaml.Media.Animation.h>

using namespace Opal;
using namespace Platform;
using namespace Windows::UI::Xaml;
using namespace Windows::UI::Xaml::Controls;
using namespace Windows::UI::Xaml::Navigation;
using namespace Windows::UI::Xaml::Media::Animation;
using namespace Windows::UI::Xaml::Interop;
using namespace Windows::Foundation::Collections;
using namespace Microsoft::UI::Xaml::Controls;
using namespace Windows::UI::Composition;
using namespace Windows::UI::Xaml::Hosting;
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
    InitializeComposition();
    _marqueeStoryboard = nullptr;
    this->Loaded += ref new RoutedEventHandler(this, &MainPage::OnPageLoaded);

    auto self = this;
    PlaybackService::Instance->SongChanged += ref new SongChangedHandler([self](PlaybackService^ sender, Song^ song) {
        self->Dispatcher->RunAsync(CoreDispatcherPriority::Normal, ref new DispatchedHandler([self, song]() {
            if (song == nullptr) {
                self->GlobalPlayerBar->Visibility = Windows::UI::Xaml::Visibility::Collapsed;
                return;
            }

            self->NowPlayingTitle->Text = song->Title;
            bool isXbox = (Windows::System::Profile::AnalyticsInfo::VersionInfo->DeviceFamily == "Windows.Xbox");
            self->PlayerExplicitBadge->Visibility = (!isXbox && song->ExplicitStatus == "explicit") ? Windows::UI::Xaml::Visibility::Visible : Windows::UI::Xaml::Visibility::Collapsed;
            self->NowPlayingArtist->Text = song->Artist;
            self->MiniThumbnail->SourceUrl = song->CoverUrl;
            self->GlobalPlayerBar->Visibility = Windows::UI::Xaml::Visibility::Visible;
            self->FavoriteBtn->IsChecked = song->IsFavorite;
            self->FavoriteIcon->Glyph = song->IsFavorite ? ref new Platform::String(L"\uEB52") : ref new Platform::String(L"\uEB51");
            self->GlobalRatingControl->Value = song->Rating;

            // Cast-forwarding: when casting is active, send the new song to the remote target
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
                self->SyncQueue();
            }
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
            
            bool isXbox = (Windows::System::Profile::AnalyticsInfo::VersionInfo->DeviceFamily == "Windows.Xbox");
            self->NowPlayingTitle->Text = track->Title;
            self->NowPlayingArtist->Text = track->Artist;
            self->PlayerExplicitBadge->Visibility = (!isXbox && track->ExplicitStatus == "explicit") ? Windows::UI::Xaml::Visibility::Visible : Windows::UI::Xaml::Visibility::Collapsed;
            
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
                bool isXbox = (Windows::System::Profile::AnalyticsInfo::VersionInfo->DeviceFamily == "Windows.Xbox");
                self->NowPlayingTitle->Text = song->Title;
                self->PlayerExplicitBadge->Visibility = (!isXbox && song->ExplicitStatus == "explicit") ? Windows::UI::Xaml::Visibility::Visible : Windows::UI::Xaml::Visibility::Collapsed;
                self->NowPlayingArtist->Text = song->Artist;
                if (self->MiniThumbnail->SourceUrl != song->CoverUrl) {
                    self->MiniThumbnail->SourceUrl = song->CoverUrl;
                }
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

    if (Windows::Storage::ApplicationData::Current->LocalSettings->Values->HasKey("InnerNavigationState"))
    {
        Platform::String^ state = dynamic_cast<Platform::String^>(Windows::Storage::ApplicationData::Current->LocalSettings->Values->Lookup("InnerNavigationState"));
        if (state != nullptr)
        {
            ContentFrame->SetNavigationState(state);
        }
        Windows::Storage::ApplicationData::Current->LocalSettings->Values->Remove("InnerNavigationState");
    }

    CastingService::Instance->Dispatcher = this->Dispatcher;

    // 3.1: Initialize GamepadService with this page's CoreWindow, ContentFrame, and SearchBox
    auto coreWindow = Windows::UI::Core::CoreWindow::GetForCurrentThread();
    Opal::Services::GamepadService::Instance->Initialize(coreWindow, ContentFrame, SearchBox);

    // Cast-forwarding is now handled inside the constructor's SongChanged subscription (fix 2.2)

    if (AnalyticsInfo::VersionInfo->DeviceFamily == "Windows.Xbox")
    {
        VisualStateManager::GoToState(this, "XboxState", false);
        MainNavigationView->AutoSuggestBox = nullptr;
    }
    
    // Initialize CastingService if authenticated (deferred from App::OnLaunched)
    if (NavidromeService::Instance->IsAuthenticated()) {
        CastingService::Instance->StartListening();
    }

    // Initial routing logic
    auto localSettings = Windows::Storage::ApplicationData::Current->LocalSettings;
    bool stateRestored = false;
    if (localSettings->Values->HasKey("InnerNavigationState"))
    {
        Platform::String^ savedState = dynamic_cast<Platform::String^>(localSettings->Values->Lookup("InnerNavigationState"));
        if (savedState != nullptr)
        {
            ContentFrame->SetNavigationState(savedState);
            stateRestored = true;
        }
        localSettings->Values->Remove("InnerNavigationState");
    }

    if (!stateRestored)
    {
        if (NavidromeService::Instance->IsAuthenticated())
        {
            ContentFrame->Navigate(Windows::UI::Xaml::Interop::TypeName(LibraryPage::typeid));
        }
        else
        {
            ContentFrame->Navigate(Windows::UI::Xaml::Interop::TypeName(LoginPage::typeid));
        }
    }

    UpdateMenuVisibility();
    
    // Subscribe to playlist changes FIRST so the VectorChanged handler fires
    // when LoadPlaylistsAsync completes, triggering the sidebar rebuild with data.
    auto vm = PlaylistsVM;
    vm->Playlists->VectorChanged += ref new VectorChangedEventHandler<PlaylistModel^>([this](IObservableVector<PlaylistModel^>^ sender, IVectorChangedEventArgs^ args) {
        auto self = this;
        this->Dispatcher->RunAsync(CoreDispatcherPriority::Normal, ref new DispatchedHandler([self]() {
            self->UpdateSidebarPlaylists();
        }));
    });
    vm->LoadPlaylistsAsync();
}

void MainPage::OnMenuItemInvoked(Microsoft::UI::Xaml::Controls::NavigationView^ sender, Microsoft::UI::Xaml::Controls::NavigationViewItemInvokedEventArgs^ args)
{
    if (args->IsSettingsInvoked)
    {
        ContentFrame->Navigate(Windows::UI::Xaml::Interop::TypeName(SettingsPage::typeid));
        return;
    }

    // On some platforms/versions, InvokedItemContainer can be null for footer items
    auto container = args->InvokedItemContainer;
    Platform::String^ tag = nullptr;

    if (container != nullptr && container->Tag != nullptr) {
        tag = container->Tag->ToString();
    }
    else {
        // Fallback to checking the invoked item content if it's a string
        auto invokedString = dynamic_cast<Platform::String^>(args->InvokedItem);
        if (invokedString != nullptr) tag = invokedString;
    }

    if (tag == nullptr) return;

    if (tag == "Search")
    {
        ToggleXboxSearch();
    }
    else if (tag == "Settings")
    {
        ContentFrame->Navigate(Windows::UI::Xaml::Interop::TypeName(SettingsPage::typeid));
    }
    else if (tag == "Library")
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
    if (VolumeText != nullptr) {
        VolumeText->Text = ((int)e->NewValue).ToString();
    }
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
    // Start active discovery now that the user is looking for devices
    CastingService::Instance->StartDiscovery();

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
    try {
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
    }
    catch (...) {
        // Guard against ItemsRepeater BringIntoView exceptions during initial layout
        DebugLogger::Instance->Log("MainPage", "Navigation selection recovered from layout exception");
    }

    // If we just logged in or authenticated, ensure playlists are loaded
    if (NavidromeService::Instance->IsAuthenticated() && PlaylistsVM->Playlists->Size == 0 && !PlaylistsVM->IsLoading) {
        PlaylistsVM->LoadPlaylistsAsync();
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
            MainNavigationView->IsPaneToggleButtonVisible = false;
            MainNavigationView->IsSettingsVisible = true;
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
    create_task(NavidromeService::Instance->SearchAsync(query, 10, 0)).then([self, sender](String^ jsonStr) {
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

void MainPage::OnViewPlaylistsClick(Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e)
{
    ContentFrame->Navigate(Windows::UI::Xaml::Interop::TypeName(PlaylistsPage::typeid));
}

void MainPage::UpdateSidebarPlaylists()
{
    // Debounce: wait 300ms of inactivity before rebuilding
    if (_sidebarDebounceTimer == nullptr) {
        _sidebarDebounceTimer = ref new DispatcherTimer();
        TimeSpan ts; ts.Duration = 3000000LL; // 300ms
        _sidebarDebounceTimer->Interval = ts;
        auto self = this;
        _sidebarDebounceTimer->Tick += ref new EventHandler<Object^>([self](Object^, Object^) {
            self->_sidebarDebounceTimer->Stop();
            self->RebuildSidebarPlaylistsInternal();
        });
    }
    _sidebarDebounceTimer->Stop();
    _sidebarDebounceTimer->Start();
}

void MainPage::RebuildSidebarPlaylistsInternal()
{
    try {
        auto savedSelection = MainNavigationView->SelectedItem;
        bool selectionWasDynamic = false;
        Platform::String^ savedTag = nullptr;

        // Check if current selection is one of the playlist sub-items
        if (savedSelection != nullptr) {
            auto navItem = dynamic_cast<Microsoft::UI::Xaml::Controls::NavigationViewItem^>(savedSelection);
            if (navItem != nullptr && navItem->Tag != nullptr) {
                for (unsigned int i = 0; i < PlaylistsNavItem->MenuItems->Size; i++) {
                    if (PlaylistsNavItem->MenuItems->GetAt(i) == savedSelection) {
                        selectionWasDynamic = true;
                        savedTag = navItem->Tag->ToString();
                        break;
                    }
                }
            }
        }

        // Clear orphaned top-level playlist items that were added after PlaylistsNavItem in previous versions
        while (MainNavigationView->MenuItems->Size > 8) {
            MainNavigationView->MenuItems->RemoveAt(8);
        }

        auto vm = ViewModels::PlaylistsViewModel::Instance;
        auto newItems = ref new Platform::Collections::Vector<Platform::Object^>();
        Platform::Object^ newSelectionMatch = nullptr;

        for (unsigned int i = 0; i < vm->Playlists->Size; i++)
        {
            auto p = vm->Playlists->GetAt(i);
            auto item = ref new Microsoft::UI::Xaml::Controls::NavigationViewItem();
            item->Tag = p->Id;

            // Layout: [Thumbnail] [Text Stack]
            auto rootGrid = ref new Grid();
            rootGrid->Margin = Thickness(-28, 4, 0, 4); // Remove hierarchical indentation
            auto col1 = ref new ColumnDefinition();
            col1->Width = GridLengthHelper::FromPixels(52);
            auto col2 = ref new ColumnDefinition();
            col2->Width = { 1.0, GridUnitType::Star };
            rootGrid->ColumnDefinitions->Append(col1);
            rootGrid->ColumnDefinitions->Append(col2);

            auto thumbBorder = ref new Border();
            thumbBorder->Width = 40;
            thumbBorder->Height = 40;
            thumbBorder->CornerRadius = CornerRadiusHelper::FromUniformRadius(6);
            thumbBorder->VerticalAlignment = Windows::UI::Xaml::VerticalAlignment::Center;
            thumbBorder->Background = dynamic_cast<Windows::UI::Xaml::Media::Brush^>(Windows::UI::Xaml::Application::Current->Resources->Lookup("AppControlBackgroundBrush"));

            auto thumb = ref new Opal::UI::Controls::ThumbnailView();
            thumb->SourceUrl = p->CoverUrl;
            thumb->DecodeWidth = 80;
            thumb->ThumbnailCornerRadius = CornerRadiusHelper::FromUniformRadius(6);
            thumbBorder->Child = thumb;
            
            rootGrid->Children->Append(thumbBorder);
            Grid::SetColumn(thumbBorder, 0);

            auto stack = ref new StackPanel();
            stack->Spacing = 0;
            stack->VerticalAlignment = Windows::UI::Xaml::VerticalAlignment::Center;

            auto title = ref new TextBlock();
            title->Text = p->Name;
            title->FontSize = 14;
            title->FontWeight = Windows::UI::Text::FontWeights::SemiBold;
            stack->Children->Append(title);

            auto info = ref new StackPanel();
            info->Orientation = Orientation::Horizontal;
            info->Spacing = 4;
            info->Opacity = 0.5;

            auto noteIcon = ref new FontIcon();
            noteIcon->FontFamily = ref new Windows::UI::Xaml::Media::FontFamily("Segoe MDL2 Assets");
            noteIcon->Glyph = L"\uE8D6"; // Music Note
            noteIcon->FontSize = 10;
            noteIcon->VerticalAlignment = Windows::UI::Xaml::VerticalAlignment::Center;
            info->Children->Append(noteIcon);

            auto count = ref new TextBlock();
            count->Text = p->SongCount.ToString();
            count->FontSize = 11;
            count->VerticalAlignment = Windows::UI::Xaml::VerticalAlignment::Center;
            info->Children->Append(count);

            auto clockIcon = ref new FontIcon();
            clockIcon->FontFamily = ref new Windows::UI::Xaml::Media::FontFamily("Segoe MDL2 Assets");
            clockIcon->Glyph = L"\uE916"; // Clock
            clockIcon->FontSize = 10;
            clockIcon->Margin = Thickness(4, 0, 0, 0);
            clockIcon->VerticalAlignment = Windows::UI::Xaml::VerticalAlignment::Center;
            info->Children->Append(clockIcon);

            auto duration = ref new TextBlock();
            duration->Text = p->Duration;
            duration->FontSize = 11;
            duration->VerticalAlignment = Windows::UI::Xaml::VerticalAlignment::Center;
            info->Children->Append(duration);

            stack->Children->Append(info);
            rootGrid->Children->Append(stack);
            Grid::SetColumn(stack, 1);

            item->Content = rootGrid;
            item->Icon = nullptr; // Using custom thumbnail instead of icon

            newItems->Append(item);

            // Track if this newly created item matches the old selection
            if (savedTag != nullptr && p->Id == savedTag) {
                newSelectionMatch = item;
            }
        }

        PlaylistsNavItem->MenuItemsSource = newItems;

        // Restore selection
        if (newSelectionMatch != nullptr) {
            MainNavigationView->SelectedItem = newSelectionMatch;
        } else if (savedSelection != nullptr) {
            MainNavigationView->SelectedItem = savedSelection;
        }

        // Force a layout refresh for hierarchical items by toggling expansion
        // This fixes a known WinUI 2 bug where items don't appear in an already-expanded parent
        // Always set expanded so the dropdown never starts collapsed, even on first load
        PlaylistsNavItem->IsExpanded = false;
        PlaylistsNavItem->IsExpanded = true;

        // Trigger a menu visibility update to force a layout pass on the NavigationView
        UpdateMenuVisibility();
    }
    catch (...) {
        DebugLogger::Instance->Log("MainPage", "Sidebar update recovered from layout exception");
    }
}

// 3.2: Shuffle Toggle
void MainPage::OnShuffleClicked(Object^ sender, RoutedEventArgs^ e) {
    auto pb = PlaybackService::Instance;
    bool newState = !pb->IsShuffleEnabled;
    pb->IsShuffleEnabled = newState;
    if (newState) pb->Shuffle(); // immediately reorder the current queue

    // Visual feedback
    auto accentBrush = dynamic_cast<Windows::UI::Xaml::Media::Brush^>(
        Windows::UI::Xaml::Application::Current->Resources->Lookup("SystemControlHighlightAccentBrush"));
    auto dimBrush = dynamic_cast<Windows::UI::Xaml::Media::Brush^>(
        Windows::UI::Xaml::Application::Current->Resources->Lookup("AppTextForegroundBrush"));
    ShuffleIcon->Foreground = newState ? accentBrush : dimBrush;
    ShuffleIcon->Opacity = newState ? 1.0 : 0.5;
}

// 3.2: Repeat Cycle (Off → All → One → Off)
void MainPage::OnRepeatClicked(Object^ sender, RoutedEventArgs^ e) {
    auto pb = PlaybackService::Instance;
    int next = (pb->RepeatMode + 1) % 3;
    pb->RepeatMode = next;

    // Glyph: &#xE8EE; = Repeat, &#xE8ED; = RepeatOne
    auto accentBrush = dynamic_cast<Windows::UI::Xaml::Media::Brush^>(
        Windows::UI::Xaml::Application::Current->Resources->Lookup("SystemControlHighlightAccentBrush"));
    auto dimBrush = dynamic_cast<Windows::UI::Xaml::Media::Brush^>(
        Windows::UI::Xaml::Application::Current->Resources->Lookup("AppTextForegroundBrush"));

    if (next == 0) {
        RepeatIcon->Glyph = L"\uE8EE";
        RepeatIcon->Foreground = dimBrush;
        RepeatIcon->Opacity = 0.5;
    } else if (next == 1) {
        RepeatIcon->Glyph = L"\uE8EE";
        RepeatIcon->Foreground = accentBrush;
        RepeatIcon->Opacity = 1.0;
    } else {
        RepeatIcon->Glyph = L"\uE8ED";
        RepeatIcon->Foreground = accentBrush;
        RepeatIcon->Opacity = 1.0;
    }
}

// 3.4: Sleep Timer — start countdown
void MainPage::OnSleepTimerClicked(Object^ sender, RoutedEventArgs^ e) {
    auto item = dynamic_cast<Windows::UI::Xaml::Controls::MenuFlyoutItem^>(sender);
    if (item == nullptr) return;
    int minutes = _wtoi(item->Tag->ToString()->Data());
    _sleepRemainingSeconds = minutes * 60;

    if (_sleepTimer != nullptr) _sleepTimer->Stop();
    _sleepTimer = ref new Windows::UI::Xaml::DispatcherTimer();
    TimeSpan ts; ts.Duration = 1 * 10000000LL; // 1 second
    _sleepTimer->Interval = ts;
    auto self = this;
    _sleepTimer->Tick += ref new Windows::Foundation::EventHandler<Object^>([self](Object^, Object^) {
        self->_sleepRemainingSeconds--;
        self->UpdateSleepTimerLabel();
        if (self->_sleepRemainingSeconds <= 0) {
            self->_sleepTimer->Stop();
            PlaybackService::Instance->Player->Pause();
            self->SleepTimerIcon->Glyph = L"\uE708";
            ToolTipService::SetToolTip(self->SleepTimerBtn, "Sleep Timer");
        }
    });
    _sleepTimer->Start();
    UpdateSleepTimerLabel();
}

void MainPage::UpdateSleepTimerLabel() {
    int m = _sleepRemainingSeconds / 60;
    wchar_t buf[32];
    swprintf_s(buf, L"\uE708 %dm", m > 0 ? m : 1);
    SleepTimerIcon->Glyph = L"\uE708";
    ToolTipService::SetToolTip(SleepTimerBtn, ref new Platform::String(buf));
}

// 3.4: Cancel sleep timer
void MainPage::OnSleepTimerCancelClicked(Object^ sender, RoutedEventArgs^ e) {
    if (_sleepTimer != nullptr) { _sleepTimer->Stop(); _sleepTimer = nullptr; }
    _sleepRemainingSeconds = 0;
    SleepTimerIcon->Glyph = L"\uE708";
    ToolTipService::SetToolTip(SleepTimerBtn, "Sleep Timer");
}


void MainPage::InitializeComposition()
{
    _compositor = ElementCompositionPreview::GetElementVisual(this)->Compositor;
    
    auto blurEffect = ref new Microsoft::Graphics::Canvas::Effects::GaussianBlurEffect();
    blurEffect->Name = "Blur";
    blurEffect->BlurAmount = 60.0f; 
    blurEffect->BorderMode = Microsoft::Graphics::Canvas::Effects::EffectBorderMode::Hard;
    blurEffect->Source = ref new CompositionEffectSourceParameter("source");

    auto factory = _compositor->CreateEffectFactory(blurEffect);
    auto effectBrush = factory->CreateBrush();
    
    // Use BackdropBrush to blur what is BEHIND the player bar
    auto backdropBrush = _compositor->CreateBackdropBrush();
    effectBrush->SetSourceParameter("source", backdropBrush);
    
    _blurVisual = _compositor->CreateSpriteVisual();
    
    Windows::Foundation::Numerics::float2 sizeAdjustment;
    sizeAdjustment.x = 1.0f;
    sizeAdjustment.y = 1.0f;
    _blurVisual->RelativeSizeAdjustment = sizeAdjustment;
    _blurVisual->Brush = effectBrush;

    ElementCompositionPreview::SetElementChildVisual(PlayerBarBackgroundCanvas, _blurVisual);
}
 
void MainPage::OnCloseSearchClick(Object^ sender, RoutedEventArgs^ e) {
    XboxSearchOverlay->Visibility = Windows::UI::Xaml::Visibility::Collapsed;
}

void MainPage::ToggleXboxSearch() {
    if (XboxSearchOverlay->Visibility == Windows::UI::Xaml::Visibility::Visible) {
        XboxSearchOverlay->Visibility = Windows::UI::Xaml::Visibility::Collapsed;
    } else {
        XboxSearchOverlay->Visibility = Windows::UI::Xaml::Visibility::Visible;
        XboxSearchBox->Focus(Windows::UI::Xaml::FocusState::Programmatic);
    }
}

void MainPage::OnTitleSizeChanged(Object^ sender, SizeChangedEventArgs^ e)
{
    // 1. Reset
    if (_marqueeStoryboard != nullptr) {
        _marqueeStoryboard->Stop();
        _marqueeStoryboard = nullptr;
    }
    TitleScrollTransform->X = 0;

    // 2. Update Clip
    TitleClip->Rect = Windows::Foundation::Rect(0, 0, (float)TitleContainer->ActualWidth, (float)TitleContainer->ActualHeight);

    // 3. Detect if cut off
    double textWidth = NowPlayingTitle->ActualWidth;
    double containerWidth = TitleContainer->ActualWidth;

    if (textWidth > containerWidth && containerWidth > 0) {
        // Start marquee
        _marqueeStoryboard = ref new Storyboard();

        auto animation = ref new Windows::UI::Xaml::Media::Animation::DoubleAnimationUsingKeyFrames();

        // Speed: ~30 pixels per second (gentle scroll)
        double scrollDistance = (textWidth - containerWidth + 40);
        double durationSeconds = scrollDistance / 30.0;
        
        auto kf1 = ref new Windows::UI::Xaml::Media::Animation::DiscreteDoubleKeyFrame();
        kf1->KeyTime = Windows::UI::Xaml::Media::Animation::KeyTimeHelper::FromTimeSpan(Windows::Foundation::TimeSpan{0});
        kf1->Value = 0.0;
        
        auto kf2 = ref new Windows::UI::Xaml::Media::Animation::DiscreteDoubleKeyFrame();
        kf2->KeyTime = Windows::UI::Xaml::Media::Animation::KeyTimeHelper::FromTimeSpan(Windows::Foundation::TimeSpan{25000000}); // 2.5s pause at start
        kf2->Value = 0.0;
        
        auto kf3 = ref new Windows::UI::Xaml::Media::Animation::LinearDoubleKeyFrame();
        kf3->KeyTime = Windows::UI::Xaml::Media::Animation::KeyTimeHelper::FromTimeSpan(Windows::Foundation::TimeSpan{25000000 + (long long)(durationSeconds * 10000000)});
        kf3->Value = -scrollDistance;

        animation->KeyFrames->Append(kf1);
        animation->KeyFrames->Append(kf2);
        animation->KeyFrames->Append(kf3);

        _marqueeStoryboard->Children->Append(animation);
        Storyboard::SetTarget(animation, TitleScrollTransform);
        Storyboard::SetTargetProperty(animation, "X");

        animation->RepeatBehavior = Windows::UI::Xaml::Media::Animation::RepeatBehaviorHelper::Forever;

        _marqueeStoryboard->Begin();
    }
}
