#pragma once

namespace Opal {
    ref class DebugLogger sealed {
    public:
        static property DebugLogger^ Instance {
            DebugLogger^ get();
        }

        void Log(Platform::String^ component, Platform::String^ message);
        void LogException(Platform::String^ functionName, Platform::Exception^ ex);
#ifdef _DEBUG
        void StartHttpServer(int port);
#endif

        // Testing Support
        void SetLogFolder(Windows::Storage::IStorageFolder^ folder);
        Platform::String^ GetLastFileError();
        void ResetLastError();

    internal:
        DebugLogger();
        
    private:
        static DebugLogger^ _instance;
        Windows::Storage::IStorageFolder^ _logFolder;
        Platform::String^ _lastFileError;

#ifdef _DEBUG
        Windows::Networking::Sockets::StreamSocketListener^ _listener;

        void OnConnectionReceived(Windows::Networking::Sockets::StreamSocketListener^ sender, Windows::Networking::Sockets::StreamSocketListenerConnectionReceivedEventArgs^ args);
#endif
    };
}
