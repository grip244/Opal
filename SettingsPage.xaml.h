#pragma once

#include "SettingsPage.g.h"

namespace Opal
{
    public ref class SettingsPage sealed
    {
    public:
        SettingsPage();

    private:
        void OnSyncClicked(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
        void OnClearCacheClicked(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
        void OnLogoutClicked(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
        void OnAutoPlayToggled(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
    };
}
