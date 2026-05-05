#pragma once
#include <windows.foundation.h>

namespace Opal
{
    namespace Converters
    {
        public ref class NullToVisibilityConverter sealed : Windows::UI::Xaml::Data::IValueConverter
        {
        public:
            virtual Platform::Object^ Convert(Platform::Object^ value, Windows::UI::Xaml::Interop::TypeName targetType, Platform::Object^ parameter, Platform::String^ language)
            {
                bool isVisible = (value != nullptr);
                
                // Support numeric zero checks
                if (value != nullptr) {
                    auto boxedInt = dynamic_cast<Platform::IBox<unsigned int>^>(value);
                    if (boxedInt != nullptr) isVisible = (boxedInt->Value > 0);
                    else {
                        auto boxedInt2 = dynamic_cast<Platform::IBox<int>^>(value);
                        if (boxedInt2 != nullptr) isVisible = (boxedInt2->Value > 0);
                    }
                }

                auto paramStr = dynamic_cast<Platform::String^>(parameter);
                if (paramStr != nullptr)
                {
                    if (paramStr == "reverse" || paramStr == "zero")
                    {
                        return isVisible ? Windows::UI::Xaml::Visibility::Collapsed : Windows::UI::Xaml::Visibility::Visible;
                    }
                }

                return isVisible ? Windows::UI::Xaml::Visibility::Visible : Windows::UI::Xaml::Visibility::Collapsed;
            }

            virtual Platform::Object^ ConvertBack(Platform::Object^ value, Windows::UI::Xaml::Interop::TypeName targetType, Platform::Object^ parameter, Platform::String^ language)
            {
                return nullptr;
            }
        };
    }
}
