#include "pch.h"
#include "TracksPage.xaml.h"
#include "Services/NavidromeService.h"
#include "Services/PlaybackService.h"
#include "Models/SharedModels.h"
#include <Windows.UI.Xaml.Media.h>
#include "ViewModels/PlaylistsViewModel.h"
#include <ppltasks.h>
#include <collection.h>
#include <algorithm>
#include <utility>
#include <vector>
#include <cwctype>
#include <random>

using namespace Opal;
using namespace Platform;
using namespace concurrency;
using namespace Windows::Foundation;
using namespace Windows::UI::Xaml;
using namespace Windows::UI::Xaml::Controls;
using namespace Windows::UI::Xaml::Media;
using namespace Windows::UI::Xaml::Media::Imaging;
using namespace Windows::Data::Json;
using namespace Windows::System::Profile;
using namespace Windows::UI::Xaml::Navigation;

TracksPage::TracksPage()
{
    _allTracks = ref new Platform::Collections::Vector<Song^>();
    _offset = 0;
    _totalCount = 0;
    _isLoadingMore = false;
    _showFavoritesOnly = false;
    InitializeComponent();
}

void TracksPage::OnNavigatedTo(NavigationEventArgs^ e)
{
    if (AnalyticsInfo::VersionInfo->DeviceFamily == "Windows.Xbox")
    {
        VisualStateManager::GoToState(this, "XboxState", false);
    }
    if (_allTracks->Size == 0)
    {
        LoadTracks();
    }
}

void TracksPage::LoadTracks()
{
    if (_isLoadingMore) return;
    _isLoadingMore = true;

    if (_offset == 0 && LoadingIndicator != nullptr) LoadingIndicator->Visibility = Windows::UI::Xaml::Visibility::Visible;

    create_task(NavidromeService::Instance->SearchAsync("*", 2000, _offset)).then([this](Platform::String^ jsonStr) {
        if (jsonStr != nullptr) {
            this->Dispatcher->RunAsync(Windows::UI::Core::CoreDispatcherPriority::Normal, ref new Windows::UI::Core::DispatchedHandler([this, jsonStr]() {
                bool hasMoreInPage = false;
                try {
                    JsonObject^ rootRaw = JsonObject::Parse(jsonStr);
                    JsonObject^ root = rootRaw->GetNamedObject("subsonic-response", nullptr);
                    if (root != nullptr) {
                        JsonObject^ result3 = root->GetNamedObject("searchResult3", nullptr);
                        if (result3 != nullptr) {
                            if (_offset == 0 && result3->HasKey("songCount")) {
                                _totalCount = (int)result3->GetNamedNumber("songCount");
                            }

                            if (result3->HasKey("song")) {
                                JsonArray^ songArray = result3->GetNamedArray("song");
                                hasMoreInPage = (songArray->Size >= 2000);
                                for (unsigned int i = 0; i < songArray->Size; i++) {
                                    JsonObject^ songObj = songArray->GetObjectAt(i);
                                    auto song = ref new Song();
                                    song->Id = songObj->GetNamedString("id", "");
                                    song->Title = songObj->HasKey("title") ? songObj->GetNamedString("title") : "Unknown Track";
                                    song->Artist = songObj->HasKey("artist") ? songObj->GetNamedString("artist") : "Unknown Artist";
                                    song->Album = songObj->HasKey("album") ? songObj->GetNamedString("album") : "Unknown Album";

                                    if (songObj->HasKey("explicitStatus")) {
                                        auto val = songObj->GetNamedValue("explicitStatus");
                                        if (val != nullptr && val->ValueType == JsonValueType::String) {
                                            song->ExplicitStatus = val->GetString();
                                        } else { song->ExplicitStatus = L""; }
                                    } else { song->ExplicitStatus = L""; }

                                    if (songObj->HasKey("duration")) {
                                        int totalSeconds = (int)songObj->GetNamedNumber("duration");
                                        song->DurationInSeconds = totalSeconds;
                                        wchar_t buf[16]; swprintf_s(buf, L"%d:%02d", totalSeconds / 60, totalSeconds % 60);
                                        song->Duration = ref new Platform::String(buf);
                                    } else { song->Duration = "--:--"; song->DurationInSeconds = 0; }

                                    if (songObj->HasKey("year")) {
                                        auto y = songObj->GetNamedValue("year");
                                        if (y->ValueType == JsonValueType::Number) {
                                            wchar_t yBuf[16]; swprintf_s(yBuf, L"%d", (int)y->GetNumber());
                                            song->Year = ref new Platform::String(yBuf);
                                        } else song->Year = y->Stringify();
                                    }

                                    song->StreamUrl = NavidromeService::Instance->GetStreamUrl(song->Id);
                                    Platform::String^ coverArtId = songObj->HasKey("coverArt") ? songObj->GetNamedString("coverArt") : song->Id;
                                    song->CoverUrl = NavidromeService::Instance->GetCoverArtUrl(coverArtId, 500);

                                    if (songObj->HasKey("starred")) song->IsFavorite = true;
                                    else song->IsFavorite = false;

                                    if (songObj->HasKey("userRating")) song->Rating = songObj->GetNamedNumber("userRating");
                                    else song->Rating = 0.0;

                                    song->PopulateSearchTerms();
                                    _allTracks->Append(song);
                                }
                                
                                _offset += songArray->Size;
                            }
                        }
                    }
                } catch (...) {}
                
                _isLoadingMore = false;
                if (LoadingIndicator != nullptr) LoadingIndicator->Visibility = Windows::UI::Xaml::Visibility::Collapsed;
                
                // Update counter with filtered count
                if (_totalCount > 0) {
                    TotalTracksCounter->Text = "Total Tracks: (" + _totalCount.ToString() + ")";
                } else {
                    TotalTracksCounter->Text = "Total Tracks: (" + _allTracks->Size.ToString() + ")";
                }
                PageTitle->Text = "Tracks";

                // Trigger filter/sort update
                this->OnFilterOrSortChanged(nullptr, nullptr);

                // Auto-load next batch if we think there are more
                if (_totalCount > (int)_allTracks->Size || hasMoreInPage) {
                    LoadTracks(); 
                }
            }));
        } else {
            _isLoadingMore = false;
            if (LoadingIndicator != nullptr) LoadingIndicator->Visibility = Windows::UI::Xaml::Visibility::Collapsed;
        }
    });
}

