#include "pch.h"
#include "ImageCacheService.h"
#include <ppltasks.h>
#include "DebugLogger.h"

using namespace Opal::Services;
using namespace Platform;
using namespace Windows::Foundation;
using namespace Windows::Storage;
using namespace Windows::UI::Xaml::Media;
using namespace Windows::UI::Xaml::Media::Imaging;
using namespace Windows::Web::Http;
using namespace Windows::Security::Cryptography;
using namespace Windows::Security::Cryptography::Core;
using namespace Windows::Storage::Streams;
using namespace concurrency;

ImageCacheService^ ImageCacheService::_instance = nullptr;

ImageCacheService^ ImageCacheService::Instance::get() {
    if (_instance == nullptr) {
        _instance = ref new ImageCacheService();
    }
    return _instance;
}

ImageCacheService::ImageCacheService() {
    _httpClient = ref new HttpClient();
    _thumbFolder = nullptr;
}

task<StorageFolder^> ImageCacheService::GetThumbFolderAsync() {
    if (_thumbFolder != nullptr) return task_from_result(_thumbFolder);
    
    return create_task(ApplicationData::Current->LocalFolder->CreateFolderAsync("Thumbnails", CreationCollisionOption::OpenIfExists)).then([this](StorageFolder^ folder) {
        _thumbFolder = folder;
        return folder;
    });
}

String^ ImageCacheService::GetCacheFileName(String^ url) {
    if (url == nullptr || url->IsEmpty()) return nullptr;

    auto hashProvider = HashAlgorithmProvider::OpenAlgorithm(HashAlgorithmNames::Md5);
    auto buffer = CryptographicBuffer::ConvertStringToBinary(url, BinaryStringEncoding::Utf8);
    auto hashed = hashProvider->HashData(buffer);
    return CryptographicBuffer::EncodeToHexString(hashed) + ".jpg";
}

IAsyncOperation<String^>^ ImageCacheService::GetImageAsync(String^ url) {
    return create_async([this, url]() -> String^ {
        if (url == nullptr || url->IsEmpty()) return nullptr;

        String^ fileName = GetCacheFileName(url);
        StorageFolder^ localFolder = ApplicationData::Current->LocalFolder;
        
        String^ finalUri = url;

        try {
            StorageFolder^ cacheFolder = create_task(GetThumbFolderAsync()).get();
            
            // Check if exists
            IStorageItem^ item = create_task(cacheFolder->TryGetItemAsync(fileName)).get();
            if (item != nullptr) {
                finalUri = "ms-appdata:///local/Thumbnails/" + fileName;
            } else {
                // Download
                auto response = create_task(_httpClient->GetAsync(ref new Uri(url))).get();
                if (response->IsSuccessStatusCode) {
                    auto file = create_task(cacheFolder->CreateFileAsync(fileName, CreationCollisionOption::ReplaceExisting)).get();
                    auto buffer = create_task(response->Content->ReadAsBufferAsync()).get();
                    create_task(FileIO::WriteBufferAsync(file, buffer)).get();
                    finalUri = "ms-appdata:///local/Thumbnails/" + fileName;
                }
            }
        }
        catch (Exception^ ex) {
            DebugLogger::Instance->LogException("ImageCacheService::GetImageAsync for " + url, ex);
        }

        return finalUri;
    });
}

IAsyncAction^ ImageCacheService::PreCacheImageAsync(String^ url) {
    return create_async([this, url]() -> task<void> {
        if (url == nullptr || url->IsEmpty()) return task_from_result();

        String^ fileName = GetCacheFileName(url);
        StorageFolder^ localFolder = ApplicationData::Current->LocalFolder;

        try {
            StorageFolder^ cacheFolder = create_task(GetThumbFolderAsync()).get();
            IStorageItem^ item = create_task(cacheFolder->TryGetItemAsync(fileName)).get();
            if (item != nullptr) return task_from_result(); // Already cached
            
            auto response = create_task(_httpClient->GetAsync(ref new Uri(url))).get();
            if (response->IsSuccessStatusCode) {
                auto file = create_task(cacheFolder->CreateFileAsync(fileName, CreationCollisionOption::ReplaceExisting)).get();
                auto buffer = create_task(response->Content->ReadAsBufferAsync()).get();
                create_task(FileIO::WriteBufferAsync(file, buffer)).get();
            }
        }
        catch (Exception^ ex) {
            DebugLogger::Instance->Log("ImageCacheService", "PreCache Failed for " + url + ": " + ex->Message);
        }
        return task_from_result();
    });
}

IAsyncAction^ ImageCacheService::EnsureInitializedAsync() {
    return create_async([this]() -> task<void> {
        try {
            create_task(GetThumbFolderAsync()).get();
        } catch (...) {}
        return task_from_result();
    });
}

IAsyncAction^ ImageCacheService::ClearCacheAsync() {
    return create_async([]() -> task<void> {
        try {
            StorageFolder^ localFolder = ApplicationData::Current->LocalFolder;
            auto cacheFolder = create_task(localFolder->GetFolderAsync("Thumbnails")).get();
            if (cacheFolder != nullptr) {
                create_task(cacheFolder->DeleteAsync()).get();
            }
        }
        catch (...) {}
        return task_from_result();
    });
}
