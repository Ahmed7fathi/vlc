/*****************************************************************************
 * download_dialog.cpp
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

#include "download_dialog.hpp"

#include <QFormLayout>
#include <QGroupBox>
#include <QScreen>
#include <QGuiApplication>
#include <QFont>
#include <QFrame>
#include <QHBoxLayout>
#include <QFileDialog>
#include <QDir>

#include <thread>
#include <cstdio>

#include "downloader_controller.hpp"
#include "../core/utils/file_manager.hpp"

#include <vlc_common.h>

namespace vlc {
namespace downloader {

DownloadDialog::DownloadDialog(qt_intf_t* p_intf, const QString& initialUrl,
                               QWidget* parent)
    : QDialog(parent)
    , m_intf(p_intf)
{
    setWindowTitle(tr("Download Media"));
    setAttribute(Qt::WA_DeleteOnClose);
    setMinimumSize(880, 900);
    resize(880, 900);

    buildUi();

    if (!initialUrl.isEmpty())
        m_urlField->setText(initialUrl);

    centerOnParent();
}

void DownloadDialog::buildUi()
{
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(32, 28, 32, 28);
    mainLayout->setSpacing(20);

    // ── Title ────────────────────────────────────────────────────────────
    auto* titleLabel = new QLabel(tr("Download Media"));
    QFont titleFont = titleLabel->font();
    titleFont.setPointSize(titleFont.pointSize() + 10);
    titleFont.setBold(true);
    titleLabel->setFont(titleFont);
    mainLayout->addWidget(titleLabel);

    // ── URL Input ────────────────────────────────────────────────────────
    auto* urlLayout = new QHBoxLayout();
    urlLayout->setSpacing(8);
    m_urlField = new QLineEdit();
    QFont urlFont = m_urlField->font();
    urlFont.setPointSize(urlFont.pointSize() + 2);
    m_urlField->setFont(urlFont);
    m_urlField->setPlaceholderText(tr("Paste a YouTube or media URL here..."));
    m_urlField->setMinimumHeight(44);
    urlLayout->addWidget(m_urlField, 1);

    m_analyzeBtn = new QPushButton(tr("Analyze"));
    m_analyzeBtn->setMinimumHeight(44);
    m_analyzeBtn->setMinimumWidth(120);
    m_analyzeBtn->setDefault(true);
    QFont btnFont = m_analyzeBtn->font();
    btnFont.setPointSize(btnFont.pointSize() + 1);
    btnFont.setBold(true);
    m_analyzeBtn->setFont(btnFont);
    m_analyzeBtn->setCursor(Qt::PointingHandCursor);
    m_analyzeBtn->setStyleSheet(
        "QPushButton { background-color: #1565c0; color: white; font-weight: bold; "
        "padding: 8px 24px; border-radius: 4px; }"
        "QPushButton:hover { background-color: #1976d2; }"
        "QPushButton:disabled { background-color: #ccc; color: #888; }");
    urlLayout->addWidget(m_analyzeBtn);
    mainLayout->addLayout(urlLayout);

    connect(m_analyzeBtn, &QPushButton::clicked, this, &DownloadDialog::onAnalyzeClicked);
    connect(m_urlField, &QLineEdit::returnPressed, this, &DownloadDialog::onAnalyzeClicked);

    // ── Progress bar (indeterminate during analysis, determinate during download) ─
    m_progressBar = new QProgressBar();
    m_progressBar->setRange(0, 0); // indeterminate initially
    m_progressBar->setTextVisible(false);
    m_progressBar->setFixedHeight(10);
    m_progressBar->hide();
    mainLayout->addWidget(m_progressBar);

    // ── Progress label (speed/ETA shown during download) ──────────────────
    m_progressLabel = new QLabel();
    QFont progressFont = m_progressLabel->font();
    progressFont.setPointSize(progressFont.pointSize() + 1);
    m_progressLabel->setFont(progressFont);
    m_progressLabel->setStyleSheet("color: #555;");
    m_progressLabel->setAlignment(Qt::AlignCenter);
    m_progressLabel->hide();
    mainLayout->addWidget(m_progressLabel);

    // ── Error label (shown when analysis fails) ───────────────────────────
    m_errorLabel = new QLabel();
    m_errorLabel->setStyleSheet(
        "QLabel { color: #c62828; font-weight: bold; padding: 12px; "
        "background-color: #ffebee; border-radius: 6px; font-size: 13px; }");
    m_errorLabel->setWordWrap(true);
    m_errorLabel->hide();
    mainLayout->addWidget(m_errorLabel);

    // ── Separator ────────────────────────────────────────────────────────
    auto* sep = new QFrame();
    sep->setFrameShape(QFrame::HLine);
    sep->setFrameShadow(QFrame::Sunken);
    mainLayout->addWidget(sep);

    // ── Info Section ─────────────────────────────────────────────────────
    m_infoSection = new QWidget();
    m_infoSection->hide();
    auto* infoLayout = new QVBoxLayout(m_infoSection);
    infoLayout->setContentsMargins(0, 0, 0, 0);
    infoLayout->setSpacing(8);

    m_titleLabel = new QLabel();
    QFont infoFont = m_titleLabel->font();
    infoFont.setPointSize(infoFont.pointSize() + 4);
    infoFont.setBold(true);
    m_titleLabel->setFont(infoFont);
    m_titleLabel->setWordWrap(true);
    infoLayout->addWidget(m_titleLabel);

    // Metadata row (duration · uploader)
    auto* metaLayout = new QHBoxLayout();
    metaLayout->setSpacing(16);

    m_durationLabel = new QLabel();
    QFont metaFont = m_durationLabel->font();
    metaFont.setPointSize(metaFont.pointSize() + 1);
    m_durationLabel->setFont(metaFont);
    m_durationLabel->setStyleSheet("color: #777;");
    metaLayout->addWidget(m_durationLabel);

    auto* metaSep = new QLabel(QStringLiteral("·"));
    metaSep->setStyleSheet("color: #ccc;");
    metaLayout->addWidget(metaSep);

    m_uploaderLabel = new QLabel();
    m_uploaderLabel->setFont(metaFont);
    m_uploaderLabel->setStyleSheet("color: #777;");
    m_uploaderLabel->setWordWrap(true);
    metaLayout->addWidget(m_uploaderLabel, 1);

    infoLayout->addLayout(metaLayout);

    m_descLabel = new QLabel();
    QFont descFont = m_descLabel->font();
    descFont.setPointSize(descFont.pointSize());
    m_descLabel->setFont(descFont);
    m_descLabel->setWordWrap(true);
    m_descLabel->setMaximumHeight(100);
    m_descLabel->setStyleSheet("color: #666;");
    infoLayout->addWidget(m_descLabel);

    // Save-to preview
    m_saveToLabel = new QLabel();
    QFont saveFont = m_saveToLabel->font();
    saveFont.setPointSize(saveFont.pointSize());
    m_saveToLabel->setFont(saveFont);
    m_saveToLabel->setStyleSheet(
        "QLabel { color: #0d47a1; font-weight: bold; padding: 10px 12px; "
        "background-color: #e3f2fd; border-radius: 6px; }");
    m_saveToLabel->setWordWrap(true);
    m_saveToLabel->hide();
    infoLayout->addWidget(m_saveToLabel);

    mainLayout->addWidget(m_infoSection);

    // ── Separator (before options) ───────────────────────────────────────
    auto* sep2 = new QFrame();
    sep2->setFrameShape(QFrame::HLine);
    sep2->setFrameShadow(QFrame::Sunken);
    sep2->hide();
    sep2->setObjectName("sep2");
    mainLayout->addWidget(sep2);

    // ── Options Section ──────────────────────────────────────────────────
    auto* optionsGroup = new QGroupBox(tr("Download Options"));
    optionsGroup->hide();
    optionsGroup->setObjectName("optionsGroup");
    QFont groupFont = optionsGroup->font();
    groupFont.setPointSize(groupFont.pointSize() + 1);
    groupFont.setBold(true);
    optionsGroup->setFont(groupFont);
    auto* optionsLayout = new QFormLayout(optionsGroup);
    optionsLayout->setSpacing(12);
    optionsLayout->setContentsMargins(16, 20, 16, 16);
    optionsLayout->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);

    // Quality
    m_qualityCombo = new QComboBox();
    m_qualityCombo->setMinimumWidth(340);
    m_qualityCombo->setMinimumHeight(36);
    QFont comboFont = m_qualityCombo->font();
    comboFont.setPointSize(comboFont.pointSize() + 1);
    m_qualityCombo->setFont(comboFont);
    optionsLayout->addRow(tr("Video quality:"), m_qualityCombo);

    // Audio
    m_audioCombo = new QComboBox();
    m_audioCombo->setMinimumWidth(340);
    m_audioCombo->setMinimumHeight(36);
    m_audioCombo->setFont(comboFont);
    optionsLayout->addRow(tr("Audio format:"), m_audioCombo);

    // Subtitles
    m_subtitleCombo = new QComboBox();
    m_subtitleCombo->setMinimumWidth(340);
    m_subtitleCombo->setMinimumHeight(36);
    m_subtitleCombo->setFont(comboFont);
    m_subtitleCombo->addItem(tr("None"), QString());
    optionsLayout->addRow(tr("Subtitles:"), m_subtitleCombo);

    // Checkboxes
    auto* cbLayout = new QVBoxLayout();
    cbLayout->setSpacing(4);

    m_audioOnlyCheck = new QCheckBox(tr("Audio only"));
    QFont cbFont = m_audioOnlyCheck->font();
    cbFont.setPointSize(cbFont.pointSize() + 1);
    m_audioOnlyCheck->setFont(cbFont);
    cbLayout->addWidget(m_audioOnlyCheck);

    m_embedMetadataCheck = new QCheckBox(tr("Embed metadata"));
    m_embedMetadataCheck->setFont(cbFont);
    m_embedMetadataCheck->setChecked(true);
    cbLayout->addWidget(m_embedMetadataCheck);

    optionsLayout->addRow(tr("Options:"), cbLayout);

    mainLayout->addWidget(optionsGroup);

    // ── Download path section ───────────────────────────────────────────
    auto* pathGroup = new QGroupBox(tr("Download Location"));
    pathGroup->setFont(groupFont);
    auto* pathLayout = new QHBoxLayout(pathGroup);
    pathLayout->setContentsMargins(16, 20, 16, 16);
    pathLayout->setSpacing(10);

    m_pathField = new QLineEdit();
    QFont pathFont = m_pathField->font();
    pathFont.setPointSize(pathFont.pointSize() + 1);
    m_pathField->setFont(pathFont);
    m_pathField->setMinimumHeight(40);
    pathLayout->addWidget(m_pathField, 1);

    m_browseBtn = new QPushButton(tr("Browse..."));
    m_browseBtn->setMinimumHeight(40);
    m_browseBtn->setMinimumWidth(100);
    m_browseBtn->setCursor(Qt::PointingHandCursor);
    pathLayout->addWidget(m_browseBtn);

    mainLayout->addWidget(pathGroup);

    mainLayout->addStretch();

    // ── Footer buttons ───────────────────────────────────────────────────
    auto* footerLayout = new QHBoxLayout();
    footerLayout->setSpacing(8);
    footerLayout->addStretch();

    auto* cancelBtn = new QPushButton(tr("Cancel"));
    cancelBtn->setMinimumHeight(42);
    cancelBtn->setMinimumWidth(100);
    QFont footerFont = cancelBtn->font();
    footerFont.setPointSize(footerFont.pointSize() + 1);
    cancelBtn->setFont(footerFont);
    cancelBtn->setCursor(Qt::PointingHandCursor);
    footerLayout->addWidget(cancelBtn);
    connect(cancelBtn, &QPushButton::clicked, this, &DownloadDialog::onCancelClicked);

    m_downloadBtn = new QPushButton(tr("Download"));
    m_downloadBtn->setMinimumHeight(42);
    m_downloadBtn->setMinimumWidth(130);
    m_downloadBtn->setFont(footerFont);
    m_downloadBtn->setEnabled(false);
    m_downloadBtn->setCursor(Qt::PointingHandCursor);
    m_downloadBtn->setStyleSheet(
        "QPushButton { background-color: #2e7d32; color: white; font-weight: bold; "
        "padding: 8px 28px; border-radius: 4px; }"
        "QPushButton:hover { background-color: #388e3c; }"
        "QPushButton:disabled { background-color: #bbb; color: #eee; }");
    footerLayout->addWidget(m_downloadBtn);
    connect(m_downloadBtn, &QPushButton::clicked, this, &DownloadDialog::onDownloadClicked);

    mainLayout->addLayout(footerLayout);

    // ── Wire path field from current settings ────────────────────────
    {
        auto* ctrl = DownloaderController::instance();
        if (ctrl)
        {
            m_pathField->setText(ctrl->defaultDownloadPath());

            connect(m_browseBtn, &QPushButton::clicked, this, &DownloadDialog::onBrowsePath);

            connect(m_pathField, &QLineEdit::textChanged, this,
                    [this, ctrl](const QString& path)
            {
                ctrl->setDefaultDownloadPath(path);
                ctrl->saveSettings();
                updateSaveToPreview();
            });

            /* Update the preview when audio-only toggles (changes extension) */
            connect(m_audioOnlyCheck, &QCheckBox::toggled,
                    this, &DownloadDialog::updateSaveToPreview);
        }
    }

    // ── Connect to controller signals ───────────────────────────────────
    auto* controller = DownloaderController::instance();
    if (controller)
    {
        connect(controller, &DownloaderController::analysisReady,
                this, &DownloadDialog::onAnalysisReady);
        connect(controller, &DownloaderController::analysisError,
                this, &DownloadDialog::onAnalysisError);

        connect(controller, &DownloaderController::downloadProgress,
                this, &DownloadDialog::onDownloadProgress);
        connect(controller, &DownloaderController::downloadCompleted,
                this, &DownloadDialog::onDownloadCompleted);
        connect(controller, &DownloaderController::downloadFailed,
                this, &DownloadDialog::onDownloadFailed);
    }
}

