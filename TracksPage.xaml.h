#pragma once

#include "Generated Files/TracksPage.g.h"
#include "Models/SharedModels.h"

namespace Opal
{
    public ref class TracksPage sealed
    {
    public:
        TracksPage();
        void OnFilterOrSortChanged(Platform::Object^ sender, Platform::Object^ e);
        void OnTrackClicked(Platform::Object^ sender, Windows::UI::Xaml::Controls::ItemClickEventArgs^ e);
        void OnPlayAllClicked(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
        void OnTrackFavoriteClicked(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
        void OnTrackRatingChanged(Microsoft::UI::Xaml::Controls::RatingControl^ sender, Platform::Object^ args);
        void OnFavoriteIconLoaded(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
        void OnTrackContextOpening(Platform::Object^ sender, Platform::Object^ e);
        void OnPlayMenuClicked(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
        void OnAddQueueMenuClicked(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
        void OnRemoveFromPlaylistClicked(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
        void OnMoreButtonClicked(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
        void OnLoadMoreClicked(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
        void OnFavoritesFilterToggled(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e); // 3.3

    protected:
        virtual void OnNavigatedTo(Windows::UI::Xaml::Navigation::NavigationEventArgs^ e) override;

    private:
        void LoadTracks();

        Platform::Collections::Vector<Song^>^ _allTracks;
        int _offset;
        int _totalCount;
        bool _isLoadingMore;
        bool _showFavoritesOnly; // 3.3
    };
}
