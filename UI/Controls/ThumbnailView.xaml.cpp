#include "pch.h"
#include "ThumbnailView.xaml.h"

using namespace Opal::UI::Controls;
using namespace Platform;
using namespace Windows::Foundation;
using namespace Windows::UI::Xaml;
using namespace Windows::UI::Xaml::Media;
using namespace Windows::UI::Xaml::Media::Imaging;

DependencyProperty^ ThumbnailView::_sourceUrlProperty = DependencyProperty::Register(
    "SourceUrl", String::typeid, ThumbnailView::typeid, ref new PropertyMetadata(nullptr, ref new PropertyChangedCallback(&ThumbnailView::OnSourceUrlChanged)));

DependencyProperty^ ThumbnailView::_decodeWidthProperty = DependencyProperty::Register(
    "DecodeWidth", double::typeid, ThumbnailView::typeid, ref new PropertyMetadata(0.0, ref new PropertyChangedCallback(&ThumbnailView::OnDecodeWidthChanged)));

DependencyProperty^ ThumbnailView::_thumbnailCornerRadiusProperty = DependencyProperty::Register(
    "ThumbnailCornerRadius", Windows::UI::Xaml::CornerRadius::typeid, ThumbnailView::typeid, ref new PropertyMetadata(Windows::UI::Xaml::CornerRadius(8.0), ref new PropertyChangedCallback(&ThumbnailView::OnThumbnailCornerRadiusChanged)));

ThumbnailView::ThumbnailView()
{
    InitializeComponent();
    VisualStateManager::GoToState(this, "Loading", false);

    _debounceTimer = ref new DispatcherTimer();
    TimeSpan ts; ts.Duration = 150 * 10000LL; // 150ms
    _debounceTimer->Interval = ts;
    _debounceTimer->Tick += ref new EventHandler<Object^>(this, &ThumbnailView::OnTimerTick);

    // Start shimmer animation
    auto shimmer = dynamic_cast<Windows::UI::Xaml::Media::Animation::Storyboard^>(Placeholder->Resources->Lookup("ShimmerAnim"));
    if (shimmer) shimmer->Begin();

    UpdateCornerRadius();
}

String^ ThumbnailView::SourceUrl::get() { return (String^)GetValue(SourceUrlProperty); }
void ThumbnailView::SourceUrl::set(String^ value) { SetValue(SourceUrlProperty, value); }

double ThumbnailView::DecodeWidth::get() { return (double)GetValue(DecodeWidthProperty); }
void ThumbnailView::DecodeWidth::set(double value) { SetValue(DecodeWidthProperty, value); }

Windows::UI::Xaml::CornerRadius ThumbnailView::ThumbnailCornerRadius::get() { return (Windows::UI::Xaml::CornerRadius)GetValue(ThumbnailCornerRadiusProperty); }
void ThumbnailView::ThumbnailCornerRadius::set(Windows::UI::Xaml::CornerRadius value) { SetValue(ThumbnailCornerRadiusProperty, value); }

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

void ThumbnailView::UpdateCornerRadius()
{
    auto radius = ThumbnailCornerRadius;
    RootGrid->CornerRadius = radius;
    Placeholder->RadiusX = radius.TopLeft;
    Placeholder->RadiusY = radius.TopLeft;
}

void ThumbnailView::UpdateImage()
{
    _debounceTimer->Stop();
    
    if (SourceUrl == nullptr || SourceUrl->IsEmpty())
    {
        VisualStateManager::GoToState(this, "Empty", true);
        return;
    }

    VisualStateManager::GoToState(this, "Loading", true);
    _debounceTimer->Start();
}

void ThumbnailView::OnTimerTick(Object^ sender, Object^ e)
{
    _debounceTimer->Stop();
    
    auto uri = ref new Uri(SourceUrl);

    // 1. High-res foreground
    auto bitmap = ref new BitmapImage();
    if (DecodeWidth > 0)
    {
        bitmap->DecodePixelWidth = (int)DecodeWidth;
        bitmap->DecodePixelType = DecodePixelType::Logical;
    }
    bitmap->UriSource = uri;
    MainImage->Source = bitmap;

    // 2. Low-res background (Software blur effect via filtering)
    auto bgBitmap = ref new BitmapImage();
    bgBitmap->DecodePixelWidth = 16; // Super tiny = naturally blurry when scaled
    bgBitmap->UriSource = uri;
    BackgroundImage->Source = bgBitmap;
}

void ThumbnailView::OnImageOpened(Object^ sender, RoutedEventArgs^ e)
{
    VisualStateManager::GoToState(this, "Loaded", true);
}

void ThumbnailView::OnImageFailed(Object^ sender, ExceptionRoutedEventArgs^ e)
{
    VisualStateManager::GoToState(this, "Error", true);
}
