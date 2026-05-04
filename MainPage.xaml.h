#pragma once

#include "Generated Files/MainPage.g.h"

namespace Opal
{
    namespace ViewModels { ref class PlaylistsViewModel; }

    public ref class MainPage sealed
    {
    public:
        MainPage();
        Windows::UI::Xaml::Controls::Frame^ GetNavigationFrame();
        
        property bool IsFullPlayerActive {
            bool get();
        }

        void UpdateSidebarPlaylists();
        property ViewModels::PlaylistsViewModel^ PlaylistsVM {
            ViewModels::PlaylistsViewModel^ get();
        }

        void OnNewPlaylistClick(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
        void OnViewPlaylistsClick(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
        void OnMenuItemInvoked(Microsoft::UI::Xaml::Controls::NavigationView^ sender, Microsoft::UI::Xaml::Controls::NavigationViewItemInvokedEventArgs^ args);
        void OnFavoriteClicked(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
        void OnGlobalRatingChanged(Microsoft::UI::Xaml::Controls::RatingControl^ sender, Platform::Object^ args);
        void OnSearchTextChanged(Windows::UI::Xaml::Controls::AutoSuggestBox^ sender, Windows::UI::Xaml::Controls::AutoSuggestBoxTextChangedEventArgs^ args);
        void OnSearchSuggestionChosen(Windows::UI::Xaml::Controls::AutoSuggestBox^ sender, Windows::UI::Xaml::Controls::AutoSuggestBoxSuggestionChosenEventArgs^ args);
        void OnSearchEntered(Windows::UI::Xaml::Controls::AutoSuggestBox^ sender, Windows::UI::Xaml::Controls::AutoSuggestBoxQuerySubmittedEventArgs^ args);
        void OnPlayPauseClicked(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
        void OnNextClicked(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
        void OnPreviousClicked(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);

        void OnVolumeChanged(Platform::Object^ sender, Windows::UI::Xaml::Controls::Primitives::RangeBaseValueChangedEventArgs^ e);
        void OnLyricsToggleClicked(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
        void OnProgressSeek(Platform::Object^ sender, Windows::UI::Xaml::Input::PointerRoutedEventArgs^ e);
        void OnThumbnailClicked(Platform::Object^ sender, Windows::UI::Xaml::Input::TappedRoutedEventArgs^ e);
        void OnThumbnailPointerEntered(Platform::Object^ sender, Windows::UI::Xaml::Input::PointerRoutedEventArgs^ e);
        void OnThumbnailPointerExited(Platform::Object^ sender, Windows::UI::Xaml::Input::PointerRoutedEventArgs^ e);
        void OnCastFlyoutOpened(Platform::Object^ sender, Platform::Object^ e);
        void OnCastNowClicked(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
        void OnDisconnectClicked(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
        void OnDeviceSelected(Platform::Object^ sender, Windows::UI::Xaml::Controls::SelectionChangedEventArgs^ e);


    private:
        void OnNavigated(Platform::Object^ sender, Windows::UI::Xaml::Navigation::NavigationEventArgs^ e);
        void OnPageLoaded(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);

        void SyncQueue();
        void UpdateMenuVisibility();

        // 3.2 Shuffle / Repeat
        void OnShuffleClicked(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
        void OnRepeatClicked(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);

        // 3.4 Sleep Timer
        void OnSleepTimerClicked(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
        void OnSleepTimerCancelClicked(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
        void UpdateSleepTimerLabel();
        Windows::UI::Xaml::DispatcherTimer^ _sleepTimer;
        int _sleepRemainingSeconds;

    };
}
