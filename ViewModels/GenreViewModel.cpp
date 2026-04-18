#include "pch.h"
#include "GenreViewModel.h"
#include "Services/NavidromeService.h"
#include <ppltasks.h>
#include "Services/DebugLogger.h"

using namespace Opal::ViewModels;
using namespace Opal;
using namespace Platform;
using namespace Platform::Collections;
using namespace Windows::Foundation;
using namespace Windows::Foundation::Collections;
using namespace Windows::UI::Xaml::Media;
using namespace Windows::UI;
using namespace concurrency;

GenreViewModel^ GenreViewModel::_instance = nullptr;

GenreViewModel^ GenreViewModel::Instance::get() {
    if (_instance == nullptr) _instance = ref new GenreViewModel();
    return _instance;
}

GenreViewModel::GenreViewModel() {
    _genres = ref new Vector<GenreModel^>();
}

IAsyncAction^ GenreViewModel::LoadGenresAsync() {
    return create_async([this]() {
        create_task(NavidromeService::Instance->GetGenresAsync()).then([this](String^ jsonStr) {
            if (jsonStr == nullptr) return;
            try {
                auto rootRaw = Windows::Data::Json::JsonObject::Parse(jsonStr);
                auto root = rootRaw->GetNamedObject("subsonic-response", nullptr);
                if (root != nullptr && root->HasKey("genres")) {
                    auto genresObj = root->GetNamedObject("genres");
                    auto genresArray = genresObj->GetNamedArray("genre");
                    
                    auto disp = App::MainDispatcher;
                    if (disp != nullptr) {
                        disp->RunAsync(Windows::UI::Core::CoreDispatcherPriority::Normal, ref new Windows::UI::Core::DispatchedHandler([this, genresArray]() {
                            _genres->Clear();
                            
                            // Sophisticated gradient pairs for a premium look
                            struct GradientPair { Color start; Color end; };
                            std::vector<GradientPair> gradients = {
                                { ColorHelper::FromArgb(255, 255, 0, 115), ColorHelper::FromArgb(255, 150, 0, 70) }, // Deep Pink -> Darker Pink
                                { ColorHelper::FromArgb(255, 123, 0, 255), ColorHelper::FromArgb(255, 70, 0, 150) },  // Purple -> Indigo
                                { ColorHelper::FromArgb(255, 0, 183, 255), ColorHelper::FromArgb(255, 0, 100, 180) }, // Sky Blue -> Deep Blue
                                { ColorHelper::FromArgb(255, 0, 255, 145), ColorHelper::FromArgb(255, 0, 150, 80) },  // Seafoam -> Forest
                                { ColorHelper::FromArgb(255, 255, 174, 0), ColorHelper::FromArgb(255, 180, 100, 0) }, // Orange -> Amber
                                { ColorHelper::FromArgb(255, 255, 60, 0),  ColorHelper::FromArgb(255, 150, 30, 0) },  // Red-Orange -> Maroon
                                { ColorHelper::FromArgb(255, 0, 102, 255), ColorHelper::FromArgb(255, 0, 50, 150) },  // Royal Blue -> Navy
                                { ColorHelper::FromArgb(255, 191, 255, 0), ColorHelper::FromArgb(255, 120, 180, 0) }, // Lime -> Olive
                                { ColorHelper::FromArgb(255, 255, 0, 212), ColorHelper::FromArgb(255, 150, 0, 120) }, // Magenta -> Plum
                                { ColorHelper::FromArgb(255, 0, 255, 204), ColorHelper::FromArgb(255, 0, 150, 120) }  // Teal -> Dark Teal
                            };

                            for (unsigned int i = 0; i < genresArray->Size; i++) {
                                auto genreObj = genresArray->GetObjectAt(i);
                                auto genre = ref new GenreModel();
                                genre->Name = genreObj->GetNamedString("value", "Unknown");
                                genre->AlbumCount = (int)genreObj->GetNamedNumber("albumCount", 0);
                                genre->SongCount = (int)genreObj->GetNamedNumber("songCount", 0);
                                
                                // Create a LinearGradientBrush for the card
                                auto pair = gradients[i % gradients.size()];
                                auto lgb = ref new LinearGradientBrush();
                                lgb->StartPoint = Windows::Foundation::Point(0, 0);
                                lgb->EndPoint = Windows::Foundation::Point(1, 1);
                                
                                auto gs1 = ref new GradientStop();
                                gs1->Color = pair.start;
                                gs1->Offset = 0.0;
                                
                                auto gs2 = ref new GradientStop();
                                gs2->Color = pair.end;
                                gs2->Offset = 1.0;
                                
                                lgb->GradientStops->Append(gs1);
                                lgb->GradientStops->Append(gs2);
                                
                                genre->BackgroundColor = lgb;
                                _genres->Append(genre);
                            }
                        }));
                    }
                }
            } catch (Exception^ ex) {
                DebugLogger::Instance->LogException("LoadGenresAsync", ex);
            }
        });
    });
}
