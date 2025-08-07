#include "directorymonitor.h"
#include "dbupdater.h"
#include <QFutureWatcher>
#include <QtConcurrent>

DirectoryMonitor::DirectoryMonitor(QObject *parent, QStringList pathsToWatch) : QObject(parent)
{
    m_scanTimer.setInterval(5000);
    m_scanTimer.setSingleShot(true);
    connect(&m_scanTimer, &QTimer::timeout, this, &DirectoryMonitor::scanPaths);

    connect(&m_pathsEnumeratedWatcher, &QFutureWatcher<QStringList>::finished, this, &DirectoryMonitor::directoriesEnumerated);
    auto future = QtConcurrent::run(this, &DirectoryMonitor::enumeratePathsAsync, pathsToWatch);
    m_pathsEnumeratedWatcher.setFuture(future);
    connect(&m_dbUpdateWatcher, &QFutureWatcher<bool>::finished, this, &DirectoryMonitor::dbUpdateFinished);
}

DirectoryMonitor::~DirectoryMonitor()
{
    m_fsWatcher.removePaths(m_fsWatcher.directories());
}

QStringList DirectoryMonitor::enumeratePathsAsync(QStringList paths)
{
    QStringList result;
    foreach (auto path, paths) {
        QFileInfo finfo(path);
        if (finfo.isDir() && finfo.isReadable())
        {
            result.append(path);
            QDirIterator it(path, QDir::AllDirs | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
            while (it.hasNext()) {
                result.append(it.next());
            }
        }
    }
    return result;
}

void DirectoryMonitor::directoriesEnumerated()
{
    auto paths = m_pathsEnumeratedWatcher.future().result();
    m_fsWatcher.addPaths(paths);
    connect(&m_fsWatcher, &QFileSystemWatcher::directoryChanged, this, &DirectoryMonitor::directoryChanged);
}

void DirectoryMonitor::directoryChanged(const QString& dirPath)
{
    qInfo() << "Directory changed fired for dir: " << dirPath;
    m_pathsWithChangedFiles << dirPath;
    m_scanTimer.start();
}

void DirectoryMonitor::scanPaths()
{
    if (m_pathsWithChangedFiles.isEmpty())
        return;

    if (m_dbUpdateWatcher.isRunning())
        return;

    m_currentlyScanningPaths = m_pathsWithChangedFiles.values();
    m_pathsWithChangedFiles.clear();

    auto future = QtConcurrent::run([paths = m_currentlyScanningPaths]() {
        DbUpdater dbUpdater;
        return dbUpdater.process(paths, DbUpdater::ProcessingOption::FixMovedFiles);
    });
    m_dbUpdateWatcher.setFuture(future);
}

void DirectoryMonitor::dbUpdateFinished()
{
    bool success = m_dbUpdateWatcher.future().result();
    if (success) {
        emit databaseUpdateComplete();
    } else {
        m_pathsWithChangedFiles.unite(QSet<QString>(m_currentlyScanningPaths.begin(), m_currentlyScanningPaths.end()));
    }
    m_currentlyScanningPaths.clear();
    if (!m_pathsWithChangedFiles.isEmpty())
        QTimer::singleShot(0, this, &DirectoryMonitor::scanPaths);
}

