#include "pch.h"
#include "GamepadService.h"
#include "Services/PlaybackService.h"
#include "Services/DebugLogger.h"
#include "LibraryPage.xaml.h"

using namespace Opal;
using namespace Opal::Services;
using namespace Platform;
using namespace Windows::System;
using namespace Windows::UI::Core;
using namespace Windows::UI::Xaml;
using namespace Windows::UI::Xaml::Controls;
using namespace Windows::UI::Xaml::Input;
using namespace Windows::Foundation;

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

    _keyDownToken = window->KeyDown +=
        ref new TypedEventHandler<CoreWindow^, KeyEventArgs^>(this, &GamepadService::OnKeyDown);

    DebugLogger::Instance->Log("GamepadService", "Initialized — global gamepad key handler registered");
}

void GamepadService::Uninitialize()
{
    if (_window.Get() != nullptr) {
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

void GamepadService::OnKeyDown(CoreWindow^ sender, KeyEventArgs^ args)
{
    auto key = args->VirtualKey;

    // ----------------------------------------------------------------
    // Text-input context: suppress all playback/nav keys except Menu.
    // ----------------------------------------------------------------
    if (IsTextInputFocused()) {
        if (key == VirtualKey::GamepadMenu) {
            // Advance to next field or submit
            auto focused = FocusManager::GetFocusedElement();
            auto control = dynamic_cast<Windows::UI::Xaml::Controls::Control^>(focused);
            if (control != nullptr) {
                bool moved = FocusManager::TryMoveFocus(FocusNavigationDirection::Next);
                if (!moved) {
                    // Last field — try to submit (simulate Enter key approach via click)
                    // LoginPage handles GamepadMenu in OnLoginFieldKeyDown directly.
                }
            }
            args->Handled = true;
        }
        // All other gamepad keys: let pass-through so typing works normally
        return;
    }

    // ----------------------------------------------------------------
    // Playback / navigation context
    // ----------------------------------------------------------------
    auto pb = PlaybackService::Instance;

    switch (key) {
    case VirtualKey::GamepadLeftShoulder:       // LB — Previous Track
        pb->PreviousSong();
        args->Handled = true;
        break;

    case VirtualKey::GamepadRightShoulder:      // RB — Next Track
        pb->NextSong();
        args->Handled = true;
        break;

    case VirtualKey::GamepadX:                  // X — Play / Pause
    {
        auto session = pb->Player->PlaybackSession;
        if (session->PlaybackState == Windows::Media::Playback::MediaPlaybackState::Playing)
            pb->Player->Pause();
        else
            pb->Player->Play();
        args->Handled = true;
        break;
    }

    case VirtualKey::GamepadY:                  // Y — Focus Search Box
        if (_searchBox.Get() != nullptr) {
            _searchBox.Get()->Focus(Windows::UI::Xaml::FocusState::Programmatic);
        }
        args->Handled = true;
        break;

    case VirtualKey::GamepadView:               // View — Toggle Now Playing
        if (_frame.Get() != nullptr) {
            auto libType = Windows::UI::Xaml::Interop::TypeName(Opal::LibraryPage::typeid);
            if (_frame.Get()->CurrentSourcePageType.Name != libType.Name) {
                _frame.Get()->Navigate(libType);
            }
            auto page = dynamic_cast<Opal::LibraryPage^>(_frame.Get()->Content);
            if (page != nullptr) page->ToggleFullPlayer();
        }
        args->Handled = true;
        break;

    case VirtualKey::GamepadLeftTrigger:        // LT — Volume Down
        AdjustVolume(-0.05);
        args->Handled = true;
        break;

    case VirtualKey::GamepadRightTrigger:       // RT — Volume Up
        AdjustVolume(+0.05);
        args->Handled = true;
        break;

    case VirtualKey::GamepadMenu:               // Menu — field advance / submit
        // Handled by individual page key handlers (LoginPage, etc.)
        break;

    default:
        break;
    }
}