void DownloadDialog::centerOnParent()
{
    if (parentWidget())
    {
        auto parentRect = parentWidget()->geometry();
        move(parentRect.center() - rect().center());
    }
    else if (auto* screen = QGuiApplication::primaryScreen())
    {
        auto screenRect = screen->availableGeometry();
        move(screenRect.center() - rect().center());
    }
}

void DownloadDialog::onAnalyzeClicked()
{
    QString url = m_urlField->text().trimmed();
    if (url.isEmpty())
        return;

    m_analyzeBtn->setEnabled(false);
    m_analyzeBtn->setText(tr("Analyzing..."));
    m_urlField->setEnabled(false);
    m_progressBar->show();

    // Create task and start analysis
    auto* controller = DownloaderController::instance();
    if (!controller)
    {
        m_analyzeBtn->setEnabled(true);
        m_analyzeBtn->setText(tr("Analyze"));
        m_urlField->setEnabled(true);
        m_progressBar->hide();
        return;
    }

    m_currentTaskId = controller->createTask(url);
    if (m_currentTaskId.isEmpty())
    {
        m_analyzeBtn->setEnabled(true);
        m_analyzeBtn->setText(tr("Analyze"));
        m_urlField->setEnabled(true);
        m_progressBar->hide();
        return;
    }

    fprintf(stderr, "[DownloadDialog] created task %s\n", m_currentTaskId.toUtf8().constData());

    // Run analysis on a background thread so the UI doesn't freeze.
    // The controller emits analysisReady/analysisError signals via
    // QueuedConnection when done — no polling needed.
    std::thread([controller, taskId = m_currentTaskId]() {
        fprintf(stderr, "[DownloadDialog] background thread starting analysis for %s\n", taskId.toUtf8().constData());
        controller->analyzeTask(taskId);
        fprintf(stderr, "[DownloadDialog] background thread finished analysis for %s\n", taskId.toUtf8().constData());
    }).detach();
}

