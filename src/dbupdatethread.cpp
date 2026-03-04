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

#include "dbupdatethread.h"
#include <QSqlQuery>
#include <QFileInfo>
#include <QDir>
#include <QDirIterator>
#include <QDebug>
#include <QSet>
#include <QStandardPaths>
#include "src/models/tablemodelkaraokesourcedirs.h"
#include <QtConcurrent>
#include "mzarchive.h"
#include "tagreader.h"
#include "karaokefileinfo.h"

SourceDir::NamingPattern g_pattern;
int g_customPatternId, g_artistCaptureGrp, g_titleCaptureGrp, g_songIdCaptureGrp;
QString g_artistRegex, g_titleRegex, g_songIdRegex;
QStringList errors;

bool DbUpdateThread::dbEntryExists(QString filepath)
{
    QSqlQuery query(database);
    query.prepare("select exists(select 1 from dbsongs where path = :filepath)");
    query.bindValue(":filepath", filepath);
    query.exec();
    if (query.first())
    {
        bool result = query.value(0).toBool();
        query.clear();
        return result;
    }
    else
    {
        query.clear();
        return false;
    }
}

QString DbUpdateThread::findMatchingAudioFile(QString cdgFilePath)
{
    qInfo() << "findMatchingAudioFile(" << cdgFilePath << ") called";
    QStringList audioExtensions;
    audioExtensions.append("mp3");
    audioExtensions.append("wav");
    audioExtensions.append("ogg");
    audioExtensions.append("mov");
    audioExtensions.append("flac");
    QFileInfo cdgInfo(cdgFilePath);
    QDir srcDir = cdgInfo.absoluteDir();
    QDirIterator it(srcDir);
    while (it.hasNext())
    {
        it.next();
        if (it.fileInfo().completeBaseName() != cdgInfo.completeBaseName())
            continue;
        if (it.fileInfo().suffix().toLower() == "cdg")
            continue;
        QString ext;
        foreach (ext, audioExtensions)
        {
            if (it.fileInfo().suffix().toLower() == ext)
            {
                qInfo() << "findMatchingAudioFile found match: " << it.filePath();
                return it.filePath();
            }
        }
    }
    qInfo() << "findMatchingAudioFile found no matches";
    return QString();
}

DbUpdateThread::DbUpdateThread(QSqlDatabase tdb, QObject *parent) :
    QThread(parent)
{
    database = tdb;
    pattern = SourceDir::SAT;
    g_pattern = pattern;
    settings = new Settings(this);
}

int DbUpdateThread::getPattern() const
{
    return pattern;
}

void DbUpdateThread::setPattern(SourceDir::NamingPattern value)
{
    pattern = value;
    g_pattern = value;
}

QString DbUpdateThread::getPath() const
{
    return path;
}

