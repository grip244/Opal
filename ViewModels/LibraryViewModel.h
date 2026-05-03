#pragma once

#include <collection.h>
#include "Models/SharedModels.h"

namespace Opal
{
    namespace ViewModels
    {
        public ref class LibraryViewModel sealed
        {
        public:
            static property LibraryViewModel^ Instance
            {
                LibraryViewModel^ get();
            }


            property Windows::Foundation::Collections::IObservableVector<AlbumID3^>^ ExploreAlbums
            {
                Windows::Foundation::Collections::IObservableVector<AlbumID3^>^ get() { return _exploreAlbums; }
            }

            property Windows::Foundation::Collections::IObservableVector<AlbumID3^>^ NewestAlbums
            {
                Windows::Foundation::Collections::IObservableVector<AlbumID3^>^ get() { return _newestAlbums; }
            }

            property Windows::Foundation::Collections::IObservableVector<AlbumID3^>^ RecentReleasedAlbums
            {
                Windows::Foundation::Collections::IObservableVector<AlbumID3^>^ get() { return _recentReleasedAlbums; }
            }

            property Windows::Foundation::Collections::IObservableVector<AlbumID3^>^ RecentPlayedAlbums
            {
                Windows::Foundation::Collections::IObservableVector<AlbumID3^>^ get() { return _recentPlayedAlbums; }
            }

            property Windows::Foundation::Collections::IObservableVector<Song^>^ SpotlightSongs
            {
                Windows::Foundation::Collections::IObservableVector<Song^>^ get() { return _spotlightSongs; }
            }


            void LoadAllCategories();
            void ClearAll();


        private:
            LibraryViewModel();
            static LibraryViewModel^ _instance;

            Platform::Collections::Vector<AlbumID3^>^ _exploreAlbums;
            Platform::Collections::Vector<AlbumID3^>^ _newestAlbums;
            Platform::Collections::Vector<AlbumID3^>^ _recentReleasedAlbums;
            Platform::Collections::Vector<AlbumID3^>^ _recentPlayedAlbums;
            Platform::Collections::Vector<Song^>^ _spotlightSongs;

            void UpdateAlbumCollection(Platform::Collections::Vector<AlbumID3^>^ targetCollection, Platform::String^ jsonStr);
            void UpdateSongCollection(Platform::Collections::Vector<Song^>^ targetCollection, Platform::String^ jsonStr);
        };
    }
}
