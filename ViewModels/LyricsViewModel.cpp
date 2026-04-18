#include "pch.h"
#include "LyricsViewModel.h"
#include "Services/LyricsService.h"

using namespace Opal::ViewModels;
using namespace Opal::Models;
using namespace Opal::Services;
using namespace Platform;
using namespace Platform::Collections;
using namespace Windows::Foundation::Collections;
using namespace Windows::UI::Xaml::Data;

LyricsViewModel^ LyricsViewModel::_instance = nullptr;

LyricsViewModel^ LyricsViewModel::Instance::get() {
    if (_instance == nullptr) {
        _instance = ref new LyricsViewModel();
    }
    return _instance;
}

LyricsViewModel::LyricsViewModel() {
    _lines = ref new Vector<LyricLine^>();
    _activeIndex = -1;
    _isTimed = false;
}

void LyricsViewModel::ActiveIndex::set(int value) {
    if (_activeIndex != value) {
        _activeIndex = value;
        OnPropertyChanged("ActiveIndex");
    }
}

void LyricsViewModel::OnPropertyChanged(String^ propertyName) {
    auto disp = App::MainDispatcher;
    if (disp == nullptr) return;
    if (disp->HasThreadAccess) {
        PropertyChanged(this, ref new PropertyChangedEventArgs(propertyName));
    } else {
        disp->RunAsync(Windows::UI::Core::CoreDispatcherPriority::Normal, ref new Windows::UI::Core::DispatchedHandler([this, propertyName]() {
            PropertyChanged(this, ref new PropertyChangedEventArgs(propertyName));
        }));
    }
}

void LyricsViewModel::IsTimed::set(bool value) {
    if (_isTimed != value) {
        _isTimed = value;
        OnPropertyChanged("IsTimed");
        OnPropertyChanged("NotTimed");
    }
}

void LyricsViewModel::SetLyrics(String^ rawLrc) {
    auto disp = App::MainDispatcher;
    if (disp == nullptr) return;

    disp->RunAsync(Windows::UI::Core::CoreDispatcherPriority::Normal, ref new Windows::UI::Core::DispatchedHandler([this, rawLrc]() {
        _lines->Clear();
        auto result = LyricsService::ParseLrc(rawLrc);
        for (auto line : result->Lines) {
            _lines->Append(line);
        }

        if (_lines->Size == 0) {
            auto line = ref new LyricLine();
            line->Text = "No lyrics found.";
            line->TimestampMs = 0;
            _lines->Append(line);
            IsTimed = false;
        } else {
            IsTimed = result->IsTimed;
        }
        
        ActiveIndex = -1;
    }));
}

void LyricsViewModel::UpdatePosition(uint32 timestampMs) {
    if (!IsTimed || _lines->Size == 0) return;

    // Manual $O(log n)$ binary search to find the last line with timestamp <= current
    int low = 0;
    int high = (int)_lines->Size - 1;
    int foundIndex = -1;

    while (low <= high) {
        int mid = low + (high - low) / 2;
        if (_lines->GetAt(mid)->TimestampMs <= timestampMs) {
            foundIndex = mid;
            low = mid + 1;
        } else {
            high = mid - 1;
        }
    }

    if (foundIndex != -1) {
        ActiveIndex = foundIndex;
    }
}
