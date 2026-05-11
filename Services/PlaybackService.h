#pragma once

#include <set>
#include <string>
#include <collection.h>
#include "Models/SharedModels.h"

namespace Opal {
    ref class PlaybackService;
    delegate void SongChangedHandler(PlaybackService^ sender, Song^ song);
    ref class PlaybackService sealed {
        public:
            static property PlaybackService^ Instance {
                PlaybackService^ get();
            }

            property Windows::Media::Playback::MediaPlayer^ Player {
                Windows::Media::Playback::MediaPlayer^ get() { return _mediaPlayer; }
            }

            property Windows::Foundation::Collections::IObservableVector<Song^>^ Queue {
                Windows::Foundation::Collections::IObservableVector<Song^>^ get() { return _queue; }
            }

            property Song^ CurrentSong {
                Song^ get() { return _currentSong; }
            }

            property unsigned int CurrentIndex {
                unsigned int get() { return _currentIndex; }
                void set(unsigned int value) { _currentIndex = value; }
            }

            void PlaySong(Song^ song);
            void PlayQueue(Windows::Foundation::Collections::IVector<Song^>^ songs, unsigned int startIndex);
            void NextSong();
            void PreviousSong();
            void Shuffle();
            void StartAutoPlayback();
            void SetQueue(Windows::Foundation::Collections::IVector<Song^>^ songs);
            void Stop();

            void UpdateEqualizer(int band, float gain);
            void ToggleEqualizer(bool enabled);
            void SetPreAmp(float gain);

            // Shuffle & Repeat (3.2)
            property bool IsShuffleEnabled {
                bool get() { return _isShuffleEnabled; }
                void set(bool v) { _isShuffleEnabled = v; _playbackList->ShuffleEnabled = v; }
            }
            // 0=Off, 1=RepeatAll, 2=RepeatOne
            property int RepeatMode {
                int get() { return _repeatMode; }
                void set(int v);
            }
            void AddToQueue(Song^ song);
            void RemoveFromQueue(unsigned int index);
            void MoveInQueue(unsigned int fromIndex, unsigned int toIndex);

            event SongChangedHandler^ SongChanged;

        private:
            PlaybackService();
            static PlaybackService^ _instance;

            Windows::Media::Playback::MediaPlayer^ _mediaPlayer;
            Windows::Media::Playback::MediaPlaybackList^ _playbackList;
            Platform::Collections::Vector<Song^>^ _queue;
            Song^ _currentSong;
            unsigned int _currentIndex;
            long long _currentSongStartTime;
            std::set<std::wstring> _playedIds;
            bool _hasScrobbledCurrentSong;
            bool _autoPlayRetryPending;
            bool _isShuffleEnabled;
            int _repeatMode; // 0=Off,1=All,2=One
            Windows::Foundation::Collections::PropertySet^ _eqProps;
            bool _isEqEnabled;

            void OnMediaEnded(Windows::Media::Playback::MediaPlayer^ sender, Platform::Object^ args);
            void OnCurrentItemChanged(Windows::Media::Playback::MediaPlaybackList^ sender, Windows::Media::Playback::CurrentMediaPlaybackItemChangedEventArgs^ args);
            void OnPositionChanged(Windows::Media::Playback::MediaPlaybackSession^ sender, Platform::Object^ args);
            Windows::Media::Playback::MediaPlaybackItem^ CreatePlaybackItem(Song^ song);
    };
}

