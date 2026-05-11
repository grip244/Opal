#pragma once
#include <vector>
#include "BiquadFilter.h"

namespace Opal::AudioDSP {
    public ref class EqualizerEffect sealed : Windows::Media::Effects::IBasicAudioEffect {
    public:
        EqualizerEffect();

        // IMediaExtension
        virtual void SetProperties(Windows::Foundation::Collections::IPropertySet^ configuration);

        // IBasicAudioEffect
        virtual property Windows::Foundation::Collections::IVectorView<Windows::Media::MediaProperties::AudioEncodingProperties^>^ SupportedEncodingProperties { 
            Windows::Foundation::Collections::IVectorView<Windows::Media::MediaProperties::AudioEncodingProperties^>^ get(); 
        }
        virtual void SetEncodingProperties(Windows::Media::MediaProperties::AudioEncodingProperties^ encodingProperties);
        virtual void ProcessFrame(Windows::Media::Effects::ProcessAudioFrameContext^ context);
        virtual void Close(Windows::Media::Effects::MediaEffectClosedReason reason);
        virtual void DiscardQueuedFrames();
        virtual property bool UseInputFrameForOutput { 
            bool get() { return false; }
        }

    private:
        Windows::Media::MediaProperties::AudioEncodingProperties^ _encodingProperties;
        Windows::Foundation::Collections::IPropertySet^ _configuration;
        
        std::vector<Opal::AudioDSP::BiquadFilter> _leftFilters;
        std::vector<Opal::AudioDSP::BiquadFilter> _rightFilters;
        float _preAmpGain;
        bool _isEnabled;
        Windows::Foundation::EventRegistrationToken _mapChangedToken;

        void UpdateFilters();
        void OnMapChanged(Windows::Foundation::Collections::IObservableMap<Platform::String^, Platform::Object^>^ sender, Windows::Foundation::Collections::IMapChangedEventArgs<Platform::String^>^ args);

        static const int NumBands = 10;
        static const float Frequencies[10];
    };
}
