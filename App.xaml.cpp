//
// App.xaml.cpp
// Implementation of the App class.
//

#include "pch.h"
#include "MainPage.xaml.h"
#include "Services/CastingService.h"
#include "Services/DebugLogger.h"

using namespace Opal;

using namespace Platform;
using namespace Windows::UI::Core;
using namespace Windows::ApplicationModel;
using namespace Windows::ApplicationModel::Activation;
using namespace Windows::Foundation;
using namespace Windows::Foundation::Collections;
using namespace Windows::UI::Xaml;
using namespace Windows::UI::Xaml::Controls;
using namespace Windows::UI::Xaml::Controls::Primitives;
using namespace Windows::UI::Xaml::Data;
using namespace Windows::UI::Xaml::Input;
using namespace Windows::UI::Xaml::Interop;
using namespace Windows::UI::Xaml::Media;
using namespace Windows::UI::Xaml::Navigation;

CoreDispatcher^ App::_mainDispatcher = nullptr;
CoreDispatcher^ App::MainDispatcher::get() { return _mainDispatcher; }
void App::MainDispatcher::set(CoreDispatcher^ value) { _mainDispatcher = value; }

/// <summary>
/// Initializes the singleton application object.  This is the first line of authored code
/// executed, and as such is the logical equivalent of main() or WinMain().
/// </summary>
App::App()
{
    InitializeComponent();
    Suspending += ref new SuspendingEventHandler(this, &App::OnSuspending);
    Resuming += ref new EventHandler<Object^>(this, &App::OnResuming);
    EnteredBackground += ref new EnteredBackgroundEventHandler(this, &App::OnEnteredBackground);
    LeavingBackground += ref new LeavingBackgroundEventHandler(this, &App::OnLeavingBackground);
#ifdef _DEBUG
    DebugLogger::Instance->StartHttpServer(5555);
#endif
}

/// <summary>
/// Invoked when the application is launched normally by the end user.  Other entry points
/// will be used such as when the application is launched to open a specific file.
/// </summary>
/// <param name="e">Details about the launch request and process.</param>
void App::OnLaunched(Windows::ApplicationModel::Activation::LaunchActivatedEventArgs^ e)
{
    App::MainDispatcher = Window::Current->Dispatcher;
   // this->RequiresPointerMode = Windows::UI::Xaml::ApplicationRequiresPointerMode::WhenRequested;
    Windows::UI::ViewManagement::ApplicationView::GetForCurrentView()->SetDesiredBoundsMode(Windows::UI::ViewManagement::ApplicationViewBoundsMode::UseCoreWindow);
    auto rootFrame = dynamic_cast<Frame^>(Window::Current->Content);

    // Do not repeat app initialization when the Window already has content,
    // just ensure that the window is active
    if (rootFrame == nullptr)
    {
        // Create a Frame to act as the navigation context and associate it with
        // a SuspensionManager key
        rootFrame = ref new Frame();

        rootFrame->NavigationFailed += ref new Windows::UI::Xaml::Navigation::NavigationFailedEventHandler(this, &App::OnNavigationFailed);

        if (e->PreviousExecutionState == ApplicationExecutionState::Terminated)
        {
            // Restore the saved session state only when appropriate, scheduling the
            // final launch steps after the restore is complete
            if (Windows::Storage::ApplicationData::Current->LocalSettings->Values->HasKey("NavigationState"))
            {
                Platform::String^ state = dynamic_cast<Platform::String^>(Windows::Storage::ApplicationData::Current->LocalSettings->Values->Lookup("NavigationState"));
                if (state != nullptr)
                {
                    rootFrame->SetNavigationState(state);
                }
            }
        }
        else
        {
            // Clear navigation state on fresh launch
            Windows::Storage::ApplicationData::Current->LocalSettings->Values->Remove("NavigationState");
            Windows::Storage::ApplicationData::Current->LocalSettings->Values->Remove("InnerNavigationState");
        }

        if (e->PrelaunchActivated == false)
        {
            if (rootFrame->Content == nullptr)
            {
                // When the navigation stack isn't restored navigate to the first page,
                // configuring the new page by passing required information as a navigation
                // parameter
                rootFrame->Navigate(TypeName(MainPage::typeid), e->Arguments);
            }
            // Place the frame in the current Window
            Window::Current->Content = rootFrame;
            // Ensure the current window is active
            Window::Current->Activate();
        }
        DebugLogger::Instance->Log("App", "Attempting to start CastingService listener");
        Opal::CastingService::Instance->StartListening();
        Opal::CastingService::Instance->StartDiscovery();
    }
    else
    {
        if (e->PrelaunchActivated == false)
        {
            if (rootFrame->Content == nullptr)
            {
                // When the navigation stack isn't restored navigate to the first page,
                // configuring the new page by passing required information as a navigation
                // parameter
                rootFrame->Navigate(TypeName(MainPage::typeid), e->Arguments);
            }
            // Ensure the current window is active
            Window::Current->Activate();
        }
    }
}


/// <summary>
/// Invoked when application execution is being suspended.  Application state is saved
/// without knowing whether the application will be terminated or resumed with the contents
/// of memory still intact.
/// </summary>
/// <param name="sender">The source of the suspend request.</param>
/// <param name="e">Details about the suspend request.</param>
void App::OnSuspending(Object^ /*sender*/, SuspendingEventArgs^ /*e*/)
{
    auto deferral = e->SuspendingOperation->GetDeferral();

    // Save application state and stop any background activity
    auto rootFrame = dynamic_cast<Frame^>(Window::Current->Content);
    if (rootFrame != nullptr)
    {
        Windows::Storage::ApplicationData::Current->LocalSettings->Values->Insert("NavigationState", rootFrame->GetNavigationState());

        auto mainPage = dynamic_cast<MainPage^>(rootFrame->Content);
        if (mainPage != nullptr)
        {
            auto innerFrame = mainPage->GetNavigationFrame();
            if (innerFrame != nullptr)
            {
                Windows::Storage::ApplicationData::Current->LocalSettings->Values->Insert("InnerNavigationState", innerFrame->GetNavigationState());
            }
        }
    }

    CastingService::Instance->StopListening();
    CastingService::Instance->StopDiscovery();

    deferral->Complete();
}

void App::OnResuming(Object^ sender, Object^ e)
{
    CastingService::Instance->StartListening();
    CastingService::Instance->StartDiscovery();
}

void App::OnEnteredBackground(Object^ /*sender*/, EnteredBackgroundEventArgs^ /*e*/)
{
    // The app is now in the background. 
    // We should minimize memory usage here if possible.
}

void App::OnLeavingBackground(Object^ /*sender*/, LeavingBackgroundEventArgs^ /*e*/)
{
    // The app is returning to the foreground.
}

/// <summary>
/// Invoked when Navigation to a certain page fails
/// </summary>
/// <param name="sender">The Frame which failed navigation</param>
/// <param name="e">Details about the navigation failure</param>
void App::OnNavigationFailed(Platform::Object^ /*sender*/, Windows::UI::Xaml::Navigation::NavigationFailedEventArgs^ e)
{
    throw ref new FailureException("Failed to load Page " + e->SourcePageType.Name);
}