#include "pch.h"
#include "PlaybackService.h"
#include "Services/NavidromeService.h"
#include <ppltasks.h>
#include "DebugLogger.h"
#include "CastingService.h"
#include "SettingsService.h"

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
    _mediaPlayer->AudioCategory = MediaPlayerAudioCategory::Media;
    
    _playbackList = ref new MediaPlaybackList();
    _playbackList->AutoRepeatEnabled = false;
    _playbackList->CurrentItemChanged += ref new TypedEventHandler<MediaPlaybackList^, CurrentMediaPlaybackItemChangedEventArgs^>(this, &PlaybackService::OnCurrentItemChanged);
    
    _mediaPlayer->Source = _playbackList;

    // Explicitly enable SMTC controls for modern keyboards and Xbox remote
    auto smtc = _mediaPlayer->SystemMediaTransportControls;
    smtc->IsPlayEnabled = true;
    smtc->IsPauseEnabled = true;
    smtc->IsNextEnabled = true;
    smtc->IsPreviousEnabled = true;

    _mediaPlayer->MediaEnded += ref new TypedEventHandler<MediaPlayer^, Object^>(this, &PlaybackService::OnMediaEnded);
    _mediaPlayer->PlaybackSession->PositionChanged += ref new TypedEventHandler<MediaPlaybackSession^, Object^>(this, &PlaybackService::OnPositionChanged);
    _queue = ref new Vector<Song^>();
    _currentIndex = 0;
    _currentSong = nullptr;
}

void PlaybackService::PlaySong(Song^ song) {
    if (song == nullptr) return;
    
    // Find index in queue
    int index = -1;
    for (unsigned int i = 0; i < _queue->Size; i++) {
        if (_queue->GetAt(i)->Id == song->Id) {
            index = (int)i;
            break;
        }
    }

    if (index != -1) {
        if (_playbackList->CurrentItemIndex != (unsigned int)index) {
            _playbackList->MoveTo((unsigned int)index);
        } else if (_mediaPlayer->PlaybackSession->PlaybackState != MediaPlaybackState::Playing) {
             _mediaPlayer->Play();
        }
    }
}

MediaPlaybackItem^ PlaybackService::CreatePlaybackItem(Song^ song) {
    auto mediaSource = MediaSource::CreateFromUri(ref new Uri(song->StreamUrl));
    auto playbackItem = ref new MediaPlaybackItem(mediaSource);
    auto displayProps = playbackItem->GetDisplayProperties();
    displayProps->Type = Windows::Media::MediaPlaybackType::Music;
    displayProps->MusicProperties->Title = song->Title != nullptr ? song->Title : "";
    displayProps->MusicProperties->Artist = song->Artist != nullptr ? song->Artist : "";
    
    Platform::String^ albumText = song->Album != nullptr ? song->Album : "";
    if (song->Year != nullptr && song->Year->Length() > 0) {
        albumText = albumText + " (" + song->Year + ")";
    }
    displayProps->MusicProperties->AlbumTitle = albumText;
    
    if (song->CoverUrl != nullptr && song->CoverUrl->Length() > 0) {
        try { displayProps->Thumbnail = Windows::Storage::Streams::RandomAccessStreamReference::CreateFromUri(ref new Uri(song->CoverUrl)); } catch(...) { }
    }
    playbackItem->ApplyDisplayProperties(displayProps);
    return playbackItem;
}

void PlaybackService::PlayQueue(IVector<Song^>^ songs, unsigned int startIndex) {
    _playbackList->Items->Clear();
    _queue->Clear();

    for (auto s : songs) {
        _queue->Append(s);
        _playbackList->Items->Append(CreatePlaybackItem(s));
    }

    _playbackList->AutoRepeatEnabled = false; // Re-enforce no-looping
    _currentIndex = startIndex;
    if (_queue->Size > startIndex) {
        _playbackList->MoveTo(startIndex);
        _mediaPlayer->Play();
    }
}

void PlaybackService::NextSong() {
    _playbackList->MoveNext();
}

void PlaybackService::PreviousSong() {
    _playbackList->MovePrevious();
}

void PlaybackService::Shuffle() {
    StartAutoPlayback();
}

void PlaybackService::OnMediaEnded(MediaPlayer^ sender, Object^ args) {
    // If at the end of the list and autoplay is enabled, fetch more
    if (_currentIndex == _queue->Size - 1 && Opal::SettingsService::Instance->IsAutoPlayEnabled) {
        StartAutoPlayback();
    }
}

