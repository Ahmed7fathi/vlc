/*****************************************************************************
 * download_orchestrator.cpp
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

#include "download_orchestrator.hpp"
#include "strategies/ytdlp_strategy.hpp"
#include "processors/ffmpeg_processor.hpp"
#include "utils/file_manager.hpp"
#include "utils/temp_file_manager.hpp"

#include <vlc_common.h>
#include <vlc_configuration.h>

#include <cstring>

namespace vlc {
namespace downloader {

using State = DownloadStateMachine::State;

DownloadOrchestrator::DownloadOrchestrator(vlc_object_t* vlcObj,
                                            EventBus& eventBus,
                                            const DownloadSettings& settings)
    : m_vlcObj(vlcObj)
    , m_eventBus(eventBus)
    , m_queue(eventBus)
    , m_settings(settings)
{
    /* Register the callback for when the queue processes the next task */
    m_queue.onProcessNext([this](std::shared_ptr<DownloadTask> task) {
        onProcessNext(std::move(task));
    });
}

std::shared_ptr<DownloadTask> DownloadOrchestrator::createTask(const std::string& url)
{
    auto task = DownloadTask::create(url);
    if (!task)
        return nullptr;

    /* Add to the queue immediately so findTask() works during analysis */
    m_queue.addTask(task);

    if (m_vlcObj)
        msg_Dbg(m_vlcObj, "DownloadOrchestrator: created task %s for %s",
                task->id().c_str(), url.c_str());

    return task;
}

void DownloadOrchestrator::analyzeTask(std::shared_ptr<DownloadTask> task)
{
    if (!task)
        return;

    /* Transition to Analyzing state */
    if (!task->transitionTo(State::Analyzing))
        return;

    /* Find a provider for this URL */
    IMediaProvider* provider = m_providers.findProvider(task->url());
    if (!provider)
    {
        std::string error = "No provider found for URL: " + task->url();
        task->setError(error);
        task->transitionTo(State::Failed);
        m_eventBus.publish(AnalysisFailed{task, error});

        if (m_vlcObj)
            msg_Err(m_vlcObj, "DownloadOrchestrator: %s", error.c_str());
        return;
    }

    if (m_vlcObj)
        msg_Dbg(m_vlcObj, "DownloadOrchestrator: analyzing %s with provider %s",
                task->url().c_str(), provider->name().c_str());

    /* Run analysis. The provider calls back on success or error. */
    provider->analyze(task,
        /* onSuccess */
        [this](std::shared_ptr<DownloadTask> t, std::unique_ptr<MediaInfo> info)
        {
            t->setMediaInfo(std::move(info));
            t->transitionTo(State::Ready);
            m_eventBus.publish(AnalysisCompleted{t});

            if (m_vlcObj)
                msg_Dbg(m_vlcObj, "DownloadOrchestrator: analysis ready for %s",
                        t->url().c_str());
        },
        /* onError */
        [this](std::shared_ptr<DownloadTask> t, const std::string& errorMsg)
        {
            t->setError(errorMsg);
            t->transitionTo(State::Failed);
            m_eventBus.publish(AnalysisFailed{t, errorMsg});

            if (m_vlcObj)
                msg_Err(m_vlcObj, "DownloadOrchestrator: analysis failed for %s: %s",
                        t->url().c_str(), errorMsg.c_str());
        });
}

bool DownloadOrchestrator::confirmDownload(std::shared_ptr<DownloadTask> task)
{
    if (!task)
        return false;

    /* Apply default settings for any unset selections */
    if (task->selectedVideoFormat() == nullptr && !task->audioOnly())
    {
        /* Auto-select the best video format based on preferred height */
        if (task->mediaInfo() && !task->mediaInfo()->videoFormats.empty())
        {
            /* Find the format closest to the preferred height */
            size_t bestIdx = 0;
            int bestHeight = 0;
            for (size_t i = 0; i < task->mediaInfo()->videoFormats.size(); ++i)
            {
                const auto& fmt = task->mediaInfo()->videoFormats[i];
                if (fmt.height <= m_settings.preferredVideoHeight && fmt.height > bestHeight)
                {
                    bestHeight = fmt.height;
                    bestIdx = i;
                }
            }
            task->selectVideoFormat(bestIdx);
        }
    }

    /* Enqueue the task */
    return m_queue.enqueue(task);
}

bool DownloadOrchestrator::cancelTask(const std::string& taskId)
{
    /* If the task is actively downloading, cancel the engine */
    auto it = m_activeEngines.find(taskId);
    if (it != m_activeEngines.end())
    {
        it->second->cancel();
        /* Engine will transition task to Cancelled on its thread */
    }

    /* Also let the queue handle state transitions */
    return m_queue.cancel(taskId);
}

bool DownloadOrchestrator::pauseTask(const std::string& taskId)
{
    /* If the task is actively downloading, cancel the engine to pause */
    auto it = m_activeEngines.find(taskId);
    if (it != m_activeEngines.end())
    {
        it->second->cancel();
    }

    return m_queue.pause(taskId);
}

bool DownloadOrchestrator::resumeTask(const std::string& taskId)
{
    return m_queue.resume(taskId);
}

bool DownloadOrchestrator::retryTask(const std::string& taskId)
{
    return m_queue.retry(taskId);
}

void DownloadOrchestrator::onEngineComplete(const std::string& taskId, bool succeeded)
{
    if (m_vlcObj)
        msg_Dbg(m_vlcObj, "DownloadOrchestrator: engine completed for task %s (%s)",
                taskId.c_str(), succeeded ? "success" : "failure");

    /* Remove the engine from the active map */
    m_activeEngines.erase(taskId);

    /* The queue may have more tasks waiting — process next */
    /* processNext is called implicitly when the state change callback fires */
}

