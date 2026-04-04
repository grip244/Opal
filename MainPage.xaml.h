#pragma once

#include "MainPage.g.h"

namespace Opal
{
    public ref class MainPage sealed
    {
    public:
        MainPage();

    private:
        void OnMenuItemInvoked(Microsoft::UI::Xaml::Controls::NavigationView^ sender, Microsoft::UI::Xaml::Controls::NavigationViewItemInvokedEventArgs^ args);
        void OnNavigated(Platform::Object^ sender, Windows::UI::Xaml::Navigation::NavigationEventArgs^ e);
        void OnPageLoaded(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);

        void OnPlayPauseClicked(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
        void OnNextClicked(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
        void OnPreviousClicked(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
        void OnShuffleClicked(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
        void OnVolumeChanged(Platform::Object^ sender, Windows::UI::Xaml::Controls::Primitives::RangeBaseValueChangedEventArgs^ e);
        void OnLyricsToggleClicked(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
        void OnProgressSeek(Platform::Object^ sender, Windows::UI::Xaml::Input::PointerRoutedEventArgs^ e);
        void OnThumbnailClicked(Platform::Object^ sender, Windows::UI::Xaml::Input::TappedRoutedEventArgs^ e);
        void OnThumbnailPointerEntered(Platform::Object^ sender, Windows::UI::Xaml::Input::PointerRoutedEventArgs^ e);
        void OnThumbnailPointerExited(Platform::Object^ sender, Windows::UI::Xaml::Input::PointerRoutedEventArgs^ e);
        void OnSearchEntered(Windows::UI::Xaml::Controls::AutoSuggestBox^ sender, Windows::UI::Xaml::Controls::AutoSuggestBoxQuerySubmittedEventArgs^ args);
    };
}
