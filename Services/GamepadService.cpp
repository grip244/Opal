#include "pch.h"
#include "GamepadService.h"
#include "Services/PlaybackService.h"
#include "Services/DebugLogger.h"
#include "LibraryPage.xaml.h"
#include "LoginPage.xaml.h"
#include "MainPage.xaml.h"
#include "Services/CastingService.h"
// No explicit include needed for WinUI 2 in C++/CX if referenced via winmd

using namespace Opal;
using namespace Opal::Services;
using namespace Platform;
using namespace Windows::System;
using namespace Windows::UI::Core;
using namespace Windows::UI::Xaml;
using namespace Windows::UI::Xaml::Controls;
using namespace Windows::UI::Xaml::Input;
using namespace Windows::Foundation;
using namespace Microsoft::UI::Xaml::Controls;

GamepadService^ GamepadService::_instance = nullptr;

GamepadService^ GamepadService::Instance::get() {
    if (_instance == nullptr) {
        _instance = ref new GamepadService();
    }
    return _instance;
}

GamepadService::GamepadService() :
    _window(nullptr), _frame(nullptr), _searchBox(nullptr)
{
}

void GamepadService::Initialize(CoreWindow^ window, Frame^ frame, AutoSuggestBox^ searchBox)
{
    _window    = window;
    _frame     = frame;
    _searchBox = searchBox;

    // 1. Register AcceleratorKeyActivated (intercepts keys before framework)
    _acceleratorKeyToken = Window::Current->Dispatcher->AcceleratorKeyActivated +=
        ref new TypedEventHandler<CoreDispatcher^, AcceleratorKeyEventArgs^>(this, &GamepadService::OnAcceleratorKeyActivated);

    // 2. Register KeyDown (standard fallback/bubbling event)
    _keyDownToken = window->KeyDown +=
        ref new TypedEventHandler<CoreWindow^, KeyEventArgs^>(this, &GamepadService::OnKeyDown);

    DebugLogger::Instance->Log("GamepadService", "Initialized — Dual input handlers (Accelerator + KeyDown) registered");
}

void GamepadService::Uninitialize()
{
    if (_window.Get() != nullptr) {
        Window::Current->Dispatcher->AcceleratorKeyActivated -= _acceleratorKeyToken;
        _window.Get()->KeyDown -= _keyDownToken;
        _window = nullptr;
    }
}

bool GamepadService::IsTextInputFocused()
{
    auto focused = FocusManager::GetFocusedElement();
    if (dynamic_cast<TextBox^>(focused)       != nullptr) return true;
    if (dynamic_cast<PasswordBox^>(focused)   != nullptr) return true;
    if (dynamic_cast<AutoSuggestBox^>(focused)!= nullptr) return true;
    return false;
}

void GamepadService::AdjustVolume(double delta)
{
    auto pb = PlaybackService::Instance;
    double newVol = pb->Player->Volume + delta;
    if (newVol < 0.0) newVol = 0.0;
    if (newVol > 1.0) newVol = 1.0;
    pb->Player->Volume = newVol;
}

void GamepadService::OnAcceleratorKeyActivated(CoreDispatcher^ sender, AcceleratorKeyEventArgs^ args)
{
    if (args->EventType != CoreAcceleratorKeyEventType::KeyDown && 
        args->EventType != CoreAcceleratorKeyEventType::SystemKeyDown) 
        return;

    if (ProcessGamepadKey(args->VirtualKey)) {
        args->Handled = true;
    }
}

void GamepadService::OnKeyDown(CoreWindow^ sender, KeyEventArgs^ args)
{
    if (ProcessGamepadKey(args->VirtualKey)) {
        args->Handled = true;
    }
}

