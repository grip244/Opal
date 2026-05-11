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

        // EQ Settings
        void SaveEqPreset(int index);
        int GetEqPreset();
        void SaveEqBand(int band, float gain);
        float GetEqBand(int band);
        void SaveEqEnabled(bool enabled);
        bool GetEqEnabled();
        void SaveEqPreAmp(float gain);
        float GetEqPreAmp();

    private:
        SettingsService();
        static SettingsService^ _instance;
    };
}
