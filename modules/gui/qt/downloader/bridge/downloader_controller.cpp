/*****************************************************************************
 * downloader_controller.cpp
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

#include "downloader_controller.hpp"
#include "download_task_model.hpp"

#include "../core/download_orchestrator.hpp"
#include "../core/events/event_bus.hpp"
#include "../core/events/download_events.hpp"
#include "../models/download_settings.hpp"
#include "../models/download_task.hpp"
#include "../models/media_info.hpp"
#include "../providers/youtube_provider.hpp"

#include "qt.hpp"

#include <vlc_common.h>
#include <vlc_configuration.h>

#include <QSettings>
#include <algorithm>
#include <cstdio>

namespace vlc {
namespace downloader {

DownloaderController* DownloaderController::s_instance = nullptr;

DownloaderController::DownloaderController(qt_intf_t* p_intf, QObject* parent)
    : QObject(parent)
    , m_intf(p_intf)
{
    if (!p_intf)
        return;

    /* Create the event bus */
    m_eventBus = new EventBus();

    /* Create the task model (connects to EventBus for live updates) */
    m_taskModel = new DownloadTaskModel(m_eventBus, this);

    /* Create default settings, then try to load saved preferences */
    DownloadSettings settings = loadSettings();

    /* Create the orchestrator */
    m_orchestrator = std::make_unique<DownloadOrchestrator>(
        VLC_OBJECT(p_intf), *m_eventBus, settings);

    /* Register available providers */
    {
        auto youtubeProvider = std::make_unique<YoutubeProvider>(VLC_OBJECT(p_intf));
        m_orchestrator->providers().registerProvider(std::move(youtubeProvider), 10);
    }

    /* Set singleton instance */
    s_instance = this;

    /* Subscribe to EventBus events to track active download count */
    setupEventBus();

    if (m_intf)
        msg_Dbg(VLC_OBJECT(m_intf), "DownloaderController: initialized");
}

DownloaderController::~DownloaderController()
{
    /* Clear singleton pointer */
    if (s_instance == this)
        s_instance = nullptr;

    m_subscriptionIds.clear();
}

bool DownloaderController::hasActiveDownloads() const
{
    return activeCount() > 0;
}

int DownloaderController::activeCount() const
{
    if (!m_orchestrator)
        return 0;
    return static_cast<int>(m_orchestrator->queue().activeCount());
}

QString DownloaderController::defaultDownloadPath() const
{
    return QString::fromStdString(
        m_orchestrator ? m_orchestrator->settings().defaultDownloadPath : "");
}

void DownloaderController::setDefaultDownloadPath(const QString& path)
{
    if (!m_orchestrator)
        return;

    auto settings = m_orchestrator->settings();
    if (settings.defaultDownloadPath == path.toStdString())
        return;

    settings.defaultDownloadPath = path.toStdString();
    m_orchestrator->setSettings(settings);
    emit defaultDownloadPathChanged();
}

std::shared_ptr<DownloadTask> DownloaderController::findTask(
    const QString& taskId) const
{
    if (!m_orchestrator)
        return nullptr;

    return m_orchestrator->queue().findTask(taskId.toStdString());
}

QString DownloaderController::createTask(const QString& url)
{
    if (!m_orchestrator || url.isEmpty())
        return {};

    auto task = m_orchestrator->createTask(url.toStdString());
    if (!task)
        return {};

    /* The TaskCreated event is published by the orchestrator.
     * The model subscribes to this event and adds the task. */
    return QString::fromStdString(task->id());
}

void DownloaderController::analyzeTask(const QString& taskId)
{
    if (!m_orchestrator)
        return;

    auto task = findTask(taskId);
    if (!task)
        return;

    m_orchestrator->analyzeTask(std::move(task));
}

bool DownloaderController::confirmDownload(const QString& taskId,
                                            int formatIdx,
                                            bool audioOnly)
{
    if (!m_orchestrator)
        return false;

    auto task = findTask(taskId);
    if (!task)
        return false;

    /* Apply user selections */
    if (formatIdx >= 0)
    {
        task->selectVideoFormat(static_cast<size_t>(formatIdx));
    }
    if (audioOnly)
    {
        task->setAudioOnly(true);
    }

    return m_orchestrator->confirmDownload(std::move(task));
}

