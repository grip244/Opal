#pragma once

#include <collection.h>

namespace Opal {
namespace Services {

    /// <summary>
    /// Centralized singleton that intercepts CoreWindow::KeyDown globally
    /// and dispatches configurable gamepad actions.
    /// 
    /// Default bindings (1.1.5 — hardcoded, no persistence yet):
    ///   LB  -> Previous Track
    ///   RB  -> Next Track
    ///   X   -> Play / Pause
    ///   Y   -> Focus Search Box
    ///   View -> Toggle Now Playing
    ///   LT  -> Volume Down (-5%)
    ///   RT  -> Volume Up   (+5%)
    ///   Menu -> Advance field / Submit (text input context)
    /// </summary>
    public ref class GamepadService sealed
    {
    public:
        static property GamepadService^ Instance {
            GamepadService^ get();
        }

        /// <summary>Register the CoreWindow and navigation frame required for dispatching.</summary>
        void Initialize(Windows::UI::Core::CoreWindow^ window,
                        Windows::UI::Xaml::Controls::Frame^ frame,
                        Windows::UI::Xaml::Controls::AutoSuggestBox^ searchBox);

        void Uninitialize();

    private:
        GamepadService();
        static GamepadService^ _instance;

        Platform::Agile<Windows::UI::Core::CoreWindow> _window;
        Platform::Agile<Windows::UI::Xaml::Controls::Frame> _frame;
        Platform::Agile<Windows::UI::Xaml::Controls::AutoSuggestBox> _searchBox;
        Windows::Foundation::EventRegistrationToken _acceleratorKeyToken;
        Windows::Foundation::EventRegistrationToken _keyDownToken;

        void OnAcceleratorKeyActivated(Windows::UI::Core::CoreDispatcher^ sender,
                                       Windows::UI::Core::AcceleratorKeyEventArgs^ args);
        void OnKeyDown(Windows::UI::Core::CoreWindow^ sender,
                       Windows::UI::Core::KeyEventArgs^ args);

        bool ProcessGamepadKey(Windows::System::VirtualKey key);

        bool IsTextInputFocused();
        void AdjustVolume(double delta);
    };

} // namespace Services
} // namespace Opal
