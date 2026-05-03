#include "pch.h"
#include "PlaylistDetailsPage.xaml.h"
#include "Services/NavidromeService.h"
#include "Services/PlaybackService.h"
#include <ppltasks.h>

using namespace Opal;
using namespace Platform;
using namespace Platform::Collections;
using namespace Windows::Foundation;
using namespace Windows::Foundation::Collections;
using namespace Windows::UI::Xaml;
using namespace Windows::UI::Xaml::Controls;
using namespace Windows::UI::Xaml::Navigation;
using namespace Windows::UI::Xaml::Media;
using namespace Windows::UI::Popups;
using namespace concurrency;

PlaylistDetailsPage::PlaylistDetailsPage()
{
    InitializeComponent();
    _songs = ref new Vector<Song^>();
}

void PlaylistDetailsPage::OnNavigatedTo(NavigationEventArgs^ e)
{
    auto param = dynamic_cast<String^>(e->Parameter);
    if (param != nullptr) {
        _playlistId = param;
        LoadPlaylistTracks();
    }
}

void PlaylistDetailsPage::LoadPlaylistTracks()
{
    LoadingRing->Visibility = Windows::UI::Xaml::Visibility::Visible;
    _songs->Clear();

    auto self = this;
    create_task(NavidromeService::Instance->GetPlaylistAsync(_playlistId)).then([self](String^ jsonStr) {
        self->Dispatcher->RunAsync(Windows::UI::Core::CoreDispatcherPriority::Normal, ref new Windows::UI::Core::DispatchedHandler([self, jsonStr]() {
            try {
                auto json = Windows::Data::Json::JsonObject::Parse(jsonStr);
                auto response = json->GetNamedObject("subsonic-response");
                if (response->GetNamedString("status") == "ok") {
                    auto playlistObj = response->GetNamedObject("playlist");
                    self->PlaylistTitle->Text = playlistObj->GetNamedString("name");
                    
                    if (playlistObj->HasKey("entry")) {
                        auto entries = playlistObj->GetNamedArray("entry");
                        for (unsigned int i = 0; i < entries->Size; i++) {
                            auto entry = entries->GetObjectAt(i);
                            auto song = ref new Song();
                            song->Id = entry->GetNamedString("id");
                            song->Title = entry->GetNamedString("title");
                            song->Artist = entry->HasKey("artist") ? entry->GetNamedString("artist") : "";
                            song->Album = entry->HasKey("album") ? entry->GetNamedString("album") : "";
                             song->DurationInSeconds = entry->HasKey("duration") ? (int)entry->GetNamedNumber("duration") : 0;
                             song->PlaylistIndex = i + 1; // 1-indexed for display

                             song->StreamUrl = NavidromeService::Instance->GetStreamUrl(song->Id);
                             Platform::String^ coverArtId = entry->HasKey("coverArt") ? entry->GetNamedString("coverArt") : song->Id;
                             song->CoverUrl = NavidromeService::Instance->GetCoverArtUrl(coverArtId, 500);

                             self->_songs->Append(song);
                        }
                    }

                    int songCount = self->_songs->Size;
                    int totalSeconds = 0;
                    for (auto s : self->_songs) totalSeconds += s->DurationInSeconds;
                    
                    int mins = totalSeconds / 60;
                    int secs = totalSeconds % 60;
                    self->PlaylistDetails->Text = songCount.ToString() + " Songs \x00b7 " + mins.ToString() + ":" + (secs < 10 ? "0" : "") + secs.ToString();
                }
            } catch (...) {}
            self->LoadingRing->Visibility = Windows::UI::Xaml::Visibility::Collapsed;
        }));
    });
}

void PlaylistDetailsPage::OnBackClicked(Object^ sender, RoutedEventArgs^ e)
{
    if (Frame->CanGoBack) Frame->GoBack();
}

