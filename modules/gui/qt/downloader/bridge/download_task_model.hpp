/*****************************************************************************
 * download_task_model.hpp
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

#ifndef VLC_DOWNLOADER_DOWNLOAD_TASK_MODEL_HPP
#define VLC_DOWNLOADER_DOWNLOAD_TASK_MODEL_HPP

#include <QAbstractListModel>
#include <QHash>
#include <QByteArray>
#include <QString>

#include <memory>
#include <vector>

namespace vlc {
namespace downloader {

class DownloadTask;
class EventBus;

/**
 * @brief QAbstractListModel wrapping the list of DownloadTask objects.
 *
 * Exposes download task properties to QML via named roles:
 *   - taskId, url, title, state, stateName
 *   - progress, speed, eta, downloadedBytes, totalBytes
 *   - errorMessage, outputPath
 *   - isActive, isTerminal
 *
 * The model connects to the EventBus to receive live updates on
 * task creation, progress changes, and state transitions.
 *
 * Usage in QML:
 * @code
 *   ListView {
 *       model: Downloader.taskModel
 *       delegate: Text { text: title + " - " + progress + "%" }
 *   }
 * @endcode
 */
class DownloadTaskModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged FINAL)

public:
    /**
     * @brief Custom roles exposed to QML.
     */
    enum Roles
    {
        TaskIdRole = Qt::UserRole + 1,
        UrlRole,
        TitleRole,
        StateRole,
        StateNameRole,
        ProgressRole,
        SpeedRole,
        EtaRole,
        DownloadedBytesRole,
        TotalBytesRole,
        ErrorMessageRole,
        OutputPathRole,
        IsActiveRole,
        IsTerminalRole,
        CurrentFileRole,
        AudioOnlyRole,
        EmbedMetadataRole,
    };
    Q_ENUM(Roles)

    /**
     * @brief Create a DownloadTaskModel.
     *
     * @param eventBus  Event bus for live updates (may be nullptr).
     * @param parent    QObject parent.
     */
    explicit DownloadTaskModel(EventBus* eventBus = nullptr,
                                QObject* parent = nullptr);
    ~DownloadTaskModel() override;

    /** Non-copyable, movable */
    DownloadTaskModel(const DownloadTaskModel&) = delete;
    DownloadTaskModel& operator=(const DownloadTaskModel&) = delete;
    DownloadTaskModel(DownloadTaskModel&&) = delete;
    DownloadTaskModel& operator=(DownloadTaskModel&&) = delete;

    // ── QAbstractListModel interface ──────────────────────────────────────

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;

    QVariant data(const QModelIndex& index,
                  int role = Qt::DisplayRole) const override;

    QHash<int, QByteArray> roleNames() const override;

    // ── Task management ──────────────────────────────────────────────────

    /**
     * @brief Add a task to the model.
     */
    void addTask(std::shared_ptr<DownloadTask> task);

    /**
     * @brief Remove a task from the model by ID.
     */
    void removeTask(const std::string& taskId);

    /**
     * @brief Find the row index of a task by ID.
     * @return Row index, or -1 if not found.
     */
    int findRow(const std::string& taskId) const;

    /**
     * @brief Get a task by row index.
     * @return Shared pointer, or nullptr if out of range.
     */
    std::shared_ptr<DownloadTask> taskAt(int row) const;

    /**
     * @brief Clear all tasks from the model.
     */
    void clear();

signals:
    /** @brief Emitted when the number of tasks changes. */
    void countChanged();

private:
    /** Handle a progress update event from the EventBus. */
    void onProgressUpdate(const std::string& taskId);

    /** Handle a state change event. */
    void onStateChanged(const std::string& taskId);

    std::vector<std::shared_ptr<DownloadTask>> m_tasks;
    EventBus* m_eventBus = nullptr;

    /** Subscription IDs for EventBus topics (for cleanup). */
    std::vector<size_t> m_subscriptionIds;

    /** Observe EventBus events for live updates. */
    void setupEventBus();
    void teardownEventBus();
};

} // namespace downloader
} // namespace vlc

#endif // VLC_DOWNLOADER_DOWNLOAD_TASK_MODEL_HPP
