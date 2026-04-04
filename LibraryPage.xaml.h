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
        void Search(Platform::String^ query);

        property bool IsFullPlayerActive {
            bool get();
        }

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
        void OnQueueItemClick(Platform::Object^ sender, Windows::UI::Xaml::Controls::ItemClickEventArgs^ e);


        void LoadArtistPage(Platform::String^ artistId);
        void LoadAlbumPage(Platform::String^ albumId);

        Platform::Collections::Vector<ArtistModel^>^ _artists;
        Platform::Collections::Vector<AlbumModel^>^ _albums;
        Platform::Collections::Vector<Song^>^ _albumSongs;
        Platform::Collections::Vector<DiscGroup^>^ _albumDiscGroups;
        Platform::Collections::Vector<Song^>^ _upcomingQueue;

        void OnPageLoaded(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
        
        // UI Events
        void OnDisconnectClicked(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
        
        void OnCarouselScrollLeft(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
        void OnCarouselScrollRight(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);

        void OnHomeClicked(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
        void OnRandomTrackClicked(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
        void OnRandomAlbumClicked(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
        void OnArtistClicked(Platform::Object^ sender, Windows::UI::Xaml::Controls::ItemClickEventArgs^ e);
        void OnAlbumClicked(Platform::Object^ sender, Windows::UI::Xaml::Controls::ItemClickEventArgs^ e);
        void OnGenericAlbumClicked(Platform::Object^ sender, Windows::UI::Xaml::Controls::ItemClickEventArgs^ e);

        void OnTrackClicked(Platform::Object^ sender, Windows::UI::Xaml::Controls::ItemClickEventArgs^ e);
        void OnPlayAlbumClicked(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
        void OnCollapseClicked(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
        void OnSpotlightTapped(Platform::Object^ sender, Windows::UI::Xaml::Input::TappedRoutedEventArgs^ e);
        void OnCarouselPrev(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
        void OnCarouselNext(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
    };
}
