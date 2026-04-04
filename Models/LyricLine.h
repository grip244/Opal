#pragma once

namespace Opal {
    namespace Models {
        [Windows::UI::Xaml::Data::Bindable]
        public ref class LyricLine sealed {
        public:
            property uint32 TimestampMs;
            property Platform::String^ Text;
        };
    }
}
