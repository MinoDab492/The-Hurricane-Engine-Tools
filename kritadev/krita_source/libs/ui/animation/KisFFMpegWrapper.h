/*
 *  SPDX-FileCopyrightText: 2020 Dmitrii Utkin <loentar@gmail.com>
 *  SPDX-FileCopyrightText: 2021 Eoin O'Neill <eoinoneill1991@gmail.com>
 *
 *  SPDX-License-Identifier: LGPL-2.1-only
 */

#ifndef KISFFMPEGWRAPPER_H
#define KISFFMPEGWRAPPER_H

#include <QObject>
#include <QProgressDialog>

#include "KisImportExportErrorCode.h"
#include <KoColorProfileConstants.h>

#include <kritaui_export.h>

class QProcess;

struct KRITAUI_EXPORT KisFFMpegWrapperSettings
{
    QString processPath;
    QStringList args;
    QString outputFile;
    bool storeOutput = false;
    QString logPath = "";
    QStringList defaultPrependArgs = {"-hide_banner", "-nostdin", "-y"};
    bool batchMode = false;
    bool binaryOutput = false;
    int totalFrames = 0;

    QString progressMessage = "";
    bool progressIndeterminate = false;

};

enum FFProbeErrorCodes {
    NONE = 0,
    UNSUPPORTED_CODEC,
    INVALID_JSON
};

const int FFMPEG_TIMEOUT = 600000;

class KRITAUI_EXPORT KisFFMpegWrapper : public QObject
{
    Q_OBJECT
public:
    explicit KisFFMpegWrapper(QObject *parent = nullptr);
    ~KisFFMpegWrapper();

    void startNonBlocking(const KisFFMpegWrapperSettings &settings);
    KisImportExportErrorCode start(const KisFFMpegWrapperSettings &settings);
    void waitForFinished(int msecs = FFMPEG_TIMEOUT);
    void reset();

    static QJsonObject findProcessPath(const QString &processName, const QString &customLocation, bool processInfo);
    static QJsonObject findFFMpeg(const QString &customLocation);
    static QJsonObject findFFProbe(const QString &customLocation);
    static QJsonObject findProcessInfo(const QString &processName, const QString &processPath, bool includeProcessInfo);
    static QByteArray runProcessAndReturn(const QString &processPath, const QStringList &args, int msecs = FFMPEG_TIMEOUT);
    static QString configuredFFMpegLocation();
    static void setConfiguredFFMpegLocation(QString& location);
    QJsonObject ffprobe(const QString &inputFile, const QString &ffprobePath);
    QJsonObject ffmpegProbe(const QString &inputFile, const QString &ffmpegPath, bool batchMode);

    // Functions to convert the ffprobe shorthands for the H.273 constants into the appropriate enum values.
    static ColorPrimaries colorPrimariesFromName(QString name);
    static TransferCharacteristics transferCharacteristicsFromName(QString name);

Q_SIGNALS:
    void sigStarted();
    void sigFinished();
    void sigFinishedWithError(QString message);
    void sigProgressUpdated(int frameNo);
    void sigReadLine(int pipe, QString line);
    void sigReadSTDOUT(QByteArray stdoutBuffer);
    void sigReadSTDERR(QByteArray stderrBuffer);

private Q_SLOTS:
    void slotReadyReadSTDOUT();
    void slotReadyReadSTDERR();
    void slotStarted();
    void slotFinished(int exitCode);

private:
    void updateProgressDialog(int progressValue);

    /**
     * @brief ffprobeCheckStreamsValid
     * @param ffprobeJsonObj JsonObject resulting from ffprobe proces.
     * @param ffprobeSTDERR STDERR output from ffprobe.
     * @return bool -> Whether we support a video format based on it's internal streams.
     */
    bool ffprobeCheckStreamsValid(const QJsonObject& ffprobeJsonObj, const QString& ffprobeSTDERR);
    
private:
    QScopedPointer<QProcess> m_process;
    QSharedPointer<QProgressDialog> m_progress = nullptr;
    KisFFMpegWrapperSettings m_processSettings;
    
    QString m_stdoutBuffer;
    QString m_stderrBuffer;
    QString m_errorMessage;
    
    QByteArray m_processSTDOUT;
    QString m_processSTDERR;

};


#endif // KISFFMPEGWRAPPER_H
