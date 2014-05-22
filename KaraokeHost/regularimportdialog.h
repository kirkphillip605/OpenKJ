#ifndef REGULARIMPORTDIALOG_H
#define REGULARIMPORTDIALOG_H

#include <QDialog>
#include "khregularsinger.h"
#include "khsong.h"

namespace Ui {
class RegularImportDialog;
}

class RegularImportDialog : public QDialog
{
    Q_OBJECT

public:
    explicit RegularImportDialog(KhSongs *dbsongs, KhRegularSingers *regSingersPtr, QWidget *parent = 0);
    ~RegularImportDialog();

private slots:
    void on_pushButtonSelectFile_clicked();

    void on_pushButtonClose_clicked();

    void on_pushButtonImport_clicked();

    void on_pushButtonImportAll_clicked();

private:
    Ui::RegularImportDialog *ui;
    KhRegularSingers *regSingers;
    KhSongs *dbSongs;
    QString curImportFile;
    void importSinger(QString name);
    KhSong *findExactSongMatch(KhRegImportSong importSong);
};

#endif // REGULARIMPORTDIALOG_H