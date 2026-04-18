#include "pch.h"
#include "GenresPage.xaml.h"
#include "LibraryPage.xaml.h"
#include "ViewModels/GenreViewModel.h"

using namespace Opal;
using namespace Platform;
using namespace Windows::UI::Xaml;
using namespace Windows::UI::Xaml::Controls;

GenresPage::GenresPage() {
    InitializeComponent();
    GenreGrid->ItemsSource = ViewModels::GenreViewModel::Instance->Genres;
    this->Loaded += ref new RoutedEventHandler(this, &GenresPage::OnPageLoaded);
}

void GenresPage::OnPageLoaded(Object^ sender, RoutedEventArgs^ e) {
    ViewModels::GenreViewModel::Instance->LoadGenresAsync();
}

void GenresPage::OnGenreClicked(Object^ sender, ItemClickEventArgs^ e) {
    auto genre = dynamic_cast<GenreModel^>(e->ClickedItem);
    if (genre != nullptr) {
        // Navigate to LibraryPage with the genre filter
        this->Frame->Navigate(Windows::UI::Xaml::Interop::TypeName(LibraryPage::typeid), "GENRE:" + genre->Name);
    }
}