void TracksPage::OnLoadMoreClicked(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e)
{
    LoadTracks();
}

void TracksPage::OnFilterOrSortChanged(Object^ sender, Object^ e)
{
    if (TracksListView == nullptr) return;
    if (_allTracks == nullptr || _allTracks->Size == 0) return;

    // 1. Text Filter
    String^ query = FilterBox->Text;
    std::wstring wQuery(query == nullptr ? L"" : query->Data());
    std::transform(wQuery.begin(), wQuery.end(), wQuery.begin(), ::towlower);

    std::vector<Song^> result;
    result.reserve(_allTracks->Size);
    for (unsigned int i = 0; i < _allTracks->Size; i++) {
        Song^ song = _allTracks->GetAt(i);

        // 3.3: Favorites filter
        if (_showFavoritesOnly && !song->IsFavorite) continue;

        bool textMatch = true;
        if (wQuery.length() > 0) {
            if (song->SearchTerms != nullptr) {
                std::wstring wTerms(song->SearchTerms->Data());
                textMatch = (wTerms.find(wQuery) != std::wstring::npos);
            } else {
                textMatch = false;
            }
        }

        if (textMatch) result.push_back(song);
    }

    // Update counter with filtered count
    wchar_t cBuf[64];
    if (_showFavoritesOnly) {
        swprintf_s(cBuf, L"Liked Tracks: (%d)", (int)result.size());
    } else if (_totalCount > 0) {
        swprintf_s(cBuf, L"Total Tracks: (%d)", _totalCount);
    } else {
        swprintf_s(cBuf, L"Total Tracks: (%d)", (int)_allTracks->Size);
    }
    TotalTracksCounter->Text = ref new Platform::String(cBuf);

    // 2. Sort
    int sortIdx = (SortByCombo != nullptr) ? SortByCombo->SelectedIndex : 0;
    bool isDescending = (SortDirectionButton != nullptr && SortDirectionButton->IsChecked != nullptr) ? SortDirectionButton->IsChecked->Value : false;

    if (SortIcon != nullptr) SortIcon->Glyph = isDescending ? L"\uE8CC" : L"\uE8CB";

    std::sort(result.begin(), result.end(), [sortIdx, isDescending](Song^ a, Song^ b) {
        bool less = false;
        if (sortIdx == 0) { // Title
            less = _wcsicmp(a->Title->Data(), b->Title->Data()) < 0;
        }
        else if (sortIdx == 1) { // Artist
            less = _wcsicmp(a->Artist->Data(), b->Artist->Data()) < 0;
        }
        else if (sortIdx == 2) { // Duration
            less = a->DurationInSeconds < b->DurationInSeconds;
        }
        return isDescending ? !less : less;
    });

    auto output = ref new Platform::Collections::Vector<Song^>();
    for (Song^ song : result) {
        output->Append(song);
    }
    TracksListView->ItemsSource = output;
}

