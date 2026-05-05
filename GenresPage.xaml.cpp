#include "pch.h"
#include "GenresPage.xaml.h"
#include "LibraryPage.xaml.h"
#include "ViewModels/GenreViewModel.h"
#include <cwctype>
#include <string>

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

void GenresPage::OnFilterOrSortChanged(Object^ sender, TextChangedEventArgs^ e)
{
    auto allGenres = ViewModels::GenreViewModel::Instance->Genres;
    String^ query = FilterBox->Text;
    if (query == nullptr || query->IsEmpty()) {
        GenreGrid->ItemsSource = allGenres;
        return;
    }

    std::wstring wQuery(query->Data());
    for (auto& c : wQuery) c = towlower(c);

    auto result = ref new Platform::Collections::Vector<GenreModel^>();
    for (auto g : allGenres) {
        std::wstring name(g->Name->Data());
        for (auto& c : name) c = towlower(c);
        if (name.find(wQuery) != std::wstring::npos) {
            result->Append(g);
        }
    }
    GenreGrid->ItemsSource = result;
}

void GenresPage::OnGenreClicked(Object^ sender, ItemClickEventArgs^ e) {
    auto genre = dynamic_cast<GenreModel^>(e->ClickedItem);
    if (genre != nullptr) {
        // Navigate to LibraryPage with the genre filter
        this->Frame->Navigate(Windows::UI::Xaml::Interop::TypeName(LibraryPage::typeid), "GENRE:" + genre->Name);
    }
}
