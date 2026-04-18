#pragma once
#include <Windows.UI.Xaml.Data.h>

namespace Opal
{
    [Windows::UI::Xaml::Data::Bindable]
    public ref class Song sealed : Windows::UI::Xaml::Data::INotifyPropertyChanged
    {
    private:
        bool _isFavorite;
        double _rating;
        
    public:
        virtual event Windows::UI::Xaml::Data::PropertyChangedEventHandler^ PropertyChanged;
        property Platform::String^ Id;
        property Platform::String^ Title;
        property Platform::String^ Artist;
        property Platform::String^ Album;
        property Platform::String^ TrackNumber;
        property Platform::String^ Year;
        property Platform::String^ Duration;
        property Windows::UI::Xaml::Media::ImageSource^ CoverArt;
        property Platform::String^ CoverUrl;
        property Platform::String^ StreamUrl;
        property int DiscNumber;
        property int DurationInSeconds;
        property int PlaylistIndex;
        property Platform::String^ ExplicitStatus;
        
        property Platform::String^ DurationStr {
            Platform::String^ get() {
                int mins = DurationInSeconds / 60;
                int secs = DurationInSeconds % 60;
                return mins.ToString() + ":" + (secs < 10 ? "0" : "") + secs.ToString();
            }
        }
        
        property bool IsFavorite {
            bool get() { return _isFavorite; }
            void set(bool value) {
                if (_isFavorite != value) {
                    _isFavorite = value;
                    NotifyPropertyChanged("IsFavorite");
                }
            }
        }
        
        property double Rating {
            double get() { return _rating; }
            void set(double value) {
                if (_rating != value) {
                    _rating = value;
                    NotifyPropertyChanged("Rating");
                }
            }
        }

        void NotifyPropertyChanged(Platform::String^ prop) {
            auto disp = App::MainDispatcher;
            if (disp == nullptr) return;
            if (disp->HasThreadAccess) {
                PropertyChanged(this, ref new Windows::UI::Xaml::Data::PropertyChangedEventArgs(prop));
            } else {
                disp->RunAsync(Windows::UI::Core::CoreDispatcherPriority::Normal, ref new Windows::UI::Core::DispatchedHandler([this, prop]() {
                    PropertyChanged(this, ref new Windows::UI::Xaml::Data::PropertyChangedEventArgs(prop));
                }));
            }
        }

        Song() {
            _isFavorite = false;
            _rating = 0;
            Id = "";
            Title = "";
            Artist = "";
            Album = "";
            TrackNumber = "";
            Year = "";
            Duration = "";
            CoverUrl = "";
            StreamUrl = "";
            ExplicitStatus = "";
        }
    };

    [Windows::UI::Xaml::Data::Bindable]
    public ref class ArtistModel sealed : Windows::UI::Xaml::Data::INotifyPropertyChanged
    {
    private:
        Windows::UI::Xaml::Media::ImageSource^ _coverArt;

    public:
        virtual event Windows::UI::Xaml::Data::PropertyChangedEventHandler^ PropertyChanged;
        property Platform::String^ Id;
        property Platform::String^ Name;
        property Windows::UI::Xaml::Media::ImageSource^ CoverArt {
            Windows::UI::Xaml::Media::ImageSource^ get() { return _coverArt; }
            void set(Windows::UI::Xaml::Media::ImageSource^ value) {
                if (_coverArt != value) {
                    _coverArt = value;
                    NotifyPropertyChanged("CoverArt");
                }
            }
        }

        void NotifyPropertyChanged(Platform::String^ prop) {
            auto disp = App::MainDispatcher;
            if (disp == nullptr) return;
            if (disp->HasThreadAccess) {
                PropertyChanged(this, ref new Windows::UI::Xaml::Data::PropertyChangedEventArgs(prop));
            } else {
                disp->RunAsync(Windows::UI::Core::CoreDispatcherPriority::Normal, ref new Windows::UI::Core::DispatchedHandler([this, prop]() {
                    PropertyChanged(this, ref new Windows::UI::Xaml::Data::PropertyChangedEventArgs(prop));
                }));
            }
        }

        ArtistModel() {}
    };

