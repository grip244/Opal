#pragma once

namespace Opal {
    ref class DebugLogger sealed {
    public:
        static property DebugLogger^ Instance {
            DebugLogger^ get();
        }

        void Log(Platform::String^ component, Platform::String^ message);
        void LogException(Platform::String^ functionName, Platform::Exception^ ex);
        void StartHttpServer(int port);

    internal:
        DebugLogger();
        
    private:
        static DebugLogger^ _instance;

        Windows::Networking::Sockets::StreamSocketListener^ _listener;

        void OnConnectionReceived(Windows::Networking::Sockets::StreamSocketListener^ sender, Windows::Networking::Sockets::StreamSocketListenerConnectionReceivedEventArgs^ args);
    };
}