void DownloadDialog::onAnalysisReady(const QString& taskId)
{
    fprintf(stderr, "[DownloadDialog] onAnalysisReady task=%s current=%s\n",
            taskId.toUtf8().constData(), m_currentTaskId.toUtf8().constData());

    /* Ignore if this doesn't match our current analysis */
    if (taskId != m_currentTaskId)
    {
        fprintf(stderr, "[DownloadDialog] ignoring stale analysisReady\n");
        return;
    }

    auto* controller = DownloaderController::instance();
    if (!controller)
        return;

    m_progressBar->hide();
    m_errorLabel->hide();

    QVariantMap info = controller->mediaInfoForTask(m_currentTaskId);
    if (info.isEmpty() || !info.contains("title"))
    {
        /* Fallback: media info not ready despite the signal.
         * This shouldn't happen — the signal fires after setMediaInfo. */
        m_errorLabel->setText(tr("Analysis completed but media info is unavailable."));
        m_errorLabel->show();
        m_analyzeBtn->setEnabled(true);
        m_analyzeBtn->setText(tr("Retry"));
        m_urlField->setEnabled(true);
        return;
    }

    // Populate info section
    m_titleLabel->setText(info.value("title").toString());
    m_durationLabel->setText(tr("Duration: %1").arg(info.value("duration", "-").toString()));
    m_uploaderLabel->setText(info.value("uploader").toString());
    m_descLabel->setText(info.value("description").toString());
    m_infoSection->show();

    // Show options section
    auto* sep2 = findChild<QFrame*>("sep2");
    if (sep2) sep2->show();
    auto* optionsGroup = findChild<QGroupBox*>("optionsGroup");
    if (optionsGroup) optionsGroup->show();

    // Populate format selectors
    m_qualityCombo->clear();
    QVariantList videoFormats = controller->videoFormatsForTask(m_currentTaskId);
    for (const auto& fmt : videoFormats)
    {
        QVariantMap m = fmt.toMap();
        QString label = QString("%1 · %2 · %3")
            .arg(m.value("resolution").toString(),
                 m.value("codec").toString(),
                 m.value("bitrate").toString());
        m_qualityCombo->addItem(label, m.value("id"));
    }

    m_audioCombo->clear();
    QVariantList audioFormats = controller->audioFormatsForTask(m_currentTaskId);
    for (const auto& fmt : audioFormats)
    {
        QVariantMap m = fmt.toMap();
        QString label = QString("%1 · %2")
            .arg(m.value("name").toString(),
                 m.value("bitrate").toString());
        m_audioCombo->addItem(label, m.value("id"));
    }

    m_subtitleCombo->clear();
    m_subtitleCombo->addItem(tr("None"), QString());
    QVariantList subs = controller->subtitleFormatsForTask(m_currentTaskId);
    for (const auto& sub : subs)
    {
        QVariantMap m = sub.toMap();
        m_subtitleCombo->addItem(m.value("name", m.value("language")).toString(), m.value("id"));
    }

    m_downloadBtn->setEnabled(true);

    /* Show the save-to preview */
    updateSaveToPreview();
}

