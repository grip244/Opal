#include "pch.h"
#include "SettingsPage.xaml.h"
#include "Services/NavidromeService.h"
#include "ViewModels/LibraryViewModel.h"
#include "ViewModels/PlaylistsViewModel.h"
#include "LoginPage.xaml.h"
#include "Services/SettingsService.h"

using namespace Opal;
using namespace Platform;
using namespace Windows::UI::Xaml;
using namespace Windows::UI::Xaml::Controls;
#include "Services/PlaybackService.h"

SettingsPage::SettingsPage()
{
    InitializeComponent();
    
    auto nav = NavidromeService::Instance;
    AccountUserText->Text = nav->GetUsername();
    AccountServerText->Text = nav->GetServerUrl();
    
    AutoPlayToggle->IsOn = SettingsService::Instance->IsAutoPlayEnabled;

    InitializeEqUI();
}

void SettingsPage::InitializeEqUI() {
    _isInitializing = true;
    auto settings = SettingsService::Instance;
    auto playback = PlaybackService::Instance;

    EqToggle->IsOn = settings->GetEqEnabled();
    float preAmp = settings->GetEqPreAmp();
    EqPreAmpSlider->Value = preAmp;
    UpdatePreAmpText(preAmp);

    EqSlider0->Value = settings->GetEqBand(0); UpdateValueText(0, (float)EqSlider0->Value);
    EqSlider1->Value = settings->GetEqBand(1); UpdateValueText(1, (float)EqSlider1->Value);
    EqSlider2->Value = settings->GetEqBand(2); UpdateValueText(2, (float)EqSlider2->Value);
    EqSlider3->Value = settings->GetEqBand(3); UpdateValueText(3, (float)EqSlider3->Value);
    EqSlider4->Value = settings->GetEqBand(4); UpdateValueText(4, (float)EqSlider4->Value);
    EqSlider5->Value = settings->GetEqBand(5); UpdateValueText(5, (float)EqSlider5->Value);
    EqSlider6->Value = settings->GetEqBand(6); UpdateValueText(6, (float)EqSlider6->Value);
    EqSlider7->Value = settings->GetEqBand(7); UpdateValueText(7, (float)EqSlider7->Value);
    EqSlider8->Value = settings->GetEqBand(8); UpdateValueText(8, (float)EqSlider8->Value);
    EqSlider9->Value = settings->GetEqBand(9); UpdateValueText(9, (float)EqSlider9->Value);

    EqPresetsCombo->SelectedIndex = settings->GetEqPreset();
    _isInitializing = false;
}

void SettingsPage::OnEqToggled(Object^ sender, RoutedEventArgs^ e) {
    if (_isInitializing) return;
    bool enabled = EqToggle->IsOn;
    SettingsService::Instance->SaveEqEnabled(enabled);
    PlaybackService::Instance->ToggleEqualizer(enabled);
}

void SettingsPage::OnEqBandChanged(Object^ sender, Windows::UI::Xaml::Controls::Primitives::RangeBaseValueChangedEventArgs^ e) {
    if (_isInitializing) return;
    Slider^ slider = (Slider^)sender;
    String^ name = slider->Name;
    int band = _wtoi(name->Data() + 8); // Skip "EqSlider"
    float gain = (float)e->NewValue;

    UpdateValueText(band, gain);
    SettingsService::Instance->SaveEqBand(band, gain);
    PlaybackService::Instance->UpdateEqualizer(band, gain);
    
    if (EqPresetsCombo->SelectedIndex != 8) {
        _isInitializing = true;
        EqPresetsCombo->SelectedIndex = 8;
        SettingsService::Instance->SaveEqPreset(8);
        _isInitializing = false;
    }
}

void SettingsPage::OnEqPreAmpChanged(Object^ sender, Windows::UI::Xaml::Controls::Primitives::RangeBaseValueChangedEventArgs^ e) {
    if (_isInitializing) return;
    float gain = (float)e->NewValue;
    UpdatePreAmpText(gain);
    SettingsService::Instance->SaveEqPreAmp(gain);
    PlaybackService::Instance->SetPreAmp(gain);
    
    if (EqPresetsCombo->SelectedIndex != 8) {
        _isInitializing = true;
        EqPresetsCombo->SelectedIndex = 8;
        SettingsService::Instance->SaveEqPreset(8);
        _isInitializing = false;
    }
}

