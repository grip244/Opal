#pragma once

#include "Generated Files/PlaylistDetailsPage.g.h"
#include "Models/SharedModels.h"

namespace Opal
{
    public ref class PlaylistDetailsPage sealed
    {
    public:
        PlaylistDetailsPage();

        property Windows::Foundation::Collections::IVector<Song^>^ Songs {
            Windows::Foundation::Collections::IVector<Song^>^ get() { return _songs; }
        }

    protected:
        virtual void OnNavigatedTo(Windows::UI::Xaml::Navigation::NavigationEventArgs^ e) override;

    private:
        Platform::String^ _playlistId;
        Platform::Collections::Vector<Song^>^ _songs;

        void LoadPlaylistTracks();
        void OnBackClicked(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
        void OnTrackClicked(Platform::Object^ sender, Windows::UI::Xaml::Controls::ItemClickEventArgs^ e);
        void OnPlayAllClicked(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
        void OnFavoriteClicked(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
        void OnDragItemsCompleted(Windows::UI::Xaml::Controls::ListViewBase^ sender, Windows::UI::Xaml::Controls::DragItemsCompletedEventArgs^ args);
        void OnTrackContextOpening(Platform::Object^ sender, Platform::Object^ e);
        void OnPlayMenuClicked(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
        void OnAddQueueMenuClicked(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
        void OnRemoveMenuClicked(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
    };
}
