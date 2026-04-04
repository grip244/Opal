#include "pch.h"
#include "LoginPage.xaml.h"
#include <Windows.System.Profile.h>
#include "LibraryPage.xaml.h" // Needed for navigation after login

using namespace Opal;
using namespace Platform;
using namespace concurrency;
using namespace Windows::Foundation;
using namespace Windows::UI::Xaml;
using namespace Windows::UI::Xaml::Controls;
using namespace Windows::UI::Xaml::Navigation;
using namespace Windows::UI::Xaml::Interop;
using namespace Windows::System::Profile;

LoginPage::LoginPage()
{
    InitializeComponent();
    this->Loaded += ref new RoutedEventHandler(this, &LoginPage::OnPageLoaded);
}

void LoginPage::OnPageLoaded(Object^ sender, RoutedEventArgs^ e)
{
    if (AnalyticsInfo::VersionInfo->DeviceFamily == "Windows.Xbox")
    {
        VisualStateManager::GoToState(this, "XboxState", false);
    }

    Platform::String^ savedUser = SettingsService::Instance->GetUsername();
    Platform::String^ savedServer = SettingsService::Instance->GetServer();

    if (savedServer != nullptr && savedServer->Length() > 0) ServerTextBox->Text = savedServer;
    if (savedUser != nullptr && savedUser->Length() > 0) UsernameTextBox->Text = savedUser;
    
    if (savedServer != nullptr && savedUser != nullptr) {
        Platform::String^ savedPass = SettingsService::Instance->GetPassword(savedServer, savedUser);
        if (savedPass != nullptr && savedPass->Length() > 0) {
            PasswordBox->Password = savedPass;
            OnConnectClicked(nullptr, nullptr); // Auto-connect
        }
    }

    ServerTextBox->Focus(Windows::UI::Xaml::FocusState::Programmatic);
}

void LoginPage::OnLoginFieldKeyDown(Object^ sender, Windows::UI::Xaml::Input::KeyRoutedEventArgs^ e)
{
    if (e->Key == Windows::System::VirtualKey::Enter || 
        e->Key == Windows::System::VirtualKey::GamepadMenu || 
        e->Key == (Windows::System::VirtualKey)187) {
        if (sender == ServerTextBox) UsernameTextBox->Focus(Windows::UI::Xaml::FocusState::Programmatic);
        else if (sender == UsernameTextBox) PasswordBox->Focus(Windows::UI::Xaml::FocusState::Programmatic);
        else if (sender == PasswordBox) OnConnectClicked(nullptr, nullptr);
        e->Handled = true;
    }
}

void LoginPage::OnConnectClicked(Object^ sender, RoutedEventArgs^ e)
{
    this->StatusText->Text = "Connecting...";
    this->ProgressRing->IsActive = true;
    this->ConnectButton->IsEnabled = false;

    String^ server = this->ServerTextBox->Text;
    String^ user = this->UsernameTextBox->Text;
    String^ pass = this->PasswordBox->Password;

    auto navService = NavidromeService::Instance;
    create_task(navService->LoginAsync(server, user, pass)).then([this, server, user, pass](bool success) {
        this->Dispatcher->RunAsync(Windows::UI::Core::CoreDispatcherPriority::Normal, ref new Windows::UI::Core::DispatchedHandler([this, success, server, user, pass]() {
            this->ProgressRing->IsActive = false;
            if (success) {
                this->OnLoginSuccess(server, user, pass);
            } else {
                this->ConnectButton->IsEnabled = true;
                this->StatusText->Text = "Error: Could not connect to Navidrome";
            }
        }));
    });
}

void LoginPage::OnLoginSuccess(String^ server, String^ user, String^ pass)
{
    if (RememberMeCheckBox->IsChecked->Value) {
        SettingsService::Instance->SaveServer(server);
        SettingsService::Instance->SaveUsername(user);
        SettingsService::Instance->SavePassword(server, user, pass);
    }

    // Navigate to LibraryPage
    this->Frame->Navigate(Windows::UI::Xaml::Interop::TypeName(LibraryPage::typeid));
}

void Opal::LoginPage::TextBlock_SelectionChanged(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e)
{

}

void Opal::LoginPage::ServerTextBox_TextChanged(Platform::Object^ sender, Windows::UI::Xaml::Controls::TextChangedEventArgs^ e)
{

}