void PlaylistDetailsPage::OnTrackClicked(Object^ sender, ItemClickEventArgs^ e)
{
    auto song = dynamic_cast<Song^>(e->ClickedItem);
    if (song != nullptr) {
        unsigned int index = 0;
        if (_songs->IndexOf(song, &index)) {
            PlaybackService::Instance->PlayQueue(_songs, index);
        }
    }
}

void PlaylistDetailsPage::OnPlayAllClicked(Object^ sender, RoutedEventArgs^ e)
{
    if (_songs->Size > 0)
    {
        PlaybackService::Instance->PlayQueue(_songs, 0);
    }
}

void PlaylistDetailsPage::OnDragItemsCompleted(ListViewBase^ sender, DragItemsCompletedEventArgs^ args)
{
    if (args->DropResult == Windows::ApplicationModel::DataTransfer::DataPackageOperation::Move) {
        auto songIds = ref new Vector<String^>();
        // ListView automatically reorders its Items collection
        for (unsigned int i = 0; i < sender->Items->Size; i++) {
            auto s = dynamic_cast<Song^>(sender->Items->GetAt(i));
            if (s != nullptr) {
                songIds->Append(s->Id);
                s->PlaylistIndex = i + 1; // Update display indices
            }
        }

        auto self = this;
        create_task(NavidromeService::Instance->ReorderPlaylistAsync(_playlistId, songIds)).then([self](task<void> prev) {
            try {
                prev.get();
            } catch (Exception^ ex) {
                self->Dispatcher->RunAsync(Windows::UI::Core::CoreDispatcherPriority::Normal, ref new Windows::UI::Core::DispatchedHandler([self, ex]() {
                    auto dialog = ref new MessageDialog("Failed to save new order: " + ex->Message);
                    dialog->ShowAsync();
                }));
            }
        });
    }
}

void PlaylistDetailsPage::OnTrackContextOpening(Object^ sender, Object^ e)
{
    // No specific logic needed here for now as items are static in the flyout
}

void PlaylistDetailsPage::OnPlayMenuClicked(Object^ sender, RoutedEventArgs^ e)
{
    auto item = dynamic_cast<MenuFlyoutItem^>(sender);
    if (item != nullptr) {
        auto song = dynamic_cast<Song^>(item->DataContext);
        if (song != nullptr) {
            unsigned int index = 0;
            if (_songs->IndexOf(song, &index)) {
                PlaybackService::Instance->PlayQueue(_songs, index);
            }
        }
    }
}

void PlaylistDetailsPage::OnAddQueueMenuClicked(Object^ sender, RoutedEventArgs^ e)
{
    auto item = dynamic_cast<MenuFlyoutItem^>(sender);
    if (item != nullptr) {
        auto song = dynamic_cast<Song^>(item->DataContext);
        if (song != nullptr) PlaybackService::Instance->AddToQueue(song);
    }
}

void PlaylistDetailsPage::OnRemoveMenuClicked(Object^ sender, RoutedEventArgs^ e)
{
    auto item = dynamic_cast<MenuFlyoutItem^>(sender);
    if (item != nullptr) {
        auto song = dynamic_cast<Song^>(item->DataContext);
        if (song != nullptr) {
            unsigned int index = 0;
            if (_songs->IndexOf(song, &index)) {
                auto self = this;
                create_task(NavidromeService::Instance->RemoveSongFromPlaylistAsync(_playlistId, (int)index)).then([self](task<void> prev) {
                    try {
                        prev.get();
                        self->Dispatcher->RunAsync(Windows::UI::Core::CoreDispatcherPriority::Normal, ref new Windows::UI::Core::DispatchedHandler([self]() {
                            self->LoadPlaylistTracks(); // Refresh
                        }));
                    } catch (Exception^ ex) {
                        self->Dispatcher->RunAsync(Windows::UI::Core::CoreDispatcherPriority::Normal, ref new Windows::UI::Core::DispatchedHandler([self, ex]() {
                            auto dialog = ref new MessageDialog("Failed to remove song: " + ex->Message);
                            dialog->ShowAsync();
                        }));
                    }
                });
            }
        }
    }
}