QStringList DbUpdateThread::findKaraokeFiles(QString directory)
{
    qInfo() << "DbUpdateThread::findKaraokeFiles(" << directory << ") called";
    QDir dir(directory);
    emit progressMessage("Finding karaoke files in " + directory);

    // Load all existing, non-dropped paths into a set in ONE query for O(1) lookups.
    QSet<QString> existingPaths;
    {
        QSqlQuery dbQuery(database);
        dbQuery.exec("SELECT path FROM dbsongs WHERE discid != '!!DROPPED!!'");
        while (dbQuery.next())
            existingPaths.insert(dbQuery.value(0).toString());
    }

    // Case-insensitive string comparator for binary_search on sorted QStringLists.
    static const auto caseInsensitiveLess = [](const QString &a, const QString &b) {
        return a.compare(b, Qt::CaseInsensitive) < 0;
    };

    // Single pass: collect karaoke candidates and audio file basenames simultaneously.
    static const QStringList audioExts = {"mp3", "wav", "ogg", "mov", "flac"};
    QStringList candidateKaraoke;
    QStringList audioBasePaths;  // path without extension, for CDG matching

    int total = 0;
    {
        QDirIterator it(dir.absolutePath(), QDirIterator::Subdirectories);
        while (it.hasNext()) {
            it.next();
            if (it.fileInfo().isDir())
                continue;
            total++;
            const QString fp  = it.filePath();
            const QString ext = it.fileInfo().suffix().toLower();
            if (ext == "zip" || ext == "cdg" || ext == "mkv" || ext == "avi" ||
                ext == "wmv" || ext == "mp4" || ext == "m4v" || ext == "mpg" ||
                ext == "mpeg")
            {
                candidateKaraoke.append(fp);
            }
            else if (audioExts.contains(ext))
            {
                audioBasePaths.append(it.fileInfo().absolutePath() + "/" +
                                      it.fileInfo().completeBaseName());
            }
            if (total % 500 == 0)
                emit stateChanged(QString("Scanning for karaoke files... %1 found").arg(total));
        }
    }
    audioBasePaths.sort();

    // Filter candidates: skip existing DB entries, and for CDG verify audio match.
    QStringList files;
    int existing = 0;
    int notInDb  = 0;

    for (const QString &fn : std::as_const(candidateKaraoke)) {
        if (existingPaths.contains(fn)) {
            existing++;
            continue;
        }
        if (fn.endsWith(".cdg", Qt::CaseInsensitive)) {
            // Only add CDG if a matching audio file was found.
            const QFileInfo fi(fn);
            const QString basePath = fi.absolutePath() + "/" + fi.completeBaseName();
            if (!std::binary_search(audioBasePaths.constBegin(), audioBasePaths.constEnd(),
                                    basePath, caseInsensitiveLess))
                continue;
        }
        files.append(fn);
        notInDb++;
    }

    emit stateChanged("Finding potential karaoke files... " + QString::number(total) +
                      " found. " + QString::number(notInDb) + " new/" +
                      QString::number(existing) + " existing");
    qInfo() << "File search results - Potential files: " << files.size()
            << " - Already in DB: " << existing << " - New: " << notInDb;
    emit progressMessage("Done searching for files.");
    qInfo() << "DbUpdateThread::findKaraokeFiles(" << directory << ") ended";
    return files;
}

QStringList DbUpdateThread::getMissingDbFiles()
{
    qInfo() << "DbUpdateThread::getMissingDbFiles() called";
    QStringList files;
    QSqlQuery query(database);
    query.exec("SELECT path from dbsongs");
    while (query.next())
    {
        QString path = query.value("path").toString();
        if (!QFile(path).exists())
        {
            files.append(path);
        }
    }
    query.clear();
    return files;
}

QStringList DbUpdateThread::getDragDropFiles()
{
    qInfo() << "DbUpdateThread::getDragDropFiles() called";
    QStringList files;
    QSqlQuery query(database);
    query.exec("SELECT path from dbsongs WHERE discid = '!!DROPPED!!'");
    while (query.next())
    {
        files.append(query.value("path").toString());
    }
    query.clear();
    qInfo() << "DbUpdateThread::getDragDropFiles() ended";
    return files;
}

QMutex errorMutex;
QStringList DbUpdateThread::getErrors()
{
    QStringList l_errors = errors;
    errorMutex.lock();
    errors.clear();
    errorMutex.unlock();
    return l_errors;
}

void DbUpdateThread::addSingleTrack(QString path)
{
    MzArchive archive;
    QSqlQuery query(database);
    query.prepare("INSERT OR IGNORE INTO dbSongs (discid,artist,title,path,filename,duration,searchstring) VALUES(:discid, :artist, :title, :path, :filename, :duration, :searchstring)");
    int duration = 0;
    QFileInfo file(path);
    archive.setArchiveFile(path);
    if (settings->dbLazyLoadDurations())
        duration = -2;
    else
        duration = archive.getSongDuration();
    QString artist;
    QString title;
    QString discid;
    KaraokeFileInfo parser;
    parser.setFileName(path);
    parser.setPattern(SourceDir::SAT);
//    parser.setSongIdRegEx("^\\S+?(?=(\\s|_)-(\\s|_))");
//    parser.setTitleRegEx("(?:^\\S+(?:\\s|_)-(?:\\s|_).+(?:\\s|_)-(?:\\s|_))(.+)",1);
//    parser.setArtistRegEx("(?<=(\\s|_)-(\\s|_))(.*?)(?=(\\s|_)-(\\s|_))", 0);
    if (duration == 0)
        duration = parser.getDuration();
    artist = parser.getArtist();
    title = parser.getTitle();
    discid = parser.getSongId();
    query.bindValue(":discid", discid);
    query.bindValue(":artist", artist);
    query.bindValue(":title", title);
    query.bindValue(":path", file.filePath());
    query.bindValue(":filename", file.completeBaseName());
    query.bindValue(":duration", duration);
    query.bindValue(":searchstring", QString(file.completeBaseName() + " " + artist + " " + title + " " + discid));
    query.exec();
    if (query.lastInsertId().isValid())
    {
        int lastInsertId = query.lastInsertId().toInt();
        query.prepare("INSERT OR IGNORE INTO mem.dbsongs (rowid,discid,artist,title,path,filename,duration,searchstring) VALUES(:rowid,:discid, :artist, :title, :path, :filename, :duration, :searchstring)");
        query.bindValue(":rowid", lastInsertId);
        query.bindValue(":discid", discid);
        query.bindValue(":artist", artist);
        query.bindValue(":title", title);
        query.bindValue(":path", file.filePath());
        query.bindValue(":filename", file.completeBaseName());
        query.bindValue(":duration", duration);
        query.bindValue(":searchstring", QString(file.completeBaseName() + " " + artist + " " + title + " " + discid));
        query.exec();
    }
}

