#pragma once

#include "Generated Files/SettingsPage.g.h"

namespace Opal
{
    public ref class SettingsPage sealed
    {
    public:
        SettingsPage();

    private:
        bool _isInitializing = false;

        void OnLogoutClicked(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
        void OnAutoPlayToggled(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
        
        void OnEqToggled(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
        void OnEqBandChanged(Platform::Object^ sender, Windows::UI::Xaml::Controls::Primitives::RangeBaseValueChangedEventArgs^ e);
        void OnEqPreAmpChanged(Platform::Object^ sender, Windows::UI::Xaml::Controls::Primitives::RangeBaseValueChangedEventArgs^ e);
        void OnEqPresetSelected(Platform::Object^ sender, Windows::UI::Xaml::Controls::SelectionChangedEventArgs^ e);

        void InitializeEqUI();
        void UpdateValueText(int band, float gain);
        void UpdatePreAmpText(float gain);
    };
}
