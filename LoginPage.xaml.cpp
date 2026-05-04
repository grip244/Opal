#include "pch.h"
#include "LoginPage.xaml.h"
#include <Windows.System.Profile.h>
#include "LibraryPage.xaml.h" // Needed for navigation after login
#include "ViewModels/LibraryViewModel.h"
#include "ViewModels/PlaylistsViewModel.h"
#include "Services/DebugLogger.h"

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
    bool rememberMe = RememberMeCheckBox->IsChecked->Value;

    auto navService = NavidromeService::Instance;
    create_task(navService->LoginAsync(server, user, pass)).then([this, server, user, pass, rememberMe](String^ resultMsg) {
        bool success = (resultMsg == "SUCCESS");
        if (success && rememberMe) {
            // Save credentials on the background thread (Safer for Xbox PasswordVault transitions)
            try {
                SettingsService::Instance->SaveServer(server);
                SettingsService::Instance->SaveUsername(user);
                SettingsService::Instance->SavePassword(server, user, pass);
            } catch (Exception^ ex) { DebugLogger::Instance->LogException("OnConnectClicked (Save Credentials)", ex); }
        }

        auto disp = this->Dispatcher;
        if (disp != nullptr) {
            disp->RunAsync(Windows::UI::Core::CoreDispatcherPriority::Normal, ref new Windows::UI::Core::DispatchedHandler([this, success, resultMsg]() {
                this->ProgressRing->IsActive = false;
                if (success) {
                    this->OnLoginSuccess();
                } else {
                    this->ConnectButton->IsEnabled = true;
                    this->StatusText->Text = ""; // Clear status text as we show the toast

                    String^ title = "Connection Error";
                    String^ sub = "An unknown error occurred.";

                    if (resultMsg == "ERR_INVALID_CREDENTIALS") {
                        title = "Authentication Failed";
                        sub = "The username or password you entered is incorrect. Please try again.";
                    } else if (resultMsg == "ERR_UNAUTHORIZED") {
                        title = "Unauthorized";
                        sub = "Your account does not have permission to access this server.";
                    } else if (resultMsg == "ERR_RATE_LIMIT") {
                        title = "Too Many Attempts";
                        sub = "Server rate limit exceeded. Please wait a few minutes and try again.";
                    } else if (resultMsg == "ERR_NETWORK_FAILURE") {
                        title = "Network Error";
                        sub = "Could not reach the server. Please check your internet connection and server URL.";
                    } else if (resultMsg == "ERR_EMPTY_CREDENTIALS") {
                        title = "Missing Information";
                        sub = "Please enter your server URL, username, and password.";
                    } else if (resultMsg == "ERR_SERVER_CONNECTION") {
                        title = "Server Error";
                        sub = "The server returned an invalid response. Please verify the URL and try again.";
                    }

                    this->ShowErrorToast(title, sub);
                }
            }));
        }
    });
}

void LoginPage::ShowErrorToast(String^ title, String^ message)
{
    ErrorToast->Title = title;
    ErrorToast->Subtitle = message;
    ErrorToast->IsOpen = true;
}

void LoginPage::OnLoginSuccess()
{
    // Defer syncing thumbnails until after navigation/initial load is complete

    // Clear stale data from previous server/session
    ViewModels::LibraryViewModel::Instance->ClearAll();
    ViewModels::PlaylistsViewModel::Instance->Clear();

    // Credentials already saved on background thread
    ViewModels::PlaylistsViewModel::Instance->LoadPlaylistsAsync();
    // Navigate to LibraryPage
    this->Frame->Navigate(Windows::UI::Xaml::Interop::TypeName(LibraryPage::typeid));
}


