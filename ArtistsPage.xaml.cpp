#include "pch.h"
#include "ArtistsPage.xaml.h"
#include "LibraryPage.xaml.h"
#include "Services/NavidromeService.h"
#include <ppltasks.h>
#include "Services/DebugLogger.h"
#include <algorithm>

using namespace Opal;
using namespace Platform;
using namespace concurrency;
using namespace Windows::Foundation;
using namespace Windows::UI::Xaml;
using namespace Windows::UI::Xaml::Controls;
using namespace Windows::UI::Xaml::Media::Imaging;
using namespace Windows::Data::Json;
using namespace Windows::System::Profile;

ArtistsPage::ArtistsPage()
{
    _artists = ref new Platform::Collections::Vector<ArtistModel^>();
    _allArtists = ref new Platform::Collections::Vector<ArtistModel^>();
    InitializeComponent();
    this->Loaded += ref new RoutedEventHandler(this, &ArtistsPage::OnPageLoaded);
}

void ArtistsPage::OnPageLoaded(Object^ sender, RoutedEventArgs^ e)
{
    if (AnalyticsInfo::VersionInfo->DeviceFamily == "Windows.Xbox")
    {
        VisualStateManager::GoToState(this, "XboxState", false);
    }
    LoadArtists();
}

void ArtistsPage::LoadArtists()
{
    create_task(NavidromeService::Instance->GetArtistsAsync()).then([this](Platform::String^ jsonStr) {
        if (jsonStr != nullptr) {
            this->Dispatcher->RunAsync(Windows::UI::Core::CoreDispatcherPriority::Normal, ref new Windows::UI::Core::DispatchedHandler([this, jsonStr]() {
                try {
                    JsonObject^ rootRaw = JsonObject::Parse(jsonStr);
                    JsonObject^ root = rootRaw->GetNamedObject("subsonic-response", nullptr);
                    if (root != nullptr) {
                        JsonObject^ artistsObj = root->GetNamedObject("artists", nullptr);
                        JsonArray^ indices = artistsObj->GetNamedArray("index", nullptr);
                        _artists->Clear();
                        _allArtists->Clear();
                        for (unsigned int k = 0; k < indices->Size; k++) {
                            JsonObject^ indexObj = indices->GetObjectAt(k);
                            JsonArray^ artistArray = indexObj->GetNamedArray("artist", nullptr);
                            for (unsigned int i = 0; i < artistArray->Size; i++) {
                                JsonObject^ artObj = artistArray->GetObjectAt(i);
                                auto am = ref new ArtistModel();
                                am->Id = artObj->GetNamedString("id", "");
                                am->Name = artObj->GetNamedString("name", "Unknown Artist");
                                am->AlbumCount = (int)artObj->GetNamedNumber("albumCount", 0);
                                am->CoverUrl = NavidromeService::Instance->GetCoverArtUrl(am->Id, 300);
                                am->PopulateSearchTerms();
                                _artists->Append(am);
                                _allArtists->Append(am);
                            }
                        }
                        this->OnFilterOrSortChanged(nullptr, nullptr);
                    }
                } catch (Exception^ ex) { DebugLogger::Instance->LogException("LoadArtists (JSON Parse)", ex); }
            }));
        }
    });
}

void ArtistsPage::OnFilterOrSortChanged(Object^ sender, Object^ e)
{
    if (ArtistsGridView == nullptr) return;
    if (_allArtists == nullptr || _allArtists->Size == 0) return;

    String^ query = FilterBox->Text;
    std::wstring wQuery(query == nullptr ? L"" : query->Data());
    for (auto& c : wQuery) c = towlower(c);

    std::vector<ArtistModel^> result;
    for (unsigned int i = 0; i < _allArtists->Size; i++) {
        auto am = _allArtists->GetAt(i);
        if (wQuery.empty()) { result.push_back(am); continue; }
        std::wstring terms(am->SearchTerms->Data());
        if (terms.find(wQuery) != std::wstring::npos) result.push_back(am);
    }

    int sortIdx = (SortByCombo != nullptr) ? SortByCombo->SelectedIndex : 0;
    bool isDescending = (SortDirectionButton != nullptr && SortDirectionButton->IsChecked != nullptr) ? SortDirectionButton->IsChecked->Value : false;

    if (SortIcon != nullptr) SortIcon->Glyph = isDescending ? L"\uE8CC" : L"\uE8CB";

    std::sort(result.begin(), result.end(), [sortIdx, isDescending](ArtistModel^ a, ArtistModel^ b) {
        if (sortIdx == 0) {
            auto aName = std::wstring(a->Name->Data());
            auto bName = std::wstring(b->Name->Data());
            return isDescending ? aName > bName : aName < bName;
        }
        // Album Count
        return isDescending ? a->AlbumCount > b->AlbumCount : a->AlbumCount < b->AlbumCount;
    });

    auto finalVec = ref new Platform::Collections::Vector<ArtistModel^>();
    for (auto am : result) {
        finalVec->Append(am);
    }
    _artists = finalVec;
    ArtistsGridView->ItemsSource = _artists;

    // Update count
    wchar_t buf[64];
    swprintf_s(buf, L"Artists (%u)", _artists->Size);
    PageTitle->Text = ref new String(buf);
}

void ArtistsPage::OnArtistClicked(Object^ sender, ItemClickEventArgs^ e)
{
    auto am = dynamic_cast<ArtistModel^>(e->ClickedItem);
    if (am != nullptr)
    {
        // Navigate to LibraryPage with Artist ID
        this->Frame->Navigate(Windows::UI::Xaml::Interop::TypeName(LibraryPage::typeid), am->Id);
    }
}

void Opal::ArtistsPage::TextBlock_SelectionChanged(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e)
{

}
