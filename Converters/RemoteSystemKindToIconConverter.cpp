#include "pch.h"
#include "RemoteSystemKindToIconConverter.h"

using namespace Opal::Converters;
using namespace Platform;
using namespace Windows::UI::Xaml;
using namespace Windows::UI::Xaml::Interop;

Object^ RemoteSystemKindToIconConverter::Convert(Object^ value, TypeName targetType, Object^ parameter, String^ language) {
    String^ kind = dynamic_cast<String^>(value);
    
    if (kind == nullptr) return ref new String(L"\xE97A"); // Default Monitor icon

    if (kind == "Xbox") {
        return ref new String(L"\xE990"); // Xbox icon
    } else if (kind == "PC" || kind == "Desktop") {
        return ref new String(L"\xE97A"); // Desktop icon
    } else if (kind == "Phone") {
        return ref new String(L"\xE8EA"); // Phone icon
    }
    
    return ref new String(L"\xE97A");
}

Object^ RemoteSystemKindToIconConverter::ConvertBack(Object^ value, TypeName targetType, Object^ parameter, String^ language) {
    return nullptr;
}
