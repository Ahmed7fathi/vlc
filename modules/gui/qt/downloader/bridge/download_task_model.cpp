/*****************************************************************************
 * download_task_model.cpp
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

#include "download_task_model.hpp"
#include "../core/events/event_bus.hpp"
#include "../core/events/download_events.hpp"
#include "../models/download_task.hpp"

#include <QString>

namespace vlc {
namespace downloader {

DownloadTaskModel::DownloadTaskModel(EventBus* eventBus, QObject* parent)
    : QAbstractListModel(parent)
    , m_eventBus(eventBus)
{
    if (m_eventBus)
        setupEventBus();
}

DownloadTaskModel::~DownloadTaskModel()
{
    teardownEventBus();
}

void DownloadTaskModel::setupEventBus()
{
    if (!m_eventBus)
        return;

    /* Subscribe to TaskCreated to add new tasks to the model.
     * Note: All model mutations use QMetaObject::invokeMethod with
     * Qt::QueuedConnection to marshal to the GUI thread, since
     * EventBus callbacks may be invoked from background threads
     * (e.g., DownloadEngine progress events). */
    m_subscriptionIds.push_back(
        m_eventBus->subscribe<TaskCreated>(
            [this](const TaskCreated& e)
            {
                /* Capture task by value (shared_ptr) for thread safety */
                auto task = e.task;
                QMetaObject::invokeMethod(this, [this, task]() {
                    addTask(task);
                }, Qt::QueuedConnection);
            }));

    /* Subscribe to TaskStateChanged for live state updates */
    m_subscriptionIds.push_back(
        m_eventBus->subscribe<TaskStateChanged>(
            [this](const TaskStateChanged& e)
            {
                std::string taskId = e.task->id();
                QMetaObject::invokeMethod(this, [this, taskId]() {
                    onStateChanged(taskId);
                }, Qt::QueuedConnection);
            }));

    /* Subscribe to DownloadProgressEvent for live progress updates */
    m_subscriptionIds.push_back(
        m_eventBus->subscribe<DownloadProgressEvent>(
            [this](const DownloadProgressEvent& e)
            {
                std::string taskId = e.task->id();
                QMetaObject::invokeMethod(this, [this, taskId]() {
                    onProgressUpdate(taskId);
                }, Qt::QueuedConnection);
            }));

    /* Subscribe to other events that affect task appearance */
    m_subscriptionIds.push_back(
        m_eventBus->subscribe<AnalysisCompleted>(
            [this](const AnalysisCompleted& e)
            {
                std::string taskId = e.task->id();
                QMetaObject::invokeMethod(this, [this, taskId]() {
                    onStateChanged(taskId);
                }, Qt::QueuedConnection);
            }));

    m_subscriptionIds.push_back(
        m_eventBus->subscribe<AnalysisFailed>(
            [this](const AnalysisFailed& e)
            {
                std::string taskId = e.task->id();
                QMetaObject::invokeMethod(this, [this, taskId]() {
                    onStateChanged(taskId);
                }, Qt::QueuedConnection);
            }));

    m_subscriptionIds.push_back(
        m_eventBus->subscribe<DownloadCompleted>(
            [this](const DownloadCompleted& e)
            {
                std::string taskId = e.task->id();
                QMetaObject::invokeMethod(this, [this, taskId]() {
                    onStateChanged(taskId);
                }, Qt::QueuedConnection);
            }));

    m_subscriptionIds.push_back(
        m_eventBus->subscribe<DownloadFailed>(
            [this](const DownloadFailed& e)
            {
                std::string taskId = e.task->id();
                QMetaObject::invokeMethod(this, [this, taskId]() {
                    onStateChanged(taskId);
                }, Qt::QueuedConnection);
            }));
}

void DownloadTaskModel::teardownEventBus()
{
    if (!m_eventBus)
        return;

    /* The EventBus and model have the same lifetime (both owned by
     * DownloaderController singleton). Handlers are automatically
     * cleaned up when the EventBus is destroyed, so explicit
     * unsubscription is not strictly necessary. However, we clear
     * the subscription IDs for correctness. */
    m_subscriptionIds.clear();
}

int DownloadTaskModel::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid())
        return 0;
    return static_cast<int>(m_tasks.size());
}

