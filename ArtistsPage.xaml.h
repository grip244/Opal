#pragma once

#include "ArtistsPage.g.h"
#include "Models/SharedModels.h"

namespace Opal
{
    public ref class ArtistsPage sealed
    {
    public:
        ArtistsPage();

    private:
        void OnPageLoaded(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
        void OnArtistClicked(Platform::Object^ sender, Windows::UI::Xaml::Controls::ItemClickEventArgs^ e);
        void LoadArtists();

        Platform::Collections::Vector<ArtistModel^>^ _artists;
        void TextBlock_SelectionChanged(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
    };
}