bool DownloaderController::cancelTask(const QString& taskId)
{
    if (!m_orchestrator)
        return false;
    return m_orchestrator->cancelTask(taskId.toStdString());
}

bool DownloaderController::pauseTask(const QString& taskId)
{
    if (!m_orchestrator)
        return false;
    return m_orchestrator->pauseTask(taskId.toStdString());
}

bool DownloaderController::resumeTask(const QString& taskId)
{
    if (!m_orchestrator)
        return false;
    return m_orchestrator->resumeTask(taskId.toStdString());
}

bool DownloaderController::retryTask(const QString& taskId)
{
    if (!m_orchestrator)
        return false;
    return m_orchestrator->retryTask(taskId.toStdString());
}

QString DownloaderController::taskIdAt(int row) const
{
    if (!m_taskModel)
        return {};
    auto task = m_taskModel->taskAt(row);
    if (!task)
        return {};
    return QString::fromStdString(task->id());
}

QString DownloaderController::stateName(const QString& taskId) const
{
    auto task = findTask(taskId);
    if (!task)
        return {};
    return QString::fromLatin1(task->stateName());
}

void DownloaderController::removeCompletedTasks()
{
    if (!m_orchestrator)
        return;

    m_orchestrator->queue().clearCompleted();
}

void DownloaderController::setupEventBus()
{
    if (!m_eventBus)
        return;

    /* Track state changes to update active download count */
    m_subscriptionIds.push_back(
        m_eventBus->subscribe<TaskStateChanged>(
            [this](const TaskStateChanged& e)
            {
                /* Active downloads are in Downloading state.
                 * State changes may enter or leave this state. */
                bool wasActive = (e.oldState == DownloadTask::State::Downloading);
                bool isActive = (e.newState == DownloadTask::State::Downloading);
                if (wasActive != isActive)
                {
                    QMetaObject::invokeMethod(this, [this]() {
                        updateActiveState();
                    }, Qt::QueuedConnection);
                }
            }));

    /* Also update on task creation/removal which affects counts */
    m_subscriptionIds.push_back(
        m_eventBus->subscribe<TaskCreated>(
            [this](const TaskCreated& /*e*/)
            {
                QMetaObject::invokeMethod(this, [this]() {
                    updateActiveState();
                }, Qt::QueuedConnection);
            }));

    /* Forward analysis completion to the UI thread via Qt signals.
     * This replaces polling: instead of the dialog checking task state
     * via a timer, the controller emits a signal when the analysis
     * event arrives from the background thread. */
    m_subscriptionIds.push_back(
        m_eventBus->subscribe<AnalysisCompleted>(
            [this](const AnalysisCompleted& e)
            {
                QString taskId = QString::fromStdString(e.task->id());
                QMetaObject::invokeMethod(this, [this, taskId]() {
                    emit analysisReady(taskId);
                }, Qt::QueuedConnection);
            }));

    m_subscriptionIds.push_back(
        m_eventBus->subscribe<AnalysisFailed>(
            [this](const AnalysisFailed& e)
            {
                QString taskId = QString::fromStdString(e.task->id());
                QString error = QString::fromStdString(e.errorMessage);
                QMetaObject::invokeMethod(this, [this, taskId, error]() {
                    emit analysisError(taskId, error);
                }, Qt::QueuedConnection);
            }));

    /* Forward download progress to the UI thread */
    m_subscriptionIds.push_back(
        m_eventBus->subscribe<DownloadProgressEvent>(
            [this](const DownloadProgressEvent& e)
            {
                QString taskId = QString::fromStdString(e.task->id());
                int percent = e.percent;
                double speed = e.speed;
                int64_t eta = e.eta;
                QMetaObject::invokeMethod(this, [this, taskId, percent, speed, eta]() {
                    emit downloadProgress(taskId, percent, speed, eta);
                }, Qt::QueuedConnection);
            }));

    /* Forward download completion to the UI thread */
    m_subscriptionIds.push_back(
        m_eventBus->subscribe<DownloadCompleted>(
            [this](const DownloadCompleted& e)
            {
                QString taskId = QString::fromStdString(e.task->id());
                QString outputPath = QString::fromStdString(e.task->outputPath());
                QMetaObject::invokeMethod(this, [this, taskId, outputPath]() {
                    emit downloadCompleted(taskId, outputPath);
                }, Qt::QueuedConnection);
            }));

    /* Forward download failure to the UI thread */
    m_subscriptionIds.push_back(
        m_eventBus->subscribe<DownloadFailed>(
            [this](const DownloadFailed& e)
            {
                QString taskId = QString::fromStdString(e.task->id());
                QString error = QString::fromStdString(e.errorMessage);
                QMetaObject::invokeMethod(this, [this, taskId, error]() {
                    emit downloadFailed(taskId, error);
                }, Qt::QueuedConnection);
            }));
}

