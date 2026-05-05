#pragma once

#include <collection.h>
#include <ppltasks.h>

namespace Opal
{
    public ref class SettingsService sealed
    {
    public:
        static property SettingsService^ Instance
        {
            SettingsService^ get();
        }

        void SaveServer(Platform::String^ server);
        void SaveUsername(Platform::String^ username);
        void SavePassword(Platform::String^ server, Platform::String^ username, Platform::String^ password);

        Platform::String^ GetServer();
        Platform::String^ GetUsername();
        Platform::String^ GetPassword(Platform::String^ server, Platform::String^ username);

        void ClearCredentials(Platform::String^ server, Platform::String^ username);
        
        property bool IsAutoPlayEnabled {
            bool get();
            void set(bool value);
        }

        property Platform::String^ Theme {
            Platform::String^ get();
            void set(Platform::String^ value);
        }

    private:
        SettingsService();
        static SettingsService^ _instance;
    };
}