QVariant DownloadTaskModel::data(const QModelIndex& index, int role) const
{
    int row = index.row();
    if (row < 0 || static_cast<size_t>(row) >= m_tasks.size())
        return {};

    const auto& task = m_tasks[row];
    if (!task)
        return {};

    switch (role)
    {
    case TaskIdRole:
        return QString::fromStdString(task->id());
    case UrlRole:
        return QString::fromStdString(task->url());
    case TitleRole:
        if (task->mediaInfo() && !task->mediaInfo()->title.empty())
            return QString::fromStdString(task->mediaInfo()->title);
        return QString::fromStdString(task->url());
    case StateRole:
        return static_cast<int>(task->state());
    case StateNameRole:
        return QString::fromLatin1(task->stateName());
    case ProgressRole:
        return task->progress();
    case SpeedRole:
        return task->speed();
    case EtaRole:
        return static_cast<qlonglong>(task->eta());
    case DownloadedBytesRole:
        return static_cast<qlonglong>(task->downloadedBytes());
    case TotalBytesRole:
        return static_cast<qlonglong>(task->totalBytes());
    case ErrorMessageRole:
        return QString::fromStdString(task->errorMessage());
    case OutputPathRole:
        return QString::fromStdString(task->outputPath());
    case IsActiveRole:
        return task->isActive();
    case IsTerminalRole:
        return task->isTerminal();
    case CurrentFileRole:
        return QString::fromStdString(task->currentFile());
    case AudioOnlyRole:
        return task->audioOnly();
    case EmbedMetadataRole:
        return task->embedMetadata();
    default:
        return {};
    }
}

QHash<int, QByteArray> DownloadTaskModel::roleNames() const
{
    return QHash<int, QByteArray>{
        {TaskIdRole,          "taskId"},
        {UrlRole,             "url"},
        {TitleRole,           "title"},
        {StateRole,           "state"},
        {StateNameRole,       "stateName"},
        {ProgressRole,        "progress"},
        {SpeedRole,           "speed"},
        {EtaRole,             "eta"},
        {DownloadedBytesRole, "downloadedBytes"},
        {TotalBytesRole,      "totalBytes"},
        {ErrorMessageRole,    "errorMessage"},
        {OutputPathRole,      "outputPath"},
        {IsActiveRole,        "isActive"},
        {IsTerminalRole,      "isTerminal"},
        {CurrentFileRole,     "currentFile"},
        {AudioOnlyRole,       "audioOnly"},
        {EmbedMetadataRole,   "embedMetadata"},
    };
}

void DownloadTaskModel::addTask(std::shared_ptr<DownloadTask> task)
{
    if (!task)
        return;

    beginInsertRows({}, static_cast<int>(m_tasks.size()),
                    static_cast<int>(m_tasks.size()));
    m_tasks.push_back(std::move(task));
    endInsertRows();

    emit countChanged();
}

void DownloadTaskModel::removeTask(const std::string& taskId)
{
    for (size_t i = 0; i < m_tasks.size(); ++i)
    {
        if (m_tasks[i] && m_tasks[i]->id() == taskId)
        {
            beginRemoveRows({}, static_cast<int>(i),
                            static_cast<int>(i));
            m_tasks.erase(m_tasks.begin() + static_cast<ptrdiff_t>(i));
            endRemoveRows();

            emit countChanged();
            return;
        }
    }
}

int DownloadTaskModel::findRow(const std::string& taskId) const
{
    for (size_t i = 0; i < m_tasks.size(); ++i)
    {
        if (m_tasks[i] && m_tasks[i]->id() == taskId)
            return static_cast<int>(i);
    }
    return -1;
}

std::shared_ptr<DownloadTask> DownloadTaskModel::taskAt(int row) const
{
    if (row < 0 || static_cast<size_t>(row) >= m_tasks.size())
        return nullptr;
    return m_tasks[row];
}

void DownloadTaskModel::clear()
{
    beginResetModel();
    m_tasks.clear();
    endResetModel();
    emit countChanged();
}

void DownloadTaskModel::onProgressUpdate(const std::string& taskId)
{
    int row = findRow(taskId);
    if (row < 0)
        return;

    QModelIndex idx = index(row);
    emit dataChanged(idx, idx, {
        ProgressRole, SpeedRole, EtaRole,
        DownloadedBytesRole, TotalBytesRole
    });
}

void DownloadTaskModel::onStateChanged(const std::string& taskId)
{
    int row = findRow(taskId);
    if (row < 0)
        return;

    QModelIndex idx = index(row);
    emit dataChanged(idx, idx, {
        StateRole, StateNameRole, IsActiveRole, IsTerminalRole,
        ErrorMessageRole, OutputPathRole,
        ProgressRole, SpeedRole, EtaRole,
        DownloadedBytesRole, TotalBytesRole
    });
}

} // namespace downloader
} // namespace vlc
