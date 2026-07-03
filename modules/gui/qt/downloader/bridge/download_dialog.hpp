/*****************************************************************************
 * download_dialog.hpp
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

#ifndef VLC_DOWNLOADER_DOWNLOAD_DIALOG_HPP
#define VLC_DOWNLOADER_DOWNLOAD_DIALOG_HPP

#include <QDialog>

#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QComboBox>
#include <QCheckBox>
#include <QTimer>
#include <QVBoxLayout>
#include <QProgressBar>

#include "downloader_controller.hpp"

struct qt_intf_t;

namespace vlc {
namespace downloader {

/**
 * @brief Pure QWidget-based download dialog.
 *
 * No QML, no QuickWidget — just standard Qt Widgets using the
 * DownloaderController C++ API directly.
 */
class DownloadDialog : public QDialog
{
    Q_OBJECT

public:
    explicit DownloadDialog(qt_intf_t* p_intf, const QString& initialUrl = {},
                            QWidget* parent = nullptr);
    ~DownloadDialog() override = default;

private slots:
    void onAnalyzeClicked();
    void onCancelClicked();
    void onDownloadClicked();
    void onBrowsePath();

    /** @brief Analysis completed on background thread — populates UI. */
    void onAnalysisReady(const QString& taskId);

    /** @brief Analysis failed — shows error. */
    void onAnalysisError(const QString& taskId, const QString& error);

    /** @brief Download progress update. */
    void onDownloadProgress(const QString& taskId, int percent,
                            double speed, int64_t eta);

    /** @brief Download completed successfully. */
    void onDownloadCompleted(const QString& taskId, const QString& outputPath);

    /** @brief Download failed. */
    void onDownloadFailed(const QString& taskId, const QString& error);

private:
    void buildUi();
    void centerOnParent();

    /** Update the "Save to: ..." preview based on media title and settings. */
    void updateSaveToPreview();

    /** Transition the dialog to "downloading" mode with progress bar. */
    void showDownloadingMode();

    /** Transition back to editing/retry mode. */
    void showReadyMode();

    qt_intf_t* m_intf = nullptr;

    // Input
    QLineEdit* m_urlField = nullptr;
    QPushButton* m_analyzeBtn = nullptr;
    QProgressBar* m_progressBar = nullptr;
    QLabel* m_progressLabel = nullptr;

    // Info section
    QWidget* m_infoSection = nullptr;
    QLabel* m_titleLabel = nullptr;
    QLabel* m_durationLabel = nullptr;
    QLabel* m_uploaderLabel = nullptr;
    QLabel* m_descLabel = nullptr;

    // Options
    QCheckBox* m_audioOnlyCheck = nullptr;
    QCheckBox* m_embedMetadataCheck = nullptr;
    QComboBox* m_qualityCombo = nullptr;
    QComboBox* m_audioCombo = nullptr;
    QComboBox* m_subtitleCombo = nullptr;

    // Download path
    QLineEdit* m_pathField = nullptr;
    QPushButton* m_browseBtn = nullptr;

    // Save-to preview
    QLabel* m_saveToLabel = nullptr;

    // Error display
    QLabel* m_errorLabel = nullptr;

    // Buttons
    QPushButton* m_downloadBtn = nullptr;

    // State
    QString m_currentTaskId;
};

} // namespace downloader
} // namespace vlc

#endif // VLC_DOWNLOADER_DOWNLOAD_DIALOG_HPP
