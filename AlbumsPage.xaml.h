#pragma once

#include "Generated Files/AlbumsPage.g.h"
#include "Models/SharedModels.h"

namespace Opal
{
    public ref class AlbumsPage sealed
    {
    public:
        AlbumsPage();
        void LoadAlbums();

    protected:
        virtual void OnNavigatedTo(Windows::UI::Xaml::Navigation::NavigationEventArgs^ e) override;

    private:
        void OnAlbumClicked(Platform::Object^ sender, Windows::UI::Xaml::Controls::ItemClickEventArgs^ e);
        void OnFilterOrSortChanged(Platform::Object^ sender, Platform::Object^ e);
        Windows::Foundation::Collections::IVector<AlbumID3^>^ _allAlbums;
        Windows::Foundation::Collections::IVector<AlbumID3^>^ _albums;
    };
}
