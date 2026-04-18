#include "pch.h"
#include "SettingsPage.xaml.h"
#include "Services/NavidromeService.h"
#include "Services/ImageCacheService.h"
#include "ViewModels/LibraryViewModel.h"
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
    ServerInfoText->Text = "Connected to: " + nav->GetServerUrl() + " as " + nav->GetUsername();
    
    AutoPlayToggle->IsOn = SettingsService::Instance->IsAutoPlayEnabled;
}

void SettingsPage::OnAutoPlayToggled(Object^ sender, RoutedEventArgs^ e)
{
    SettingsService::Instance->IsAutoPlayEnabled = AutoPlayToggle->IsOn;
}

void SettingsPage::OnSyncClicked(Object^ sender, RoutedEventArgs^ e)
{
    SyncButton->IsEnabled = false;
    StatusText->Text = "Sync started in background...";
    ViewModels::LibraryViewModel::Instance->SyncLibraryThumbnails();
}

void SettingsPage::OnClearCacheClicked(Object^ sender, RoutedEventArgs^ e)
{
    Services::ImageCacheService::Instance->ClearCacheAsync();
    StatusText->Text = "Cache cleared.";
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

    this->Frame->Navigate(Windows::UI::Xaml::Interop::TypeName(LoginPage::typeid));
    this->Frame->BackStack->Clear();
}
