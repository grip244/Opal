#include "pch.h"
#include "TimeSpanToStringConverter.h"

using namespace Opal::Converters;
using namespace Platform;
using namespace Windows::UI::Xaml::Interop;

Object^ TimeSpanToStringConverter::Convert(Object^ value, TypeName targetType, Object^ parameter, String^ language) {
    if (value == nullptr) return "0:00";
    
    double ticks = 0;
    auto box = dynamic_cast<IBox<double>^>(value);
    if (box != nullptr) {
        ticks = box->Value;
    } else {
        auto boxInt = dynamic_cast<IBox<int>^>(value);
        if (boxInt != nullptr) ticks = (double)boxInt->Value;
        else return value->ToString();
    }
    
    // UWP Timespan Ticks are 100ns units
    long long totalSeconds = (long long)(ticks / 10000000);
    
    int hours = (int)(totalSeconds / 3600);
    int minutes = (int)((totalSeconds % 3600) / 60);
    int seconds = (int)(totalSeconds % 60);
    
    wchar_t buf[32];
    if (hours > 0) {
        swprintf_s(buf, L"%d:%02d:%02d", hours, minutes, seconds);
    } else {
        swprintf_s(buf, L"%d:%02d", minutes, seconds);
    }
    
    return ref new String(buf);
}

Object^ TimeSpanToStringConverter::ConvertBack(Object^ value, TypeName targetType, Object^ parameter, String^ language) {
    return nullptr;
}
