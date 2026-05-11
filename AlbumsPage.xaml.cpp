#include "pch.h"
#include "AlbumsPage.xaml.h"
#include "LibraryPage.xaml.h"
#include "Services/NavidromeService.h"
#include <ppltasks.h>
#include "Services/DebugLogger.h"
#include <algorithm>
#include <utility>
#include <vector>
#include <cwctype>

#include <Windows.UI.Xaml.Media.Animation.h>

using namespace Opal;
using namespace Platform;
using namespace concurrency;
using namespace Windows::Foundation;
using namespace Windows::UI::Xaml;
using namespace Windows::UI::Xaml::Controls;
using namespace Windows::UI::Xaml::Media;
using namespace Windows::UI::Xaml::Media::Animation;
using namespace Windows::UI::Xaml::Media::Imaging;
using namespace Windows::Data::Json;
using namespace Windows::System::Profile;
using namespace Windows::UI::Xaml::Navigation;

AlbumsPage::AlbumsPage()
{
    _allAlbums = ref new Platform::Collections::Vector<AlbumID3^>();
    _albums = ref new Platform::Collections::Vector<AlbumID3^>();
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
                            std::vector<AlbumID3^> albums;
                            albums.reserve(albumArray->Size);

                            for (unsigned int i = 0; i < albumArray->Size; i++) {
                                JsonObject^ albObj = albumArray->GetObjectAt(i);
                                auto am = ref new AlbumID3();
                                am->Id = albObj->GetNamedString("id", "");
                                am->Title = albObj->HasKey("title") ? albObj->GetNamedString("title") : (albObj->HasKey("name") ? albObj->GetNamedString("name") : "Unknown Album");
                                am->Artist = albObj->HasKey("artist") ? albObj->GetNamedString("artist") : (albObj->HasKey("artistName") ? albObj->GetNamedString("artistName") : "Unknown Artist");
                                
                                if (albObj->HasKey("explicitStatus")) {
                                    auto val = albObj->GetNamedValue("explicitStatus");
                                    if (val != nullptr && val->ValueType == JsonValueType::String) {
                                        am->ExplicitStatus = val->GetString();
                                    } else { am->ExplicitStatus = L""; }
                                } else { am->ExplicitStatus = L""; }
                                
                                if (albObj->HasKey("version")) {
                                    auto val = albObj->GetNamedValue("version");
                                    if (val != nullptr && val->ValueType == JsonValueType::String) am->Version = val->GetString();
                                }
                                if (albObj->HasKey("sortName")) {
                                    auto val = albObj->GetNamedValue("sortName");
                                    if (val != nullptr && val->ValueType == JsonValueType::String) am->SortName = val->GetString();
                                }
                                
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
                                
                                String^ coverArtId = albObj->HasKey("coverArt") ? albObj->GetNamedString("coverArt") : am->Id;
                                am->CoverUrl = NavidromeService::Instance->GetCoverArtUrl(coverArtId, 500);
                                am->PopulateSearchTerms();
                                albums.push_back(am);
                            }
                            _albums->Clear();
                            _allAlbums = ref new Platform::Collections::Vector<AlbumID3^>(std::move(albums));
                            this->OnFilterOrSortChanged(nullptr, nullptr);
                        }
                    }
                } catch (Exception^ ex) { DebugLogger::Instance->LogException("LoadAlbums (JSON Parse)", ex); }
            }));
        }
    });
}

void AlbumsPage::OnFilterOrSortChanged(Object^ sender, Object^ e)
{
    if (AlbumsGridView == nullptr) return;
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

    std::vector<AlbumID3^> result;
    result.reserve(_allAlbums->Size);
    for (unsigned int i = 0; i < _allAlbums->Size; i++) {
        auto am = _allAlbums->GetAt(i);
        bool textMatch = true;
        if (wQuery.length() > 0) {
            std::wstring wTerms(am->SearchTerms->Data());
            textMatch = (wTerms.find(wQuery) != std::wstring::npos);
        }

        bool yearMatch = true;
        if (YearFilterCombo != nullptr && YearFilterCombo->SelectedIndex > 0) {
            int yr = 0;
            if (am->Year != nullptr && am->Year->Length() > 0) {
                try { yr = _wtoi(am->Year->Data()); } catch (Exception^ ex) { DebugLogger::Instance->LogException("OnFilterOrSortChanged (_wtoi yr)", ex); }
            }
            yearMatch = (yr >= minYear && yr <= maxYear);
        }

        if (textMatch && yearMatch) result.push_back(am);
    }

    // 3. Sort
    int sortIdx = (SortByCombo != nullptr) ? SortByCombo->SelectedIndex : 0;
    bool isDescending = (SortDirectionButton != nullptr && SortDirectionButton->IsChecked != nullptr) ? SortDirectionButton->IsChecked->Value : false;

    if (SortIcon != nullptr) SortIcon->Glyph = isDescending ? L"\uE8CC" : L"\uE8CB";

    std::sort(result.begin(), result.end(), [sortIdx, isDescending](AlbumID3^ a, AlbumID3^ b) {
        if (sortIdx == 0) { // A-Z
            auto aName = std::wstring(a->Title->Data());
            auto bName = std::wstring(b->Title->Data());
            return isDescending ? aName > bName : aName < bName;
        }
        else { // Year
            int ya = 0, yb = 0;
            if (a->Year != nullptr && a->Year->Length() > 0) { try { ya = _wtoi(a->Year->Data()); } catch (...) { } }
            if (b->Year != nullptr && b->Year->Length() > 0) { try { yb = _wtoi(b->Year->Data()); } catch (...) { } }
            return isDescending ? ya > yb : ya < yb;
        }
    });

    auto output = ref new Platform::Collections::Vector<AlbumID3^>();
    for (auto am : result) {
        output->Append(am);
    }
    AlbumsGridView->ItemsSource = output;

    // Update count
    wchar_t buf[64];
    swprintf_s(buf, L"Albums (%u)", output->Size);
    PageTitle->Text = ref new String(buf);
}

void AlbumsPage::OnAlbumClicked(Object^ sender, ItemClickEventArgs^ e)
{
    auto am = dynamic_cast<AlbumID3^>(e->ClickedItem);
    if (am != nullptr)
    {
        // Prepare connected animation
        auto container = dynamic_cast<GridViewItem^>(AlbumsGridView->ContainerFromItem(e->ClickedItem));
        if (container != nullptr) {
            ConnectedAnimationService::GetForCurrentView()->PrepareToAnimate("ForwardConnectedAnimation", container);
        }

        // Navigate to LibraryPage with Album ID
        this->Frame->Navigate(Windows::UI::Xaml::Interop::TypeName(LibraryPage::typeid), am);
    }
}