int DbUpdateThread::addDroppedFile(QString path)
{
    QSqlQuery query(database);
    query.prepare("SELECT songid FROM dbsongs WHERE path = :path LIMIT 1");
    query.bindValue(":path", path);
    query.exec();
    if (query.first())
    {
        return query.value("songid").toInt();
    }
    query.prepare("INSERT OR IGNORE INTO dbSongs (discid,artist,title,path,filename) VALUES(:discid, :artist, :title, :path, :filename)");
    QFileInfo file(path);
    QString artist = "-Dropped File-";
    QString title = QFileInfo(path).fileName();
    QString discid = "!!DROPPED!!";
    query.bindValue(":discid", discid);
    query.bindValue(":artist", artist);
    query.bindValue(":title", title);
    query.bindValue(":path", file.filePath());
    query.bindValue(":filename", file.completeBaseName());
    query.exec();
    if (query.lastInsertId().isValid())
    {
        int lastInsertId = query.lastInsertId().toInt();
        query.clear();
        query.prepare("INSERT OR IGNORE INTO mem.dbSongs (rowid,discid,artist,title,path,filename) VALUES(:rowid,:discid, :artist, :title, :path, :filename)");
        query.bindValue(":rowid", lastInsertId);
        query.bindValue(":discid", discid);
        query.bindValue(":artist", artist);
        query.bindValue(":title", title);
        query.bindValue(":path", file.filePath());
        query.bindValue(":filename", file.completeBaseName());
        query.exec();
        return lastInsertId;
    }
    else
    {
        query.clear();
        return -1;
    }
}

