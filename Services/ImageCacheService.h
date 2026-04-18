#pragma once

#include <ppltasks.h>
#include <string>

namespace Opal {
    namespace Services {
        public ref class ImageCacheService sealed {
        public:
            static property ImageCacheService^ Instance {
                ImageCacheService^ get();
            }

            /// <summary>
            /// Gets a cached image for the given URL. Downloads and caches it if it doesn't exist.
            /// </summary>
            Windows::Foundation::IAsyncOperation<Platform::String^>^ GetImageAsync(Platform::String^ url);
            /// <summary>
            /// Proactively downloads an image into the cache without returning the image source.
            /// Useful for background synchronization.
            /// </summary>
            Windows::Foundation::IAsyncAction^ PreCacheImageAsync(Platform::String^ url);
            Windows::Foundation::IAsyncAction^ EnsureInitializedAsync();

            /// <summary>
            /// Clears all cached thumbnails.
            /// </summary>
            Windows::Foundation::IAsyncAction^ ClearCacheAsync();

        private:
            ImageCacheService();
            static ImageCacheService^ _instance;
            
            Platform::String^ GetCacheFileName(Platform::String^ url);
            
            Windows::Web::Http::HttpClient^ _httpClient;
            Windows::Storage::StorageFolder^ _thumbFolder;
            concurrency::task<Windows::Storage::StorageFolder^> GetThumbFolderAsync();
        };
    }
}
