#pragma once

#include "LoginPage.g.h"
#include "Services/NavidromeService.h"
#include "Services/SettingsService.h"

namespace Opal
{
    [Windows::Foundation::Metadata::WebHostHidden]
    public ref class LoginPage sealed
    {
    public:
        LoginPage();

    private:
        void OnLoginFieldKeyDown(Platform::Object^ sender, Windows::UI::Xaml::Input::KeyRoutedEventArgs^ e);
        void OnConnectClicked(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
        void OnLoginSuccess();
        void OnPageLoaded(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);


        void TextBlock_SelectionChanged(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
        void ServerTextBox_TextChanged(Platform::Object^ sender, Windows::UI::Xaml::Controls::TextChangedEventArgs^ e);
    };
}
