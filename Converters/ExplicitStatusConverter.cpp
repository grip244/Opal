#include "pch.h"
#include "ExplicitStatusConverter.h"

using namespace Opal::Converters;
using namespace Platform;
using namespace Windows::UI::Xaml;
using namespace Windows::UI::Xaml::Interop;

Object^ ExplicitStatusConverter::Convert(Object^ value, TypeName targetType, Object^ parameter, String^ language)
{
    String^ status = dynamic_cast<String^>(value);
    
    if (status != nullptr && status == L"explicit")
    {
        return Visibility::Visible;
    }
    
    return Visibility::Collapsed;
}

Object^ ExplicitStatusConverter::ConvertBack(Object^ value, TypeName targetType, Object^ parameter, String^ language)
{
    throw ref new NotImplementedException();
}
