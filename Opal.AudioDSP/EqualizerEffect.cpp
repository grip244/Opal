#include "pch.h"
#include "EqualizerEffect.h"
#include <cmath>
#include <algorithm>

using namespace Opal::AudioDSP;
using namespace Platform;
using namespace Windows::Foundation;
using namespace Windows::Foundation::Collections;
using namespace Windows::Media::Effects;
using namespace Windows::Media::MediaProperties;
using namespace Windows::Storage::Streams;

const float EqualizerEffect::Frequencies[10] = { 32, 64, 125, 250, 500, 1000, 2000, 4000, 8000, 16000 };

EqualizerEffect::EqualizerEffect() : 
    _preAmpGain(1.0f),
    _isEnabled(true)
{
    _leftFilters.resize(NumBands);
    _rightFilters.resize(NumBands);
}



IVectorView<AudioEncodingProperties^>^ EqualizerEffect::SupportedEncodingProperties::get() {
    auto props = ref new Platform::Collections::Vector<AudioEncodingProperties^>();
    
    // Support IEEE Float (standard for audio effects)
    auto p1 = AudioEncodingProperties::CreatePcm(44100, 2, 32);
    p1->Subtype = MediaEncodingSubtypes::Float;
    props->Append(p1);

    auto p2 = AudioEncodingProperties::CreatePcm(48000, 2, 32);
    p2->Subtype = MediaEncodingSubtypes::Float;
    props->Append(p2);

    return props->GetView();
}

void EqualizerEffect::SetEncodingProperties(AudioEncodingProperties^ encodingProperties) {
    _encodingProperties = encodingProperties;
    UpdateFilters();
}

void EqualizerEffect::SetProperties(IPropertySet^ configuration) {
    _configuration = configuration;
    if (_configuration != nullptr) {
        _mapChangedToken = _configuration->MapChanged += ref new MapChangedEventHandler<String^, Object^>(this, &EqualizerEffect::OnMapChanged);
        UpdateFilters();
    }
}

void EqualizerEffect::OnMapChanged(IObservableMap<String^, Object^>^ sender, IMapChangedEventArgs<String^>^ args) {
    UpdateFilters();
}

void EqualizerEffect::UpdateFilters() {
    if (_encodingProperties == nullptr) return;

    float sampleRate = (float)_encodingProperties->SampleRate;
    float Q = 1.414f; // Standard Q for 1-octave bands

    if (_configuration != nullptr) {
        if (_configuration->HasKey("IsEnabled")) {
            _isEnabled = (bool)_configuration->Lookup("IsEnabled");
        }
        if (_configuration->HasKey("PreAmp")) {
            _preAmpGain = std::pow(10.0f, (float)_configuration->Lookup("PreAmp") / 20.0f);
        }

        for (int i = 0; i < NumBands; i++) {
            String^ key = "Band" + i;
            float gain = 0.0f;
            if (_configuration->HasKey(key)) {
                gain = (float)_configuration->Lookup(key);
            }
            _leftFilters[i].SetupPeaking(Frequencies[i], sampleRate, Q, gain);
            _rightFilters[i].SetupPeaking(Frequencies[i], sampleRate, Q, gain);
        }
    }
}

void EqualizerEffect::ProcessFrame(ProcessAudioFrameContext^ context) {
    if (!_isEnabled) return;

    auto inputFrame = context->InputFrame;
    auto outputFrame = context->OutputFrame;

    auto inputBuffer = inputFrame->LockBuffer(Windows::Media::AudioBufferAccessMode::Read);
    auto outputBuffer = outputFrame->LockBuffer(Windows::Media::AudioBufferAccessMode::Write);

    IMemoryBufferReference^ inputRef = inputBuffer->CreateReference();
    IMemoryBufferReference^ outputRef = outputBuffer->CreateReference();

    Microsoft::WRL::ComPtr<IMemoryBufferByteAccess> inputByteAccess;
    if (SUCCEEDED(reinterpret_cast<IInspectable*>(inputRef)->QueryInterface(IID_PPV_ARGS(&inputByteAccess)))) {
        byte* inputData;
        uint32 inputCapacity;
        if (SUCCEEDED(inputByteAccess->GetBuffer(&inputData, &inputCapacity))) {
            Microsoft::WRL::ComPtr<IMemoryBufferByteAccess> outputByteAccess;
            if (SUCCEEDED(reinterpret_cast<IInspectable*>(outputRef)->QueryInterface(IID_PPV_ARGS(&outputByteAccess)))) {
                byte* outputData;
                uint32 outputCapacity;
                if (SUCCEEDED(outputByteAccess->GetBuffer(&outputData, &outputCapacity))) {
                    float* fin = (float*)inputData;
                    float* fout = (float*)outputData;

                    uint32 sampleCount = inputBuffer->Length / sizeof(float);

                    for (uint32 i = 0; i < sampleCount; i += 2) {
                        float left = fin[i] * _preAmpGain;
                        float right = fin[i + 1] * _preAmpGain;

                        for (int b = 0; b < NumBands; b++) {
                            left = _leftFilters[b].Process(left);
                            right = _rightFilters[b].Process(right);
                        }

                        fout[i] = left;
                        fout[i + 1] = right;
                    }
                }
            }
        }
    }
}

void EqualizerEffect::Close(MediaEffectClosedReason reason) {
    if (_configuration != nullptr) {
        _configuration->MapChanged -= _mapChangedToken;
        _configuration = nullptr;
    }
}

void EqualizerEffect::DiscardQueuedFrames() {
    for (int i = 0; i < NumBands; i++) {
        _leftFilters[i].Reset();
        _rightFilters[i].Reset();
    }
}