    [Windows::UI::Xaml::Data::Bindable]
    public ref class AlbumID3 sealed : Windows::UI::Xaml::Data::INotifyPropertyChanged
    {
    private:
        Windows::UI::Xaml::Media::ImageSource^ _coverArt;

    public:
        virtual event Windows::UI::Xaml::Data::PropertyChangedEventHandler^ PropertyChanged;
        property Platform::String^ Id;
        property Platform::String^ Title;
        property Platform::String^ Artist;
        property Platform::String^ Year;
        property Windows::UI::Xaml::Media::ImageSource^ CoverArt {
            Windows::UI::Xaml::Media::ImageSource^ get() { return _coverArt; }
            void set(Windows::UI::Xaml::Media::ImageSource^ value) {
                if (_coverArt != value) {
                    _coverArt = value;
                    NotifyPropertyChanged("CoverArt");
                }
            }
        }
        
        // OpenSubsonic Extensions
        property Platform::String^ Version;
        property Platform::String^ ExplicitStatus; // "explicit", "clean", or "" (default)
        property Platform::String^ SortName;
        property Platform::String^ CoverUrl;
        
        void NotifyPropertyChanged(Platform::String^ prop) {
            auto disp = App::MainDispatcher;
            if (disp == nullptr) return;
            if (disp->HasThreadAccess) {
                PropertyChanged(this, ref new Windows::UI::Xaml::Data::PropertyChangedEventArgs(prop));
            } else {
                disp->RunAsync(Windows::UI::Core::CoreDispatcherPriority::Normal, ref new Windows::UI::Core::DispatchedHandler([this, prop]() {
                    PropertyChanged(this, ref new Windows::UI::Xaml::Data::PropertyChangedEventArgs(prop));
                }));
            }
        }

        AlbumID3() {}
    };

    [Windows::UI::Xaml::Data::Bindable]
    public ref class DiscGroup sealed
    {
    public:
        property Platform::String^ Title;
        property Windows::Foundation::Collections::IVector<Song^>^ Items;
        DiscGroup() {
            Items = ref new Platform::Collections::Vector<Song^>();
        }
    };

    [Windows::UI::Xaml::Data::Bindable]
    public ref class SearchResultModel sealed
    {
    public:
        property Platform::String^ Id;
        property Platform::String^ Type; // "Artist", "Album", or "Song"
        property Platform::String^ Title;
        property Platform::String^ Subtitle;
        property Platform::String^ Icon;
        property Windows::UI::Xaml::Media::ImageSource^ Image;
        SearchResultModel() {}
    };

    [Windows::UI::Xaml::Data::Bindable]
    public ref class GenreModel sealed : Windows::UI::Xaml::Data::INotifyPropertyChanged
    {
    public:
        virtual event Windows::UI::Xaml::Data::PropertyChangedEventHandler^ PropertyChanged;
        property Platform::String^ Name;
        property int AlbumCount;
        property int SongCount;
        property Windows::UI::Xaml::Media::Brush^ BackgroundColor;

        void NotifyPropertyChanged(Platform::String^ prop) {
            auto disp = App::MainDispatcher;
            if (disp == nullptr) return;
            if (disp->HasThreadAccess) {
                PropertyChanged(this, ref new Windows::UI::Xaml::Data::PropertyChangedEventArgs(prop));
            } else {
                disp->RunAsync(Windows::UI::Core::CoreDispatcherPriority::Normal, ref new Windows::UI::Core::DispatchedHandler([this, prop]() {
                    PropertyChanged(this, ref new Windows::UI::Xaml::Data::PropertyChangedEventArgs(prop));
                }));
            }
        }

        GenreModel() {}
    };

    [Windows::UI::Xaml::Data::Bindable]
    public ref class RemoteDevice sealed
    {
    public:
        RemoteDevice(Platform::String^ name, Platform::String^ ip) {
            _name = name;
            _ip = ip;
        }

        property Platform::String^ Name {
            Platform::String^ get() { return _name; }
        }

        property Platform::String^ Ip {
            Platform::String^ get() { return _ip; }
        }

    private:
        Platform::String^ _name;
        Platform::String^ _ip;
    };

    public ref class TrackData sealed
    {
    public:
        property Platform::String^ Id;
        property Platform::String^ Title;
        property Platform::String^ Artist;
        property Platform::String^ Album;
        property Platform::String^ CoverUrl;
        property Platform::String^ StreamUrl;
        property Platform::String^ ExplicitStatus;

        TrackData() {
            Id = "";
            Title = "";
            Artist = "";
            Album = "";
            CoverUrl = "";
            StreamUrl = "";
            ExplicitStatus = "";
        }
    };

    [Windows::UI::Xaml::Data::Bindable]
    public ref class PlaylistModel sealed : Windows::UI::Xaml::Data::INotifyPropertyChanged
    {
    private:
        Windows::UI::Xaml::Media::ImageSource^ _coverArt;

    public:
        virtual event Windows::UI::Xaml::Data::PropertyChangedEventHandler^ PropertyChanged;
        property Platform::String^ Id;
        property Platform::String^ Name;
        property int SongCount;
        property Platform::String^ Duration;
        property Platform::String^ Owner;
        property Platform::String^ Comment;
        property bool IsPublic;
        property Windows::UI::Xaml::Media::ImageSource^ CoverArt {
            Windows::UI::Xaml::Media::ImageSource^ get() { return _coverArt; }
            void set(Windows::UI::Xaml::Media::ImageSource^ value) {
                if (_coverArt != value) {
                    _coverArt = value;
                    NotifyPropertyChanged("CoverArt");
                }
            }
        }
        
        void NotifyPropertyChanged(Platform::String^ prop) {
            PropertyChanged(this, ref new Windows::UI::Xaml::Data::PropertyChangedEventArgs(prop));
        }

        PlaylistModel() {}
    };
}
