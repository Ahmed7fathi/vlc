# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added

#### YouTube Video Downloader (download videos directly from VLC)

A new feature to download videos from YouTube and other supported sites directly from VLC's interface. Includes a complete C++ backend for media analysis, download orchestration, and a Qt UI for the interactive download workflow.

- **New files (downloader framework):**
  - `modules/gui/qt/downloader/models/` — Data models: `DownloadTask`, `DownloadStateMachine`, `DownloadSettings`, `MediaInfo`.
  - `modules/gui/qt/downloader/core/` — Core engine: YouTube provider, yt-dlp strategy, download engine, download queue, orchestrator, event bus, process runner, JSON parser, file/temp managers, FFmpeg processor, processing pipeline.
  - `modules/gui/qt/downloader/bridge/` — C++/QML bridge: `DownloaderController` (singleton), `DownloadTaskModel` (QAbstractListModel), `DownloadDialog` (QWidget-based dialog).
  - `modules/gui/qt/downloader/qml/` — 7 QML UI files: `DownloadDialog.qml`, `QualitySelector.qml`, `AudioSelector.qml`, `SubtitleSelector.qml`, `ProgressDialog.qml`, `DownloadQueueView.qml`, `DownloadSettings.qml`.

- **Modified files:**
  - `modules/gui/qt/maininterface/mainui.cpp` — Registers `VLC.Downloader` QML module with lazy `DownloaderController` singleton and `DownloadTaskModel` type.
  - `modules/gui/qt/maininterface/qml/MainDisplay.qml` — Adds "Downloads" tab entry pointing to the download queue page.
  - `modules/gui/qt/maininterface/qml/BannerSources.qml` — Adds "Download Media" toolbar button with active-download count badge.
  - `modules/gui/qt/maininterface/qml/DownloadsDisplay.qml` (new) — Self-contained download queue page with inline ListView, progress bars, state badges, action buttons, and progress dialog.
  - `modules/gui/qt/dialogs/dialogs_provider.cpp/hpp` — Integrates download dialog opening from the toolbar button.
  - `modules/gui/qt/meson.build` — Adds all downloader sources and the `VLC.Downloader` QML module to the build.
  - `src/libvlc-module.c` — Adds VLC configuration variables for download path and settings.
  - `test/modules/meson.build` — Adds test infrastructure.

- **Build dependencies:** Requires `yt-dlp` (runtime) for media extraction and `ffmpeg` for optional post-processing.

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
- **Glow effect** — A soft circular glow aura (2.2× the handle size) fades in behind the handle on hover/drag (opacity 0.2/0.25) and fades out when idle, matching the handle's size transitions.
- **Visual polish** — Added white border for contrast on any background and an inner highlight ring for a refined look.
- **Cursor feedback** — Cursor changes to a pointing hand (`Qt.PointingHandCursor`) when hovering over or dragging the seek bar.

#### Key design decisions:

- **Single QML Image element** — Uses QML's built-in async image loading which keeps the old pixmap displayed until the new one is decoded, avoiding flicker/flash.
- **Progressive pregeneration** — Thumbnails are pre-generated in batches (3 at a time) to avoid flooding the preparser on long videos.
- **Same-media hash guard** — Pipeline reconfiguration events (e.g. fullscreen toggle) do NOT clear the thumbnail state if the media URL hasn't changed.
- **Position latching** — The popup's `x` position latches to the last hover position on exit, preventing it from snapping to the playback cursor during fade-out.
- **No exit transition** — The popup disappears instantly when the mouse leaves, avoiding any visible position snap.
- **Debounce timers** — 150ms debounce on hover thumbnail requests, 50ms debounce on QML URL updates to prevent rapid flickering.

### Fixed

#### Downloader: Task stuck in "Downloading" state after completion

Fixed a critical bug where download tasks remained stuck in the "Downloading" state indefinitely after the download completed. The `DownloadStateMachine` rejected the direct `Downloading → Completed` transition because it required a mandatory stop at `PostProcessing`. When the processing pipeline was empty (no FFmpeg steps), the `DownloadEngine` jumped directly to `Completed`, but the state machine silently returned `false`, leaving the task permanently in `Downloading`.

- `modules/gui/qt/downloader/models/download_state_machine.cpp` — Added `S::Completed` to valid transitions from `S::Downloading`
- `modules/gui/qt/downloader/bridge/download_task_model.cpp` — Added progress roles to `onStateChanged` dataChanged signal

#### Downloader: Playlist URLs cause yt-dlp to analyze forever

URLs containing `?list=...` (playlist parameter) caused yt-dlp to enumerate the entire playlist instead of analyzing just the single video. Added `--no-playlist` flag to yt-dlp arguments in both the analysis and download phases.

- `modules/gui/qt/downloader/providers/youtube_provider.cpp` — Added `--no-playlist` to analysis args
- `modules/gui/qt/downloader/core/strategies/ytdlp_strategy.cpp` — Added `--no-playlist` to download args

#### Downloader: Downloads page glitching when switching tabs

Fixed page loading failure on the Downloads tab caused by incorrect `ColorContext` declaration pattern (`readonly property` instead of child element) and complex QML component usage (`Widgets.ActionButtonPrimary`, `Widgets.IconButton` with animations). Replaced with simpler `IconLabel + MouseArea` pattern and removed animations.

- `modules/gui/qt/maininterface/qml/DownloadsDisplay.qml` — Simplified: removed Popup, replaced IconButton with IconLabel+MouseArea, removed animations

#### Download Media Dialog UI/UX improvements

Redesigned the Download Media dialog for better usability: increased default size from 680×680 to 880×900 with proportional internal spacing. Larger fonts, taller controls, better margins, and a cleaner layout throughout.

- `modules/gui/qt/downloader/bridge/download_dialog.cpp` — Complete dialog redesign:
  - Size: 760×720 → 880×900 (min 720×680 → 880×900)
  - Margins: 24,20,24,20 → 32,28,32,28; spacing: 16 → 20px
  - Title font: +8pt → +10pt bold
  - URL field: 40px → 44px tall, font +2pt
  - Analyze button: 40×100 → 44×120, blue styled
  - Combo boxes: 280×32 → 340×36, font +1pt
  - Footer buttons: 38×90 → 42×100 / 38×110 → 42×130
  - Metadata row: horizontal layout (Duration · Uploader)
  - Path field: editable (was read-only)
  - Pointing hand cursors on all buttons

#### Downloader: Cancel button not killing the yt-dlp process

Fixed the cancel button so it properly terminates the yt-dlp process. The `ProcessRunner::cancel()` method only sent `SIGTERM` without waiting or reaping the child — some processes (or their sub-processes) could survive `SIGTERM`. The `DownloadEngine` also transitioned cancelled tasks to `Failed` state instead of `Cancelled`.

- `modules/gui/qt/downloader/core/utils/process_runner.cpp` — `cancel()` now sends SIGTERM, polls with `waitpid(WNOHANG)` for 3 seconds, then escalates to SIGKILL + blocking `waitpid()` to fully reap the child
- `modules/gui/qt/downloader/core/engine/download_engine.cpp` — In `runThread()`, check cancellation FIRST when the strategy fails; transition to `Cancelled` state instead of `Failed`

  - Fixed compiler warning (missing field initializers in `Result` struct)
