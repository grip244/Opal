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
#include <vector>
#include <cwctype>

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
    _tracks = ref new Platform::Collections::Vector<Song^>();
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
    create_task(NavidromeService::Instance->GetSongListAsync("getRandomSongs", 500)).then([this](Platform::String^ jsonStr) {
        if (jsonStr != nullptr) {
            this->Dispatcher->RunAsync(Windows::UI::Core::CoreDispatcherPriority::Normal, ref new Windows::UI::Core::DispatchedHandler([this, jsonStr]() {
                try {
                    JsonObject^ rootRaw = JsonObject::Parse(jsonStr);
                    JsonObject^ root = rootRaw->GetNamedObject("subsonic-response", nullptr);
                    if (root != nullptr) {
                        JsonObject^ randomSongsObj = root->GetNamedObject("randomSongs", nullptr);
                        if (randomSongsObj != nullptr) {
                            JsonArray^ songArray = randomSongsObj->GetNamedArray("song", nullptr);
                            _tracks->Clear();
                            _allTracks->Clear();
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
                                Platform::String^ coverArtId = songObj->HasKey("coverArt") ? songObj->GetNamedString("coverArt") : "";
                                song->CoverUrl = NavidromeService::Instance->GetCoverArtUrl(coverArtId, 500);
                                try { song->CoverArt = ref new BitmapImage(ref new Uri(song->CoverUrl)); } catch (...) {}
                                
                                if (songObj->HasKey("starred")) song->IsFavorite = true;
                                else song->IsFavorite = false;
                                
                                if (songObj->HasKey("userRating")) song->Rating = songObj->GetNamedNumber("userRating");
                                else song->Rating = 0.0;
                                
                                _allTracks->Append(song);
                            }
                            this->OnFilterOrSortChanged(nullptr, nullptr);
                        }
                    }
                } catch (...) {}
            }));
        }
    });
}

void TracksPage::OnFilterOrSortChanged(Object^ sender, Object^ e)
{
    if (_allTracks == nullptr || _allTracks->Size == 0) return;

    // 1. Text Filter
    String^ query = FilterBox->Text;
    std::wstring wQuery(query == nullptr ? L"" : query->Data());
    for (auto& c : wQuery) c = towlower(c);

    std::vector<Song^> result;
    for (unsigned int i = 0; i < _allTracks->Size; i++) {
        Song^ song = _allTracks->GetAt(i);
        bool textMatch = true;
        if (wQuery.length() > 0) {
            std::wstring wTitle(song->Title->Data());
            std::wstring wArtist(song->Artist->Data());
            std::wstring wAlbum(song->Album->Data());
            for (auto& c : wTitle) c = towlower(c);
            for (auto& c : wArtist) c = towlower(c);
            for (auto& c : wAlbum) c = towlower(c);
            textMatch = (wTitle.find(wQuery) != std::wstring::npos || 
                         wArtist.find(wQuery) != std::wstring::npos ||
                         wAlbum.find(wQuery) != std::wstring::npos);
        }

        if (textMatch) result.push_back(song);
    }

    // 2. Sort
    int sortIdx = (SortByCombo != nullptr) ? SortByCombo->SelectedIndex : 0;
    std::sort(result.begin(), result.end(), [sortIdx](Song^ a, Song^ b) {
        if (sortIdx == 0) { // Title
            return std::wstring(a->Title->Data()) < std::wstring(b->Title->Data());
        }
        else if (sortIdx == 1) { // Artist
            return std::wstring(a->Artist->Data()) < std::wstring(b->Artist->Data());
        }
        else { // Duration
            return a->DurationInSeconds > b->DurationInSeconds; // Longest first
        }
    });

    auto output = ref new Platform::Collections::Vector<Song^>();
    for (auto item : result) output->Append(item);
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
    
    MenuFlyoutSubItem^ addToPlaylistSub = nullptr;
    for (unsigned int i = 0; i < menu->Items->Size; i++) {
        auto item = menu->Items->GetAt(i);
        if (item->Name == "AddToPlaylistMenu") addToPlaylistSub = dynamic_cast<MenuFlyoutSubItem^>(item);
    }
    
    if (addToPlaylistSub != nullptr) {
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
        if (song) PlaybackService::Instance->Queue->Append(song);
    }
}

void TracksPage::OnRemoveFromPlaylistClicked(Object^ sender, RoutedEventArgs^ e) {}


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
