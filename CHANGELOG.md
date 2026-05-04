# Changelog

All notable changes to this project will be documented in this file.

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
