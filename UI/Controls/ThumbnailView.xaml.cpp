#include "pch.h"
#include "ThumbnailView.xaml.h"
#include "Services/DebugLogger.h"
#include <Windows.UI.Composition.h>
#include <Windows.UI.Xaml.Hosting.h>
#include <Windows.Foundation.Numerics.h>

using namespace Opal::UI::Controls;
using namespace Platform;
using namespace Windows::Foundation;
using namespace Windows::Foundation::Numerics;
using namespace Windows::UI::Composition;
using namespace Windows::UI::Xaml;
using namespace Windows::UI::Xaml::Hosting;
using namespace Windows::UI::Xaml::Media;
using namespace Windows::UI::Xaml::Media::Imaging;

DependencyProperty^ ThumbnailView::_sourceUrlProperty = DependencyProperty::Register(
    "SourceUrl", String::typeid, ThumbnailView::typeid, ref new PropertyMetadata(nullptr, ref new PropertyChangedCallback(&ThumbnailView::OnSourceUrlChanged)));

DependencyProperty^ ThumbnailView::_decodeWidthProperty = DependencyProperty::Register(
    "DecodeWidth", double::typeid, ThumbnailView::typeid, ref new PropertyMetadata(0.0, ref new PropertyChangedCallback(&ThumbnailView::OnDecodeWidthChanged)));

DependencyProperty^ ThumbnailView::_thumbnailCornerRadiusProperty = DependencyProperty::Register(
    "ThumbnailCornerRadius", Windows::UI::Xaml::CornerRadius::typeid, ThumbnailView::typeid, ref new PropertyMetadata(Windows::UI::Xaml::CornerRadius(12.0), ref new PropertyChangedCallback(&ThumbnailView::OnThumbnailCornerRadiusChanged)));

DependencyProperty^ ThumbnailView::_isBlurEnabledProperty = DependencyProperty::Register(
    "IsBlurEnabled", bool::typeid, ThumbnailView::typeid, ref new PropertyMetadata(false, ref new PropertyChangedCallback(&ThumbnailView::OnIsBlurEnabledChanged)));

ThumbnailView::ThumbnailView()
{
    InitializeComponent();
    InitializeComposition();
    
    VisualStateManager::GoToState(this, "Loading", false);

    _debounceTimer = ref new DispatcherTimer();
    TimeSpan ts; ts.Duration = 100 * 10000LL; // 100ms
    _debounceTimer->Interval = ts;
    _debounceTimer->Tick += ref new EventHandler<Object^>(this, &ThumbnailView::OnTimerTick);

    this->Unloaded += ref new RoutedEventHandler([this](Object^, RoutedEventArgs^) {
        _debounceTimer->Stop();
    });

    auto shimmer = dynamic_cast<Windows::UI::Xaml::Media::Animation::Storyboard^>(Placeholder->Resources->Lookup("ShimmerAnim"));
    if (shimmer) shimmer->Begin();

    UpdateCornerRadius();
}

void ThumbnailView::InitializeComposition()
{
    if (!IsBlurEnabled) {
        BackgroundCanvas->Visibility = Windows::UI::Xaml::Visibility::Collapsed;
        return;
    }

    BackgroundCanvas->Visibility = Windows::UI::Xaml::Visibility::Visible;
    _compositor = ElementCompositionPreview::GetElementVisual(this)->Compositor;
    
    auto blurEffect = ref new Microsoft::Graphics::Canvas::Effects::GaussianBlurEffect();
    blurEffect->Name = "Blur";
    blurEffect->BlurAmount = 90.0f;
    blurEffect->BorderMode = Microsoft::Graphics::Canvas::Effects::EffectBorderMode::Soft;
    blurEffect->Source = ref new CompositionEffectSourceParameter("source");

    auto saturationEffect = ref new Microsoft::Graphics::Canvas::Effects::SaturationEffect();
    saturationEffect->Saturation = 1.85f;
    saturationEffect->Source = blurEffect;

    auto contrastEffect = ref new Microsoft::Graphics::Canvas::Effects::ContrastEffect();
    contrastEffect->Contrast = 0.15f;
    contrastEffect->Source = saturationEffect;

    auto tempEffect = ref new Microsoft::Graphics::Canvas::Effects::TemperatureAndTintEffect();
    tempEffect->Temperature = -0.15f;
    tempEffect->Tint = 0.05f;
    tempEffect->Source = contrastEffect;

    auto factory = _compositor->CreateEffectFactory(tempEffect);
    auto effectBrush = factory->CreateBrush();
    
    _blurVisual = _compositor->CreateSpriteVisual();
    
    Windows::Foundation::Numerics::float2 sizeAdjustment;
    sizeAdjustment.x = 1.0f;
    sizeAdjustment.y = 1.0f;
    _blurVisual->RelativeSizeAdjustment = sizeAdjustment;
    _blurVisual->Brush = effectBrush;

    ElementCompositionPreview::SetElementChildVisual(BackgroundCanvas, _blurVisual);
}

