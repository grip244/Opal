#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <functional>

namespace Opal {
    namespace Services {
        struct NativeLyricLine {
            uint32_t TimestampMs;
            std::wstring Text;
        };

        struct NativeLyricsResult {
            std::vector<NativeLyricLine> Lines;
            bool IsTimed;
        };

        class LyricsParser {
        public:
            using ErrorLogger = std::function<void(const std::string&, const std::wstring&)>;
            static NativeLyricsResult ParseLrc(const std::wstring& lyricsW, ErrorLogger logger = nullptr);
        };
    }
}
