#include "pch.h"
#include "AlbumsPage.xaml.h"
#include "LibraryPage.xaml.h"
#include "Services/NavidromeService.h"
#include <ppltasks.h>
#include <algorithm>
#include <vector>
#include <cwctype>

using namespace Opal;
using namespace Platform;
using namespace concurrency;
using namespace Windows::Foundation;
using namespace Windows::UI::Xaml;
using namespace Windows::UI::Xaml::Controls;
using namespace Windows::UI::Xaml::Media::Imaging;
using namespace Windows::Data::Json;
using namespace Windows::System::Profile;
using namespace Windows::UI::Xaml::Navigation;

AlbumsPage::AlbumsPage()
{
    _allAlbums = ref new Platform::Collections::Vector<AlbumModel^>();
    _albums = ref new Platform::Collections::Vector<AlbumModel^>();
    InitializeComponent();
}

void AlbumsPage::OnNavigatedTo(NavigationEventArgs^ e)
{
    if (AnalyticsInfo::VersionInfo->DeviceFamily == "Windows.Xbox")
    {
        VisualStateManager::GoToState(this, "XboxState", false);
    }
    LoadAlbums();
}

void AlbumsPage::LoadAlbums()
{
    create_task(NavidromeService::Instance->GetAlbumListAsync("alphabeticalByName", 500, 0)).then([this](Platform::String^ jsonStr) {
        if (jsonStr != nullptr) {
            this->Dispatcher->RunAsync(Windows::UI::Core::CoreDispatcherPriority::Normal, ref new Windows::UI::Core::DispatchedHandler([this, jsonStr]() {
                try {
                    JsonObject^ rootRaw = JsonObject::Parse(jsonStr);
                    JsonObject^ root = rootRaw->GetNamedObject("subsonic-response", nullptr);
                    if (root != nullptr) {
                        JsonObject^ albumListObj = root->HasKey("albumList2") ? root->GetNamedObject("albumList2") : (root->HasKey("albumList") ? root->GetNamedObject("albumList") : nullptr);
                        if (albumListObj != nullptr) {
                            JsonArray^ albumArray = albumListObj->GetNamedArray("album", nullptr);
                            _albums->Clear();
                            for (unsigned int i = 0; i < albumArray->Size; i++) {
                                JsonObject^ albObj = albumArray->GetObjectAt(i);
                                auto am = ref new AlbumModel();
                                am->Id = albObj->GetNamedString("id", "");
                                am->Title = albObj->HasKey("title") ? albObj->GetNamedString("title") : (albObj->HasKey("name") ? albObj->GetNamedString("name") : "Unknown Album");
                                am->Artist = albObj->HasKey("artist") ? albObj->GetNamedString("artist") : (albObj->HasKey("artistName") ? albObj->GetNamedString("artistName") : "Unknown Artist");
                                
                                if (albObj->HasKey("year")) {
                                    auto yearVal = albObj->GetNamedValue("year");
                                    if (yearVal->ValueType == JsonValueType::String) am->Year = yearVal->GetString();
                                    else if (yearVal->ValueType == JsonValueType::Number) {
                                        wchar_t yBuf[16];
                                        swprintf_s(yBuf, L"%d", (int)yearVal->GetNumber());
                                        am->Year = ref new Platform::String(yBuf);
                                    }
                                } else {
                                    am->Year = "";
                                }
                                
                                Platform::String^ coverUrlStr = NavidromeService::Instance->GetCoverArtUrl(am->Id, 500);
                                try { am->CoverArt = ref new BitmapImage(ref new Uri(coverUrlStr)); } catch (...) {}
                                
                                _allAlbums->Append(am);
                            }
                            this->OnFilterOrSortChanged(nullptr, nullptr);
                        }
                    }
                } catch (...) {}
            }));
        }
    });
}

void AlbumsPage::OnFilterOrSortChanged(Object^ sender, Object^ e)
{
    if (_allAlbums == nullptr || _allAlbums->Size == 0) return;

    // 1. Text Filter
    String^ query = FilterBox->Text;
    std::wstring wQuery(query == nullptr ? L"" : query->Data());
    for (auto& c : wQuery) c = towlower(c);

    // 2. Year Filter
    int minYear = 0, maxYear = 9999;
    if (YearFilterCombo != nullptr && YearFilterCombo->SelectedIndex > 0) {
        int idx = YearFilterCombo->SelectedIndex;
        if (idx == 1) { minYear = 2020; }
        else if (idx == 2) { minYear = 2010; maxYear = 2019; }
        else if (idx == 3) { minYear = 2000; maxYear = 2009; }
        else if (idx == 4) { minYear = 1990; maxYear = 1999; }
        else if (idx == 5) { minYear = 1980; maxYear = 1989; }
        else if (idx == 6) { maxYear = 1979; }
    }

    std::vector<AlbumModel^> result;
    for (auto am : _allAlbums) {
        bool textMatch = true;
        if (wQuery.length() > 0) {
            std::wstring wTitle(am->Title->Data());
            std::wstring wArtist(am->Artist->Data());
            for (auto& c : wTitle) c = towlower(c);
            for (auto& c : wArtist) c = towlower(c);
            textMatch = (wTitle.find(wQuery) != std::wstring::npos || wArtist.find(wQuery) != std::wstring::npos);
        }

        bool yearMatch = true;
        if (YearFilterCombo != nullptr && YearFilterCombo->SelectedIndex > 0) {
            int yr = 0;
            if (am->Year != nullptr && am->Year->Length() > 0) {
                try { yr = _wtoi(am->Year->Data()); } catch (...) {}
            }
            yearMatch = (yr >= minYear && yr <= maxYear);
        }

        if (textMatch && yearMatch) result.push_back(am);
    }

    // 3. Sort
    int sortIdx = (SortByCombo != nullptr) ? SortByCombo->SelectedIndex : 0;
    std::sort(result.begin(), result.end(), [sortIdx](AlbumModel^ a, AlbumModel^ b) {
        if (sortIdx == 0) { // A-Z
            return std::wstring(a->Title->Data()) < std::wstring(b->Title->Data());
        }
        else { // Year
            int ya = 0, yb = 0;
            if (a->Year != nullptr && a->Year->Length() > 0) { try { ya = _wtoi(a->Year->Data()); } catch (...) {} }
            if (b->Year != nullptr && b->Year->Length() > 0) { try { yb = _wtoi(b->Year->Data()); } catch (...) {} }
            if (sortIdx == 1) return ya < yb; // Oldest
            return ya > yb; // Newest
        }
    });

    auto output = ref new Platform::Collections::Vector<AlbumModel^>();
    for (auto item : result) output->Append(item);
    AlbumsGridView->ItemsSource = output;
}

void AlbumsPage::OnAlbumClicked(Object^ sender, ItemClickEventArgs^ e)
{
    auto am = dynamic_cast<AlbumModel^>(e->ClickedItem);
    if (am != nullptr)
    {
        // Navigate to LibraryPage with Album ID
        this->Frame->Navigate(Windows::UI::Xaml::Interop::TypeName(LibraryPage::typeid), am);
    }
}
