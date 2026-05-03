#include "pch.h"
#include "DebugLogger.h"
#include <ppltasks.h>

using namespace Opal;
using namespace Platform;
using namespace Windows::Storage;
using namespace concurrency;

namespace OpalTests {
    // A simple test harness for DebugLogger
    public ref class DebugLoggerTests sealed {
    public:
        static IAsyncOperation<String^>^ RunAllTestsAsync() {
            return create_async([]() -> task<String^> {
                auto logger = DebugLogger::Instance;
                std::wstring report = L"--- DebugLogger Test Report ---\n";
                auto tempFolder = ApplicationData::Current->TemporaryFolder;

                // Scenario 1: Happy path
                logger->ResetLastError();
                logger->SetLogFolder(tempFolder);
                logger->Log("Test", "Happy Path Message");
                
                // Wait for async write task to complete
                wait(200); 

                return create_task(tempFolder->TryGetItemAsync("log.txt")).then([logger, &report, tempFolder](IStorageItem^ item) {
                    if (item != nullptr && item->IsOfType(StorageItemTypes::File)) {
                        report += L"[PASS] Scenario 1: Log file created successfully.\n";
                    } else {
                        report += L"[FAIL] Scenario 1: Log file not found in temp folder.\n";
                    }

                    // Scenario 2: Testing nullptr folder handling
                    logger->ResetLastError();
                    logger->SetLogFolder(nullptr);
                    logger->Log("Test", "Null Folder Message");
                    wait(100);
                    
                    if (logger->GetLastFileError() == nullptr) {
                        report += L"[PASS] Scenario 2: Null folder handled gracefully (no error recorded).\n";
                    } else {
                        report += L"[FAIL] Scenario 2: Unexpected error recorded for null folder.\n";
                    }

                    // Scenario 3 & 4: Testing failure/Access Denied
                    // Note: We'll use the InstallationFolder as it is read-only for the app at runtime
                    auto installFolder = Windows::ApplicationModel::Package::Current->InstalledLocation;
                    logger->ResetLastError();
                    logger->SetLogFolder(installFolder);
                    logger->Log("Test", "Blocked Write Message");
                    
                    wait(200); // Wait for failure

                    auto lastError = logger->GetLastFileError();
                    if (lastError != nullptr && lastError->Length() > 0) {
                        report += L"[PASS] Scenario 3/4: Access Denied captured: " + std::wstring(lastError->Data()) + L"\n";
                    } else {
                        report += L"[FAIL] Scenario 3/4: No error captured when writing to restricted folder.\n";
                    }

                    // Reset to default
                    logger->SetLogFolder(Windows::Storage::ApplicationData::Current->LocalFolder);
                    
                    return ref new String(report.c_str());
                });
            });
        }
    };
}
