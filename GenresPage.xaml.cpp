#include "pch.h"
#include "GenresPage.xaml.h"
#include "LibraryPage.xaml.h"
#include "ViewModels/GenreViewModel.h"
#include <cwctype>
#include <string>
#include <algorithm>
#include <vector>

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

void GenresPage::OnFilterOrSortChanged(Object^ sender, Object^ e)
{
    if (GenreGrid == nullptr) return;
    auto allGenres = ViewModels::GenreViewModel::Instance->Genres;
    if (allGenres == nullptr || allGenres->Size == 0) return;

    // 1. Filter
    String^ query = FilterBox->Text;
    std::wstring wQuery(query == nullptr ? L"" : query->Data());
    std::transform(wQuery.begin(), wQuery.end(), wQuery.begin(), ::towlower);

    std::vector<GenreModel^> result;
    for (auto g : allGenres) {
        if (wQuery.empty()) { result.push_back(g); continue; }
        std::wstring name(g->Name->Data());
        std::transform(name.begin(), name.end(), name.begin(), ::towlower);
        if (name.find(wQuery) != std::wstring::npos) {
            result.push_back(g);
        }
    }

    // 2. Sort
    int sortIdx = (SortByCombo != nullptr) ? SortByCombo->SelectedIndex : 0;
    bool isDescending = (SortDirectionButton != nullptr && SortDirectionButton->IsChecked != nullptr) ? SortDirectionButton->IsChecked->Value : false;

    if (SortIcon != nullptr) SortIcon->Glyph = isDescending ? L"\uE8CC" : L"\uE8CB";

    std::sort(result.begin(), result.end(), [sortIdx, isDescending](GenreModel^ a, GenreModel^ b) {
        bool less = false;
        if (sortIdx == 0) { // A-Z
            less = _wcsicmp(a->Name->Data(), b->Name->Data()) < 0;
        }
        else { // Album Count
            less = a->AlbumCount < b->AlbumCount;
        }
        return isDescending ? !less : less;
    });

    auto finalVec = ref new Platform::Collections::Vector<GenreModel^>();
    for (auto g : result) {
        finalVec->Append(g);
    }
    GenreGrid->ItemsSource = finalVec;
}

void GenresPage::OnGenreClicked(Object^ sender, ItemClickEventArgs^ e) {
    auto genre = dynamic_cast<GenreModel^>(e->ClickedItem);
    if (genre != nullptr) {
        // Navigate to LibraryPage with the genre filter
        this->Frame->Navigate(Windows::UI::Xaml::Interop::TypeName(LibraryPage::typeid), "GENRE:" + genre->Name);
    }
}
