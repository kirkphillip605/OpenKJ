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

#ifndef DATABASEMANAGER_H
#define DATABASEMANAGER_H

#include <QtSql>
#include <QDir>
#include <functional>

class DatabaseManager
{
public:
    // Callback invoked once during schema migration to v106 to import
    // regular-singer song history.  Parameters: singerName, filepath,
    // artist, title, songid, keychange.
    using SongImportCallback = std::function<void(const QString &singerName,
                                                   const QString &filepath,
                                                   const QString &artist,
                                                   const QString &title,
                                                   const QString &songid,
                                                   int keychange)>;

    // Opens (or creates) the SQLite database, applies all schema migrations
    // and returns the database handle.  songImportCb is called for every
    // song that must be migrated into the history tables during the v106
    // schema upgrade.  Pass nullptr if migration is not needed.
    static QSqlDatabase initDatabase(const QDir &dataDir,
                                     const QString &altDataDir,
                                     const SongImportCallback &songImportCb = nullptr);
};

#endif // DATABASEMANAGER_H
