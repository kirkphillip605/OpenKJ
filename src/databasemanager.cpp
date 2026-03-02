/*
 * Copyright (c) 2013-2019 Thomas Isaac Lightburn
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

#include "databasemanager.h"
#include <QDebug>

QSqlDatabase DatabaseManager::initDatabase(const QDir &dataDir,
                                            const QString &altDataDir,
                                            const SongImportCallback &songImportCb)
{
    QSqlDatabase database = QSqlDatabase::addDatabase("QSQLITE");
    if (altDataDir.isEmpty()) {
        database.setDatabaseName(dataDir.absolutePath() + QDir::separator() + "openkj.sqlite");
    } else {
        QDir customDir(altDataDir);
        database.setDatabaseName(customDir.absolutePath() + QDir::separator() + "openkj.sqlite");
    }
    database.open();

    QSqlQuery query(
            "CREATE TABLE IF NOT EXISTS dbSongs ( songid INTEGER PRIMARY KEY AUTOINCREMENT, Artist COLLATE NOCASE, Title COLLATE NOCASE, DiscId COLLATE NOCASE, 'Duration' INTEGER, path VARCHAR(700) NOT NULL UNIQUE, filename COLLATE NOCASE, searchstring TEXT)");
    query.exec(
            "CREATE TABLE IF NOT EXISTS rotationSingers ( singerid INTEGER PRIMARY KEY AUTOINCREMENT, name COLLATE NOCASE UNIQUE, 'position' INTEGER NOT NULL, 'regular' LOGICAL DEFAULT(0), 'regularid' INTEGER)");
    query.exec(
            "CREATE TABLE IF NOT EXISTS queueSongs ( qsongid INTEGER PRIMARY KEY AUTOINCREMENT, singer INT, song INTEGER NOT NULL, artist INT, title INT, discid INT, path INT, keychg INT, played LOGICAL DEFAULT(0), 'position' INT)");
    query.exec(
            "CREATE TABLE IF NOT EXISTS regularSingers ( regsingerid INTEGER PRIMARY KEY AUTOINCREMENT, Name COLLATE NOCASE UNIQUE, ph1 INT, ph2 INT, ph3 INT)");
    query.exec(
            "CREATE TABLE IF NOT EXISTS regularSongs ( regsongid INTEGER PRIMARY KEY AUTOINCREMENT, regsingerid INTEGER NOT NULL, songid INTEGER NOT NULL, 'keychg' INTEGER, 'position' INTEGER)");
    query.exec("CREATE TABLE IF NOT EXISTS sourceDirs ( path VARCHAR(255) UNIQUE, pattern INTEGER)");
    query.exec(
            "CREATE TABLE IF NOT EXISTS bmsongs ( songid INTEGER PRIMARY KEY AUTOINCREMENT, Artist COLLATE NOCASE, Title COLLATE NOCASE, path VARCHAR(700) NOT NULL UNIQUE, Filename COLLATE NOCASE, Duration TEXT, searchstring TEXT)");
    query.exec(
            "CREATE TABLE IF NOT EXISTS bmplaylists ( playlistid INTEGER PRIMARY KEY AUTOINCREMENT, title COLLATE NOCASE NOT NULL UNIQUE)");
    query.exec(
            "CREATE TABLE IF NOT EXISTS bmplsongs ( plsongid INTEGER PRIMARY KEY AUTOINCREMENT, playlist INT, position INT, Artist INT, Title INT, Filename INT, Duration INT, path INT)");
    query.exec("CREATE TABLE IF NOT EXISTS bmsrcdirs ( path NOT NULL)");
    query.exec("PRAGMA synchronous=OFF");
    query.exec("PRAGMA cache_size=300000");
    query.exec("PRAGMA temp_store=2");

    int schemaVersion = 0;
    query.exec("PRAGMA user_version");
    if (query.first())
        schemaVersion = query.value(0).toInt();
    qInfo() << "Database schema version: " << schemaVersion;

    if (schemaVersion < 100) {
        qInfo() << "Updating database schema to version 100";
        query.exec("ALTER TABLE sourceDirs ADD COLUMN custompattern INTEGER");
        query.exec("PRAGMA user_version = 100");
        qInfo() << "DB Schema update to v100 completed";
    }
    if (schemaVersion < 101) {
        qInfo() << "Updating database schema to version 101";
        query.exec(
                "CREATE TABLE custompatterns ( patternid INTEGER PRIMARY KEY AUTOINCREMENT, name TEXT, artistregex TEXT, artistcapturegrp INT, titleregex TEXT, titlecapturegrp INT, discidregex TEXT, discidcapturegrp INT)");
        query.exec("PRAGMA user_version = 101");
        qInfo() << "DB Schema update to v101 completed";
    }
    if (schemaVersion < 102) {
        qInfo() << "Updating database schema to version 102";
        query.exec("CREATE UNIQUE INDEX idx_path ON dbsongs(path)");
        query.exec("PRAGMA user_version = 102");
        qInfo() << "DB Schema update to v102 completed";
    }
    if (schemaVersion < 103) {
        qInfo() << "Updating database schema to version 103";
        query.exec("ALTER TABLE dbsongs ADD COLUMN searchstring TEXT");
        query.exec("UPDATE dbsongs SET searchstring = filename || ' ' || artist || ' ' || title || ' ' || discid");
        query.exec("PRAGMA user_version = 103");
        qInfo() << "DB Schema update to v103 completed";
    }
    if (schemaVersion < 105) {
        qInfo() << "Updating database schema to version 105";
        query.exec("ALTER TABLE rotationSingers ADD COLUMN addts TIMESTAMP");
        query.exec("PRAGMA user_version = 105");
        qInfo() << "DB Schema update to v105 completed";
    }
    if (schemaVersion < 106) {
        qInfo() << "Updating database schema to version 106";
        query.exec(
                "CREATE TABLE dbSongHistory ( id INTEGER PRIMARY KEY AUTOINCREMENT, filepath TEXT, artist TEXT, title TEXT, songid TEXT, timestamp TIMESTAMP)");
        query.exec("CREATE INDEX idx_filepath ON dbSongHistory(filepath)");
        query.exec("ALTER TABLE dbsongs ADD COLUMN plays INT DEFAULT(0)");
        query.exec("ALTER TABLE dbsongs ADD COLUMN lastplay TIMESTAMP");
        query.exec("CREATE TABLE historySingers(id INTEGER PRIMARY KEY AUTOINCREMENT, name TEXT NOT NULL UNIQUE)");
        query.exec(
                "CREATE TABLE historySongs(id INTEGER PRIMARY KEY AUTOINCREMENT, historySinger INT NOT NULL, filepath TEXT NOT NULL, artist TEXT, title TEXT, songid TEXT, keychange INT DEFAULT(0), plays INT DEFAULT(0), lastplay TIMESTAMP)");
        query.exec("CREATE INDEX idx_historySinger on historySongs(historySinger)");
        query.exec("PRAGMA user_version = 106");
        qInfo() << "DB Schema update to v106 completed";

        if (songImportCb) {
            qInfo() << "Importing old regular singers data into singer history";
            QSqlQuery singersQuery;
            singersQuery.exec("SELECT regsingerid,name FROM regularSingers");
            while (singersQuery.next()) {
                qInfo() << "Running import for singer: " << singersQuery.value("name");
                QSqlQuery songsQuery;
                songsQuery.exec(
                        "SELECT dbsongs.artist,dbsongs.title,dbsongs.discid,regularsongs.keychg,dbsongs.path FROM regularsongs,dbsongs WHERE dbsongs.songid == regularsongs.songid AND regularsongs.regsingerid == " +
                        singersQuery.value("regsingerid").toString() + " ORDER BY regularsongs.position");
                while (songsQuery.next()) {
                    qInfo() << "Importing song: " << songsQuery.value(4).toString();
                    songImportCb(
                            singersQuery.value("name").toString(),
                            songsQuery.value(4).toString(),
                            songsQuery.value(0).toString(),
                            songsQuery.value(1).toString(),
                            songsQuery.value(2).toString(),
                            songsQuery.value(3).toInt()
                    );
                }
                qInfo() << "Import complete for singer: " << singersQuery.value("name").toString();
            }
        }
    }

    return database;
}
