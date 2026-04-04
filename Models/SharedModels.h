#pragma once

namespace Opal
{
    [Windows::UI::Xaml::Data::Bindable]
    public ref class Song sealed
    {
    public:
        property Platform::String^ Id;
        property Platform::String^ Title;
        property Platform::String^ Artist;
        property Platform::String^ Album;
        property Platform::String^ TrackNumber;
        property Platform::String^ Duration;
        property Windows::UI::Xaml::Media::ImageSource^ CoverArt;
        property Platform::String^ CoverUrl;
        property Platform::String^ StreamUrl;
        property int DiscNumber;
        property int DurationInSeconds;

        Song() {}
    };

    [Windows::UI::Xaml::Data::Bindable]
    public ref class ArtistModel sealed
    {
    public:
        property Platform::String^ Id;
        property Platform::String^ Name;
        property Windows::UI::Xaml::Media::ImageSource^ CoverArt;
        ArtistModel() {}
    };

    [Windows::UI::Xaml::Data::Bindable]
    public ref class AlbumModel sealed
    {
    public:
        property Platform::String^ Id;
        property Platform::String^ Title;
        property Platform::String^ Artist;
        property Platform::String^ Year;
        property Windows::UI::Xaml::Media::ImageSource^ CoverArt;
        AlbumModel() {}
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
}
