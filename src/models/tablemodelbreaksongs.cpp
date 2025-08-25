#include "tablemodelbreaksongs.h"

#include <QDataStream>
#include <QMimeData>
#include <QSqlQuery>
#include <QString>
#include <spdlog/spdlog.h>
#include <okjsongbookapi.h>


std::ostream& operator<<(std::ostream& os, const BreakSong& b)
{
    return os << "{(artist="
                    << b.artist.toStdString()
                    << ")(title="
                    << b.title.toStdString()
                    << ")(path="
                    << b.path.toStdString()
                    << ")(fname="
                    << b.filename.toStdString()
                    << ")(duration="
                    << b.duration
                    << ")(sstring="
                    << b.searchString
                    << ")}";
}

std::ostream & operator<<(std::ostream& os, const QString& s)
{
    return os << s.toStdString();
}

TableModelBreakSongs::TableModelBreakSongs(QObject *parent)
    : QAbstractTableModel(parent)
{
    m_logger = spdlog::get("logger");
    loadDatabase();
}

QVariant TableModelBreakSongs::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (role == Qt::FontRole) {
        auto font = m_settings.applicationFont();
        font.setBold(true);
        return font;
    }
    if (role == Qt::DisplayRole && orientation == Qt::Horizontal)
    {
        switch (section) {
        case COL_ID:
            return "ID";
        case COL_ARTIST:
            return "Artist";
        case COL_TITLE:
            return "Title";
        case COL_FILENAME:
            return "Filename";
        case COL_DURATION:
            return "Duration";
        }
    }
    return QVariant();
}

int TableModelBreakSongs::rowCount([[maybe_unused]]const QModelIndex &parent) const
{
    return m_filteredSongs.size();
}

int TableModelBreakSongs::columnCount([[maybe_unused]]const QModelIndex &parent) const
{
    return 5;
}

QVariant TableModelBreakSongs::data(const QModelIndex &index, int role) const
{
    if (!index.isValid())
        return QVariant();
    if (role == Qt::FontRole)
        return m_settings.applicationFont();
    if (role == Qt::DisplayRole)
    {
        switch (index.column()) {
        case COL_ID:
            return m_filteredSongs.at(index.row()).id;
        case COL_ARTIST:
            return m_filteredSongs.at(index.row()).artist;
        case COL_TITLE:
            return m_filteredSongs.at(index.row()).title;
        case COL_FILENAME:
            return m_filteredSongs.at(index.row()).filename;
        case COL_DURATION:
            return QTime(0,0,0,0).addSecs(m_filteredSongs.at(index.row()).duration).toString("m:ss");
        }
    }
    if (role == Qt::TextAlignmentRole)
    {
        switch (index.column()) {
        case COL_DURATION:
            return Qt::AlignRight + Qt::AlignVCenter;
        default:
            return Qt::AlignLeft + Qt::AlignVCenter;
        }
    }
    return QVariant();
}

void TableModelBreakSongs::loadDatabase()
{
    emit layoutAboutToBeChanged();
    m_allSongs.clear();
    m_filteredSongs.clear();
    QSqlQuery query;
    query.exec("SELECT songid,artist,title,path,filename,duration,searchstring FROM bmsongs");
    while (query.next())
    {
        m_allSongs.emplace_back(
        BreakSong{
            query.value(0).toInt(),
            query.value(1).toString(),
            query.value(2).toString(),
            query.value(3).toString(),
            query.value(4).toString(),
            query.value(5).toInt(),
            query.value(6).toString().toLower().toStdString(),
        });
    }
    emit layoutChanged();
    search(m_lastSearch);
    sort(m_lastSortColumn, m_lastSortOrder);
}

void TableModelBreakSongs::search(const QString &searchStr)
{
    m_lastSearch = searchStr;
    emit layoutAboutToBeChanged();
    std::vector<std::string> searchTerms;
    std::string s = searchStr.toStdString();
    std::string::size_type prev_pos = 0, pos = 0;
    while((pos = s.find(' ', pos)) != std::string::npos)
    {
        searchTerms.emplace_back(s.substr(prev_pos, pos - prev_pos));
        prev_pos = ++pos;
    }
    searchTerms.emplace_back(s.substr(prev_pos, pos - prev_pos));
    m_filteredSongs.clear();
    m_filteredSongs.resize(m_allSongs.size());
    auto it = std::copy_if(m_allSongs.begin(), m_allSongs.end(), m_filteredSongs.begin(), [&searchTerms] (BreakSong &song)
    {
        for (auto term : searchTerms)
        {
            if (song.searchString.find(term) == std::string::npos)
                return false;
        }
        return true;
    });
    m_filteredSongs.resize(std::distance(m_filteredSongs.begin(), it));
    emit layoutChanged();
}


void TableModelBreakSongs::sort(int column, Qt::SortOrder order)
{
    emit layoutAboutToBeChanged();
    if (order == Qt::AscendingOrder)
    {
        std::sort(m_filteredSongs.begin(), m_filteredSongs.end(), [&column] (BreakSong &a, BreakSong &b) {
            switch (column) {
            case COL_ARTIST:
                return (a.artist.toLower() < b.artist.toLower());
            case COL_TITLE:
                return (a.title.toLower() < b.title.toLower());
            case COL_FILENAME:
                return (a.filename.toLower() < b.filename.toLower());
            case COL_DURATION:
                return (a.duration < b.duration);
            default:
                return (a.id < b.id);
            }
    });
    }
    else
    {
        std::sort(m_filteredSongs.rbegin(), m_filteredSongs.rend(), [&column] (BreakSong &a, BreakSong &b) {
            switch (column) {
            case COL_ARTIST:
                return (a.artist.toLower() < b.artist.toLower());
            case COL_TITLE:
                return (a.title.toLower() < b.title.toLower());
            case COL_FILENAME:
                return (a.filename.toLower() < b.filename.toLower());
            case COL_DURATION:
                return (a.duration < b.duration);
            default:
                return (a.id < b.id);
            }
        });
    }
    emit layoutChanged();
}

QMimeData *TableModelBreakSongs::mimeData(const QModelIndexList &indexes) const
{
    QMimeData *mimeData = new QMimeData();
    QByteArray encodedData;
    QDataStream stream(&encodedData, QIODevice::WriteOnly);
    QList<int> songids;
    for (const QModelIndex &index : indexes) {
        if (index.isValid()) {
            if(index.column() == 4)
                songids.append(m_filteredSongs.at(index.row()).id);
        }
    }
    stream << songids;
    mimeData->setData("application/vnd.bmsongid.list", encodedData);
    return mimeData;
}

BreakSong &TableModelBreakSongs::getSong(const int breakSongId)
{
    auto it = std::find_if(m_allSongs.begin(), m_allSongs.end(), [&breakSongId] (BreakSong &song) {
        return (song.id == breakSongId);
    });
    return *it;
}

int TableModelBreakSongs::getSongId(const QString &filePath)
{
    auto it = std::find_if(m_allSongs.begin(), m_allSongs.end(), [&filePath] (BreakSong &song) {
        return (song.path == filePath);
    });
    if (it == m_allSongs.end())
        return -1;
    return it->id;
}

Qt::ItemFlags TableModelBreakSongs::flags([[maybe_unused]]const QModelIndex &index) const
{
    return Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsDragEnabled;
}
