#include "pch.h"
#include "StringFormatConverter.h"

using namespace Opal::Converters;
using namespace Platform;
using namespace Windows::UI::Xaml::Interop;

Object^ StringFormatConverter::Convert(Object^ value, TypeName targetType, Object^ parameter, String^ language) {
    String^ format = dynamic_cast<String^>(parameter);
    if (format == nullptr || format->IsEmpty()) return value != nullptr ? value->ToString() : "";
    
    std::wstring fmt = format->Data();
    std::wstring val = value != nullptr ? value->ToString()->Data() : L"";
    
    size_t pos = fmt.find(L"{0}");
    if (pos != std::wstring::npos) {
        fmt.replace(pos, 3, val);
    }
    
    return ref new String(fmt.c_str());
}

Object^ StringFormatConverter::ConvertBack(Object^ value, TypeName targetType, Object^ parameter, String^ language) {
    return nullptr;
}