void PlaybackService::OnCurrentItemChanged(MediaPlaybackList^ sender, CurrentMediaPlaybackItemChangedEventArgs^ args) {
    auto item = sender->CurrentItem;
    if (item == nullptr) return;

    unsigned int index = sender->CurrentItemIndex;
    if (index >= _queue->Size) return;

    _currentIndex = index;
    _currentSong = _queue->GetAt(index);
    _hasScrobbledCurrentSong = false;
    _playedIds.insert(std::wstring(_currentSong->Id->Data()));

    // Notify Navidrome (Now Playing)
    NavidromeService::Instance->ScrobbleAsync(_currentSong->Id, false);

    // Auto Play Proactive Fetch: If we are on the last song, fetch more now for gapless transition
    if (_currentIndex == _queue->Size - 1 && Opal::SettingsService::Instance->IsAutoPlayEnabled) {
        StartAutoPlayback();
    }

    // Notify UI
    auto disp = App::MainDispatcher;
    if (disp != nullptr) {
        disp->RunAsync(Windows::UI::Core::CoreDispatcherPriority::Normal, ref new Windows::UI::Core::DispatchedHandler([this]() {
             SongChanged(this, _currentSong);
        }));
    }
}

void PlaybackService::OnPositionChanged(MediaPlaybackSession^ sender, Object^ args) {
    if (_currentSong != nullptr && !_hasScrobbledCurrentSong) {
        auto dur = sender->NaturalDuration.Duration;
        auto pos = sender->Position.Duration;
        if (dur > 0 && pos >= dur / 2) {
            _hasScrobbledCurrentSong = true;
            NavidromeService::Instance->ScrobbleAsync(_currentSong->Id, true);
        }
    }
}

void PlaybackService::RemoveFromQueue(unsigned int index) {
    if (index >= _queue->Size) return;
    
    _queue->RemoveAt(index);
    _playbackList->Items->RemoveAt(index);

    if (_queue->Size == 0) {
        _currentSong = nullptr;
    }
}

void PlaybackService::MoveInQueue(unsigned int fromIndex, unsigned int toIndex) {
    if (fromIndex >= _queue->Size || toIndex >= _queue->Size) return;
    if (fromIndex == toIndex) return;

    Song^ song = _queue->GetAt(fromIndex);
    auto item = _playbackList->Items->GetAt(fromIndex);
    
    _queue->RemoveAt(fromIndex);
    _playbackList->Items->RemoveAt(fromIndex);

    _queue->InsertAt(toIndex, song);
    _playbackList->Items->InsertAt(toIndex, item);

    // CurrentIndex is updated implicitly by the MediaPlaybackList
}

void PlaybackService::StartAutoPlayback() {
    unsigned int oldSize = _queue->Size;
    create_task(NavidromeService::Instance->GetSongListAsync("getRandomSongs", 200)).then([this, oldSize](Platform::String^ jsonStr) {
        if (jsonStr != nullptr) {
            try {
                auto rootRaw = Windows::Data::Json::JsonObject::Parse(jsonStr);
                auto root = rootRaw->GetNamedObject("subsonic-response", nullptr);
                if (root != nullptr) {
                    auto randomSongs = root->GetNamedObject("randomSongs", nullptr);
                    auto songsArray = randomSongs->GetNamedArray("song", nullptr);
                    
                    auto disp = App::MainDispatcher;
                    if (disp != nullptr) {
                        disp->RunAsync(Windows::UI::Core::CoreDispatcherPriority::Normal, ref new Windows::UI::Core::DispatchedHandler([this, songsArray, oldSize]() {
                            bool resumed = false;
                            for (unsigned int i = 0; i < songsArray->Size; i++) {
                                auto songObj = songsArray->GetObjectAt(i);
                                auto song = ref new Song();
                                song->Id = songObj->GetNamedString("id", "");
                                
                                // Skip if already played recently in this session
                                if (_playedIds.find(std::wstring(song->Id->Data())) != _playedIds.end()) continue;
                                
                                song->Title = songObj->GetNamedString("title", "Unknown Track");
                                song->Artist = songObj->GetNamedString("artist", "Unknown Artist");
                                song->Album = songObj->GetNamedString("album", "Unknown Album");
                                song->StreamUrl = NavidromeService::Instance->GetStreamUrl(song->Id);
                                song->CoverUrl = NavidromeService::Instance->GetCoverArtUrl(songObj->HasKey("coverArt") ? songObj->GetNamedString("coverArt") : songObj->GetNamedString("id", ""), 500);
                                if (songObj->HasKey("duration")) song->DurationInSeconds = (int)songObj->GetNamedNumber("duration");
                                
                                try { song->CoverArt = ref new Windows::UI::Xaml::Media::Imaging::BitmapImage(ref new Windows::Foundation::Uri(song->CoverUrl)); } catch (...) {}
                                
                                _queue->Append(song);
                                _playbackList->Items->Append(CreatePlaybackItem(song));
                                
                                // If the previous queue had already ended and cleared the player, start playing the first new song
                                if (!resumed && _playbackList->CurrentItem == nullptr && oldSize > 0) {
                                    _playbackList->MoveTo(oldSize);
                                    _mediaPlayer->Play();
                                    resumed = true;
                                }
                            }
                            
                            if (_queue->Size == oldSize && songsArray->Size > 0) { 
                                // If everything was a duplicate, clear history and try one more time
                                _playedIds.clear(); 
                                StartAutoPlayback(); 
                            }
                    }));
                    }
                }
            } catch (Exception^ ex) { DebugLogger::Instance->LogException("StartAutoPlayback (JSON)", ex); }
        }
    });
}