void DownloadDialog::updateSaveToPreview()
{
    auto* controller = DownloaderController::instance();
    if (!controller || m_currentTaskId.isEmpty())
        return;

    QVariantMap info = controller->mediaInfoForTask(m_currentTaskId);
    if (info.isEmpty() || !info.contains("title"))
        return;

    /* Use the same sanitizer as the download engine for an accurate preview */
    QString title = info.value("title").toString();
    QString safeName = QString::fromStdString(
        FileManager::sanitizeFilename(title.toStdString()));

    /* Determine extension based on audio-only setting.
     * For video: always .mp4 (we force --merge-output-format mp4).
     * For audio-only: default to .m4a (common AAC output from YouTube). */
    QString ext = m_audioOnlyCheck->isChecked()
        ? QStringLiteral(".m4a")
        : QStringLiteral(".mp4");

    /* Get the download directory */
    QString dir = m_pathField->text();
    if (dir.isEmpty())
        dir = controller->defaultDownloadPath();
    if (dir.isEmpty())
        dir = QDir::homePath() + QStringLiteral("/Downloads");

    m_saveToLabel->setText(tr("Save to: %1/%2%3").arg(dir, safeName, ext));
    m_saveToLabel->show();
}

void DownloadDialog::onAnalysisError(const QString& taskId, const QString& error)
{
    fprintf(stderr, "[DownloadDialog] onAnalysisError task=%s error=%s\n",
            taskId.toUtf8().constData(), error.toUtf8().constData());

    /* Ignore if this doesn't match our current analysis */
    if (taskId != m_currentTaskId)
    {
        fprintf(stderr, "[DownloadDialog] ignoring stale analysisError\n");
        return;
    }

    m_progressBar->hide();
    m_errorLabel->setText(error.isEmpty()
        ? tr("Analysis failed. Check that yt-dlp is installed and the URL is valid.")
        : tr("Analysis failed: %1").arg(error));
    m_errorLabel->show();

    m_analyzeBtn->setEnabled(true);
    m_analyzeBtn->setText(tr("Retry"));
    m_urlField->setEnabled(true);

    /* Don't clear m_currentTaskId — user can click Retry to re-analyze */
}

