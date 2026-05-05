# Changelog

All notable changes to this project will be documented in this file.

## [1.1.6] - 2026-05-04

### Added
- **Phase 1: Core Aesthetics**
    - **Dynamic Theme Engine**: Full support for Light/Dark/System theme switching with persistent user preference on startup.
    - **Metadata Density**: Standardized information display across the library; added album counts to artist cards and song counts/durations to playlists.
    - **Premium Login Depth**: Integrated subtle radial background accents to the login screen for a high-end feel.
- **Phase 2: Premium Motion Design**
    - **Connected Animations**: Implemented fluid "fly-in" transitions when navigating from library grids to detail views.
    - **Advanced Spotlight UX**: Added auto-advancing (5s) spotlight carousels with boundary-aware navigation arrow visibility.
    - **Visual Feedback**: Integrated accent-colored "Now Playing" indicators across all track lists and grids.
- **Phase 3: Console (Xbox) Optimization**
    - **Standardized Padding**: Resolved "zoomed-in" layout issues by enforcing 48px margins and safe-area offsets on all pages.
    - **Responsive Scaling**: Optimized typography and element dimensions specifically for 10-foot TV viewing distances.
    - **XYFocus Hardening**: Guaranteed 100% D-pad navigation compatibility for all new UI components and dialogs.
- **Phase 4: Cherry on Top (Detail Polish)**
    - **Artist Badges**: Album count metadata now appears as stylized accent badges overlaid on artist avatars.
    - **Refined Iconography**: Replaced legacy lyrics/volume icons with professional-grade symbols (Microphone/Speaker).

### Fixed
- **Stability & Performance**:
    - Fixed a critical "Duplication assignment" XAML error in several library pages.
    - Resolved C++ template syntax errors (`IBox`, `TryStart`) to ensure build stability on modern SDKs.
    - Fixed a runtime crash where `NullToVisibilityConverter` was missing on certain pages.
    - Resolved persistent Visibility enum qualification errors in the C++ backend.

## [1.1.5] - 2026-05-03

### Added
- **Xbox Gamepad Support**: Full navigation and playback control using Xbox controllers.
- **Background Persistence**: Implemented `ExtendedExecutionSession` to prevent app suspension during background playback and scrobbling.
- **Transport Toggles**: Added Shuffle (Fisher-Yates) and Repeat (Off/All/One) modes.
- **Sleep Timer**: New timer in the player bar (15/30/60/90 mins) to automatically pause playback.
- **Favorites Filter**: Filter your track library to show only liked songs.
- **Playlist Deletion Confirmation**: Added a safety dialog before removing playlists.
- **Total Tracks Counter**: Real-time counter on the Tracks page showing library size.
- **Playlist Customization**: Added dedicated Genre, Year, and Favorites columns to the Playlist Details page.
- **Manual Reordering**: Integrated drag-and-drop handles for playlists with real-time visual index updates.

### Changed
- **Unlimited Track Browsing**: Removed the 500-track limitation; supports infinite background loading.
- **Premium Visuals Overhaul**: Implemented high-intensity Gaussian blur with hardware-accelerated color grading (Contrast, Temperature, Tint) for a cinematic look.
- **Hero Panel Design**: Updated spotlight carousel with bold typography, improved metadata layout, and linear gradient overlays.
- **Session Security**: Logout/Disconnect now properly clears all cached credentials and UI state.
- **Intelligent Thumbnailing**: `ThumbnailView` now distinguishes between cinematic hero backgrounds and standard list thumbnails (clear by default).
- **Theming & Aesthetics**: Enhanced Playlist Details page with System Accent Color integration and full Light/Dark mode support.
- **Improved Loading States**: Restored and optimized shimmer skeleton animations during asynchronous image fetching.
- **Improved Diagnostics**: `DebugLogger` now includes human-readable exception messages for better troubleshooting.

### Fixed
- **Critical Crash (Vector Orphan)**: Fixed a bug where updating playlists broke UI data-binding subscriptions.
- **Critical Crash (Recursion)**: Implemented guards to prevent infinite loops in Autoplay track fetching.
- **Stability (Underflow)**: Fixed unsigned integer underflow when interacting with an empty playback queue.
- **Memory Leaks**: Resolved `BackRequested` handler accumulation in `LibraryPage`.
- **Event Duplication**: Removed redundant `SongChanged` subscriptions in `MainPage`.
- **ItemsRepeater Stability**: Fixed navigation exceptions during sidebar updates.
- **Casting Service**: Resolved race conditions in asynchronous lambda captures.
- **Compiler Fix (E2227)**: Refactored asynchronous WinRT operations to resolve template instantiation errors in PPL task chains.
- **Xbox Background Stability**: Optimized application lifecycle events and `MediaPlayer` configuration for reliable background execution.
- **XAML Namespace Stability**: Resolved undeclared 'controls' prefix errors and invalid FontIcon properties.
- **Type Safety**: Resolved undefined `ThumbnailView` errors in XAML backend logic.
