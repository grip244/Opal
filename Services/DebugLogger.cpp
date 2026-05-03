#include "pch.h"
#include "DebugLogger.h"
#include <mutex>
#include <vector>
#include <string>
#include <ppltasks.h>

using namespace Opal;
using namespace Platform;
using namespace Windows::Networking::Sockets;
using namespace Windows::Storage::Streams;
using namespace concurrency;

DebugLogger^ DebugLogger::_instance = nullptr;
std::mutex g_logMutex;
std::vector<std::wstring> g_logs;

DebugLogger^ DebugLogger::Instance::get() {
    if (_instance == nullptr) {
        _instance = ref new DebugLogger();
    }
    return _instance;
}

DebugLogger::DebugLogger() {
    _listener = ref new StreamSocketListener();
    _listener->ConnectionReceived += ref new Windows::Foundation::TypedEventHandler<StreamSocketListener^, StreamSocketListenerConnectionReceivedEventArgs^>(this, &DebugLogger::OnConnectionReceived);
}

static concurrency::task<void> g_lastWriteTask = concurrency::create_task([]{});

void DebugLogger::Log(String^ component, String^ message) {
    if (component == nullptr) component = "System";
    if (message == nullptr) message = "";

    SYSTEMTIME st;
    GetLocalTime(&st);
    wchar_t timeBuf[64];
    swprintf_s(timeBuf, L"[%02d:%02d:%02d.%03d] ", st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);

    std::wstring logStr(timeBuf);
    logStr += L"[";
    logStr += component->Data();
    logStr += L"] ";
    logStr += message->Data();

    OutputDebugString(logStr.c_str());
    OutputDebugString(L"\n");

    std::lock_guard<std::mutex> lock(g_logMutex);
    g_logs.push_back(logStr);
    
    // Also write to file for guaranteed access (bypasses loopback isolation)
    try {
        auto localFolder = Windows::Storage::ApplicationData::Current->LocalFolder;
        g_lastWriteTask = g_lastWriteTask.then([localFolder, logStr]() {
            return create_task(localFolder->CreateFileAsync("log.txt", Windows::Storage::CreationCollisionOption::OpenIfExists)).then([logStr](task<Windows::Storage::StorageFile^> t) {
                try {
                    auto file = t.get();
                    return create_task(Windows::Storage::FileIO::AppendTextAsync(file, ref new String((logStr + L"\r\n").c_str())));
                } catch (...) {
                    return create_task([]{});
                }
            }).then([](task<void> innerTask) {
                try { innerTask.get(); } catch (...) {}
            });
        });
    } catch (...) {}

    // limit internal storage size
    if (g_logs.size() > 1000) {
        g_logs.erase(g_logs.begin());
    }
}

void DebugLogger::LogException(String^ functionName, Exception^ ex) {
    if (ex == nullptr) return;
    Log("Error", "Error in " + functionName + ": " + ex->HResult.ToString());
}

void DebugLogger::StartHttpServer(int port) {
    try {
        create_task(_listener->BindServiceNameAsync(port.ToString())).then([port](task<void> t) {
            try {
                t.get();
                DebugLogger::Instance->Log("DebugLogger", "HTTP Server started on port " + port.ToString());
            } catch (Platform::Exception^ ex) {
                 DebugLogger::Instance->Log("DebugLogger", "Failed to bind: " + ex->Message);
            }
        });
    } catch (...) {}
}

void DebugLogger::OnConnectionReceived(StreamSocketListener^ sender, StreamSocketListenerConnectionReceivedEventArgs^ args) {
    try {
        auto socket = args->Socket;
        auto writer = ref new DataWriter(socket->OutputStream);

        std::wstring responseBody;
        {
            std::lock_guard<std::mutex> lock(g_logMutex);
            for (const auto& l : g_logs) {
                responseBody += l + L"\r\n";
            }
        }

        std::wstring header = L"HTTP/1.1 200 OK\r\nContent-Type: text/plain; charset=utf-8\r\nConnection: close\r\n\r\n";
        std::wstring fullResponse = header + responseBody;

        writer->WriteString(ref new String(fullResponse.c_str()));
        create_task(writer->StoreAsync()).then([writer, socket](unsigned int) {
            return create_task(writer->FlushAsync());
        }).then([writer, socket](bool) {
            // Clean up resources properly after transfer
            delete writer;
            delete socket;
        }).then([](task<void> t) {
            try { t.get(); } catch (...) {} // Catch final task exceptions
        });
    } catch (...) {}
}
