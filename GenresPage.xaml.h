#pragma once
#include "Generated Files/GenresPage.g.h"
#include "ViewModels/GenreViewModel.h"

namespace Opal {
    public ref class GenresPage sealed {
    public:
        GenresPage();

    private:
        void OnGenreClicked(Platform::Object^ sender, Windows::UI::Xaml::Controls::ItemClickEventArgs^ e);
        void OnPageLoaded(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
        void OnFilterOrSortChanged(Platform::Object^ sender, Platform::Object^ e);
    };
}
