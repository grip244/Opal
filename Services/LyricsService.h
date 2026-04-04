#pragma once

#include <collection.h>
#include <ppltasks.h>
#include <string>
#include <vector>
#include "Models/LyricLine.h"

namespace Opal {
    namespace Services {
        public ref class LyricsResult sealed {
        public:
            property Windows::Foundation::Collections::IVector<Models::LyricLine^>^ Lines;
            property bool IsTimed;
        };

        public ref class LyricsService sealed {
        public:
            static LyricsResult^ ParseLrc(Platform::String^ rawLrc);
            static LyricsResult^ ParseOpenSubsonicJson(Platform::String^ jsonStr);

            static Windows::Foundation::IAsyncOperation<LyricsResult^>^ FetchFromLrcLibAsync(Platform::String^ artist, Platform::String^ title, Platform::String^ album, double durationSeconds);
            static Windows::Foundation::IAsyncOperation<LyricsResult^>^ FetchFromNeteaseAsync(Platform::String^ artist, Platform::String^ title);
        };
    }
}
