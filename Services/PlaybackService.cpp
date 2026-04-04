#include "pch.h"
#include "PlaybackService.h"
#include "Services/NavidromeService.h"
#include <ppltasks.h>

using namespace Opal;
using namespace Platform;
using namespace Platform::Collections;
using namespace Windows::Foundation;
using namespace Windows::Foundation::Collections;
using namespace Windows::Media::Playback;
using namespace Windows::Media::Core;
using namespace concurrency;

PlaybackService^ PlaybackService::_instance = nullptr;

PlaybackService^ PlaybackService::Instance::get() {
    if (_instance == nullptr) {
        _instance = ref new PlaybackService();
    }
    return _instance;
}

PlaybackService::PlaybackService() {
    _mediaPlayer = ref new MediaPlayer();
    _mediaPlayer->AutoPlay = true;
    _mediaPlayer->Volume = 1.0;
    _mediaPlayer->MediaEnded += ref new TypedEventHandler<MediaPlayer^, Object^>(this, &PlaybackService::OnMediaEnded);
    _queue = ref new Vector<Song^>();
    _currentIndex = 0;
    _currentSong = nullptr;
}

void PlaybackService::PlaySong(Song^ song) {
    if (song == nullptr) return;
    
    _currentSong = song;
    _playedIds.insert(std::wstring(song->Id->Data()));

    _mediaPlayer->Source = MediaSource::CreateFromUri(ref new Uri(song->StreamUrl));
    _mediaPlayer->Play();
    
    // Notify Navidrome that we've started playing (Now Playing notification)
    NavidromeService::Instance->ScrobbleAsync(song->Id, false);

    auto displayUpdater = _mediaPlayer->SystemMediaTransportControls->DisplayUpdater;
    displayUpdater->Type = Windows::Media::MediaPlaybackType::Music;
    displayUpdater->MusicProperties->Title = song->Title;
    displayUpdater->MusicProperties->Artist = song->Artist;
    displayUpdater->MusicProperties->AlbumTitle = song->Album;
    
    if (song->CoverUrl != nullptr && song->CoverUrl->Length() > 0) {
        try { displayUpdater->Thumbnail = Windows::Storage::Streams::RandomAccessStreamReference::CreateFromUri(ref new Uri(song->CoverUrl)); } catch(...) {}
    }
    displayUpdater->Update();

    SongChanged(this, song);
}

void PlaybackService::PlayQueue(IVector<Song^>^ songs, unsigned int startIndex) {
    if (songs != _queue) {
        auto t = ref new Vector<Song^>();
        for (auto s : songs) t->Append(s);
        _queue->Clear();
        for (auto s : t) _queue->Append(s);
    }
    _currentIndex = startIndex;
    if (_queue->Size > startIndex) PlaySong(_queue->GetAt(startIndex));
}

void PlaybackService::NextSong() {
    if (_queue->Size > _currentIndex + 1) {
        _currentIndex++;
        PlaySong(_queue->GetAt(_currentIndex));
    } else {
        StartAutoPlayback();
    }
}

void PlaybackService::PreviousSong() {
    if (_currentIndex > 0) {
        _currentIndex--;
        PlaySong(_queue->GetAt(_currentIndex));
    }
}

void PlaybackService::Shuffle() {
    StartAutoPlayback();
}

void PlaybackService::OnMediaEnded(MediaPlayer^ sender, Object^ args) {
    if (_currentSong != nullptr) {
        // Send final scrobble to Navidrome once song is completed
        NavidromeService::Instance->ScrobbleAsync(_currentSong->Id, true);
    }
    NextSong();
}

void PlaybackService::StartAutoPlayback() {
    create_task(NavidromeService::Instance->GetSongListAsync("getRandomSongs", 50)).then([this](Platform::String^ jsonStr) {
        if (jsonStr != nullptr) {
            try {
                auto rootRaw = Windows::Data::Json::JsonObject::Parse(jsonStr);
                auto root = rootRaw->GetNamedObject("subsonic-response", nullptr);
                if (root != nullptr) {
                    auto randomSongs = root->GetNamedObject("randomSongs", nullptr);
                    auto songsArray = randomSongs->GetNamedArray("song", nullptr);
                    
                    Windows::ApplicationModel::Core::CoreApplication::MainView->CoreWindow->Dispatcher->RunAsync(Windows::UI::Core::CoreDispatcherPriority::Normal, ref new Windows::UI::Core::DispatchedHandler([this, songsArray]() {
                        _queue->Clear();
                        for (unsigned int i = 0; i < songsArray->Size; i++) {
                            auto songObj = songsArray->GetObjectAt(i);
                            auto song = ref new Song();
                            song->Id = songObj->GetNamedString("id", "");
                            if (_playedIds.find(std::wstring(song->Id->Data())) != _playedIds.end()) continue;
                            song->Title = songObj->GetNamedString("title", "Unknown Track");
                            song->Artist = songObj->GetNamedString("artist", "Unknown Artist");
                            song->Album = songObj->GetNamedString("album", "Unknown Album");
                            song->StreamUrl = NavidromeService::Instance->GetStreamUrl(song->Id);
                            song->CoverUrl = NavidromeService::Instance->GetCoverArtUrl(songObj->GetNamedString("coverArt", ""), 500);
                            if (songObj->HasKey("duration")) song->DurationInSeconds = (int)songObj->GetNamedNumber("duration");
                            try { song->CoverArt = ref new Windows::UI::Xaml::Media::Imaging::BitmapImage(ref new Windows::Foundation::Uri(song->CoverUrl)); } catch (...) {}
                            _queue->Append(song);
                        }
                        if (_queue->Size == 0 && songsArray->Size > 0) { _playedIds.clear(); StartAutoPlayback(); return; }
                        if (_queue->Size > 0) { _currentIndex = 0; PlaySong(_queue->GetAt(0)); }
                    }));
                }
            } catch (...) {}
        }
    });
}