ProcessingPipeline DownloadOrchestrator::createPipeline(const DownloadTask& task) const
{
    ProcessingPipeline pipeline;

    /* Add post-processing steps based on task settings */

    /* 1. Embed metadata if requested */
    if (task.embedMetadata())
    {
        pipeline.addStep(
            std::make_unique<FFmpegProcessor>(m_vlcObj,
                FFmpegProcessor::Operation::EmbedMetadata));
    }

    /* 2. Embed subtitles if requested */
    if (task.embedSubtitles())
    {
        pipeline.addStep(
            std::make_unique<FFmpegProcessor>(m_vlcObj,
                FFmpegProcessor::Operation::EmbedSubtitles));
    }

    return pipeline;
}

void DownloadOrchestrator::onProcessNext(std::shared_ptr<DownloadTask> task)
{
    if (!task)
        return;

    if (m_vlcObj)
        msg_Dbg(m_vlcObj, "DownloadOrchestrator: processing next task %s (%s)",
                task->id().c_str(), task->url().c_str());

    /* Apply filename template and output path from settings */
    if (task->filenameTemplate().empty())
        task->setFilenameTemplate(m_settings.filenameTemplate);

    std::string finalPath;
    if (task->outputPath().empty())
    {
        /* Build output path using the title if available */
        std::string filename;
        if (task->mediaInfo() && !task->mediaInfo()->title.empty())
            filename = task->mediaInfo()->title;
        else
            filename = "download";

        filename = FileManager::sanitizeFilename(filename);

        std::string ext;
        if (task->audioOnly() && task->selectedAudioFormat())
            ext = "." + task->selectedAudioFormat()->extension;
        else
            /* For video downloads, we always force --merge-output-format mp4
             * in YtdlpStrategy, so the output extension is always .mp4.
             * Don't use selectedVideoFormat()->extension here because formats
             * like "mhtml" or "3gp" are not valid output targets. */
            ext = ".mp4";

        /* Resolve the full output path with collision avoidance */
        auto result = FileManager::resolveOutputPath(
            m_settings.defaultDownloadPath, filename, ext, true);

        if (result.succeeded)
        {
            finalPath = result.path;
            task->setOutputPath(finalPath);
        }
        else
        {
            /* Fallback: use the raw path without collision avoidance */
            finalPath = m_settings.defaultDownloadPath + "/" + filename + ext;
            task->setOutputPath(finalPath);
        }
    }
    else
    {
        finalPath = task->outputPath();
    }

    /* Ensure the output directory exists */
    if (!FileManager::ensureParentDirectories(finalPath))
    {
        std::string error = "Failed to create output directory for: " + finalPath;
        task->setError(error);
        task->transitionTo(State::Failed);
        m_eventBus.publish(DownloadFailed{task, error});

        if (m_vlcObj)
            msg_Err(m_vlcObj, "DownloadOrchestrator: %s", error.c_str());
        return;
    }

    /* Create a TempFileManager that owns the temp download file.
     * Ownership is transferred to DownloadEngine so the file
     * stays alive during the entire download operation. */
    std::string tempExt;
    if (task->audioOnly() && task->selectedAudioFormat())
        tempExt = "." + task->selectedAudioFormat()->extension;
    else
        /* Temp file extension must match the final output extension.
         * We force --merge-output-format mp4, so the temp file is .mp4. */
        tempExt = ".mp4";

    auto tempMgr = std::make_unique<TempFileManager>();
    std::string tempPath = tempMgr->createTempFile("vlc_download", tempExt);
    if (tempPath.empty())
    {
        std::string error = "Failed to create temporary download file";
        task->setError(error);
        task->transitionTo(State::Failed);
        m_eventBus.publish(DownloadFailed{task, error});

        if (m_vlcObj)
            msg_Err(m_vlcObj, "DownloadOrchestrator: %s", error.c_str());
        return;
    }

    /* Resolve yt-dlp path from VLC config.
     * ytdl-path is defined in modules/demux/ytdl.c, not in core libvlc-module.c.
     * Guard with config_FindConfig to avoid asserting in config_GetPsz. */
    std::string ytdlpPath = "yt-dlp";
    if (m_vlcObj && config_FindConfig("ytdl-path"))
    {
        char* configPath = var_InheritString(m_vlcObj, "ytdl-path");
        if (configPath)
        {
            ytdlpPath = configPath;
            free(configPath);
        }
    }

    /* Create the download strategy */
    auto strategy = std::make_unique<YtdlpStrategy>(m_vlcObj, ytdlpPath);

    /* Create the post-processing pipeline */
    ProcessingPipeline pipeline = createPipeline(*task);

    /* Create a cancellation token for this download */
    auto token = std::make_shared<CancellationToken>();

    /* Create the download engine */
    auto engine = std::make_unique<DownloadEngine>(m_vlcObj, m_eventBus);

    std::string taskId = task->id();

    /* Register completion callback to clean up the active engine */
    engine->onComplete(
        [this, taskId](bool succeeded, const std::string& /*outputPath*/)
        {
            onEngineComplete(taskId, succeeded);
        });

    /* Store the engine in the active map before starting (avoids race) */
    m_activeEngines[taskId] = std::move(engine);

    /* Start the download */
    DownloadEngine* rawEngine = m_activeEngines[taskId].get();
    rawEngine->start(std::move(task), std::move(strategy), std::move(pipeline),
                     tempPath, std::move(tempMgr), finalPath, std::move(token));
}

} // namespace downloader
} // namespace vlc