String^ ThumbnailView::SourceUrl::get() { return (String^)GetValue(SourceUrlProperty); }
void ThumbnailView::SourceUrl::set(String^ value) { SetValue(SourceUrlProperty, value); }

double ThumbnailView::DecodeWidth::get() { return (double)GetValue(DecodeWidthProperty); }
void ThumbnailView::DecodeWidth::set(double value) { SetValue(DecodeWidthProperty, value); }

Windows::UI::Xaml::CornerRadius ThumbnailView::ThumbnailCornerRadius::get() { return (Windows::UI::Xaml::CornerRadius)GetValue(ThumbnailCornerRadiusProperty); }
void ThumbnailView::ThumbnailCornerRadius::set(Windows::UI::Xaml::CornerRadius value) { SetValue(ThumbnailCornerRadiusProperty, value); }

bool ThumbnailView::IsBlurEnabled::get() { return (bool)GetValue(IsBlurEnabledProperty); }
void ThumbnailView::IsBlurEnabled::set(bool value) { SetValue(IsBlurEnabledProperty, value); }

void ThumbnailView::OnSourceUrlChanged(DependencyObject^ d, DependencyPropertyChangedEventArgs^ e)
{
    auto control = (ThumbnailView^)d;
    control->UpdateImage();
}

void ThumbnailView::OnDecodeWidthChanged(DependencyObject^ d, DependencyPropertyChangedEventArgs^ e)
{
    auto control = (ThumbnailView^)d;
    control->UpdateImage();
}

void ThumbnailView::OnThumbnailCornerRadiusChanged(DependencyObject^ d, DependencyPropertyChangedEventArgs^ e)
{
    auto control = (ThumbnailView^)d;
    control->UpdateCornerRadius();
}

void ThumbnailView::OnIsBlurEnabledChanged(DependencyObject^ d, DependencyPropertyChangedEventArgs^ e)
{
    auto control = (ThumbnailView^)d;
    control->InitializeComposition();
    control->UpdateImage();
}

void ThumbnailView::UpdateCornerRadius()
{
    RootGrid->CornerRadius = ThumbnailCornerRadius;
    Placeholder->RadiusX = ThumbnailCornerRadius.TopLeft;
    Placeholder->RadiusY = ThumbnailCornerRadius.TopLeft;
}

void ThumbnailView::UpdateImage()
{
    _debounceTimer->Stop();
    
    MainImage->Source = nullptr;
    ElementCompositionPreview::SetElementChildVisual(BackgroundCanvas, nullptr);
    _blurVisual = nullptr;

    if (SourceUrl == nullptr || SourceUrl->IsEmpty())
    {
        VisualStateManager::GoToState(this, "Empty", true);
        return;
    }

    VisualStateManager::GoToState(this, "Loading", true);
    
    auto shimmer = dynamic_cast<Windows::UI::Xaml::Media::Animation::Storyboard^>(Placeholder->Resources->Lookup("ShimmerAnim"));
    if (shimmer) shimmer->Begin();

    _debounceTimer->Start();
}

void ThumbnailView::OnTimerTick(Object^ sender, Object^ e)
{
    _debounceTimer->Stop();

    // Guard: SourceUrl may have been cleared by virtualization recycling
    // between when the timer was started and when this tick fires.
    if (SourceUrl == nullptr || SourceUrl->IsEmpty()) return;

    try
    {
        auto uri = ref new Uri(SourceUrl);

        if (IsBlurEnabled) {
            if (_blurVisual == nullptr) InitializeComposition();
            if (_blurVisual != nullptr) {
                auto surface = LoadedImageSurface::StartLoadFromUri(uri, Size(128, 128));
                auto surfaceBrush = _compositor->CreateSurfaceBrush(surface);
                surfaceBrush->Stretch = CompositionStretch::UniformToFill;
                auto effectBrush = dynamic_cast<CompositionEffectBrush^>(_blurVisual->Brush);
                if (effectBrush != nullptr) effectBrush->SetSourceParameter("source", surfaceBrush);
            }
        }

        if (!IsBlurEnabled || DecodeWidth > 0) {
            auto bitmap = ref new BitmapImage();
            if (DecodeWidth > 0) {
                bitmap->DecodePixelWidth = (int)DecodeWidth;
                bitmap->DecodePixelType = DecodePixelType::Logical;
            }
            bitmap->UriSource = uri;
            MainImage->Source = bitmap;
        }
    }
    catch (Exception^ ex) {
        DebugLogger::Instance->Log("ThumbnailView", "Image load error: " + ex->Message);
    }
}

void ThumbnailView::OnImageOpened(Object^ sender, RoutedEventArgs^ e)
{
    VisualStateManager::GoToState(this, "Loaded", true);
}

void ThumbnailView::OnImageFailed(Object^ sender, ExceptionRoutedEventArgs^ e)
{
    VisualStateManager::GoToState(this, "Error", true);
}
