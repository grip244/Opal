#pragma once

#include "Generated Files/UI/Controls/ThumbnailView.g.h"

namespace Opal
{
    namespace UI
    {
        namespace Controls
        {
            [Windows::Foundation::Metadata::WebHostHidden]
            public ref class ThumbnailView sealed
            {
            public:
                ThumbnailView();

                property Platform::String^ SourceUrl
                {
                    Platform::String^ get();
                    void set(Platform::String^ value);
                }

                property double DecodeWidth
                {
                    double get();
                    void set(double value);
                }

                property Windows::UI::Xaml::CornerRadius ThumbnailCornerRadius
                {
                    Windows::UI::Xaml::CornerRadius get();
                    void set(Windows::UI::Xaml::CornerRadius value);
                }
 
                property bool IsBlurEnabled
                {
                    bool get();
                    void set(bool value);
                }

                static property Windows::UI::Xaml::DependencyProperty^ SourceUrlProperty
                {
                    Windows::UI::Xaml::DependencyProperty^ get() { return _sourceUrlProperty; }
                }

                static property Windows::UI::Xaml::DependencyProperty^ DecodeWidthProperty
                {
                    Windows::UI::Xaml::DependencyProperty^ get() { return _decodeWidthProperty; }
                }

                static property Windows::UI::Xaml::DependencyProperty^ ThumbnailCornerRadiusProperty
                {
                    Windows::UI::Xaml::DependencyProperty^ get() { return _thumbnailCornerRadiusProperty; }
                }
 
                static property Windows::UI::Xaml::DependencyProperty^ IsBlurEnabledProperty
                {
                    Windows::UI::Xaml::DependencyProperty^ get() { return _isBlurEnabledProperty; }
                }

            private:
                static void OnSourceUrlChanged(Windows::UI::Xaml::DependencyObject^ d, Windows::UI::Xaml::DependencyPropertyChangedEventArgs^ e);
                static void OnDecodeWidthChanged(Windows::UI::Xaml::DependencyObject^ d, Windows::UI::Xaml::DependencyPropertyChangedEventArgs^ e);
                static void OnThumbnailCornerRadiusChanged(Windows::UI::Xaml::DependencyObject^ d, Windows::UI::Xaml::DependencyPropertyChangedEventArgs^ e);
                static void OnIsBlurEnabledChanged(Windows::UI::Xaml::DependencyObject^ d, Windows::UI::Xaml::DependencyPropertyChangedEventArgs^ e);
                
                void UpdateImage();
                void UpdateCornerRadius();
                void InitializeComposition();
                void OnImageOpened(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
                void OnImageFailed(Platform::Object^ sender, Windows::UI::Xaml::ExceptionRoutedEventArgs^ e);

                static Windows::UI::Xaml::DependencyProperty^ _sourceUrlProperty;
                static Windows::UI::Xaml::DependencyProperty^ _decodeWidthProperty;
                static Windows::UI::Xaml::DependencyProperty^ _thumbnailCornerRadiusProperty;
                static Windows::UI::Xaml::DependencyProperty^ _isBlurEnabledProperty;

                Windows::UI::Composition::Compositor^ _compositor;
                Windows::UI::Composition::SpriteVisual^ _blurVisual;

                Windows::UI::Xaml::DispatcherTimer^ _debounceTimer;
                void OnTimerTick(Platform::Object^ sender, Platform::Object^ e);
            };
        }
    }
}
