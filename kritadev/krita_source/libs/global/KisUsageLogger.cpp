/*
 *  SPDX-FileCopyrightText: 2019 Boudewijn Rempt <boud@valdyas.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */
#include "KisUsageLogger.h"

#include <QScreen>
#include <QGlobalStatic>
#include <QDebug>
#include <QDateTime>
#include <QSysInfo>
#include <QStandardPaths>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QDesktopWidget>
#include <QClipboard>
#include <QThread>
#include <QApplication>
#include <klocalizedstring.h>
#include <KritaVersionWrapper.h>
#include <QGuiApplication>
#include <QStyle>
#include <QStyleFactory>
#include <QTextCodec>

#ifdef Q_OS_WIN
#include "KisWindowsPackageUtils.h"
#include <windows.h>
#endif

#ifdef Q_OS_ANDROID
#include <QtAndroidExtras/QtAndroid>
#endif

#include <clocale>

Q_GLOBAL_STATIC(KisUsageLogger, s_instance)

const QString KisUsageLogger::s_sectionHeader("================================================================================\n");

struct KisUsageLogger::Private {
    bool active {false};
    QFile logFile;
    QFile sysInfoFile;
};

KisUsageLogger::KisUsageLogger()
    : d(new Private)
{
    if (!QFileInfo(QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation)).exists()) {
        QDir().mkpath(QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation));
    }
    d->logFile.setFileName(QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + "/krita.log");
    d->sysInfoFile.setFileName(QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + "/krita-sysinfo.log");

    QFileInfo fi(d->logFile.fileName());
    if (fi.size() > 100 * 1000 * 1000) { // 100 mb seems a reasonable max
        d->logFile.open(QIODevice::WriteOnly | QIODevice::Truncate);
        d->logFile.close();
    }
    else {
        rotateLog();
    }

    d->logFile.open(QFile::Append | QFile::Text);
    d->sysInfoFile.open(QFile::WriteOnly | QFile::Text);
}

KisUsageLogger::~KisUsageLogger()
{
    if (d->active) {
        close();
    }
}

void KisUsageLogger::initialize()
{
    s_instance->d->active = true;

    QString systemInfo = basicSystemInfo();
    s_instance->d->sysInfoFile.write(systemInfo.toUtf8());
}

QString KisUsageLogger::basicSystemInfo()
{
    QString systemInfo;

    // NOTE: This is intentionally not translated!

    // Krita version info
    systemInfo.append("Krita\n");
    systemInfo.append("\n Version: ").append(KritaVersionWrapper::versionString(true));
#ifdef Q_OS_WIN
    {
        using namespace KisWindowsPackageUtils;
        QString packageFamilyName;
        QString packageFullName;
        systemInfo.append("\n Installation type: ");
        if (tryGetCurrentPackageFamilyName(&packageFamilyName) && tryGetCurrentPackageFullName(&packageFullName)) {
            systemInfo.append("Store / MSIX package\n    Family Name: ")
                .append(packageFamilyName)
                .append("\n    Full Name: ")
                .append(packageFullName);
        } else {
            systemInfo.append("installer / portable package");
        }
    }
#endif
    systemInfo.append("\n Hidpi: ").append(QCoreApplication::testAttribute(Qt::AA_EnableHighDpiScaling) ? "true" : "false");
    systemInfo.append("\n\n");

    systemInfo.append("Qt\n");
    systemInfo.append("\n  Version (compiled): ").append(QT_VERSION_STR);
    systemInfo.append("\n  Version (loaded): ").append(qVersion());
    systemInfo.append("\n\n");

    // OS information
    systemInfo.append("OS Information\n");
    systemInfo.append("\n  Build ABI: ").append(QSysInfo::buildAbi());
    systemInfo.append("\n  Build CPU: ").append(QSysInfo::buildCpuArchitecture());
    systemInfo.append("\n  CPU: ").append(QSysInfo::currentCpuArchitecture());
    systemInfo.append("\n  Kernel Type: ").append(QSysInfo::kernelType());
    systemInfo.append("\n  Kernel Version: ").append(QSysInfo::kernelVersion());
    systemInfo.append("\n  Pretty Productname: ").append(QSysInfo::prettyProductName());
    systemInfo.append("\n  Product Type: ").append(QSysInfo::productType());
    systemInfo.append("\n  Product Version: ").append(QSysInfo::productVersion());

#ifdef Q_OS_ANDROID
    QString manufacturer =
        QAndroidJniObject::getStaticObjectField("android/os/Build", "MANUFACTURER", "Ljava/lang/String;").toString();
    const QString model =
        QAndroidJniObject::getStaticObjectField("android/os/Build", "MODEL", "Ljava/lang/String;").toString();
    manufacturer[0] = manufacturer[0].toUpper();
    systemInfo.append("\n  Product Model: ").append(manufacturer + " " + model);
#elif defined(Q_OS_LINUX)
    systemInfo.append("\n  Desktop: ").append(qgetenv("XDG_CURRENT_DESKTOP"));
#endif
    systemInfo.append("\n\n");

    return systemInfo;
}

