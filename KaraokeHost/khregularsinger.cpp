/*
 * Copyright (c) 2013-2014 Thomas Isaac Lightburn
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

#include "khregularsinger.h"
#include <QSqlQuery>
#include <QSqlRecord>
#include <QVariant>
#include <QDebug>

KhRegularSinger::KhRegularSinger(QObject *parent) :
    QObject(parent)
{
    regindex = -1;
    name = "Empty KhRegularSinger";
    regSongs = new KhRegularSongs(-1, this);
}

KhRegularSinger::KhRegularSinger(QString singerName, QObject *parent)
{
    Q_UNUSED(parent);
    QSqlQuery query;
    query.exec("INSERT INTO regularsingers (name) VALUES(\"" + singerName + "\")");
    regindex = query.lastInsertId().toInt();
    name = singerName;
    regSongs = new KhRegularSongs(regindex, this);
}

KhRegularSinger::KhRegularSinger(QString singerName, int singerID, QObject *parent)
{
    Q_UNUSED(parent);
    regindex = singerID;
    name = singerName;
    regSongs = new KhRegularSongs(regindex, this);
}


KhRegularSingers::KhRegularSingers(QObject *parent) :
    QObject(parent)
{
    regularSingers = new QList<KhRegularSinger *>;
    loadFromDB();
}

QString KhRegularSinger::getName() const
{
    return name;
}

void KhRegularSinger::setName(const QString &value)
{
    name = value;
}

QList<KhRegularSinger *> *KhRegularSingers::getRegularSingers()
{
    return regularSingers;
}

KhRegularSinger *KhRegularSingers::getByIndex(int regIndex)
{
    for (int i=0; i < regularSingers->size(); i++)
    {
        if (regularSingers->at(i)->getIndex() == regIndex)
            return regularSingers->at(i);
    }
    return new KhRegularSinger();
}

KhRegularSinger *KhRegularSingers::getByName(QString regName)
{
    for (int i=0; i < regularSingers->size(); i++)
    {
        if (regularSingers->at(i)->getName() == regName)
            return regularSingers->at(i);
    }
    return new KhRegularSinger();
}

bool KhRegularSingers::exists(QString searchName)
{
    for (int i=0; i < regularSingers->size(); i++)
    {
        if (regularSingers->at(i)->getName() == searchName)
            return true;
    }
    return false;
}

int KhRegularSingers::add(QString name)
{
    if (!exists(name))
    {
        KhRegularSinger *singer = new KhRegularSinger(name);
        regularSingers->push_back(singer);
        return singer->getIndex();
    }
    return -1;
}

int KhRegularSingers::size()
{
    return regularSingers->size();
}

KhRegularSinger *KhRegularSingers::at(int index)
{
    return regularSingers->at(index);
}

void KhRegularSingers::loadFromDB()
{
    regularSingers->clear();
    QSqlQuery query("SELECT ROWID,name FROM regularSingers");
    int regsingerid = query.record().indexOf("ROWID");
    int name = query.record().indexOf("name");
    while (query.next()) {
        KhRegularSinger *singer = new KhRegularSinger();
        singer->setIndex(query.value(regsingerid).toInt());
        singer->setName(query.value(name).toString());
        regularSingers->push_back(singer);
    }
}

int KhRegularSinger::getIndex() const
{
    return regindex;
}

void KhRegularSinger::setIndex(int value)
{
    regindex = value;
    regSongs = new KhRegularSongs(regindex);
}

KhRegularSongs *KhRegularSinger::getRegSongs() const
{
    return regSongs;
}

int KhRegularSinger::addSong(int songIndex, int keyChange, int position)
{
    if (regindex == -1)
    {
        qDebug() << "KhRegularSinger::addSong() - Tried to add data to an uninitialized regular!";
                    return -1;
    }
    qDebug() << "KhRegularSinger::addSong(" << songIndex << "," << keyChange << "," << position << ") call on regular singer: " << regindex;
    QSqlQuery query;
    QString sql = "INSERT INTO regularsongs (singer, song, keychg, position) VALUES(" + QString::number(regindex) + "," + QString::number(songIndex) + "," + QString::number(keyChange) + "," + QString::number(position) + ")";
    qDebug() << "Doing sql: " << sql;
    query.exec(sql);
    KhRegularSong *song = new KhRegularSong;
    song->setRegSongIndex(query.lastInsertId().toInt());
    song->setRegSingerIndex(regindex);
    song->setSongIndex(songIndex);
    song->setKeyChange(keyChange);
    song->setPosition(position);
    regSongs->getRegSongs()->push_back(song);
    return query.lastInsertId().toInt();
}

KhRegularSong *KhRegularSinger::getSongByIndex(int index)
{
    for (int i=0; i < regSongs->getRegSongs()->size(); i++)
    {
        if (regSongs->getRegSongs()->at(i)->getRegSongIndex() == index)
            return regSongs->getRegSongs()->at(i);
    }
    return new KhRegularSong();
}

int KhRegularSinger::songsSize()
{
    return regSongs->getRegSongs()->size();
}
