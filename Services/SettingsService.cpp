#include "pch.h"
#include "SettingsService.h"
#include "DebugLogger.h"

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
    if (username == nullptr || username->Length() == 0 || password == nullptr) return;

    try {
        auto vault = ref new PasswordVault();
        
        // Try to remove existing credential for this user safely
        try {
            auto creds = vault->RetrieveAll();
            for (auto c : creds) {
                if (c->Resource == "Opal.Password" && c->UserName == username) {
                    vault->Remove(c);
                }
            }
        } catch (...) {
            // It's okay if retrieval/removal fails if it didn't exist
        }

        vault->Add(ref new PasswordCredential("Opal.Password", username, password));
    }
    catch (Exception^ ex) {
        // Suppress vault errors on Xbox to prevent crashes during login transitions
        DebugLogger::Instance->LogException("SettingsService::SavePassword", ex);
    }
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
    if (username == nullptr || username->Length() == 0) return "";
    
    try
    {
        auto vault = ref new PasswordVault();
        auto creds = vault->RetrieveAll();
        for (auto cred : creds) {
            if (cred->Resource == "Opal.Password" && cred->UserName == username) {
                cred->RetrievePassword();
                return cred->Password;
            }
        }
    }
    catch (Exception^ ex) {
        DebugLogger::Instance->LogException("SettingsService::GetPassword", ex);
        return "";
    }
    catch (...) {
        DebugLogger::Instance->Log("SettingsService", "Unknown exception in GetPassword");
        return "";
    }
    return "";
}

void SettingsService::ClearCredentials(String^ server, String^ username)
{
    auto vault = ref new PasswordVault();
    try {
        vault->Remove(vault->Retrieve("Opal.Password", username));
    } catch (...) {
        DebugLogger::Instance->Log("SettingsService", "Unknown exception in ClearCredentials");
    }
}

bool SettingsService::IsAutoPlayEnabled::get()
{
    auto vals = ApplicationData::Current->LocalSettings->Values;
    if (vals->HasKey("Opal.AutoPlay")) {
        auto val = vals->Lookup("Opal.AutoPlay");
        return (bool)val;
    }
    return true; // Default to on
}

void SettingsService::IsAutoPlayEnabled::set(bool value)
{
    ApplicationData::Current->LocalSettings->Values->Insert("Opal.AutoPlay", PropertyValue::CreateBoolean(value));
}

String^ SettingsService::Theme::get()
{
    auto vals = ApplicationData::Current->LocalSettings->Values;
    if (vals->HasKey("Opal.Theme")) return safe_cast<String^>(vals->Lookup("Opal.Theme"));
    return "Default";
}

void SettingsService::Theme::set(String^ value)
{
    ApplicationData::Current->LocalSettings->Values->Insert("Opal.Theme", value);
}
