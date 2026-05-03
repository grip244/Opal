//
// App.xaml.h
// Declaration of the App class.
//

#pragma once

#include "App.g.h"
#include "Converters/RemoteSystemKindToIconConverter.h"

namespace Opal
{
	/// <summary>
	/// Provides application-specific behavior to supplement the default Application class.
	/// </summary>
	ref class App sealed
	{
	public:
		static property Windows::UI::Core::CoreDispatcher^ MainDispatcher {
			Windows::UI::Core::CoreDispatcher^ get();
			void set(Windows::UI::Core::CoreDispatcher^ value);
		}
	protected:
		virtual void OnLaunched(Windows::ApplicationModel::Activation::LaunchActivatedEventArgs^ e) override;

	internal:
		App();

	private:
		static Windows::UI::Core::CoreDispatcher^ _mainDispatcher;
		void OnSuspending(Platform::Object^ sender, Windows::ApplicationModel::SuspendingEventArgs^ e);
		void OnResuming(Platform::Object^ sender, Platform::Object^ e);
		void OnEnteredBackground(Platform::Object^ sender, Windows::ApplicationModel::EnteredBackgroundEventArgs^ e);
		void OnLeavingBackground(Platform::Object^ sender, Windows::ApplicationModel::LeavingBackgroundEventArgs^ e);
		void OnResuming(Platform::Object^ sender, Platform::Object^ e);
		void OnNavigationFailed(Platform::Object ^sender, Windows::UI::Xaml::Navigation::NavigationFailedEventArgs ^e);
	};
}
