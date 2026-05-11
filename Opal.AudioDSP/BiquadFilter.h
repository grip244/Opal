#pragma once
#include <cmath>

namespace Opal::AudioDSP {
    class BiquadFilter {
    public:
        enum class FilterType { Peaking };

        BiquadFilter() : b0(0), b1(0), b2(0), a1(0), a2(0), z1(0), z2(0) {}

        void SetupPeaking(float frequency, float sampleRate, float Q, float gainDb) {
            float A = std::pow(10.0f, gainDb / 40.0f);
            float omega = 2.0f * 3.1415926535f * frequency / sampleRate;
            float sinW = std::sin(omega);
            float cosW = std::cos(omega);
            float alpha = sinW / (2.0f * Q);

            float a0 = 1.0f + alpha / A;
            b0 = (1.0f + alpha * A) / a0;
            b1 = (-2.0f * cosW) / a0;
            b2 = (1.0f - alpha * A) / a0;
            a1 = (-2.0f * cosW) / a0;
            a2 = (1.0f - alpha / A) / a0;
        }

        inline float Process(float in) {
            float out = b0 * in + z1;
            z1 = b1 * in - a1 * out + z2;
            z2 = b2 * in - a2 * out;
            return out;
        }

        void Reset() {
            z1 = z2 = 0;
        }

    private:
        float b0, b1, b2, a1, a2;
        float z1, z2;
    };
}