void KisUsageLogger::writeLocaleSysInfo()
{
    if (!s_instance->d->active) {
        return;
    }
    QString systemInfo;
    systemInfo.append("Locale\n");
    systemInfo.append("\n  Languages: ").append(KLocalizedString::languages().join(", "));
    systemInfo.append("\n  C locale: ").append(std::setlocale(LC_ALL, nullptr));
    systemInfo.append("\n  QLocale current: ").append(QLocale().bcp47Name());
    systemInfo.append("\n  QLocale system: ").append(QLocale::system().bcp47Name());
    const QTextCodec *codecForLocale = QTextCodec::codecForLocale();
    systemInfo.append("\n  QTextCodec for locale: ").append(codecForLocale->name());
#ifdef Q_OS_WIN
    {
        systemInfo.append("\n  Process ACP: ");
        CPINFOEXW cpInfo {};
        if (GetCPInfoExW(CP_ACP, 0, &cpInfo)) {
            systemInfo.append(QString::fromWCharArray(cpInfo.CodePageName));
        } else {
            // Shouldn't happen, but just in case
            systemInfo.append(QString::number(GetACP()));
        }
        wchar_t lcData[2];
        int result = GetLocaleInfoEx(LOCALE_NAME_SYSTEM_DEFAULT, LOCALE_IDEFAULTANSICODEPAGE | LOCALE_RETURN_NUMBER, lcData, sizeof(lcData) / sizeof(lcData[0]));
        if (result == 2) {
            systemInfo.append("\n  System locale default ACP: ");
            int systemACP = lcData[1] << 16 | lcData[0];
            if (systemACP == CP_ACP) {
                systemInfo.append("N/A");
            } else if (GetCPInfoExW(systemACP, 0, &cpInfo)) {
                systemInfo.append(QString::fromWCharArray(cpInfo.CodePageName));
            } else {
                // Shouldn't happen, but just in case
                systemInfo.append(QString::number(systemACP));
            }
        }
    }
#endif
    systemInfo.append("\n\n");
    s_instance->d->sysInfoFile.write(systemInfo.toUtf8());
}

void KisUsageLogger::close()
{
    log("CLOSING SESSION");
    s_instance->d->active = false;
    s_instance->d->logFile.flush();
    s_instance->d->logFile.close();
    s_instance->d->sysInfoFile.flush();
    s_instance->d->sysInfoFile.close();
}

void KisUsageLogger::log(const QString &message)
{
    if (!s_instance->d->active) return;
    if (!s_instance->d->logFile.isOpen()) return;

    s_instance->d->logFile.write(QDateTime::currentDateTime().toString(Qt::RFC2822Date).toUtf8());
    s_instance->d->logFile.write(": ");
    write(message);
}

void KisUsageLogger::write(const QString &message)
{
    if (!s_instance->d->active) return;
    if (!s_instance->d->logFile.isOpen()) return;

    s_instance->d->logFile.write(message.toUtf8());
    s_instance->d->logFile.write("\n");

    s_instance->d->logFile.flush();
}

void KisUsageLogger::writeSysInfo(const QString &message)
{
    if (!s_instance->d->active) return;
    if (!s_instance->d->sysInfoFile.isOpen()) return;

    s_instance->d->sysInfoFile.write(message.toUtf8());
    s_instance->d->sysInfoFile.write("\n");

    s_instance->d->sysInfoFile.flush();

}


