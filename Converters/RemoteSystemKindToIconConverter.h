#pragma once

#include <windows.ui.xaml.data.h>

namespace Opal {
    namespace Converters {
        /// <summary>
        /// Maps RemoteSystemKinds to Segoe MDL2 Assets glyphs for visual distinction.
        /// </summary>
        public ref class RemoteSystemKindToIconConverter sealed : Windows::UI::Xaml::Data::IValueConverter {
        public:
            virtual Platform::Object^ Convert(Platform::Object^ value, Windows::UI::Xaml::Interop::TypeName targetType, Platform::Object^ parameter, Platform::String^ language);
            virtual Platform::Object^ ConvertBack(Platform::Object^ value, Windows::UI::Xaml::Interop::TypeName targetType, Platform::Object^ parameter, Platform::String^ language);
        };
    }
}