DownloadSettings DownloaderController::loadSettings() const
{
    DownloadSettings settings;

    /* Compute defaults */
    auto getDefaultPath = []() -> std::string {
        const char* home = std::getenv("HOME");
        if (home)
            return std::string(home) + "/Downloads";
        return "/tmp";
    };
    settings.defaultDownloadPath = getDefaultPath();
    settings.preferredVideoHeight = 720;
    settings.maxConcurrentDownloads = 3;

    /* Try to load saved preferences from QSettings.
     * This is the persistence mechanism — saveSettings() writes here.
     * The old VLC config_PutPsz could never be read back (the keys
     * were never registered as proper config items), so QSettings is
     * the only reliable way to persist user preferences. */
    {
        QSettings qs(QStringLiteral("VideoLAN"), QStringLiteral("VLC"));
        qs.beginGroup(QStringLiteral("Downloader"));

        QString savedPath = qs.value(QStringLiteral("downloadPath")).toString();
        if (!savedPath.isEmpty())
            settings.defaultDownloadPath = savedPath.toStdString();

        int savedHeight = qs.value(QStringLiteral("preferredHeight"), -1).toInt();
        if (savedHeight > 0)
            settings.preferredVideoHeight = savedHeight;

        int savedMaxConcurrent = qs.value(QStringLiteral("maxConcurrent"), -1).toInt();
        if (savedMaxConcurrent > 0)
            settings.maxConcurrentDownloads = savedMaxConcurrent;

        qs.endGroup();
    }

    return settings;
}

void DownloaderController::saveSettings()
{
    if (!m_intf || !m_orchestrator)
        return;

    auto settings = m_orchestrator->settings();

    /* Save to QSettings (persists across sessions) */
    QSettings qs(QStringLiteral("VideoLAN"), QStringLiteral("VLC"));
    qs.beginGroup(QStringLiteral("Downloader"));
    qs.setValue(QStringLiteral("downloadPath"),
                QString::fromStdString(settings.defaultDownloadPath));
    qs.setValue(QStringLiteral("preferredHeight"), settings.preferredVideoHeight);
    qs.setValue(QStringLiteral("maxConcurrent"), settings.maxConcurrentDownloads);
    qs.endGroup();
    qs.sync();

    /* Also save to VLC config for other components */
    config_PutPsz("download-dir",
                  settings.defaultDownloadPath.c_str());
    config_PutInt("download-quality",
                  settings.preferredVideoHeight);
    config_PutInt("download-max-concurrent",
                  static_cast<int64_t>(settings.maxConcurrentDownloads));
    config_PutInt("download-embed-metadata",
                  settings.embedMetadata ? 1 : 0);
    config_PutInt("download-embed-subs",
                  settings.embedSubtitles ? 1 : 0);
    config_SaveConfigFile(VLC_OBJECT(m_intf));

    if (m_intf)
        msg_Dbg(VLC_OBJECT(m_intf), "DownloaderController: settings saved");
}

