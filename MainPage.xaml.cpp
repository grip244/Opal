#include "pch.h"
#include "MainPage.xaml.h"
#include "LoginPage.xaml.h"
#include "LibraryPage.xaml.h"
#include "ArtistsPage.xaml.h"
#include "AlbumsPage.xaml.h"
#include "Services/NavidromeService.h"
#include "Services/PlaybackService.h"

using namespace Opal;
using namespace Platform;
using namespace Windows::UI::Xaml;
using namespace Windows::UI::Xaml::Controls;
using namespace Windows::UI::Xaml::Navigation;
using namespace Windows::UI::Xaml::Interop;
using namespace Microsoft::UI::Xaml::Controls;
using namespace Windows::Foundation;
using namespace Windows::UI::Core;
using namespace Windows::System::Profile;

MainPage::MainPage()
{
    InitializeComponent();
    this->Loaded += ref new RoutedEventHandler(this, &MainPage::OnPageLoaded);

    PlaybackService::Instance->SongChanged += ref new SongChangedHandler([this](PlaybackService^ sender, Song^ song) {
        this->Dispatcher->RunAsync(CoreDispatcherPriority::Normal, ref new DispatchedHandler([this, song]() {
            NowPlayingTitle->Text = song->Title;
            NowPlayingArtist->Text = song->Artist;
            MiniThumbnail->Source = song->CoverArt;
            GlobalPlayerBar->Visibility = Windows::UI::Xaml::Visibility::Visible;
        }));
    });

    auto timer = ref new DispatcherTimer();
    TimeSpan ts; ts.Duration = 500 * 10000LL;
    timer->Interval = ts;
    timer->Tick += ref new Windows::Foundation::EventHandler<Object^>([this](Object^ sender, Object^ args) {
        auto pb = PlaybackService::Instance;
        auto session = pb->Player->PlaybackSession;
        if (session->PlaybackState != Windows::Media::Playback::MediaPlaybackState::None) {
            GlobalPlayerBar->Visibility = Windows::UI::Xaml::Visibility::Visible;
            ProgressSlider->Maximum = (double)session->NaturalDuration.Duration;
            ProgressSlider->Value = (double)session->Position.Duration;
            
            int curSecs = (int)(session->Position.Duration / 10000000LL);
            int durSecs = (int)(session->NaturalDuration.Duration / 10000000LL);
            wchar_t cBuf[16], dBuf[16];
            swprintf_s(cBuf, L"%d:%02d", curSecs / 60, curSecs % 60);
            swprintf_s(dBuf, L"%d:%02d", durSecs / 60, durSecs % 60);
            CurrentTimeText->Text = ref new String(cBuf);
            TotalTimeText->Text = ref new String(dBuf);
            
            auto song = pb->CurrentSong;
            if (song != nullptr) {
                NowPlayingTitle->Text = song->Title;
                NowPlayingArtist->Text = song->Artist;
                if (MiniThumbnail->Source != song->CoverArt) MiniThumbnail->Source = song->CoverArt;
            }
            
            if (session->PlaybackState == Windows::Media::Playback::MediaPlaybackState::Playing) PlayPauseIcon->Symbol = Symbol::Pause;
            else PlayPauseIcon->Symbol = Symbol::Play;
        }
    });
    timer->Start();
}

void MainPage::OnPageLoaded(Object^ sender, RoutedEventArgs^ e)
{
    if (AnalyticsInfo::VersionInfo->DeviceFamily == "Windows.Xbox")
    {
        VisualStateManager::GoToState(this, "XboxState", false);
    }
    // Initial routing logic
    if (NavidromeService::Instance->IsAuthenticated())
    {
        ContentFrame->Navigate(Windows::UI::Xaml::Interop::TypeName(LibraryPage::typeid));
    }
    else
    {
        ContentFrame->Navigate(Windows::UI::Xaml::Interop::TypeName(LoginPage::typeid));
    }
}

void MainPage::OnMenuItemInvoked(Microsoft::UI::Xaml::Controls::NavigationView^ sender, Microsoft::UI::Xaml::Controls::NavigationViewItemInvokedEventArgs^ args)
{
    if (args->IsSettingsInvoked)
    {
        // Settings navigation if needed
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
    }
}

void MainPage::OnPlayPauseClicked(Object^ sender, RoutedEventArgs^ e) {
    auto pb = PlaybackService::Instance;
    if (pb->Player->PlaybackSession->PlaybackState == Windows::Media::Playback::MediaPlaybackState::Playing) pb->Player->Pause();
    else pb->Player->Play();
}

void MainPage::OnNextClicked(Object^ sender, RoutedEventArgs^ e) { PlaybackService::Instance->NextSong(); }
void MainPage::OnPreviousClicked(Object^ sender, RoutedEventArgs^ e) { PlaybackService::Instance->PreviousSong(); }
void MainPage::OnShuffleClicked(Object^ sender, RoutedEventArgs^ e) { PlaybackService::Instance->Shuffle(); }
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
    TimeSpan ts; ts.Duration = (long long)ProgressSlider->Value; 
    pb->Player->PlaybackSession->Position = ts; 
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
}
void MainPage::OnSearchEntered(AutoSuggestBox^ sender, AutoSuggestBoxQuerySubmittedEventArgs^ args)
{
    if (args->QueryText != nullptr && args->QueryText->Length() > 0)
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
