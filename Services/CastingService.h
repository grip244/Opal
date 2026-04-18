#pragma once

#include "Models/SharedModels.h"

namespace Opal {
    public ref class CastingService sealed {
    public:
        static property CastingService^ Instance {
            CastingService^ get();
        }

        void StartListening();
        property bool IsRemoteControlled;
        void StopListening();
        void CastTrack(Platform::String^ ipAddress, TrackData^ track);
        void SendCommand(Platform::String^ command);
        
        // Discovery Logic
        void StartDiscovery();
        void StopDiscovery();
        property Windows::Foundation::Collections::IObservableVector<RemoteDevice^>^ DiscoveredDevices {
            Windows::Foundation::Collections::IObservableVector<RemoteDevice^>^ get();
        }
        event Windows::Foundation::EventHandler<Platform::Object^>^ DevicesChanged;
        
        property Windows::UI::Core::CoreDispatcher^ Dispatcher;
        
        property bool IsCastingActive;
        property Platform::String^ TargetDeviceIp;

        event Windows::Foundation::EventHandler<TrackData^>^ TrackReceived;
        event Windows::Foundation::EventHandler<Platform::Object^>^ QueueSynced;

    private:
        CastingService();
        static CastingService^ _instance;

        Windows::Networking::Sockets::StreamSocketListener^ _listener;
        bool _isListening;
        bool _isBinding;
        
        // Discovery members
        Windows::Networking::Sockets::DatagramSocket^ _udpSocket;
        Windows::Foundation::Collections::IObservableVector<RemoteDevice^>^ _discoveredDevices;
        Windows::UI::Xaml::DispatcherTimer^ _discoveryTimer;
        void OnUdpMessageReceived(Windows::Networking::Sockets::DatagramSocket^ sender, Windows::Networking::Sockets::DatagramSocketMessageReceivedEventArgs^ args);
        void SendDiscoveryBeacon();
        void OnConnectionReceived(Windows::Networking::Sockets::StreamSocketListener^ sender, Windows::Networking::Sockets::StreamSocketListenerConnectionReceivedEventArgs^ args);
        void ProcessMessage(Platform::String^ rawJson);
        
    };
}
