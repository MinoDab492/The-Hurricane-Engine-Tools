/*
 *  SPDX-FileCopyrightText: 2016 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "KisVideoSaver.h"

#include <QDebug>
#include <QFileInfo>
#include <QFileSystemWatcher>
#include <QProcess>
#include <QProgressDialog>
#include <QEventLoop>
#include <QTemporaryFile>
#include <QTemporaryDir>
#include <QTime>

#include <KisDocument.h>
#include <kis_image.h>
#include <kis_image_animation_interface.h>
#include <kis_time_span.h>
#include <KoColorSpace.h>
#include <KoColorSpaceRegistry.h>
#include <KoColorModelStandardIds.h>
#include <KoResourcePaths.h>
#include "kis_config.h"
#include "KisAnimationRenderingOptions.h"
#include "animation/KisFFMpegWrapper.h"

#include "KisPart.h"

KisAnimationVideoSaver::KisAnimationVideoSaver(KisDocument *doc, bool batchMode)
    : m_image(doc->image())
    , m_doc(doc)
    , m_batchMode(batchMode)
{
}

KisAnimationVideoSaver::~KisAnimationVideoSaver()
{
}

KisImageSP KisAnimationVideoSaver::image()
{
    return m_image;
}

KisImportExportErrorCode KisAnimationVideoSaver::encode(const QString &savedFilesMask, const KisAnimationRenderingOptions &options)
{
    if (!QFileInfo(options.ffmpegPath).exists()) {
        m_doc->setErrorMessage(i18n("ffmpeg could not be found at %1", options.ffmpegPath));
        return ImportExportCodes::Failure;
    }

    KisImportExportErrorCode resultOuter = ImportExportCodes::OK;

    KisImageAnimationInterface *animation = m_image->animationInterface();

    const int sequenceNumberingOffset = options.sequenceStart;
    const KisTimeSpan clipRange = KisTimeSpan::fromTimeToTime(sequenceNumberingOffset + options.firstFrame,
                                                        sequenceNumberingOffset + options.lastFrame);

     // export dimensions could be off a little bit, so the last force option tweaks the pixels for the export to work
    const QString exportDimensions =
        QString("scale=w=")
            .append(QString::number(options.width))
            .append(":h=")
            .append(QString::number(options.height))
            .append(":flags=")
            .append(options.scaleFilter);
            //.append(":force_original_aspect_ratio=decrease"); HOTFIX for even:odd dimension images.

    const QString resultFile = options.resolveAbsoluteVideoFilePath();
    const QFileInfo resultFileInfo(resultFile);  
    const QDir videoDir(resultFileInfo.absolutePath());

    const QString suffix = resultFileInfo.suffix().toLower();
    const QString palettePath = videoDir.filePath("KritaTempPalettegen_\%06d.png");
    QStringList additionalOptionsList = options.customFFMpegOptions.split(' ', QString::SkipEmptyParts);

    QScopedPointer<KisFFMpegWrapper> ffmpegWrapper(new KisFFMpegWrapper(this));
    
    {
        
        QStringList paletteArgs;
        QStringList simpleFilterArgs;
        QStringList complexFilterArgs;
        QStringList args;
        
        args << "-r" << QString::number(options.frameRate)
             << "-start_number" << QString::number(clipRange.start())
             << "-i" << savedFilesMask;        

        const int lavfiOptionsIndex = additionalOptionsList.indexOf("-lavfi");

        if ( lavfiOptionsIndex != -1 ) {
            complexFilterArgs << additionalOptionsList.takeAt(lavfiOptionsIndex + 1);

            additionalOptionsList.removeAt( lavfiOptionsIndex );
        }                  
      
        if ( suffix == "gif" ) {
            paletteArgs << "-r" << QString::number(options.frameRate)
                        << "-start_number" << QString::number(clipRange.start())
                        << "-i" << savedFilesMask;
            
            const int paletteOptionsIndex = additionalOptionsList.indexOf("-palettegen");
            QString pallettegenString = "palettegen";
            
            if ( paletteOptionsIndex != -1 ) {
                pallettegenString = additionalOptionsList.takeAt(paletteOptionsIndex + 1);

                additionalOptionsList.removeAt( paletteOptionsIndex );
            }
                        
            if (m_image->width() != options.width || m_image->height() != options.height) {
                paletteArgs << "-vf" << (exportDimensions + "," + pallettegenString );
            } else {
                paletteArgs << "-vf" << pallettegenString;
            }
                 
            paletteArgs << "-y" << palettePath;


            QStringList ffmpegArgs;
            ffmpegArgs << "-v" << "debug"
                         << "-nostdin"
                         << paletteArgs;

            KisFFMpegWrapperSettings ffmpegSettings;
            ffmpegSettings.args = ffmpegArgs;
            ffmpegSettings.processPath = options.ffmpegPath;

            ffmpegSettings.progressIndeterminate = true;
            ffmpegSettings.progressMessage = i18nc("Animation export dialog for palette exporting. arg1: file-suffix",
                                               "Creating palette for %1 file format.", "[suffix]");
            ffmpegSettings.logPath = QDir::tempPath() + QDir::separator() + "krita" + QDir::separator() + "ffmpeg.log";
            
            KisImportExportErrorCode result = ffmpegWrapper->start(ffmpegSettings);

            if (!result.isOk()) {
                return result;
            }
            
            if (lavfiOptionsIndex == -1) {
                complexFilterArgs << "[0:v][1:v] paletteuse";
            }
            
            args << "-i" << palettePath;

            // We need to kill the process so we can reuse it later down the chain. BUG:446320
            ffmpegWrapper->reset();
        }
        
        QFileInfo audioFileInfo = animation->audioChannelFileName();
        if (options.includeAudio && audioFileInfo.exists()) {
            const int msecStart = clipRange.start() * 1000 / animation->framerate();
            const int msecDuration = clipRange.duration() * 1000 / animation->framerate();

            const QTime startTime = QTime::fromMSecsSinceStartOfDay(msecStart);
            const QTime durationTime = QTime::fromMSecsSinceStartOfDay(msecDuration);
            const QString ffmpegTimeFormat = QStringLiteral("H:m:s.zzz");

            args << "-ss" << QLocale::c().toString(startTime, ffmpegTimeFormat);
            args << "-t" << QLocale::c().toString(durationTime, ffmpegTimeFormat);
            args << "-i" << audioFileInfo.absoluteFilePath();
        }
      
        // if we are exporting out at a different image size, we apply scaling filter
        // export options HAVE to go after input options, so make sure this is after the audio import
        if (m_image->width() != options.width || m_image->height() != options.height) {
            simpleFilterArgs << exportDimensions;
        }

        if ( !complexFilterArgs.isEmpty() ) { 
            args << "-lavfi" << (!simpleFilterArgs.isEmpty() ? simpleFilterArgs.join(",").append("[0:v];"):"") + complexFilterArgs.join(";");
        } else if ( !simpleFilterArgs.isEmpty() ) {
            args << "-vf" << simpleFilterArgs.join(",");
        }
        
        args << additionalOptionsList;

        dbgFile << "savedFilesMask" << savedFilesMask 
                << "start" << QString::number(clipRange.start()) 
                << "duration" << clipRange.duration();


        KisFFMpegWrapperSettings ffmpegSettings;
        ffmpegSettings.processPath = options.ffmpegPath;
        ffmpegSettings.args = args;
        ffmpegSettings.outputFile = resultFile;
        ffmpegSettings.totalFrames = clipRange.duration();
        ffmpegSettings.logPath = QDir::tempPath() + QDir::separator() + "krita" + QDir::separator() + "ffmpeg.log";
        ffmpegSettings.progressMessage = i18nc("Animation export dialog for tracking ffmpeg progress. arg1: file-suffix, arg2: progress frame number, arg3: totalFrameCount.",
                                               "Creating desired %1 file: %2/%3 frames.", "[suffix]", "[progress]", "[framecount]");

        resultOuter = ffmpegWrapper->start(ffmpegSettings);
    }
     

    return resultOuter;
}

KisImportExportErrorCode KisAnimationVideoSaver::convert(KisDocument *document, const QString &savedFilesMask, const KisAnimationRenderingOptions &options, bool batchMode)
{
    KisAnimationVideoSaver videoSaver(document, batchMode);
    KisImportExportErrorCode res = videoSaver.encode(savedFilesMask, options);
    return res;
}
