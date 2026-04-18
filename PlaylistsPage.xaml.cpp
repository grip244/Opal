#include "pch.h"
#include "PlaylistsPage.xaml.h"
#include "PlaylistDetailsPage.xaml.h"
#include "LibraryPage.xaml.h"
#include "Services/NavidromeService.h"
#include <Windows.UI.Xaml.Media.h>
#include <ppltasks.h>
#include <Windows.Storage.Pickers.h>
#include <Windows.Storage.Streams.h>

using namespace Opal;
using namespace Platform;
using namespace Windows::UI::Xaml;
using namespace Windows::UI::Xaml::Controls;
using namespace Windows::UI::Xaml::Media;
using namespace concurrency;
using namespace Windows::Storage;
using namespace Windows::Storage::Pickers;
using namespace Windows::Storage::Streams;

PlaylistsPage::PlaylistsPage()
{
    InitializeComponent();
    PlaylistsVM->LoadPlaylistsAsync();
}

void PlaylistsPage::OnPlaylistClick(Object^ sender, ItemClickEventArgs^ e)
{
    auto playlist = dynamic_cast<PlaylistModel^>(e->ClickedItem);
    if (playlist != nullptr)
    {
        // Navigate to the new dedicated playlist experience.
        this->Frame->Navigate(Windows::UI::Xaml::Interop::TypeName(PlaylistDetailsPage::typeid), playlist->Id);
    }
}

void PlaylistsPage::OnCreateNewClick(Object^ sender, RoutedEventArgs^ e)
{
    auto input = ref new TextBox();
    input->PlaceholderText = "Playlist Name";
    input->Margin = Windows::UI::Xaml::Thickness(0, 10, 0, 0);

    auto dialog = ref new ContentDialog();
    dialog->Title = "Create New Playlist";
    dialog->Content = input;
    dialog->PrimaryButtonText = "Create";
    dialog->CloseButtonText = "Cancel";
    dialog->DefaultButton = ContentDialogButton::Primary;

    create_task(dialog->ShowAsync()).then([this, input](ContentDialogResult result) {
        if (result == ContentDialogResult::Primary && input->Text->Length() > 0) {
            PlaylistsVM->CreatePlaylist(input->Text);
        }
    });
}
void PlaylistsPage::OnEditPlaylistClick(Object^ sender, RoutedEventArgs^ e)
{
    auto item = dynamic_cast<MenuFlyoutItem^>(sender);
    if (item == nullptr) return;
    auto playlist = dynamic_cast<PlaylistModel^>(item->DataContext);
    if (playlist == nullptr) return;

    auto dialog = ref new ContentDialog();
    dialog->Title = "Edit Playlist Details";
    dialog->PrimaryButtonText = "Save";
    dialog->SecondaryButtonText = "Cancel";

    auto stack = ref new StackPanel();
    stack->Spacing = 12;
    stack->Padding = Windows::UI::Xaml::Thickness(0, 10, 0, 0);

    auto nameBox = ref new TextBox();
    nameBox->Header = "Name";
    nameBox->Text = playlist->Name;
    nameBox->PlaceholderText = "Enter playlist name";

    auto commentBox = ref new TextBox();
    commentBox->Header = "Comment (Optional)";
    commentBox->Text = playlist->Comment != nullptr ? playlist->Comment : "";
    commentBox->PlaceholderText = "Add a description...";
    commentBox->AcceptsReturn = true;
    commentBox->Height = 80;

    auto publicCheck = ref new CheckBox();
    publicCheck->Content = "Public (visible to others)";
    publicCheck->IsChecked = playlist->IsPublic;

    stack->Children->Append(nameBox);
    stack->Children->Append(commentBox);
    stack->Children->Append(publicCheck);
    dialog->Content = stack;

    create_task(dialog->ShowAsync()).then([this, playlist, nameBox, commentBox, publicCheck](ContentDialogResult result) {
        if (result == ContentDialogResult::Primary) {
            PlaylistsVM->UpdatePlaylistMetadata(playlist->Id, nameBox->Text, commentBox->Text, publicCheck->IsChecked->Value);
        }
    });
}

void PlaylistsPage::OnUploadCoverClick(Object^ sender, RoutedEventArgs^ e)
{
    auto item = dynamic_cast<MenuFlyoutItem^>(sender);
    if (item == nullptr) return;
    auto playlist = dynamic_cast<PlaylistModel^>(item->DataContext);
    if (playlist == nullptr) return;

    auto picker = ref new FileOpenPicker();
    picker->FileTypeFilter->Append(".jpg");
    picker->FileTypeFilter->Append(".jpeg");
    picker->FileTypeFilter->Append(".png");
    picker->SuggestedStartLocation = PickerLocationId::PicturesLibrary;
    picker->ViewMode = PickerViewMode::Thumbnail;

    create_task(picker->PickSingleFileAsync()).then([this, playlist](StorageFile^ file) {
        if (file != nullptr) {
            auto mimeType = file->ContentType;
            create_task(file->OpenAsync(FileAccessMode::Read)).then([this, playlist, mimeType](IRandomAccessStream^ stream) {
                auto reader = ref new DataReader(stream->GetInputStreamAt(0));
                create_task(reader->LoadAsync((uint32)stream->Size)).then([this, playlist, reader, mimeType](uint32 loaded) {
                    auto buffer = reader->ReadBuffer(loaded);
                    PlaylistsVM->UploadPlaylistImage(playlist->Id, buffer, mimeType);
                });
            });
        }
    });
}

void PlaylistsPage::OnDeletePlaylistClick(Object^ sender, RoutedEventArgs^ e)
{
    auto item = dynamic_cast<MenuFlyoutItem^>(sender);
    if (item == nullptr) return;
    auto playlist = dynamic_cast<PlaylistModel^>(item->DataContext);
    if (playlist != nullptr) {
        PlaylistsVM->DeletePlaylist(playlist->Id);
    }
}

void PlaylistsPage::OnMoreButtonClicked(Object^ sender, RoutedEventArgs^ e)
{
    auto btn = dynamic_cast<Button^>(sender);
    if (btn != nullptr) {
        auto parent = VisualTreeHelper::GetParent(btn);
        while (parent != nullptr && dynamic_cast<StackPanel^>(parent) == nullptr) {
            parent = VisualTreeHelper::GetParent(parent);
        }
        auto panel = dynamic_cast<StackPanel^>(parent);
        if (panel != nullptr) {
            auto flyout = panel->ContextFlyout;
            if (flyout != nullptr) {
                flyout->ShowAt(panel);
            }
        }
    }
}