bool GamepadService::ProcessGamepadKey(VirtualKey key)
{
    // Handle GamepadMenu (≡) even if text input is focused (to support Virtual Keyboard)
    if (key == VirtualKey::GamepadMenu || key == (VirtualKey)187) {
        if (_frame.Get() != nullptr) {
            auto loginType = Windows::UI::Xaml::Interop::TypeName(Opal::LoginPage::typeid);
            if (_frame.Get()->CurrentSourcePageType.Name == loginType.Name) {
                auto page = dynamic_cast<Opal::LoginPage^>(_frame.Get()->Content);
                if (page != nullptr) {
                    // We'll call a public method we'll add to LoginPage
                    page->HandleStartButton();
                    return true;
                }
            }
        }
    }

    // ----------------------------------------------------------------
    // Text-input context: pass-through to specific page handlers (like LoginPage)
    // ----------------------------------------------------------------
    if (IsTextInputFocused()) {
        // Transport keys (Gamepad-specific) should still work even if a text box is focused
        // This allows Play/Pause/Skip/Volume while the search overlay is active.
        bool isTransport = (key == VirtualKey::GamepadX || 
                          key == VirtualKey::GamepadLeftShoulder || 
                          key == VirtualKey::GamepadRightShoulder ||
                          key == VirtualKey::GamepadLeftTrigger ||
                          key == VirtualKey::GamepadRightTrigger ||
                          key == (VirtualKey)176 || key == (VirtualKey)177 || // Media keys
                          key == (VirtualKey)179 || key == (VirtualKey)178);

        if (!isTransport) return false;
    }

    auto pb = PlaybackService::Instance;

    switch (key) {
    case VirtualKey::GamepadLeftShoulder:       // LB
    case (VirtualKey)177:                       // Media Previous Track
        pb->PreviousSong();
        return true;

    case (VirtualKey)176:                       // Media Next Track
    case VirtualKey::GamepadRightShoulder:      // RB
        pb->NextSong();
        return true;

    case VirtualKey::GamepadX:                  // X
    case VirtualKey::X:                         // Keyboard X fallback
    case VirtualKey::Space:                     // Spacebar fallback
    case (VirtualKey)179:                       // Media Play/Pause
    {
        // Don't trigger Play/Pause on Keyboard 'X' if typing
        if (key == VirtualKey::X && IsTextInputFocused()) return false;
        
        auto session = pb->Player->PlaybackSession;
        auto state = session->PlaybackState;
        bool isActive = (state == Windows::Media::Playback::MediaPlaybackState::Playing || 
                         state == Windows::Media::Playback::MediaPlaybackState::Buffering ||
                         state == Windows::Media::Playback::MediaPlaybackState::Opening);

        if (isActive) {
            pb->Player->Pause();
            if (Opal::CastingService::Instance->IsCastingActive) Opal::CastingService::Instance->SendCommand("PAUSE");
        } else {
            pb->Player->Play();
            if (Opal::CastingService::Instance->IsCastingActive) Opal::CastingService::Instance->SendCommand("PLAY");
        }
        return true;
    }

    case (VirtualKey)178:                       // Media Stop
        pb->Stop(); // Clears queue and hides player bar
        return true; 
        
    case VirtualKey::GamepadY:                  // Y
    case VirtualKey::Y:                         // Keyboard Y fallback
        // Don't trigger Search on Keyboard 'Y' if typing
        if (key == VirtualKey::Y && IsTextInputFocused()) return false;

        if (Windows::System::Profile::AnalyticsInfo::VersionInfo->DeviceFamily == "Windows.Xbox") {
            // Fix: Window::Current->Content is a Frame on Opal, not directly MainPage
            auto frame = dynamic_cast<Frame^>(Window::Current->Content);
            auto root = (frame != nullptr) ? dynamic_cast<MainPage^>(frame->Content) : nullptr;
            if (root != nullptr) {
                root->ToggleXboxSearch();
            }
        }
        else if (_searchBox.Get() != nullptr) {
            _searchBox.Get()->Focus(Windows::UI::Xaml::FocusState::Programmatic);
            
            // If the search box is in a NavigationView, we might need to open the pane
            auto navView = dynamic_cast<Microsoft::UI::Xaml::Controls::NavigationView^>(_frame.Get()->Parent);
            if (navView != nullptr) navView->IsPaneOpen = true;
        }
        return true;

    case VirtualKey::GamepadView:               // View
        if (_frame.Get() != nullptr) {
            auto libType = Windows::UI::Xaml::Interop::TypeName(Opal::LibraryPage::typeid);
            if (_frame.Get()->CurrentSourcePageType.Name != libType.Name) {
                _frame.Get()->Navigate(libType);
            }
            
            auto page = dynamic_cast<Opal::LibraryPage^>(_frame.Get()->Content);
            if (page != nullptr) {
                page->ToggleFullPlayer();
                
                // Fix: Ensure the sidebar (NavigationView Pane) is CLOSED when showing the full player
                auto navView = dynamic_cast<Microsoft::UI::Xaml::Controls::NavigationView^>(_frame.Get()->Parent);
                if (navView != nullptr) {
                    navView->IsPaneOpen = false;
                }
            }
        }
        return true;

    case VirtualKey::GamepadLeftTrigger:        // LT
    case (VirtualKey)174:                       // Raw VolumeDown
        AdjustVolume(-0.10); 
        return true;

    case VirtualKey::GamepadRightTrigger:       // RT
    case (VirtualKey)175:                       // Raw VolumeUp
        AdjustVolume(+0.10); 
        return true;

    default:
        break;
    }

    return false;
}