void DbUpdateThread::startUnthreaded()
{
    emit progressChanged(0);
    emit progressMaxChanged(0);
    emit stateChanged("Verifing that files in DB are present on disk");
    emit stateChanged("Finding potential karaoke files...");
    QStringList missingFiles = getMissingDbFiles();
    QStringList newSongs = findKaraokeFiles(path);
    QStringList dragDropFiles = getDragDropFiles();
    qInfo() << "Creating QSqlQuery";
    QSqlQuery query(database);
    qInfo() << "Created";

    // Try to find out if any of the new files found have been moved and fix the db entry
    emit stateChanged("Detecting and updating moved files...");
    emit progressMaxChanged(missingFiles.size());
    int count = 0;
    qInfo() << "Looking for missing files";
    database.transaction();
    for (int f=0; f<missingFiles.size(); f++)
    {
        emit progressMessage("Looking for matches to missing db song: " + missingFiles.at(f));
        qInfo() << "Looking for match for missing file: " << missingFiles.at(f);
        bool matchfound = false;
        QString newFile;
        QString missingFile;
        query.prepare("UPDATE dbsongs SET path = :newpath WHERE path = :oldpath");
        for (int i=0; i < newSongs.size(); i++)
        {
            missingFile = missingFiles.at(f);
            newFile = newSongs.at(i);
            if (QFileInfo(newSongs.at(i)).fileName() == QFileInfo(missingFiles.at(f)).fileName())
            {
//                query.prepare("UPDATE dbsongs SET path = :newpath WHERE path = :oldpath");
                query.bindValue(":newpath", newFile);
                query.bindValue(":oldpath", missingFile);
                query.exec();
                qInfo() << "Missing file found at new location";
                qInfo() << "  old: " << missingFile;
                qInfo() << "  new: " << newFile;
                newSongs.removeAt(i);
                emit progressMessage("Found match! Modifying existing song.");
                matchfound = true;
                break;
            }
        }
        if (!matchfound)
            emit progressMessage("No match found");

        count++;
        emit progressChanged(count);
    }
    qInfo() << "Committing transaction";
    database.commit();
    qInfo() << "Processing dragged/dropped files";
    for (int f=0; f < dragDropFiles.size(); f++)
    {
        QString dropFile = dragDropFiles.at(f);
        qInfo() << "Looking for matches for drop file: " << dropFile;
        query.prepare("UPDATE dbsongs SET discid = :discid, artist = :artist, title = :title, filename = :filename, duration = :duration, searchstring = :searchstring WHERE path = :path");
        QString artist;
        QString title;
        QString discid;
        QString filePath;
        QString fileName;
        int duration = 0;
        QString searchString;

        for (int i=0; i < newSongs.size(); i++)
        {
            if (newSongs.at(i) == dropFile)
            {
                qInfo() << "Found match for drop file: " << dropFile;
                QFileInfo file(dropFile);
                filePath = file.filePath();
                fileName = file.completeBaseName();
                KaraokeFileInfo parser;
                parser.setFileName(dropFile);
                parser.setPattern(g_pattern, path);
                artist = parser.getArtist();
                title = parser.getTitle();
                discid = parser.getSongId();
                duration = parser.getDuration();

                if (artist == "" && title == "" && discid == "")
                {
                    // Something went wrong, no metadata found. File is probably named wrong. If we didn't try media tags, give it a shot
                    if (g_pattern != SourceDir::METADATA)
                    {
                        parser.setPattern(SourceDir::METADATA, path);
                        artist = parser.getArtist();
                        title = parser.getTitle();
                        discid = parser.getSongId();
                    }
                    // If we still don't have any metadata, just throw filename into the title field
                    if (artist == "" && title == "" && discid == "")
                        title = file.completeBaseName();
                }
                searchString = QString(file.completeBaseName() + " " + artist + " " + title + " " + discid);
                query.bindValue(":discid", discid);
                query.bindValue(":artist", artist);
                query.bindValue(":title", title);
                query.bindValue(":path", filePath);
                query.bindValue(":filename", fileName);
                query.bindValue(":duration", duration);
                query.bindValue(":searchstring", searchString);
                query.exec();
                newSongs.removeAt(i);
                break;
            }
        }
    }
    qInfo() << "Adding new songs";
    emit progressMaxChanged(newSongs.size());
    emit progressMessage("Found " + QString::number(newSongs.size()) + " potential karaoke files.");
    QString fName;
    QString filePath;
    QString discid;
    QString artist;
    QString title;
    QString searchString;
    int duration = 0;
    qInfo() << "Setting sqlite synchronous mode to OFF";
    query.exec("PRAGMA synchronous=OFF");
    qInfo() << query.lastError();
    qInfo() << "Increasing sqlite cache size";
    query.exec("PRAGMA cache_size=500000");
    qInfo() << query.lastError();
    query.exec("PRAGMA temp_store=2");
    qInfo() << "Beginning transaction";
    database.transaction();
    emit progressMessage("Checking if files are valid and getting durations...");
    emit stateChanged("Validating karaoke files and getting song durations...");
    qInfo() << "Preparing statement";
    query.prepare("INSERT OR IGNORE INTO dbSongs (discid,artist,title,path,filename,duration,searchstring) VALUES(:discid, :artist, :title, :path, :filename, :duration, :searchstring)");
    qInfo() << "Statement prepared";
    qInfo() << "Creating MzArchive instance";
    MzArchive archive;
    KaraokeFileInfo parser;
    qInfo() << "looping over songs";
    int loops = 0;
    for (int i=0; i < newSongs.count(); i++)
    {
        QString fileName = newSongs.at(i);
        //qInfo() << "Beginning processing file: " << fileName;
        QFileInfo file(fileName);
        //emit progressMessage("Processing file: " + file.completeBaseName());
#ifdef Q_OS_WIN
        if (fileName.contains("*") || fileName.contains("?") || fileName.contains("<") || fileName.contains(">") || fileName.contains("|"))
        {
            // illegal character
            errorMutex.lock();
            errors.append("Illegal character in filename: " + fileName);
            errorMutex.unlock();
            emit progressMessage("Illegal character in filename: " + fileName);
            emit progressChanged(i + 1);
            continue;
        }
#endif
        if (fileName.endsWith(".zip", Qt::CaseInsensitive))
        {
            archive.setArchiveFile(fileName);
            if (!settings->dbSkipValidation())
            {
                if (!archive.isValidKaraokeFile())
                {
                    errorMutex.lock();
                    errors.append(archive.getLastError() + ": " + fileName);
                    errorMutex.unlock();
                    //emit progressMessage(archive.getLastError() + ": " + fileName);
                    if (loops >= 5)
                    {
                        emit stateChanged("Validating karaoke files and getting song durations... " + QString::number(i + 1) + " of " + QString::number(newSongs.size()));
                        emit progressChanged(i + 1);
                        loops = 0;
                    }
                    continue;
                }
            }
            if (settings->dbLazyLoadDurations())
                duration = -2;
            else
                duration = archive.getSongDuration();
        }
        parser.setFileName(fileName);
        parser.setPattern(g_pattern, path);
        artist = parser.getArtist();
        title = parser.getTitle();
        discid = parser.getSongId();
        if (!fileName.endsWith(".zip", Qt::CaseInsensitive))
        {
            if (settings->dbLazyLoadDurations())
                duration = -3;
            else
                duration = parser.getDuration();
        }
        if (artist == "" && title == "" && discid == "")
        {
            // Something went wrong, no metadata found. File is probably named wrong. If we didn't try media tags, give it a shot
            if (g_pattern != SourceDir::METADATA)
            {
                parser.setPattern(SourceDir::METADATA, path);
                artist = parser.getArtist();
                title = parser.getTitle();
                discid = parser.getSongId();
            }
            // If we still don't have any metadata, just throw filename into the title field
            if (artist == "" && title == "" && discid == "")
                title = file.completeBaseName();
        }
        fName = file.completeBaseName();
        filePath = fileName;
        searchString = QString(fName + " " + artist + " " + title + " " + discid);
        query.bindValue(":discid", discid);
        query.bindValue(":artist", artist);
        query.bindValue(":title", title);
        query.bindValue(":path", filePath);
        query.bindValue(":filename", fName);
        query.bindValue(":duration", duration);
        query.bindValue(":searchstring", searchString);
        query.exec();
        if (loops >= 5)
        {
            emit progressChanged(i + 1);
            emit stateChanged("Validating karaoke files and getting song durations... " + QString::number(i + 1) + " of " + QString::number(newSongs.size()));
            loops = 0;
        }
        //qInfo() << "Done processing file: " << fileName;
        loops++;
    }
    qInfo() << "Done looping";
    qInfo() << "Committing transaction";
    database.commit();
    qInfo() << "QSqlDatabase last error: " << database.lastError();
    qInfo() << "QSqlQuery last error: " << query.lastError();
    emit progressMessage("Done processing new files.");
    if (errors.size() > 0)
    {
        emit errorsGenerated(errors);
    }
    //emit databaseUpdateComplete();
}

