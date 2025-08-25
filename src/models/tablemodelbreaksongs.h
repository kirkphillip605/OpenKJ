#ifndef TABLEMODELBREAKSONGSNEW_H
#define TABLEMODELBREAKSONGSNEW_H

#include <QAbstractTableModel>
#include <QTime>
#include <chrono>
#include <spdlog/async_logger.h>
#include "spdlog/spdlog.h"
#include "spdlog/fmt/ostr.h"
#include "settings.h"

struct BreakSong {
    int id{0};
    QString artist;
    QString title;
    QString path;
    QString filename;
    int duration;
    std::string searchString;

};


class TableModelBreakSongs : public QAbstractTableModel
{
    Q_OBJECT

public:
    enum {COL_ID=0,COL_ARTIST,COL_TITLE,COL_FILENAME,COL_DURATION};
    explicit TableModelBreakSongs(QObject *parent = nullptr);
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    void sort(int column, Qt::SortOrder order) override;
    QMimeData *mimeData(const QModelIndexList &indexes) const override;
    Qt::ItemFlags flags(const QModelIndex &index) const override;
    BreakSong &getSong(const int breakSongId);
    int getSongId(const QString &filePath);
    void loadDatabase();
    void search(const QString &searchStr);


private:
    std::string m_loggingPrefix{"[BreakSongsModel]"};
    std::shared_ptr<spdlog::logger> m_logger;
    std::vector<BreakSong> m_filteredSongs;
    std::vector<BreakSong> m_allSongs;
    QString m_lastSearch;
    Qt::SortOrder m_lastSortOrder{Qt::AscendingOrder};
    int m_lastSortColumn{1};
    Settings m_settings;

};

#endif // TABLEMODELBREAKSONGSNEW_H
