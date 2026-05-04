#pragma once

#include <collection.h>
#include <ppltasks.h>
#include "Models/LyricLine.h"
#include "LyricsServiceInternal.h"

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
        };
    }
}
