#include "pch.h"
#include "SettingsService.h"

using namespace Opal;
using namespace Platform;
using namespace Windows::Security::Credentials;
using namespace Windows::Storage;
using namespace Windows::Foundation;

SettingsService^ SettingsService::_instance = nullptr;

SettingsService^ SettingsService::Instance::get()
{
    if (_instance == nullptr)
    {
        _instance = ref new SettingsService();
    }
    return _instance;
}

SettingsService::SettingsService()
{
}

void SettingsService::SaveServer(String^ server)
{
    ApplicationData::Current->LocalSettings->Values->Insert("Opal.Server", server);
}

void SettingsService::SaveUsername(String^ username)
{
    ApplicationData::Current->LocalSettings->Values->Insert("Opal.Username", username);
}

void SettingsService::SavePassword(String^ server, String^ username, String^ password)
{
    auto vault = ref new PasswordVault();
    try { vault->Remove(vault->Retrieve("Opal.Password", username)); } catch (...) {}
    vault->Add(ref new PasswordCredential("Opal.Password", username, password));
}

Platform::String^ SettingsService::GetServer()
{
    auto vals = ApplicationData::Current->LocalSettings->Values;
    if (vals->HasKey("Opal.Server")) return safe_cast<String^>(vals->Lookup("Opal.Server"));
    return "";
}

Platform::String^ SettingsService::GetUsername()
{
    auto vals = ApplicationData::Current->LocalSettings->Values;
    if (vals->HasKey("Opal.Username")) return safe_cast<String^>(vals->Lookup("Opal.Username"));
    return "";
}

Platform::String^ SettingsService::GetPassword(String^ server, String^ username)
{
    auto vault = ref new PasswordVault();
    try
    {
        auto cred = vault->Retrieve("Opal.Password", username);
        cred->RetrievePassword();
        return cred->Password;
    }
    catch (...) { return ""; }
}

void SettingsService::ClearCredentials(String^ server, String^ username)
{
    auto vault = ref new PasswordVault();
    try { vault->Remove(vault->Retrieve("Opal.Password", username)); } catch (...) {}
}