QString DownloaderController::debugDump() const
{
    QString info;
    info += QStringLiteral("DownloaderController: this=%1, m_taskModel=%2")
        .arg(reinterpret_cast<quintptr>(this), 0, 16)
        .arg(reinterpret_cast<quintptr>(m_taskModel), 0, 16);

    if (m_taskModel)
    {
        int count = m_taskModel->rowCount();
        info += QStringLiteral(", count=%1").arg(count);
        for (int i = 0; i < count; ++i)
        {
            QModelIndex idx = m_taskModel->index(i, 0);
            QString id = m_taskModel->data(idx, DownloadTaskModel::TaskIdRole).toString();
            QString title = m_taskModel->data(idx, DownloadTaskModel::TitleRole).toString();
            info += QStringLiteral("\n  [%1] id=%2 title=%3")
                .arg(i).arg(id).arg(title);
        }
    }

    if (m_orchestrator)
    {
        info += QStringLiteral(", orchestrator=%1")
            .arg(reinterpret_cast<quintptr>(m_orchestrator.get()), 0, 16);
    }

    fprintf(stderr, "[QML Debug] %s\n", info.toUtf8().constData());
    return info;
}

void DownloaderController::updateActiveState()
{
    /* Emit signals for both properties */
    emit hasActiveDownloadsChanged();
    emit activeCountChanged();
}

// ── Media info accessors ────────────────────────────────────────────────

QVariantMap DownloaderController::mediaInfoForTask(const QString& taskId) const
{
    auto task = findTask(taskId);
    if (!task)
        return {};

    auto* info = task->mediaInfo();
    if (!info)
        return {};

    QVariantMap map;
    map[QStringLiteral("title")] = QString::fromStdString(info->title);
    map[QStringLiteral("uploader")] = QString::fromStdString(info->uploader);
    map[QStringLiteral("description")] = QString::fromStdString(info->description);
    map[QStringLiteral("thumbnailUrl")] = QString::fromStdString(info->thumbnailUrl);
    map[QStringLiteral("url")] = QString::fromStdString(info->url);
    map[QStringLiteral("webpageUrl")] = QString::fromStdString(info->webpageUrl);
    map[QStringLiteral("extractor")] = QString::fromStdString(info->extractor);
    map[QStringLiteral("uploadDate")] = QString::fromStdString(info->uploadDate);
    map[QStringLiteral("viewCount")] = static_cast<qlonglong>(info->viewCount);

    /* Format duration as HH:MM:SS or MM:SS */
    int64_t d = info->duration;
    if (d > 0)
    {
        int hours = static_cast<int>(d / 3600);
        int mins = static_cast<int>((d % 3600) / 60);
        int secs = static_cast<int>(d % 60);
        if (hours > 0)
        {
            char buf[24] = {};
            std::snprintf(buf, sizeof(buf), "%d:%02d:%02d", hours, mins, secs);
            map[QStringLiteral("duration")] = QString::fromLatin1(buf);
        }
        else
        {
            char buf[24] = {};
            std::snprintf(buf, sizeof(buf), "%d:%02d", mins, secs);
            map[QStringLiteral("duration")] = QString::fromLatin1(buf);
        }
    }

    return map;
}

