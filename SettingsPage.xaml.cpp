#include "pch.h"
#include "SettingsPage.xaml.h"
#include "Services/NavidromeService.h"
#include "ViewModels/LibraryViewModel.h"
#include "ViewModels/PlaylistsViewModel.h"
#include "LoginPage.xaml.h"
#include "Services/SettingsService.h"

using namespace Opal;
using namespace Platform;
using namespace Windows::UI::Xaml;
using namespace Windows::UI::Xaml::Controls;

SettingsPage::SettingsPage()
{
    InitializeComponent();
    
    auto nav = NavidromeService::Instance;
    AccountUserText->Text = nav->GetUsername();
    AccountServerText->Text = nav->GetServerUrl();
    
    AutoPlayToggle->IsOn = SettingsService::Instance->IsAutoPlayEnabled;
}

void SettingsPage::OnAutoPlayToggled(Object^ sender, RoutedEventArgs^ e)
{
    SettingsService::Instance->IsAutoPlayEnabled = AutoPlayToggle->IsOn;
}

void SettingsPage::OnLogoutClicked(Object^ sender, RoutedEventArgs^ e)
{
    auto nav = NavidromeService::Instance;
    
    // Clear saved credentials from Vault
    SettingsService::Instance->ClearCredentials(nav->GetServerUrl(), nav->GetUsername());
    
    // Clear stored user/server to prevent auto-fill and auto-connect
    SettingsService::Instance->SaveServer("");
    SettingsService::Instance->SaveUsername("");
    
    // Clear active session
    nav->SetCredentials(nullptr, nullptr, nullptr);

    // Clear UI data
    ViewModels::LibraryViewModel::Instance->ClearAll();
    ViewModels::PlaylistsViewModel::Instance->Clear();

    this->Frame->Navigate(Windows::UI::Xaml::Interop::TypeName(LoginPage::typeid));
    this->Frame->BackStack->Clear();
}
