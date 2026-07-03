/*****************************************************************************
 * downloader_controller.hpp
 *****************************************************************************
 * Copyright (C) 2024 VLC authors and VideoLAN
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifndef VLC_DOWNLOADER_DOWNLOADER_CONTROLLER_HPP
#define VLC_DOWNLOADER_DOWNLOADER_CONTROLLER_HPP

#include <QObject>
#include <QString>
#include <QStringList>
#include <QUrl>

#include <memory>

#include "download_task_model.hpp"

struct vlc_object_t;
struct qt_intf_t;

namespace vlc {
namespace downloader {

class DownloadOrchestrator;
class EventBus;
class DownloadTask;
struct DownloadSettings;

/**
 * @brief QML singleton controller for the download system.
 *
 * Exposes the download framework to QML via VLC.Downloader module.
 * Follows the same pattern as MainCtx and PlayerController.
 *
 * Usage in QML:
 * @code
 *   import VLC.Downloader
 *
 *   DownloaderController.createTask("https://youtube.com/watch?v=...")
 *   DownloaderController.taskModel  // access the task list model
 * @endcode
 *
 * This class is registered as a QML singleton instance via
 * qmlRegisterSingletonInstance in MainUI::registerQMLTypes().
 */
class DownloaderController : public QObject
{
    Q_OBJECT

    /**
     * @brief The task list model exposed to QML.
     *
     * Exposed as QAbstractItemModel* (not DownloadTaskModel*) so that QML's
     * ListView can properly bind to it. QML needs to recognize the model
     * through the QAbstractItemModel interface to set up rows/columns,
     * signal connections for rowsInserted/rowsRemoved/dataChanged, etc.
     * Using the concrete class with namespace qualifier can prevent QML
     * from properly recognizing the model type. */
    Q_PROPERTY(QAbstractItemModel* taskModel
               READ taskModel CONSTANT FINAL)

    /**
     * @brief Whether any downloads are currently active (progress).
     */
    Q_PROPERTY(bool hasActiveDownloads READ hasActiveDownloads
               NOTIFY hasActiveDownloadsChanged FINAL)

    /**
     * @brief Number of active (downloading) tasks.
     */
    Q_PROPERTY(int activeCount READ activeCount
               NOTIFY activeCountChanged FINAL)

    /**
     * @brief Default download path from settings.
     */
    Q_PROPERTY(QString defaultDownloadPath READ defaultDownloadPath
               WRITE setDefaultDownloadPath NOTIFY defaultDownloadPathChanged FINAL)

public:
    /**
     * @brief Create a DownloaderController.
     *
     * @param p_intf     VLC interface object.
     * @param parent     QObject parent.
     */
    explicit DownloaderController(qt_intf_t* p_intf, QObject* parent = nullptr);
    ~DownloaderController() override;

    /** Non-copyable, non-movable */
    DownloaderController(const DownloaderController&) = delete;
    DownloaderController& operator=(const DownloaderController&) = delete;

    /** Global singleton instance accessor for C++ code (e.g. DialogsProvider). */
    static DownloaderController* instance() { return s_instance; }

    // ── Properties ──────────────────────────────────────────────────────

    DownloadTaskModel* taskModel() const { return m_taskModel; }

    bool hasActiveDownloads() const;
    int activeCount() const;

    QString defaultDownloadPath() const;
    void setDefaultDownloadPath(const QString& path);

    // ── QML-invokable methods ───────────────────────────────────────────

    /**
     * @brief Create a new download task for a URL.
     * @param url  The media URL to download.
     * @return The task ID string, or empty on failure.
     */
    Q_INVOKABLE QString createTask(const QString& url);

    /**
     * @brief Analyze a task (find provider, extract media info).
     * @param taskId  The task ID returned by createTask().
     */
    Q_INVOKABLE void analyzeTask(const QString& taskId);

    /**
     * @brief Confirm a task for download with optional format selection.
     * @param taskId     The task ID.
     * @param formatIdx  Video format index (-1 for auto).
     * @param audioOnly  Whether to download only audio.
     */
    Q_INVOKABLE bool confirmDownload(const QString& taskId,
                                      int formatIdx = -1,
                                      bool audioOnly = false);

    /**
     * @brief Cancel a task.
     * @param taskId  The task ID.
     */
    Q_INVOKABLE bool cancelTask(const QString& taskId);

    /**
     * @brief Pause a running download.
     * @param taskId  The task ID.
     */
    Q_INVOKABLE bool pauseTask(const QString& taskId);

