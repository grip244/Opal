# Changelog

All notable changes to this project will be documented in this file.

## [1.2.0] - 2026-05-08

### Added
- **Major Xbox "Stitched" UI Overhaul**:
    - **Cinematic Featured Carousel**: Replaced the static single-song banner with a high-performance, multi-item `FlipView` carousel.
    - **Infinite Scrolling**: Implemented seamless "infinite" wrap-around logic for all spotlight carousels (Xbox and PC).
    - **Automatic Rotation**: Integrated an intelligent timer that automatically cycles featured content every 5 seconds.
    - **Metadata Tags**: Added stylized Genre and Year tags to featured items with theme-aware background rendering.
    - **Connect to Device**: Integrated a dedicated Xbox "Connect to Device" (Casting) header for remote control and handover.
- **Glassmorphism Design System**:
    - **Frosted Player Bar**: Implemented a hardware-accelerated BackdropBlur effect for the global player pill, creating a modern frosted-glass aesthetic.
    - **System Accent Integration**: UI elements like the "Now Playing" progress and featured tags now dynamically respect the system accent color.
- **Improved Library Controls**:
    - **Multi-Directional Sorting**: Added full support for Ascending/Descending sort orders across Artists, Albums, Tracks, and Genres.
    - **Smart Playlists Integration**: Foundation for Navidrome-native Smart Playlists (.nsp) to support server-side dynamic collections.
    - **Artists Page Shimmer**: Implemented a high-performance lazy-loading skeleton system for the Artists library.

### Fixed
- **Xbox Player Bar Stability**:
    - **Restored Timestamps**: Fixed a bug where song progress (Current/Total) was hidden in the Xbox state.
    - **Song Title Visibility**: Resolved a layout issue where the song title was missing due to zero-width container measurement; restored marquee scrolling.
    - **Volume Control**: Restored the volume slider in the vertical flyout which was previously collapsed.
    - **Rating Scaling**: Optimized `RatingControl` scaling to prevent oversized stars on TV displays.
- **Navigation & Code-Behind**:
    - **Xbox Settings Button**: Fixed a critical bug where the sidebar settings button failed to navigate on Xbox.
    - **Navigation State Persistence**: Improved handling of inner-frame navigation state when switching between PC and Xbox views.
    - **Navigation Reliability**: Fixed "View Playlists" navigation to ensure clicking either the header text or the icon correctly opens the playlists page.
    - **Sidebar Integrity**: Resolved a bug where the Playlists dropdown would occasionally disappear from the sidebar during background library updates.
    - **C++/CX Compilation**: Resolved `Visibility::Visible` ambiguity errors and qualified namespaces for better build reliability.
- **Performance**:
    - Optimized `ThumbnailView` memory usage during rapid carousel scrolling.
    - Improved `LibraryViewModel` spotlight binding to ensure consistent data across different UI states.

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
