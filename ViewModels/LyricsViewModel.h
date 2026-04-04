#pragma once

#include <collection.h>
#include "Models/LyricLine.h"
#include <Windows.UI.Xaml.Data.h>

namespace Opal {
    namespace ViewModels {
        [Windows::UI::Xaml::Data::Bindable]
        public ref class LyricsViewModel sealed : Windows::UI::Xaml::Data::INotifyPropertyChanged {
        public:
            static property LyricsViewModel^ Instance
            {
                LyricsViewModel^ get();
            }

            property Windows::Foundation::Collections::IObservableVector<Models::LyricLine^>^ Lines {
                Windows::Foundation::Collections::IObservableVector<Models::LyricLine^>^ get() { return _lines; }
            }
            
            property int ActiveIndex {
                int get() { return _activeIndex; }
                void set(int value);
            }

            void UpdatePosition(uint32 timestampMs);
            void SetLyrics(Platform::String^ rawLrc);

            property bool IsTimed {
                bool get() { return _isTimed; }
                void set(bool value);
            }
            
            property bool NotTimed {
                bool get() { return !_isTimed; }
            }

            virtual event Windows::UI::Xaml::Data::PropertyChangedEventHandler^ PropertyChanged;

        protected:
            void OnPropertyChanged(Platform::String^ propertyName);

        private:
            LyricsViewModel();
            static LyricsViewModel^ _instance;

            int _activeIndex;
            bool _isTimed;
            Platform::Collections::Vector<Models::LyricLine^>^ _lines;
        };
    }
}
