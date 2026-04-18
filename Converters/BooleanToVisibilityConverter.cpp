#include "pch.h"
#include "BooleanToVisibilityConverter.h"

using namespace Opal::Converters;
using namespace Platform;
using namespace Windows::UI::Xaml;
using namespace Windows::UI::Xaml::Interop;

Object^ BooleanToVisibilityConverter::Convert(Object^ value, TypeName targetType, Object^ parameter, String^ language) {
    auto boxedBool = dynamic_cast<IBox<bool>^>(value);
    bool b = (boxedBool != nullptr && boxedBool->Value);
    
    // Support parameter "Invert"
    String^ param = dynamic_cast<String^>(parameter);
    if (param != nullptr && param == "Invert") {
        b = !b;
    }

    return b ? Visibility::Visible : Visibility::Collapsed;
}

Object^ BooleanToVisibilityConverter::ConvertBack(Object^ value, TypeName targetType, Object^ parameter, String^ language) {
    auto vis = dynamic_cast<IBox<Visibility>^>(value);
    bool b = (vis != nullptr && vis->Value == Visibility::Visible);
    
    String^ param = dynamic_cast<String^>(parameter);
    if (param != nullptr && param == "Invert") {
        b = !b;
    }
    
    return b;
}
