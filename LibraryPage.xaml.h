#pragma once

#include <set>
#include <string>
#include "LibraryPage.g.h"
#include "Models/SharedModels.h"
#include "ViewModels/LibraryViewModel.h"
#include "ViewModels/LyricsViewModel.h"
#include "Services/SettingsService.h"
#include "Services/NavidromeService.h"
#include "Services/LyricsService.h"
#include "Services/PlaybackService.h"

namespace Opal
{
    public ref class LibraryPage sealed
    {
    public:
        LibraryPage();
        
        property Opal::ViewModels::LyricsViewModel^ LyricsVM {
            Opal::ViewModels::LyricsViewModel^ get() { return Opal::ViewModels::LyricsViewModel::Instance; }
        }

        property Opal::ViewModels::LibraryViewModel^ LibraryVM {
            Opal::ViewModels::LibraryViewModel^ get() { return Opal::ViewModels::LibraryViewModel::Instance; }
        }
        
        property Windows::Foundation::Collections::IVector<Song^>^ PlaybackQueue {
            Windows::Foundation::Collections::IVector<Song^>^ get() { return PlaybackService::Instance->Queue; }
        }

        property Windows::Foundation::Collections::IObservableVector<Song^>^ UpcomingQueue {
            Windows::Foundation::Collections::IObservableVector<Song^>^ get() { return _upcomingQueue; }
        }

        void LoadHomePage();
        void ToggleFullPlayer();
        void ShowPlayer(bool showLyrics);
        void OnSyncThumbnailsClicked(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
        void Search(Platform::String^ query);

        property bool IsFullPlayerActive {
            bool get();
        }

        void OnTrackClicked(Platform::Object^ sender, Windows::UI::Xaml::Controls::ItemClickEventArgs^ e);
        void OnFavoriteIconLoaded(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
        void OnTrackFavoriteClicked(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
        void OnTrackRatingChanged(Microsoft::UI::Xaml::Controls::RatingControl^ sender, Platform::Object^ args);
        void OnPlayAlbumClicked(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
        void OnCollapseClicked(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
        void OnSpotlightTapped(Platform::Object^ sender, Windows::UI::Xaml::Input::TappedRoutedEventArgs^ e);
        void OnCarouselPrev(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
        void OnCarouselNext(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
        void OnHomeClicked(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
        void OnArtistClicked(Platform::Object^ sender, Windows::UI::Xaml::Controls::ItemClickEventArgs^ e);
        void OnAlbumClicked(Platform::Object^ sender, Windows::UI::Xaml::Controls::ItemClickEventArgs^ e);
        void OnGenericAlbumClicked(Platform::Object^ sender, Windows::UI::Xaml::Controls::ItemClickEventArgs^ e);
        void OnGenreClicked(Platform::Object^ sender, Windows::UI::Xaml::Controls::ItemClickEventArgs^ e);
        void OnBackClicked(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
        void OnTrackContextOpening(Platform::Object^ sender, Platform::Object^ e);
        void OnPlayMenuClicked(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
        void OnAddQueueMenuClicked(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
        void OnMoreButtonClicked(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
        void OnQueueItemClick(Platform::Object^ sender, Windows::UI::Xaml::Controls::ItemClickEventArgs^ e);
        void OnQueueDragItemsCompleted(Windows::UI::Xaml::Controls::ListViewBase^ sender, Windows::UI::Xaml::Controls::DragItemsCompletedEventArgs^ args);
        void OnRemoveFromQueueClick(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
        void OnClearQueueClick(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);

    protected:
        virtual void OnNavigatedTo(Windows::UI::Xaml::Navigation::NavigationEventArgs^ e) override;

    private:
        int _lastLyricIndex;
        void UpdateLyricsHighlight();
        void UpdateUpcomingQueue();

        void PlaySong(unsigned int index);
        void NextSong();
        void PreviousSong();
        void ShuffleQueue();
        void LoadLyrics(Song^ song);
        void OnLyricsToggleClicked(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
        void SyncQueueToCastingTarget();


        void LoadArtistPage(Platform::String^ artistId);
        void LoadAlbumPage(Platform::String^ albumId);
        void LoadGenreAlbums(Platform::String^ genre);

        Platform::Collections::Vector<ArtistModel^>^ _artists;
        Platform::Collections::Vector<AlbumID3^>^ _albums;
        Platform::Collections::Vector<Song^>^ _albumSongs;
        Platform::Collections::Vector<DiscGroup^>^ _albumDiscGroups;
        Platform::Collections::Vector<Song^>^ _upcomingQueue;

        void OnPageLoaded(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
        
        // UI Events
        void OnDisconnectClicked(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
        
        void OnCarouselScrollLeft(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
        void OnCarouselScrollRight(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
        void OnRandomTrackClicked(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
        void OnRandomAlbumClicked(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
    };
}