void SettingsPage::OnEqPresetSelected(Object^ sender, SelectionChangedEventArgs^ e) {
    if (_isInitializing) return;
    int index = EqPresetsCombo->SelectedIndex;
    if (index < 0) return;

    SettingsService::Instance->SaveEqPreset(index);

    // Presets in dB
    static const float Presets[8][10] = {
        {0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // Flat
        {4, 3, 1, -1, -2, -1, 1, 3, 4, 4}, // Rock
        {5, 4, 2, 1, 0, 0, 1, 2, 3, 3}, // Electronic
        {2, 2, 1, 0, 1, 2, 3, 2, 1, 2}, // Acoustic
        {-2, -1, 0, 1, 3, 4, 3, 1, 0, -1}, // Vocal
        {4, 3, 1, 1, -1, -1, 0, 1, 2, 3}, // R&B
        {5.5, 5, 2, 0, -1, 0, 1, 1, 2, 3.5}, // Hip Hop
        {0.5, 1, 2, 1, 0.5, -1, 1, 3, 2, 1} // Country
    };

    if (index >= 0 && index < 8) {
        _isInitializing = true; // Prevent sliders from triggering Custom override
        
        auto settings = SettingsService::Instance;
        auto playback = PlaybackService::Instance;

        Slider^ sliders[] = { EqSlider0, EqSlider1, EqSlider2, EqSlider3, EqSlider4, EqSlider5, EqSlider6, EqSlider7, EqSlider8, EqSlider9 };

        for (int i = 0; i < 10; i++) {
            float val = Presets[index][i];
            sliders[i]->Value = val;
            UpdateValueText(i, val);
            settings->SaveEqBand(i, val);
            playback->UpdateEqualizer(i, val);
        }

        // Auto-adjust pre-amp if boosting
        float maxBoost = 0;
        for(int i=0; i<10; i++) if(Presets[index][i] > maxBoost) maxBoost = Presets[index][i];
        float preAmp = (maxBoost > 0) ? -maxBoost : 0;
        EqPreAmpSlider->Value = preAmp;
        UpdatePreAmpText(preAmp);
        settings->SaveEqPreAmp(preAmp);
        playback->SetPreAmp(preAmp);
        
        _isInitializing = false;
    }
}

void SettingsPage::UpdateValueText(int band, float gain) {
    wchar_t buf[16];
    swprintf_s(buf, L"%+.1f", gain);
    String^ text = ref new String(buf);
    
    switch(band) {
        case 0: EqValue0->Text = text; break;
        case 1: EqValue1->Text = text; break;
        case 2: EqValue2->Text = text; break;
        case 3: EqValue3->Text = text; break;
        case 4: EqValue4->Text = text; break;
        case 5: EqValue5->Text = text; break;
        case 6: EqValue6->Text = text; break;
        case 7: EqValue7->Text = text; break;
        case 8: EqValue8->Text = text; break;
        case 9: EqValue9->Text = text; break;
    }
}

void SettingsPage::UpdatePreAmpText(float gain) {
    wchar_t buf[32];
    swprintf_s(buf, L"%+.1f dB", gain);
    EqPreAmpValue->Text = ref new String(buf);
}

void SettingsPage::OnAutoPlayToggled(Object^ sender, RoutedEventArgs^ e)
{
    SettingsService::Instance->IsAutoPlayEnabled = AutoPlayToggle->IsOn;
}

void SettingsPage::OnLogoutClicked(Object^ sender, RoutedEventArgs^ e)
{
    auto nav = NavidromeService::Instance;
    
    // Clear saved credentials from Vault
    SettingsService::Instance->ClearCredentials(nav->GetServerUrl(), nav->GetUsername());
    
    // Clear stored user/server to prevent auto-fill and auto-connect
    SettingsService::Instance->SaveServer("");
    SettingsService::Instance->SaveUsername("");
    
    // Clear active session
    nav->SetCredentials(nullptr, nullptr, nullptr);

    // Clear UI data
    ViewModels::LibraryViewModel::Instance->ClearAll();
    ViewModels::PlaylistsViewModel::Instance->Clear();

    this->Frame->Navigate(Windows::UI::Xaml::Interop::TypeName(LoginPage::typeid));
    this->Frame->BackStack->Clear();
}
