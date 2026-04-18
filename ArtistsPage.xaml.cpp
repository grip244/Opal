#include "pch.h"
#include "ArtistsPage.xaml.h"
#include "LibraryPage.xaml.h"
#include "Services/NavidromeService.h"
#include <ppltasks.h>
#include "Services/DebugLogger.h"

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
                        for (unsigned int k = 0; k < indices->Size; k++) {
                            JsonObject^ indexObj = indices->GetObjectAt(k);
                            JsonArray^ artistArray = indexObj->GetNamedArray("artist", nullptr);
                            for (unsigned int i = 0; i < artistArray->Size; i++) {
                                JsonObject^ artObj = artistArray->GetObjectAt(i);
                                auto am = ref new ArtistModel();
                                am->Id = artObj->GetNamedString("id", "");
                                am->Name = artObj->GetNamedString("name", "Unknown Artist");
                                Platform::String^ coverUrlStr = NavidromeService::Instance->GetCoverArtUrl(am->Id, 500);
                                try { am->CoverArt = ref new BitmapImage(ref new Uri(coverUrlStr)); } catch (Exception^ ex) { DebugLogger::Instance->LogException("LoadArtists (CoverArt)", ex); }
                                _artists->Append(am);
                            }
                        }
                        this->ArtistsGridView->ItemsSource = _artists;
                    }
                } catch (Exception^ ex) { DebugLogger::Instance->LogException("LoadArtists (JSON Parse)", ex); }
            }));
        }
    });
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
