#pragma once
#include "PlaylistsPage.g.h"
#include "ViewModels/PlaylistsViewModel.h"
#include "Converters/NullToVisibilityConverter.h"

namespace Opal
{
    [Windows::Foundation::Metadata::WebHostHidden]
    public ref class PlaylistsPage sealed
    {
    public:
        PlaylistsPage();

        property ViewModels::PlaylistsViewModel^ PlaylistsVM {
            ViewModels::PlaylistsViewModel^ get() { return ViewModels::PlaylistsViewModel::Instance; }
        }

        void OnPlaylistClick(Platform::Object^ sender, Windows::UI::Xaml::Controls::ItemClickEventArgs^ e);
        void OnCreateNewClick(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
        void OnEditPlaylistClick(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
        void OnUploadCoverClick(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
        void OnDeletePlaylistClick(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
        void OnMoreButtonClicked(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
    };
}