QVariantList DownloaderController::videoFormatsForTask(const QString& taskId) const
{
    auto task = findTask(taskId);
    if (!task)
        return {};

    auto* info = task->mediaInfo();
    if (!info)
        return {};

    QVariantList list;
    for (const auto& fmt : info->videoFormats)
    {
        QVariantMap entry;
        entry[QStringLiteral("id")] = QString::fromStdString(fmt.id);

        /* Build resolution string */
        if (fmt.height > 0)
        {
            char buf[16] = {};
            std::snprintf(buf, sizeof(buf), "%dp", fmt.height);
            entry[QStringLiteral("resolution")] = QString::fromLatin1(buf);
        }
        else
        {
            entry[QStringLiteral("resolution")] = QStringLiteral("Audio Only");
        }

        entry[QStringLiteral("codec")] = QString::fromStdString(fmt.codec);
        entry[QStringLiteral("extension")] = QString::fromStdString(fmt.extension);
        entry[QStringLiteral("hasAudio")] = fmt.hasAudio;
        entry[QStringLiteral("hasVideo")] = fmt.hasVideo;

        /* Format bitrate */
        if (fmt.bitrate > 0)
        {
            if (fmt.bitrate >= 1000000)
            {
                char buf[16] = {};
                std::snprintf(buf, sizeof(buf), "~%.0f Mbps",
                             static_cast<double>(fmt.bitrate) / 1000000.0);
                entry[QStringLiteral("bitrate")] = QString::fromLatin1(buf);
            }
            else
            {
                char buf[16] = {};
                std::snprintf(buf, sizeof(buf), "~%.0f Kbps",
                             static_cast<double>(fmt.bitrate) / 1000.0);
                entry[QStringLiteral("bitrate")] = QString::fromLatin1(buf);
            }
        }
        else
        {
            entry[QStringLiteral("bitrate")] = QString();
        }

        /* Format file size */
        if (fmt.filesize > 0)
        {
            if (fmt.filesize >= 1073741824LL)
            {
                char buf[16] = {};
                std::snprintf(buf, sizeof(buf), "~%.1f GB",
                             static_cast<double>(fmt.filesize) / 1073741824.0);
                entry[QStringLiteral("fileSize")] = QString::fromLatin1(buf);
            }
            else if (fmt.filesize >= 1048576)
            {
                char buf[16] = {};
                std::snprintf(buf, sizeof(buf), "~%.0f MB",
                             static_cast<double>(fmt.filesize) / 1048576.0);
                entry[QStringLiteral("fileSize")] = QString::fromLatin1(buf);
            }
            else
            {
                char buf[16] = {};
                std::snprintf(buf, sizeof(buf), "~%.0f KB",
                             static_cast<double>(fmt.filesize) / 1024.0);
                entry[QStringLiteral("fileSize")] = QString::fromLatin1(buf);
            }
        }
        else
        {
            entry[QStringLiteral("fileSize")] = QString();
        }

        list.append(entry);
    }

    return list;
}

QVariantList DownloaderController::audioFormatsForTask(const QString& taskId) const
{
    auto task = findTask(taskId);
    if (!task)
        return {};

    auto* info = task->mediaInfo();
    if (!info)
        return {};

    QVariantList list;
    for (const auto& fmt : info->audioFormats)
    {
        QVariantMap entry;
        entry[QStringLiteral("id")] = QString::fromStdString(fmt.id);
        entry[QStringLiteral("codec")] = QString::fromStdString(fmt.codec);
        entry[QStringLiteral("extension")] = QString::fromStdString(fmt.extension);

        /* Build name: "AAC 192 Kbps" or similar */
        {
            QString name = QString::fromStdString(fmt.codec);
            if (fmt.bitrate > 0)
            {
                char buf[16] = {};
                std::snprintf(buf, sizeof(buf), " (%d Kbps)", fmt.bitrate);
                name += QString::fromLatin1(buf);
            }
            entry[QStringLiteral("name")] = name;
        }

        /* Format bitrate for display */
        if (fmt.bitrate > 0)
        {
            char buf[16] = {};
            std::snprintf(buf, sizeof(buf), "%d Kbps", fmt.bitrate);
            entry[QStringLiteral("bitrate")] = QString::fromLatin1(buf);
        }
        else
        {
            entry[QStringLiteral("bitrate")] = QString();
        }

        /* Format sample rate for display */
        if (fmt.sampleRate > 0)
        {
            char buf[16] = {};
            std::snprintf(buf, sizeof(buf), "%d Hz", fmt.sampleRate);
            entry[QStringLiteral("sampleRate")] = QString::fromLatin1(buf);
        }
        else
        {
            entry[QStringLiteral("sampleRate")] = QString();
        }

        list.append(entry);
    }

    return list;
}

QVariantList DownloaderController::subtitleFormatsForTask(const QString& taskId) const
{
    auto task = findTask(taskId);
    if (!task)
        return {};

    auto* info = task->mediaInfo();
    if (!info)
        return {};

    QVariantList list;
    for (const auto& sub : info->subtitles)
    {
        QVariantMap entry;
        entry[QStringLiteral("id")] = QString::fromStdString(sub.id);
        entry[QStringLiteral("language")] = QString::fromStdString(sub.language);
        entry[QStringLiteral("name")] = QString::fromStdString(sub.name);
        entry[QStringLiteral("isAutomatic")] = sub.isAutomatic;
        entry[QStringLiteral("type")] = sub.isAutomatic
            ? QStringLiteral("Automatic (VTT)")
            : QStringLiteral("External (SRT)");
        list.append(entry);
    }

    return list;
}

} // namespace downloader
} // namespace vlc