void TracksPage::OnTrackClicked(Object^ sender, ItemClickEventArgs^ e)
{
    auto song = dynamic_cast<Song^>(e->ClickedItem);
    if (song != nullptr)
    {
        PlaybackService::Instance->PlaySong(song);
    }
}

void TracksPage::OnPlayAllClicked(Object^ sender, RoutedEventArgs^ e)
{
    auto itemsSource = dynamic_cast<Platform::Collections::Vector<Song^>^>(TracksListView->ItemsSource);
    if (itemsSource != nullptr && itemsSource->Size > 0)
    {
        PlaybackService::Instance->PlayQueue(itemsSource, 0);
    }
}

void TracksPage::OnShuffleAllClicked(Object^ sender, RoutedEventArgs^ e)
{
    auto itemsSource = dynamic_cast<Platform::Collections::Vector<Song^>^>(TracksListView->ItemsSource);
    if (itemsSource != nullptr && itemsSource->Size > 0)
    {
        std::vector<Song^> songs;
        songs.reserve(itemsSource->Size);
        for (Song^ s : itemsSource) songs.push_back(s);
        
        std::random_device rd;
        std::mt19937 g(rd());
        std::shuffle(songs.begin(), songs.end(), g);
        
        auto output = ref new Platform::Collections::Vector<Song^>(std::move(songs));
        PlaybackService::Instance->PlayQueue(output, 0);
    }
}

void TracksPage::OnFavoriteIconLoaded(Object^ sender, RoutedEventArgs^ e) {
    auto icon = dynamic_cast<FontIcon^>(sender);
    if (!icon) return;
    auto song = dynamic_cast<Song^>(icon->Tag);
    if (song) {
        icon->Glyph = song->IsFavorite ? ref new Platform::String(L"\uEB52") : ref new Platform::String(L"\uEB51");
    }
}

void TracksPage::OnTrackFavoriteClicked(Object^ sender, RoutedEventArgs^ e) {
    auto btn = dynamic_cast<Windows::UI::Xaml::Controls::Primitives::ToggleButton^>(sender);
    if (!btn) return;
    auto song = dynamic_cast<Song^>(btn->Tag);
    if (song != nullptr) {
        bool isFav = false;
        if (btn->IsChecked != nullptr) isFav = btn->IsChecked->Value;
        else isFav = !song->IsFavorite;
        
        song->IsFavorite = isFav;
        auto icon = dynamic_cast<FontIcon^>(btn->Content);
        if (icon) icon->Glyph = isFav ? ref new Platform::String(L"\uEB52") : ref new Platform::String(L"\uEB51");
        
        NavidromeService::Instance->ToggleFavoriteAsync(song->Id, isFav);
    }
}

void TracksPage::OnTrackRatingChanged(Microsoft::UI::Xaml::Controls::RatingControl^ sender, Object^ args) {
    auto song = dynamic_cast<Song^>(sender->Tag);
    if (song != nullptr) {
        double newVar = sender->Value;
        if (song->Rating != newVar && newVar >= 0) {
            song->Rating = newVar;
            NavidromeService::Instance->SetRatingAsync(song->Id, (int)newVar);
        }
    }
}