    /**
     * @brief Resume a paused download.
     * @param taskId  The task ID.
     */
    Q_INVOKABLE bool resumeTask(const QString& taskId);

    /**
     * @brief Retry a failed download.
     * @param taskId  The task ID.
     */
    Q_INVOKABLE bool retryTask(const QString& taskId);

    /**
     * @brief Remove all completed (terminal-state) tasks from the queue.
     */
    Q_INVOKABLE void removeCompletedTasks();

    /**
     * @brief Get the task ID at a given model row index.
     */
    Q_INVOKABLE QString taskIdAt(int row) const;

    /**
     * @brief Get a human-readable state name for a task.
     */
    Q_INVOKABLE QString stateName(const QString& taskId) const;

    /**
     * @brief Save current settings to VLC persistent config.
     */
    Q_INVOKABLE void saveSettings();

    /**
     * @brief Debug helper: prints diagnostic info to stderr.
     * Bypasses VLC's QML console.log suppression.
     */
    Q_INVOKABLE QString debugDump() const;

    // ── Media info accessors for QML (return QVariant-friendly types) ──

    /**
     * @brief Get the MediaInfo for a task as a QVariantMap.
     *
     * Keys: title, duration (formatted string), uploader,
     * description, thumbnailUrl.
     * Returns an empty map if the task or its media info is not available.
     */
    Q_INVOKABLE QVariantMap mediaInfoForTask(const QString& taskId) const;

    /**
     * @brief Get available video formats for a task as a QVariantList.
     *
     * Each entry is a QVariantMap with keys: id, resolution (e.g. "1080p"),
     * codec, bitrate (formatted), fileSize (formatted), extension, hasVideo.
     */
    Q_INVOKABLE QVariantList videoFormatsForTask(const QString& taskId) const;

    /**
     * @brief Get available audio formats for a task as a QVariantList.
     *
     * Each entry is a QVariantMap with keys: id, name, codec,
     * bitrate (formatted), sampleRate (formatted).
     */
    Q_INVOKABLE QVariantList audioFormatsForTask(const QString& taskId) const;

    /**
     * @brief Get available subtitle tracks for a task as a QVariantList.
     *
     * Each entry is a QVariantMap with keys: id, language, name, isAutomatic.
     */
    Q_INVOKABLE QVariantList subtitleFormatsForTask(const QString& taskId) const;

signals:
    /** @brief Emitted when the active download count changes. */
    void hasActiveDownloadsChanged();

    /** @brief Emitted when the active task count changes. */
    void activeCountChanged();

    /** @brief Emitted when the default download path changes. */
    void defaultDownloadPathChanged();

    /**
     * @brief Emitted when URL analysis completes successfully.
     * Sent via QueuedConnection from the background analysis thread.
     */
    void analysisReady(const QString& taskId);

    /**
     * @brief Emitted when URL analysis fails.
     * Sent via QueuedConnection from the background analysis thread.
     */
    void analysisError(const QString& taskId, const QString& error);

    /**
     * @brief Emitted periodically during download with progress updates.
     */
    void downloadProgress(const QString& taskId, int percent,
                          double speed, int64_t eta);

    /**
     * @brief Emitted when a download completes successfully.
     * Includes the output path where the file was saved.
     */
    void downloadCompleted(const QString& taskId, const QString& outputPath);

    /**
     * @brief Emitted when a download fails.
     */
    void downloadFailed(const QString& taskId, const QString& error);

private:
    /** Find a task by ID. Returns nullptr if not found. */
    std::shared_ptr<DownloadTask> findTask(const QString& taskId) const;

    /** Emit active count signals. */
    void updateActiveState();

    /** Subscribe to EventBus events to track active downloads. */
    void setupEventBus();

    /** Load download settings from persistent storage (QSettings). */
    DownloadSettings loadSettings() const;

    qt_intf_t* m_intf = nullptr;
    EventBus* m_eventBus = nullptr;
    DownloadTaskModel* m_taskModel = nullptr;
    std::unique_ptr<DownloadOrchestrator> m_orchestrator;

    /** EventBus subscription IDs */
    std::vector<size_t> m_subscriptionIds;

    /** Singleton instance pointer */
    static DownloaderController* s_instance;
};

} // namespace downloader
} // namespace vlc

#endif // VLC_DOWNLOADER_DOWNLOADER_CONTROLLER_HPP
