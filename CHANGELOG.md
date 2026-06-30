# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added

#### Timeline Thumbnail Preview (YouTube-style seek bar previews)

A new feature that shows video thumbnail previews when hovering over the seek bar, similar to YouTube's timeline preview. Includes:

- **New files:**
  - `modules/gui/qt/util/timeline_preview_controller.cpp/hpp` — C++ bridge between the thumbnail backend and QML. Manages hover state, thumbnail requests, debouncing, position tracking, chapter info, and progressive pregeneration.
  - `modules/gui/qt/util/thumbnail_cache.cpp/hpp` — Disk-based LRU thumbnail cache with configurable size limit (default 500 MB). Stores thumbnails in `$VLC_CACHE_DIR/timeline-thumbnails/` and enforces cache size via LRU eviction.
  - `modules/gui/qt/util/thumbnail_generator.cpp/hpp` — Asynchronous thumbnail generator using VLC's preparser API. Queues thumbnail requests and processes them in batches (up to 3 concurrent). Falls back to direct I420→RGB888 conversion if `picture_Export` fails.
  - `modules/gui/qt/player/qml/TimelinePreviewPopup.qml` — QML popup component with enter animation, timestamp overlay, chapter info row, and downward-pointing arrow pointer. Features adaptive aspect ratio sizing.

- **Modified files:**
  - `modules/gui/qt/maininterface/mainui.cpp` — Registers `TimelinePreviewController` as a QML singleton (`VLC.MainInterface.TimelinePreview`).
  - `modules/gui/qt/player/qml/SliderBar.qml` — Integrates timeline preview: activates on hover, tracks cursor position, latches last hover position on exit, handles media length changes and input changes.
  - `modules/gui/qt/meson.build` — Adds new source files and QML module to the build.
  - `modules/gui/qt/qt.cpp` — Adds 4 VLC settings: `timeline-thumbnail-enabled`, `timeline-thumbnail-interval`, `timeline-thumbnail-width`, `timeline-thumbnail-cache-size`.

- **Build instructions:**
  - Updated `README.md` with comprehensive build/run instructions including quick rebuild tips for the Qt plugin.

### Changed

#### Seek Bar Redesign (hover expansion, circular handle, cursor feedback)

Enhanced the seek bar (`SliderBar.qml`) with a modern, interactive design:

- **Hover expansion** — The seek bar track smoothly animates from 5dp to 10dp height when hovering (configurable `_barExpandFactor` of 2.0), with a subtle `InOutSine` easing.
- **Circular grab handle** — Replaced the previous hidden-on-idle rectangular handle with an always-visible circular dot that has three distinct sizes:
  - **Idle** (10dp) — shows current playback position at all times
  - **Hover** (16dp) — grows when hovering over the bar
  - **Active** (22dp) — largest when dragging or interacting
- **Visual polish** — Added white border for contrast on any background and an inner highlight ring for a refined look.
- **Cursor feedback** — Cursor changes to a pointing hand (`Qt.PointingHandCursor`) when hovering over or dragging the seek bar.

#### Key design decisions:

- **Single QML Image element** — Uses QML's built-in async image loading which keeps the old pixmap displayed until the new one is decoded, avoiding flicker/flash.
- **Progressive pregeneration** — Thumbnails are pre-generated in batches (3 at a time) to avoid flooding the preparser on long videos.
- **Same-media hash guard** — Pipeline reconfiguration events (e.g. fullscreen toggle) do NOT clear the thumbnail state if the media URL hasn't changed.
- **Position latching** — The popup's `x` position latches to the last hover position on exit, preventing it from snapping to the playback cursor during fade-out.
- **No exit transition** — The popup disappears instantly when the mouse leaves, avoiding any visible position snap.
- **Debounce timers** — 150ms debounce on hover thumbnail requests, 50ms debounce on QML URL updates to prevent rapid flickering.
