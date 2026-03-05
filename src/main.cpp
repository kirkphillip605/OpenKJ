/*
 * Copyright (c) 2013-2019 Thomas Isaac Lightburn
 *
 *
 * This file is part of OpenKJ.
 *
 * OpenKJ is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/


#include <QApplication>
#include "mainwindow.h"
#include <QSplashScreen>
#include <QStringList>
#include <QDebug>
#include <QMessageBox>
#include <QCommandLineParser>
#include <QMutex>
#include "settings.h"
#include "idledetect.h"
#include "runguard/runguard.h"
#include "okjversion.h"
#include "theme.h"

QDataStream &operator<<(QDataStream &out, const SfxEntry &obj)
{
    out << obj.name << obj.path;
    qInfo() << "returning " << obj.name << " " << obj.path;
    return out;
}

QDataStream &operator>>(QDataStream &in, SfxEntry &obj)
{
   qInfo() << "setting " << obj.name << " " << obj.path;
   in >> obj.name >> obj.path;
   return in;
}

QString altDataDir{};
Settings settings;

IdleDetect *filter;

QFile logFile;
QTextStream logStream;
QStringList logContents;
auto startTime = std::chrono::high_resolution_clock::now();

void myMessageOutput(QtMsgType type, const QMessageLogContext &context, const QString &msg)
{
    static QMutex mutex;
    QMutexLocker locker(&mutex);
    bool loggingEnabled = settings.logEnabled();
    auto currentTime = std::chrono::high_resolution_clock::now();
    unsigned int elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(currentTime - startTime).count();
    if (loggingEnabled && !logFile.isOpen())
    {
        QString logDir = settings.logDir();
        QDir dir;
        QString logFilePath;
        QString filename = "openkj-debug-" + QDateTime::currentDateTime().toString("yyyy-MM-dd-hhmm") + "-log";
        dir.mkpath(logDir);
        logFilePath = logDir + QDir::separator() + filename;
        logFile.setFileName(logFilePath);
    (void)logFile.open(QFile::WriteOnly);
        logStream.setDevice(&logFile);
    }


    QByteArray localMsg = msg.toLocal8Bit();
    switch (type) {
    case QtDebugMsg:
        fprintf(stderr, "DEBG: %s (%s)\n", localMsg.constData(), context.function);
        if (loggingEnabled) logStream << "DEBG: " << localMsg << " (" << context.function << ")\n";
        if (loggingEnabled) logContents.append(QString::number(elapsed) + QString(" - DEBG: " + localMsg + " (" + context.function + ")"));
        break;
    case QtInfoMsg:
        fprintf(stderr, "INFO: %s (%s)\n", localMsg.constData(), context.function);
        if (loggingEnabled) logStream << "INFO: " << localMsg << " (" << context.function << ")\n";
        if (loggingEnabled) logContents.append(QString::number(elapsed) + QString(" - INFO: " + localMsg + " (" + context.function + ")"));
        break;
    case QtWarningMsg:
        fprintf(stderr, "WARN: %s (%s)\n", localMsg.constData(), context.function);
        if (loggingEnabled) logStream << "WARN: " << localMsg << " (" << context.function << ")\n";
        logContents.append(QString::number(elapsed) + QString(" - WARN: " + localMsg + " (" + context.function + ")"));
        break;
    case QtCriticalMsg:
        fprintf(stderr, "CRIT: %s (%s)\n", localMsg.constData(), context.function);
        if (loggingEnabled) logStream << "CRIT: " << localMsg << " (" << context.function << ")\n";
        logContents.append(QString::number(elapsed) + QString(" - CRIT: " + localMsg + " (" + context.function + ")"));
        break;
    case QtFatalMsg:
        fprintf(stderr, "FATAL!!: %s (%s)\n", localMsg.constData(), context.function);
        if (loggingEnabled) logStream << "FATAL!!: " << localMsg << " (" << context.function << ")\n";
        logContents.append(QString::number(elapsed) + QString(" - FATAL!!: " + localMsg + " (" + context.function + ")"));
        if (loggingEnabled) logStream.flush();
        abort();
    }
    if (loggingEnabled) logStream.flush();
}

int main(int argc, char *argv[])
{
    QCoreApplication::setOrganizationName("OpenKJ");
    QCoreApplication::setOrganizationDomain("OpenKJ.org");
    QCoreApplication::setApplicationName("OpenKJ");
    QCoreApplication::setApplicationVersion(OKJ_VERSION_STRING);
    QCommandLineParser parser;
    parser.setApplicationDescription("OpenKJ");
    parser.addHelpOption();
    parser.addVersionOption();
    QCommandLineOption dataDirectoryOption(QStringList() << "d" << "data-directory",
                                             "Overrides the path that OpenKJ will use for its config and database files",
                                             QCoreApplication::translate("main", "data-directory"));
    parser.addOption(dataDirectoryOption);


    //QLoggingCategory::setFilterRules("*.debug=true");
    qInstallMessageHandler(myMessageOutput);
    qRegisterMetaType<SfxEntry>("SfxEntry");
    qRegisterMetaType<QList<SfxEntry> >("QList<SfxEntry>");
    QApplication a(argc, argv);
    parser.process(a);
    if (parser.isSet(dataDirectoryOption))
    {
        qInfo() << "User specified alternate data directory at the command line, using " << parser.value(dataDirectoryOption) << " for config and db files";
        altDataDir = parser.value(dataDirectoryOption);
        settings.reload();
    }
#ifdef MAC_OVERRIDE_GST
    // This points GStreamer paths to the framework contained in the app bundle.  Not needed on brew installs.
    QString appDir = QCoreApplication::applicationDirPath();
    qInfo() << "Application dir: " << appDir;
    appDir.remove(appDir.length() - 5, 5);
    qputenv("GST_PLUGIN_SYSTEM_PATH", QString(appDir + "Frameworks/GStreamer.framework/Versions/Current/lib/gstreamer-1.0").toLocal8Bit());
    qputenv("GST_PLUGIN_SCANNER", QString(appDir + "Frameworks/GStreamer.framework/Versions/Current/libexec/gstreamer-1.0/gst-plugin-scanner").toLocal8Bit());
    qputenv("GTK_PATH", QString(appDir + "Frameworks/GStreamer.framework/Versions/Current/").toLocal8Bit());
    qputenv("GIO_EXTRA_MODULES", QString(appDir + "Frameworks/GStreamer.framework/Versions/Current/lib/gio/modules").toLocal8Bit());
    qWarning() << "MacOS detected, changed GST env vars to point to the bundled framework";
    qWarning() << qgetenv("GST_PLUGIN_SYSTEM_PATH") << Qt::endl << qgetenv("GST_PLUGIN_SCANNER") << Qt::endl << qgetenv("GTK_PATH") << Qt::endl << qgetenv("GIO_EXTRA_MODULES") << Qt::endl;
#endif

    filter = new IdleDetect;
    a.installEventFilter(filter);
    qputenv("GST_DEBUG", "*:3");

    // Initialize the Qlementine style engine with the user-selected theme.
    // This replaces the legacy Fusion palette and QSS theme systems.
    AppTheme::initializeStyle(settings.appTheme());

    a.setFont(settings.applicationFont(), "QWidget");
    a.setFont(settings.applicationFont(), "QMenu");

    RunGuard guard("SharedMemorySingleInstanceProtectorOpenKJ");
    if (QCoreApplication::applicationDirPath() == "/app/bin") {
        qInfo() << "RunGuard disabled due to flatpak sandbox";
    } else {
        RunGuard guard("SharedMemorySingleInstanceProtectorOpenKJ");
        if (!guard.tryToRun()) {
            QMessageBox msgBox;
            msgBox.setText("OpenKJ is already running!");
            msgBox.setInformativeText(
                    "In order to protect the database, you can only run one instance of OpenKJ at a time.\nExiting now.");
            msgBox.setIcon(QMessageBox::Critical);
            msgBox.exec();
            return 1;
        }
    }
     if (!settings.lastStartupOk())
     {
         QMessageBox msgBox;
         msgBox.setText("OpenKJ appears to have failed to startup on the last run.");
         msgBox.setInformativeText("Would you like to attempt to recover by loading safe settings?");
         msgBox.setIcon(QMessageBox::Warning);
         msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
         msgBox.setDefaultButton(QMessageBox::Yes);
         int ret = msgBox.exec();
         switch (ret) {
         case QMessageBox::Yes:
             settings.setSafeStartupMode(true);
             break;
         default:
             settings.setSafeStartupMode(false);
             qInfo() << "User declined to safe load settings after startup crash";
         }
     }
#ifdef Q_OS_DARWIN
     if (settings.lastRunVersion() != OKJ_VERSION_STRING)
         settings.setSafeStartupMode(true);
#endif
     settings.setLastRunVersion(OKJ_VERSION_STRING);
     settings.setStartupOk(false);
    MainWindow w;
    w.show();

    return a.exec();
}
