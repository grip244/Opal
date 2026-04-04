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


            property Windows::Foundation::Collections::IObservableVector<AlbumModel^>^ ExploreAlbums
            {
                Windows::Foundation::Collections::IObservableVector<AlbumModel^>^ get() { return _exploreAlbums; }
            }

            property Windows::Foundation::Collections::IObservableVector<AlbumModel^>^ NewestAlbums
            {
                Windows::Foundation::Collections::IObservableVector<AlbumModel^>^ get() { return _newestAlbums; }
            }

            property Windows::Foundation::Collections::IObservableVector<AlbumModel^>^ RecentReleasedAlbums
            {
                Windows::Foundation::Collections::IObservableVector<AlbumModel^>^ get() { return _recentReleasedAlbums; }
            }

            property Windows::Foundation::Collections::IObservableVector<Song^>^ SpotlightSongs
            {
                Windows::Foundation::Collections::IObservableVector<Song^>^ get() { return _spotlightSongs; }
            }


            void LoadAllCategories();

        private:
            LibraryViewModel();
            static LibraryViewModel^ _instance;

            Platform::Collections::Vector<AlbumModel^>^ _exploreAlbums;
            Platform::Collections::Vector<AlbumModel^>^ _newestAlbums;
            Platform::Collections::Vector<AlbumModel^>^ _recentReleasedAlbums;
            Platform::Collections::Vector<Song^>^ _spotlightSongs;
        };
    }
}
