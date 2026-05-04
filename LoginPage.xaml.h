#pragma once

#include "Generated Files/LoginPage.g.h"
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
        void ShowErrorToast(Platform::String^ title, Platform::String^ message);
        void OnPageLoaded(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);


    };
}