void DownloadDialog::onDownloadClicked()
{
    if (m_currentTaskId.isEmpty())
        return;

    auto* controller = DownloaderController::instance();
    if (!controller)
        return;

    int formatIdx = m_qualityCombo->currentIndex();
    bool audioOnly = m_audioOnlyCheck->isChecked();

    controller->confirmDownload(m_currentTaskId, formatIdx, audioOnly);

    /* Transition to downloading mode — keep dialog open with progress */
    showDownloadingMode();
}

void DownloadDialog::onBrowsePath()
{
    QString dir = QFileDialog::getExistingDirectory(this,
        tr("Choose Download Folder"),
        m_pathField->text(),
        QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
    if (!dir.isEmpty())
        m_pathField->setText(dir);
}

void DownloadDialog::showDownloadingMode()
{
    m_downloadBtn->setEnabled(false);
    m_downloadBtn->setText(tr("Downloading..."));
    m_analyzeBtn->setEnabled(false);
    m_urlField->setEnabled(false);
    m_qualityCombo->setEnabled(false);
    m_audioCombo->setEnabled(false);
    m_subtitleCombo->setEnabled(false);
    m_audioOnlyCheck->setEnabled(false);
    m_embedMetadataCheck->setEnabled(false);
    m_errorLabel->hide();

    /* Switch progress bar to determinate mode for download progress */
    m_progressBar->setRange(0, 100);
    m_progressBar->setValue(0);
    m_progressBar->setTextVisible(true);
    m_progressBar->setFormat(tr("Starting download..."));
    m_progressBar->show();

    m_progressLabel->setText(tr("Preparing..."));
    m_progressLabel->show();
}

void DownloadDialog::showReadyMode()
{
    m_downloadBtn->setEnabled(true);
    m_downloadBtn->setText(tr("Download"));
    m_analyzeBtn->setEnabled(true);
    m_analyzeBtn->setText(tr("Retry"));
    m_urlField->setEnabled(true);
    m_qualityCombo->setEnabled(true);
    m_audioCombo->setEnabled(true);
    m_subtitleCombo->setEnabled(true);
    m_audioOnlyCheck->setEnabled(true);
    m_embedMetadataCheck->setEnabled(true);

    m_progressBar->hide();
    m_progressLabel->hide();
}

void DownloadDialog::onDownloadProgress(const QString& taskId, int percent,
                                         double speed, int64_t eta)
{
    if (taskId != m_currentTaskId)
        return;

    m_progressBar->setValue(percent);

    /* Build progress text: "45% · 2.3 MB/s · ETA 1m 30s" */
    QString text;
    if (percent >= 0)
    {
        m_progressBar->setFormat(QStringLiteral("%p%"));

        text = QStringLiteral("%1% · ").arg(percent);

        /* Format speed */
        if (speed >= 1000000.0)
            text += QStringLiteral("%1 MB/s").arg(speed / 1000000.0, 0, 'f', 1);
        else if (speed >= 1000.0)
            text += QStringLiteral("%1 KB/s").arg(speed / 1000.0, 0, 'f', 0);
        else if (speed > 0.0)
            text += QStringLiteral("%1 B/s").arg(static_cast<int>(speed));
        else
            text += QStringLiteral("? B/s");

        /* Format ETA */
        if (eta > 0)
        {
            text += QStringLiteral(" · ETA ");
            if (eta >= 3600)
                text += QStringLiteral("%1h %2m").arg(eta / 3600).arg((eta % 3600) / 60);
            else if (eta >= 60)
                text += QStringLiteral("%1m %2s").arg(eta / 60).arg(eta % 60);
            else
                text += QStringLiteral("%1s").arg(eta);
        }
    }
    m_progressLabel->setText(text);
}

void DownloadDialog::onDownloadCompleted(const QString& taskId,
                                           const QString& outputPath)
{
    if (taskId != m_currentTaskId)
        return;

    m_progressBar->setValue(100);
    m_progressBar->setFormat(tr("Download complete!"));

    /* Show the download path */
    QString msg = tr("Downloaded to: %1").arg(outputPath);
    m_progressLabel->setText(msg);

    fprintf(stderr, "[DownloadDialog] %s\n", msg.toUtf8().constData());

    /* Close the dialog after a short delay */
    QTimer::singleShot(3000, this, &QDialog::accept);
}

void DownloadDialog::onDownloadFailed(const QString& taskId, const QString& error)
{
    if (taskId != m_currentTaskId)
        return;

    m_progressBar->hide();
    m_progressLabel->hide();

    m_errorLabel->setText(tr("Download failed: %1").arg(error));
    m_errorLabel->show();

    /* Re-enable controls so the user can adjust settings and retry */
    showReadyMode();
}

void DownloadDialog::onCancelClicked()
{
    if (!m_currentTaskId.isEmpty())
    {
        auto* controller = DownloaderController::instance();
        if (controller)
            controller->cancelTask(m_currentTaskId);
    }
    reject();
}

} // namespace downloader
} // namespace vlc