void TracksPage::OnTrackContextOpening(Object^ sender, Object^ e)
{
    auto menu = dynamic_cast<MenuFlyout^>(sender);
    if (menu == nullptr) return;
    
    bool isXbox = (Windows::System::Profile::AnalyticsInfo::VersionInfo->DeviceFamily == "Windows.Xbox");

    MenuFlyoutSubItem^ addToPlaylistSub = nullptr;
    for (unsigned int i = 0; i < menu->Items->Size; i++) {
        auto item = menu->Items->GetAt(i);
        if (item->Name == "AddToPlaylistMenu") {
            addToPlaylistSub = dynamic_cast<MenuFlyoutSubItem^>(item);
            if (isXbox) addToPlaylistSub->Visibility = Windows::UI::Xaml::Visibility::Collapsed;
            else addToPlaylistSub->Visibility = Windows::UI::Xaml::Visibility::Visible;
        }
    }
    
    if (addToPlaylistSub != nullptr && !isXbox) {
        addToPlaylistSub->Items->Clear();
        auto vm = ViewModels::PlaylistsViewModel::Instance;
        for (unsigned int i = 0; i < vm->Playlists->Size; i++) {
            auto p = vm->Playlists->GetAt(i);
            auto item = ref new MenuFlyoutItem();
            item->Text = p->Name;
            item->Tag = p;
            item->Click += ref new RoutedEventHandler([this, p, menu](Object^ s, RoutedEventArgs^ args) {
                auto grid = dynamic_cast<Grid^>(menu->Target);
                if (grid != nullptr) {
                    auto song = dynamic_cast<Song^>(grid->DataContext);
                    if (song != nullptr) {
                        ViewModels::PlaylistsViewModel::Instance->AddSongToPlaylist(p->Id, song->Id);
                    }
                }
            });
            addToPlaylistSub->Items->Append(item);
        }
    }
}

void TracksPage::OnPlayMenuClicked(Object^ sender, RoutedEventArgs^ e) {
    auto item = dynamic_cast<MenuFlyoutItem^>(sender);
    auto grid = dynamic_cast<Grid^>(dynamic_cast<MenuFlyout^>(item->Parent)->Target);
    if (grid != nullptr) {
        auto song = dynamic_cast<Song^>(grid->DataContext);
        if (song) PlaybackService::Instance->PlaySong(song);
    }
}

void TracksPage::OnAddQueueMenuClicked(Object^ sender, RoutedEventArgs^ e) {
    auto item = dynamic_cast<MenuFlyoutItem^>(sender);
    auto grid = dynamic_cast<Grid^>(dynamic_cast<MenuFlyout^>(item->Parent)->Target);
    if (grid != nullptr) {
        auto song = dynamic_cast<Song^>(grid->DataContext);
        if (song) PlaybackService::Instance->AddToQueue(song);
    }
}

void TracksPage::OnRemoveFromPlaylistClicked(Object^ sender, RoutedEventArgs^ e) {}

void TracksPage::OnFavoritesFilterToggled(Object^ sender, RoutedEventArgs^ e) {
    auto btn = dynamic_cast<Windows::UI::Xaml::Controls::Primitives::ToggleButton^>(sender);
    if (btn != nullptr) {
        _showFavoritesOnly = (btn->IsChecked != nullptr && btn->IsChecked->Value);
        OnFilterOrSortChanged(nullptr, nullptr);
    }
}


void TracksPage::OnMoreButtonClicked(Object^ sender, RoutedEventArgs^ e)
{
    auto btn = dynamic_cast<Button^>(sender);
    if (btn != nullptr) {
        auto grid = dynamic_cast<Grid^>(VisualTreeHelper::GetParent(btn));
        if (grid != nullptr) {
            auto flyout = grid->ContextFlyout;
            if (flyout != nullptr) {
                flyout->ShowAt(grid);
            }
        }
    }
}