void DbUpdateThread::setPath(const QString &value)
{
    path = value;

}

void DbUpdateThread::run()
{
    emit databaseAboutToUpdate();

    database.open();
    emit progressChanged(0);
    emit progressMaxChanged(0);
    emit stateChanged("Verifing that files in DB are present on disk");
    emit stateChanged("Finding potential karaoke files...");
    QStringList missingFiles = getMissingDbFiles();
    QStringList newSongs = findKaraokeFiles(path);
    QStringList dragDropFiles = getDragDropFiles();

//    qInfo() << "Creating thread db connection";
//    QSqlDatabase database = genUniqueDbConn();
////    database.setDatabaseName(QStandardPaths::writableLocation(QStandardPaths::DataLocation) + QDir::separator() + "openkj.sqlite");
//    database.open();
//    qInfo() << "Created";
    qInfo() << "Creating QSqlQuery";
    QSqlQuery query(database);
    qInfo() << "Created";

    // Try to find out if any of the new files found have been moved and fix the db entry
    emit stateChanged("Detecting and updating moved files...");
    emit progressMaxChanged(missingFiles.size());
    int count = 0;
    qInfo() << "Looking for missing files";
    database.transaction();
    for (int f=0; f<missingFiles.size(); f++)
    {
        emit progressMessage("Looking for matches to missing db song: " + missingFiles.at(f));
        qInfo() << "Looking for match for missing file: " << missingFiles.at(f);
        bool matchfound = false;
        QString newFile;
        QString missingFile;
        query.prepare("UPDATE dbsongs SET path = :newpath WHERE path = :oldpath");
        for (int i=0; i < newSongs.size(); i++)
        {
            missingFile = missingFiles.at(f);
            newFile = newSongs.at(i);
            if (QFileInfo(newSongs.at(i)).fileName() == QFileInfo(missingFiles.at(f)).fileName())
            {
//                query.prepare("UPDATE dbsongs SET path = :newpath WHERE path = :oldpath");
                query.bindValue(":newpath", newFile);
                query.bindValue(":oldpath", missingFile);
                query.exec();
                qInfo() << "Missing file found at new location";
                qInfo() << "  old: " << missingFile;
                qInfo() << "  new: " << newFile;
                newSongs.removeAt(i);
                emit progressMessage("Found match! Modifying existing song.");
                matchfound = true;
                break;
            }
        }
        if (!matchfound)
            emit progressMessage("No match found");

        count++;
        emit progressChanged(count);
    }
    qInfo() << "Committing transaction";
    database.commit();
    qInfo() << "Processing dragged/dropped files";
    database.transaction();
    for (int f=0; f < dragDropFiles.size(); f++)
    {
        QString dropFile = dragDropFiles.at(f);
        qInfo() << "Looking for matches for drop file: " << dropFile;
        query.prepare("UPDATE dbsongs SET discid = :discid, artist = :artist, title = :title, filename = :filename, duration = :duration, searchstring = :searchstring WHERE path = :path");
        QString artist;
        QString title;
        QString discid;
        QString filePath;
        QString fileName;
        int duration = 0;
        QString searchString;

        for (int i=0; i < newSongs.size(); i++)
        {
            if (newSongs.at(i) == dropFile)
            {
                qInfo() << "Found match for drop file: " << dropFile;
                QFileInfo file(dropFile);
                filePath = file.filePath();
                fileName = file.completeBaseName();
//                int duration = 0;
//                query.prepare("UPDATE dbsongs SET discid = :discid, artist = :artist, title = :title, filename = :filename, duration = :duration, searchstring = :searchstring WHERE path = :path");
//                QString artist;
//                QString title;
//                QString discid;
                KaraokeFileInfo parser;
                parser.setFileName(dropFile);
                parser.setPattern(g_pattern, path);
                artist = parser.getArtist();
                title = parser.getTitle();
                discid = parser.getSongId();
                duration = parser.getDuration();

                if (artist == "" && title == "" && discid == "")
                {
                    // Something went wrong, no metadata found. File is probably named wrong. If we didn't try media tags, give it a shot
                    if (g_pattern != SourceDir::METADATA)
                    {
                        parser.setPattern(SourceDir::METADATA, path);
                        artist = parser.getArtist();
                        title = parser.getTitle();
                        discid = parser.getSongId();
                    }
                    // If we still don't have any metadata, just throw filename into the title field
                    if (artist == "" && title == "" && discid == "")
                        title = file.completeBaseName();
                }
                searchString = QString(file.completeBaseName() + " " + artist + " " + title + " " + discid);
                query.bindValue(":discid", discid);
                query.bindValue(":artist", artist);
                query.bindValue(":title", title);
                query.bindValue(":path", filePath);
                query.bindValue(":filename", fileName);
                query.bindValue(":duration", duration);
                query.bindValue(":searchstring", searchString);
                query.exec();
                newSongs.removeAt(i);
                break;
            }
        }
    }
    database.commit();
    qInfo() << "Adding new songs";
    // Add new songs to the database
    emit progressMaxChanged(newSongs.size());
    emit progressMessage("Found " + QString::number(newSongs.size()) + " potential karaoke files.");
    QString fName;
    QString filePath;
    QString discid;
    QString artist;
    QString title;
    QString searchString;
    int duration = 0;
    qInfo() << "Setting sqlite synchronous mode to OFF";
    query.exec("PRAGMA synchronous=OFF");
    qInfo() << query.lastError();
    qInfo() << "Increasing sqlite cache size";
    query.exec("PRAGMA cache_size=500000");
    qInfo() << query.lastError();
    query.exec("PRAGMA temp_store=2");
    qInfo() << "Beginning transaction";
    database.transaction();
    emit progressMessage("Checking if files are valid and getting durations...");
    emit stateChanged("Validating karaoke files and getting song durations...");
    qInfo() << "Preparing statement";
    query.prepare("INSERT OR IGNORE INTO dbSongs (discid,artist,title,path,filename,duration,searchstring) VALUES(:discid, :artist, :title, :path, :filename, :duration, :searchstring)");
    qInfo() << "Statement prepared";
//    qInfo() << "Creating QProcess";
//    QProcess *process = new QProcess(this);
    qInfo() << "Creating MzArchive instance";
    MzArchive archive;
    KaraokeFileInfo parser;
    qInfo() << "looping over songs";
    for (int i=0; i < newSongs.count(); i++)
    {
        QString fileName = newSongs.at(i);
        QFileInfo file(fileName);
        emit progressMessage("Processing file: " + file.completeBaseName());
#ifdef Q_OS_WIN
        if (fileName.contains("*") || fileName.contains("?") || fileName.contains("<") || fileName.contains(">") || fileName.contains("|"))
        {
            // illegal character
            errorMutex.lock();
            errors.append("Illegal character in filename: " + fileName);
            errorMutex.unlock();
            emit progressMessage("Illegal character in filename: " + fileName);
            emit progressChanged(i + 1);
            continue;
        }
#endif
        if (fileName.endsWith(".zip", Qt::CaseInsensitive))
        {
            //MzArchive *archive = new MzArchive(process);
            archive.setArchiveFile(fileName);

            if (!archive.isValidKaraokeFile())
            {
                errorMutex.lock();
                errors.append(archive.getLastError() + ": " + fileName);
                errorMutex.unlock();
                emit progressMessage(archive.getLastError() + ": " + fileName);
                emit progressChanged(i + 1);
                continue;
            }

            duration = archive.getSongDuration();
        }

//        KaraokeFileInfo parser;
        parser.setFileName(fileName);

        parser.setPattern(g_pattern, path);
        artist = parser.getArtist();
        title = parser.getTitle();
        discid = parser.getSongId();
        if (!fileName.endsWith(".zip", Qt::CaseInsensitive))
            duration = parser.getDuration();

        if (artist == "" && title == "" && discid == "")
        {
            // Something went wrong, no metadata found. File is probably named wrong. If we didn't try media tags, give it a shot
            if (g_pattern != SourceDir::METADATA)
            {
                parser.setPattern(SourceDir::METADATA, path);
                artist = parser.getArtist();
                title = parser.getTitle();
                discid = parser.getSongId();
            }
            // If we still don't have any metadata, just throw filename into the title field
            if (artist == "" && title == "" && discid == "")
                title = file.completeBaseName();
        }
        fName = file.completeBaseName();
        filePath = fileName;
        searchString = QString(fName + " " + artist + " " + title + " " + discid);
        query.bindValue(":discid", discid);
        query.bindValue(":artist", artist);
        query.bindValue(":title", title);
        query.bindValue(":path", filePath);
        query.bindValue(":filename", fName);
        query.bindValue(":duration", duration);
        query.bindValue(":searchstring", searchString);
        query.exec();
        //qInfo() << query.lastError();
        emit progressChanged(i + 1);
        emit stateChanged("Validating karaoke files and getting song durations... " + QString::number(i + 1) + " of " + QString::number(newSongs.size()));
       // msleep(60);

    }
    qInfo() << "Done looping";
//    delete process;
//    delete archive;
    qInfo() << "Committing transaction";
    database.commit();
    qInfo() << "QSqlDatabase last error: " << database.lastError();
    qInfo() << "QSqlQuery last error: " << query.lastError();
    emit progressMessage("Done processing new files.");
    if (errors.size() > 0)
    {
        emit errorsGenerated(errors);
    }
    database.close();
    emit databaseUpdateComplete();
//    qInfo() << "Removing thread db connection";
//    query.clear();
//    QSqlDatabase::removeDatabase(database.connectionName());
//    qInfo() << "Removed";
}
