#pragma once
#include "Models/SharedModels.h"

namespace Opal {
    namespace ViewModels {
        public ref class GenreViewModel sealed {
        public:
            static property GenreViewModel^ Instance {
                GenreViewModel^ get();
            }

            property Windows::Foundation::Collections::IObservableVector<Opal::GenreModel^>^ Genres {
                Windows::Foundation::Collections::IObservableVector<Opal::GenreModel^>^ get() { return _genres; }
            }

            Windows::Foundation::IAsyncAction^ LoadGenresAsync();

        private:
            GenreViewModel();
            static GenreViewModel^ _instance;
            Platform::Collections::Vector<Opal::GenreModel^>^ _genres;
        };
    }
}
