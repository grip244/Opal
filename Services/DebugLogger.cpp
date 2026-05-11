#include "pch.h"
#include "DebugLogger.h"
#include <mutex>
#include <vector>
#include <string>
#include <sstream>
#include <ppltasks.h>

using namespace Opal;
using namespace Platform;
using namespace Windows::Networking;
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
#ifdef _DEBUG
    _listener = ref new StreamSocketListener();
    _listener->ConnectionReceived += ref new Windows::Foundation::TypedEventHandler<StreamSocketListener^, StreamSocketListenerConnectionReceivedEventArgs^>(this, &DebugLogger::OnConnectionReceived);
#endif
    _logFolder = Windows::Storage::ApplicationData::Current->LocalFolder;
    _lastFileError = nullptr;
}

void DebugLogger::SetLogFolder(Windows::Storage::IStorageFolder^ folder) {
    _logFolder = folder;
}

Platform::String^ DebugLogger::GetLastFileError() {
    return _lastFileError;
}

void DebugLogger::ResetLastError() {
    _lastFileError = nullptr;
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
        auto currentFolder = _logFolder;
        g_lastWriteTask = g_lastWriteTask.then([this, currentFolder, logStr]() {
            if (currentFolder == nullptr) return create_task([] {});

            return create_task(currentFolder->CreateFileAsync("log.txt", Windows::Storage::CreationCollisionOption::OpenIfExists)).then([this, logStr](task<Windows::Storage::StorageFile^> t) {
                try {
                    auto file = t.get();
                    return create_task(Windows::Storage::FileIO::AppendTextAsync(file, ref new String((logStr + L"\r\n").c_str())));
                } catch (Exception^ ex) {
                    _lastFileError = ex->Message;
                    return create_task([]{});
                } catch (...) {
                    _lastFileError = "Unknown error";
                    return create_task([]{});
                }
            }).then([this](task<void> innerTask) {
                try { 
                    innerTask.get(); 
                } catch (Exception^ ex) {
                    _lastFileError = ex->Message;
                } catch (...) {
                    _lastFileError = "Unknown error";
                }
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
    Log("Error", "Error in " + functionName + ": " + ex->HResult.ToString() + " \u2014 " + ex->Message);
}

#ifdef _DEBUG
void DebugLogger::StartHttpServer(int port) {
    try {
        auto hostName = ref new HostName("127.0.0.1");
        create_task(_listener->BindEndpointAsync(hostName, port.ToString())).then([port](task<void> t) {
            try {
                t.get();
                DebugLogger::Instance->Log("DebugLogger", "HTTP Server started on localhost:" + port.ToString());
            } catch (Platform::Exception^ ex) {
                 DebugLogger::Instance->Log("DebugLogger", "Failed to bind: " + ex->Message);
            }
        });
    } catch (...) {}
}

void DebugLogger::OnConnectionReceived(StreamSocketListener^ sender, StreamSocketListenerConnectionReceivedEventArgs^ args) {
    try {
        auto socket = args->Socket;
        auto reader = ref new DataReader(socket->InputStream);
        reader->InputStreamOptions = InputStreamOptions::Partial;

        create_task(reader->LoadAsync(1024)).then([this, socket, reader](unsigned int bytesRead) {
            if (bytesRead == 0) {
                delete reader;
                delete socket;
                return task_from_result();
            }

            String^ request = reader->ReadString(bytesRead);
            std::wstring reqStr = request->Data();

            // Require simple authentication token in the request
            if (reqStr.find(L"auth=OpalDebug") == std::wstring::npos) {
                auto writer = ref new DataWriter(socket->OutputStream);
                std::wstring response = L"HTTP/1.1 401 Unauthorized\r\nContent-Type: text/plain\r\nConnection: close\r\n\r\nUnauthorized. Debug token required.";
                writer->WriteString(ref new String(response.c_str()));
                return create_task(writer->StoreAsync()).then([writer, socket, reader](unsigned int) {
                    delete writer;
                    delete reader;
                    delete socket;
                });
            }

            auto writer = ref new DataWriter(socket->OutputStream);
            std::wstringstream ss;
            {
                std::lock_guard<std::mutex> lock(g_logMutex);
                for (const auto& l : g_logs) {
                    ss << l << L"\r\n";
                }
            }
            std::wstring responseBody = ss.str();

            std::wstring header = L"HTTP/1.1 200 OK\r\nContent-Type: text/plain; charset=utf-8\r\nConnection: close\r\n\r\n";
            std::wstring fullResponse = header + responseBody;

            writer->WriteString(ref new String(fullResponse.c_str()));
            return create_task(writer->StoreAsync()).then([writer, socket, reader](unsigned int) {
                return create_task(writer->FlushAsync()).then([writer, socket, reader](bool) {
                    delete writer;
                    delete reader;
                    delete socket;
                });
            });
        }).then([](task<void> t) {
            try { t.get(); } catch (...) {}
        });
    } catch (...) {}
}
#endif
