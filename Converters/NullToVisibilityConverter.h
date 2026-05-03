#pragma once


namespace Opal
{
    namespace Converters
    {
        public ref class NullToVisibilityConverter sealed : Windows::UI::Xaml::Data::IValueConverter
        {
        public:
            virtual Platform::Object^ Convert(Platform::Object^ value, Windows::UI::Xaml::Interop::TypeName targetType, Platform::Object^ parameter, Platform::String^ language)
            {
                bool isNull = (value == nullptr);
                
                auto paramStr = dynamic_cast<Platform::String^>(parameter);
                if (paramStr != nullptr && paramStr == "reverse")
                {
                    return isNull ? Windows::UI::Xaml::Visibility::Visible : Windows::UI::Xaml::Visibility::Collapsed;
                }

                return isNull ? Windows::UI::Xaml::Visibility::Collapsed : Windows::UI::Xaml::Visibility::Visible;
            }

            virtual Platform::Object^ ConvertBack(Platform::Object^ value, Windows::UI::Xaml::Interop::TypeName targetType, Platform::Object^ parameter, Platform::String^ language)
            {
                return nullptr;
            }
        };
    }
}