void KisUsageLogger::writeHeader()
{
    Q_ASSERT(s_instance->d->sysInfoFile.isOpen());
    s_instance->d->logFile.write(s_sectionHeader.toUtf8());

    QString sessionHeader = QString("SESSION: %1. Executing %2\n\n")
            .arg(QDateTime::currentDateTime().toString(Qt::RFC2822Date))
            .arg(qApp->arguments().join(' '));

    s_instance->d->logFile.write(sessionHeader.toUtf8());

    QString KritaAndQtVersion;
    KritaAndQtVersion.append("Krita Version: ").append(KritaVersionWrapper::versionString(true))
            .append(", Qt version compiled: ").append(QT_VERSION_STR)
            .append(", loaded: ").append(qVersion())
            .append(". Process ID: ")
            .append(QString::number(qApp->applicationPid())).append("\n");

    KritaAndQtVersion.append("-- -- -- -- -- -- -- --\n");
    s_instance->d->logFile.write(KritaAndQtVersion.toUtf8());
    s_instance->d->logFile.flush();
    log(QString("Style: %1. Available styles: %2")
        .arg(qApp->style()->objectName(),
             QStyleFactory::keys().join(", ")));

}

QString KisUsageLogger::screenInformation()
{
    QList<QScreen*> screens = qApp->screens();

    QString info;
    info.append("Display Information");
    info.append("\nNumber of screens: ").append(QString::number(screens.size()));

    for (int i = 0; i < screens.size(); ++i ) {
        QScreen *screen = screens[i];
        info.append("\n\tScreen: ").append(QString::number(i));
        info.append("\n\t\tName: ").append(screen->name());
        info.append("\n\t\tDepth: ").append(QString::number(screen->depth()));
        info.append("\n\t\tScale: ").append(QString::number(screen->devicePixelRatio()));
        info.append("\n\t\tPhysical DPI").append(QString::number(screen->physicalDotsPerInch()));
        info.append("\n\t\tLogical DPI").append(QString::number(screen->logicalDotsPerInch()));
        info.append("\n\t\tPhysical Size: ").append(QString::number(screen->physicalSize().width()))
                .append(", ")
                .append(QString::number(screen->physicalSize().height()));
        info.append("\n\t\tPosition: ").append(QString::number(screen->geometry().x()))
                .append(", ")
                .append(QString::number(screen->geometry().y()));
        info.append("\n\t\tResolution in pixels: ").append(QString::number(screen->geometry().width()))
                .append("x")
                .append(QString::number(screen->geometry().height()));
        info.append("\n\t\tManufacturer: ").append(screen->manufacturer());
        info.append("\n\t\tModel: ").append(screen->model());
        info.append("\n\t\tRefresh Rate: ").append(QString::number(screen->refreshRate()));

    }
    info.append("\n");
    return info;
}

void KisUsageLogger::rotateLog()
{
    if (d->logFile.exists()) {
        {
            // Check for CLOSING SESSION
            d->logFile.open(QFile::ReadOnly);
            QString log = QString::fromUtf8(d->logFile.readAll());
            if (!log.split(s_sectionHeader).last().contains("CLOSING SESSION")) {
                log.append("\nKRITA DID NOT CLOSE CORRECTLY\n");
                QString crashLog = QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation) + QStringLiteral("/kritacrash.log");
                if (QFileInfo(crashLog).exists()) {
                    QFile f(crashLog);
                    f.open(QFile::ReadOnly);
                    QString crashes = QString::fromUtf8(f.readAll());
                    f.close();

                    QStringList crashlist = crashes.split("-------------------");
                    log.append(QString("\nThere were %1 crashes in total in the crash log.\n").arg(crashlist.size()));

                    if (crashes.size() > 0) {
                        log.append(crashlist.last());
                    }
                }
                d->logFile.close();
                d->logFile.open(QFile::WriteOnly);
                d->logFile.write(log.toUtf8());
            }
            d->logFile.flush();
            d->logFile.close();
        }

        {
            // Rotate
            d->logFile.open(QFile::ReadOnly);
            QString log = QString::fromUtf8(d->logFile.readAll());
            d->logFile.close();
            QStringList logItems = log.split("SESSION:");
            QStringList keptItems;
            int sectionCount = logItems.size();
            if (sectionCount > s_maxLogs) {
                for (int i = sectionCount - s_maxLogs; i < sectionCount; ++i) {
                    if (logItems.size() > i ) {
                        keptItems.append(logItems[i]);
                    }
                }

                d->logFile.open(QFile::WriteOnly);
                d->logFile.write(keptItems.join("\nSESSION:").toUtf8());
                d->logFile.flush();
                d->logFile.close();
            }
        }


    }
}

