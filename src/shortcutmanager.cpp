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

#include "shortcutmanager.h"
#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QMessageBox>
#include <QCheckBox>

extern Settings settings;

ShortcutManager::ShortcutManager(MainWindow *mainWindow, QObject *parent)
    : QObject(parent), m_mainWindow(mainWindow)
{
}

void ShortcutManager::setup()
{
    MainWindow *mw = m_mainWindow;

    scutKSelectNextSinger = new QShortcut(settings.loadShortcutKeySequence("kSelectNextSinger"), mw, nullptr, nullptr,
                                          Qt::ApplicationShortcut);
    connect(scutKSelectNextSinger, &QShortcut::activated, [mw]() {
        int nextSinger{-1};
        QString nextSongPath;
        bool empty{false};
        int curSingerId{mw->rotModel.currentSinger()};
        int curPos{mw->rotModel.getSingerPosition(curSingerId)};
        if (curSingerId == -1)
            curPos = mw->rotModel.rowCount() - 1;
        int loops = 0;
        while ((nextSongPath == "") && (!empty)) {
            if (loops > mw->rotModel.rowCount()) {
                empty = true;
            } else {
                if (++curPos >= mw->rotModel.rowCount()) {
                    curPos = 0;
                }
                nextSinger = mw->rotModel.singerIdAtPosition(curPos);
                nextSongPath = mw->rotModel.nextSongPath(nextSinger);
                loops++;
            }
        }
        if (empty) {
            QMessageBox::information(mw, "Unable to select next",
                                     "Sorry, no unsung karaoke songs are currently in any singer's queue");
            return;
        }
        mw->ui->tableViewRotation->clearSelection();
        mw->ui->tableViewRotation->selectRow(mw->rotModel.getSingerPosition(nextSinger));
    });

    scutKPlayNextUnsung = new QShortcut(settings.loadShortcutKeySequence("kPlayNextUnsung"), mw, nullptr, nullptr,
                                        Qt::ApplicationShortcut);
    connect(scutKPlayNextUnsung, &QShortcut::activated, [mw]() {
        if (auto state = mw->kMediaBackend.state(); state == MediaBackend::PlayingState ||
                                                    state == MediaBackend::PausedState) {
            if (settings.showSongInterruptionWarning()) {
                QMessageBox msgBox(mw);
                auto *cb = new QCheckBox("Show this warning in the future");
                cb->setChecked(settings.showSongInterruptionWarning());
                msgBox.setIcon(QMessageBox::Warning);
                msgBox.setText("Interrupt currenly playing karaoke song?");
                msgBox.setInformativeText(
                        "There is currently a karaoke song playing.  If you continue, the current song will be stopped.  Are you sure?");
                QPushButton *yesButton = msgBox.addButton(QMessageBox::Yes);
                msgBox.addButton(QMessageBox::Cancel);
                msgBox.setCheckBox(cb);
                connect(cb, &QCheckBox::toggled, &settings, &Settings::setShowSongInterruptionWarning);
                msgBox.exec();
                if (msgBox.clickedButton() != yesButton) {
                    return;
                }
            }
            mw->kMediaBackend.stop();
        }
        int nextSinger{-1};
        QString nextSongPath;
        bool empty{false};
        int curSingerId{mw->rotModel.currentSinger()};
        int curPos{mw->rotModel.getSingerPosition(curSingerId)};
        if (curSingerId == -1)
            curPos = mw->rotModel.rowCount() - 1;
        int loops = 0;
        while ((nextSongPath == "") && (!empty)) {
            if (loops > mw->rotModel.rowCount()) {
                empty = true;
            } else {
                if (++curPos >= mw->rotModel.rowCount()) {
                    curPos = 0;
                }
                nextSinger = mw->rotModel.singerIdAtPosition(curPos);
                nextSongPath = mw->rotModel.nextSongPath(nextSinger);
                loops++;
            }
        }
        if (empty) {
            QMessageBox::information(mw, "Unable to play next",
                                     "Sorry, no unsung karaoke songs are currently in any singer's queue");
            return;
        }
        mw->curSinger = mw->rotModel.getSingerName(nextSinger);
        mw->curArtist = mw->rotModel.nextSongArtist(nextSinger);
        mw->curTitle  = mw->rotModel.nextSongTitle(nextSinger);

        if (settings.treatAllSingersAsRegs() || mw->rotModel.singerIsRegular(nextSinger)) {
            mw->historySongsModel.saveSong(
                    mw->curSinger,
                    nextSongPath,
                    mw->curArtist,
                    mw->curTitle,
                    mw->rotModel.nextSongSongId(nextSinger),
                    mw->rotModel.nextSongKeyChg(nextSinger)
            );
        }
        mw->karaokeSongsModel.updateSongHistory(mw->karaokeSongsModel.getIdForPath(nextSongPath));
        mw->play(nextSongPath);
        mw->kMediaBackend.setPitchShift(mw->rotModel.nextSongKeyChg(nextSinger));
        mw->qModel.setPlayed(mw->rotModel.nextSongQueueId(nextSinger));
        mw->rotModel.setCurrentSinger(nextSinger);
        mw->rotDelegate.setCurrentSinger(nextSinger);
        if (settings.rotationAltSortOrder()) {
            auto curSingerPos = mw->rotModel.getSingerPosition(nextSinger);
            mw->m_curSingerOriginalPosition = curSingerPos;
            if (curSingerPos != 0)
                mw->rotModel.singerMove(curSingerPos, 0);
        }
        mw->ui->labelArtist->setText(mw->curArtist);
        mw->ui->labelTitle->setText(mw->curTitle);
        mw->ui->labelSinger->setText(mw->curSinger);
        mw->ui->tableViewRotation->clearSelection();
        mw->ui->tableViewRotation->selectRow(mw->rotModel.getSingerPosition(mw->rotModel.currentSinger()));
    });

    scutAddSinger = new QShortcut(settings.loadShortcutKeySequence("addSinger"), mw, nullptr, nullptr,
                                  Qt::ApplicationShortcut);
    connect(scutAddSinger, &QShortcut::activated, mw, &MainWindow::on_buttonAddSinger_clicked);

    scutBFfwd = new QShortcut(settings.loadShortcutKeySequence("bFfwd"), mw, nullptr, nullptr,
                              Qt::ApplicationShortcut);
    connect(scutBFfwd, &QShortcut::activated, [mw]() {
        auto mediaState = mw->bmMediaBackend.state();
        if (mediaState == MediaBackend::PlayingState || mediaState == MediaBackend::PausedState) {
            int curPos  = mw->bmMediaBackend.position();
            int duration = mw->bmMediaBackend.duration();
            if (curPos + 5000 < duration)
                mw->bmMediaBackend.setPosition(curPos + 5000);
        }
    });

    scutBPause = new QShortcut(settings.loadShortcutKeySequence("bPause"), mw, nullptr, nullptr,
                               Qt::ApplicationShortcut);
    connect(scutBPause, &QShortcut::activated, [mw]() {
        auto mediaState = mw->bmMediaBackend.state();
        if (mediaState == MediaBackend::PlayingState) {
            mw->bmMediaBackend.pause();
            mw->ui->buttonBmPause->setChecked(true);
        } else if (mediaState == MediaBackend::PausedState) {
            mw->bmMediaBackend.play();
            mw->ui->buttonBmPause->setChecked(false);
        }
    });

    scutBRestartSong = new QShortcut(settings.loadShortcutKeySequence("bRestartSong"), mw, nullptr, nullptr,
                                     Qt::ApplicationShortcut);
    connect(scutBRestartSong, &QShortcut::activated, [mw]() {
        auto mediaState = mw->bmMediaBackend.state();
        if (mediaState == MediaBackend::PlayingState || mediaState == MediaBackend::PausedState) {
            mw->bmMediaBackend.setPosition(0);
        }
    });

    scutBRwnd = new QShortcut(settings.loadShortcutKeySequence("bRwnd"), mw, nullptr, nullptr,
                              Qt::ApplicationShortcut);
    connect(scutBRwnd, &QShortcut::activated, [mw]() {
        auto mediaState = mw->bmMediaBackend.state();
        if (mediaState == MediaBackend::PlayingState || mediaState == MediaBackend::PausedState) {
            int curPos = mw->bmMediaBackend.position();
            if (curPos - 5000 > 0)
                mw->bmMediaBackend.setPosition(curPos - 5000);
            else
                mw->bmMediaBackend.setPosition(0);
        }
    });

    scutBStop = new QShortcut(settings.loadShortcutKeySequence("bStop"), mw, nullptr, nullptr,
                              Qt::ApplicationShortcut);
    connect(scutBStop, &QShortcut::activated, [mw]() {
        mw->bmMediaBackend.stop();
    });

    scutBVolDn = new QShortcut(settings.loadShortcutKeySequence("bVolDn"), mw, nullptr, nullptr,
                               Qt::ApplicationShortcut);
    connect(scutBVolDn, &QShortcut::activated, [mw]() {
        int curVol = mw->bmMediaBackend.getVolume();
        if (curVol > 0)
            mw->bmMediaBackend.setVolume(curVol - 1);
    });

    scutBVolMute = new QShortcut(settings.loadShortcutKeySequence("bVolMute"), mw, nullptr, nullptr,
                                 Qt::ApplicationShortcut);
    connect(scutBVolMute, &QShortcut::activated, [mw]() {
        mw->bmMediaBackend.setMuted(!mw->bmMediaBackend.isMuted());
    });

    scutBVolUp = new QShortcut(settings.loadShortcutKeySequence("bVolUp"), mw, nullptr, nullptr,
                               Qt::ApplicationShortcut);
    connect(scutBVolUp, &QShortcut::activated, [mw]() {
        int curVol = mw->bmMediaBackend.getVolume();
        if (curVol < 100)
            mw->bmMediaBackend.setVolume(curVol + 1);
    });

    scutJumpToSearch = new QShortcut(settings.loadShortcutKeySequence("jumpToSearch"), mw, nullptr, nullptr,
                                     Qt::ApplicationShortcut);
    connect(scutJumpToSearch, &QShortcut::activated, [mw]() {
        mw->activateWindow();
        if (!mw->hasFocus() && !mw->ui->lineEdit->hasFocus())
            mw->setFocus();
        if (mw->ui->lineEdit->hasFocus())
            mw->ui->lineEdit->clear();
        else
            mw->ui->lineEdit->setFocus();
    });

    scutKFfwd = new QShortcut(settings.loadShortcutKeySequence("kFfwd"), mw, nullptr, nullptr,
                              Qt::ApplicationShortcut);
    connect(scutKFfwd, &QShortcut::activated, [mw]() {
        auto mediaState = mw->kMediaBackend.state();
        if (mediaState == MediaBackend::PlayingState || mediaState == MediaBackend::PausedState) {
            int curPos   = mw->kMediaBackend.position();
            int duration = mw->kMediaBackend.duration();
            if (curPos + 5000 < duration)
                mw->kMediaBackend.setPosition(curPos + 5000);
        }
    });

    scutKPause = new QShortcut(settings.loadShortcutKeySequence("kPause"), mw, nullptr, nullptr,
                               Qt::ApplicationShortcut);
    connect(scutKPause, &QShortcut::activated, mw, &MainWindow::on_buttonPause_clicked);

    scutKRestartSong = new QShortcut(settings.loadShortcutKeySequence("kRestartSong"), mw, nullptr, nullptr,
                                     Qt::ApplicationShortcut);
    connect(scutKRestartSong, &QShortcut::activated, [mw]() {
        auto mediaState = mw->kMediaBackend.state();
        if (mediaState == MediaBackend::PlayingState || mediaState == MediaBackend::PausedState) {
            mw->kMediaBackend.setPosition(0);
        }
    });

    scutKRwnd = new QShortcut(settings.loadShortcutKeySequence("kRwnd"), mw, nullptr, nullptr,
                              Qt::ApplicationShortcut);
    connect(scutKRwnd, &QShortcut::activated, [mw]() {
        auto mediaState = mw->kMediaBackend.state();
        if (mediaState == MediaBackend::PlayingState || mediaState == MediaBackend::PausedState) {
            int curPos = mw->kMediaBackend.position();
            if (curPos - 5000 > 0)
                mw->kMediaBackend.setPosition(curPos - 5000);
            else
                mw->kMediaBackend.setPosition(0);
        }
    });

    scutKStop = new QShortcut(settings.loadShortcutKeySequence("kStop"), mw, nullptr, nullptr,
                              Qt::ApplicationShortcut);
    connect(scutKStop, &QShortcut::activated, [mw]() {
        mw->kMediaBackend.stop();
    });

    scutKVolDn = new QShortcut(settings.loadShortcutKeySequence("kVolDn"), mw, nullptr, nullptr,
                               Qt::ApplicationShortcut);
    connect(scutKVolDn, &QShortcut::activated, [mw]() {
        int curVol = mw->kMediaBackend.getVolume();
        if (curVol > 0)
            mw->kMediaBackend.setVolume(curVol - 1);
    });

    scutKVolMute = new QShortcut(settings.loadShortcutKeySequence("kVolMute"), mw, nullptr, nullptr,
                                 Qt::ApplicationShortcut);
    connect(scutKVolMute, &QShortcut::activated, [mw]() {
        mw->kMediaBackend.setMuted(!mw->kMediaBackend.isMuted());
    });

    scutKVolUp = new QShortcut(settings.loadShortcutKeySequence("kVolUp"), mw, nullptr, nullptr,
                               Qt::ApplicationShortcut);
    connect(scutKVolUp, &QShortcut::activated, [mw]() {
        int curVol = mw->kMediaBackend.getVolume();
        if (curVol < 100)
            mw->kMediaBackend.setVolume(curVol + 1);
    });

    scutLoadRegularSinger = new QShortcut(settings.loadShortcutKeySequence("loadRegularSinger"), mw, nullptr, nullptr,
                                          Qt::ApplicationShortcut);
    connect(scutLoadRegularSinger, &QShortcut::activated, mw, &MainWindow::on_buttonRegulars_clicked);

    scutToggleSingerWindow = new QShortcut(settings.loadShortcutKeySequence("toggleSingerWindow"), mw, nullptr,
                                           nullptr, Qt::ApplicationShortcut);
    connect(scutToggleSingerWindow, &QShortcut::activated, [mw]() {
        if (mw->cdgWindow->isVisible())
            mw->cdgWindow->hide();
        else
            mw->cdgWindow->show();
    });

    scutRequests = new QShortcut(settings.loadShortcutKeySequence("showIncomingRequests"), mw, nullptr, nullptr,
                                 Qt::ApplicationShortcut);
    connect(scutRequests, &QShortcut::activated, mw, &MainWindow::on_pushButtonIncomingRequests_clicked);

    scutDeleteSinger = new QShortcut(QKeySequence(QKeySequence::Delete), mw->ui->tableViewRotation, nullptr, nullptr,
                                     Qt::WidgetShortcut);
    connect(scutDeleteSinger, &QShortcut::activated, [mw]() {
        auto indexes = mw->ui->tableViewRotation->selectionModel()->selectedRows(0);
        std::vector<int> singerIds;
        std::for_each(indexes.begin(), indexes.end(), [&](auto index) {
            singerIds.emplace_back(index.data(Qt::UserRole).toInt());
        });
        if (singerIds.empty())
            return;
        if (settings.showSingerRemovalWarning()) {
            QMessageBox msgBox(mw);
            auto *cb = new QCheckBox("Show this warning in the future");
            cb->setChecked(settings.showSingerRemovalWarning());
            msgBox.setIcon(QMessageBox::Warning);
            if (singerIds.size() == 1) {
                msgBox.setText("Are you sure you want to remove this singer?");
                msgBox.setInformativeText(
                        "Unless this singer is a tracked regular, you will be unable retrieve any queue data for this singer once they are deleted.");
            } else {
                msgBox.setText("Are you sure you want to remove these singers?");
                msgBox.setInformativeText(
                        "Unless these singers are tracked regulars, you will be unable retrieve any queue data for them once they are deleted.");
            }
            QPushButton *yesButton = msgBox.addButton(QMessageBox::Yes);
            msgBox.addButton(QMessageBox::Cancel);
            msgBox.setCheckBox(cb);
            connect(cb, &QCheckBox::toggled, &settings, &Settings::setShowSingerRemovalWarning);
            msgBox.exec();
            if (msgBox.clickedButton() != yesButton) {
                return;
            }
        }

        mw->qModel.loadSinger(-1);
        std::for_each(singerIds.begin(), singerIds.end(), [mw](auto singerId) {
            if (mw->rotModel.currentSinger() == singerId) {
                mw->rotModel.setCurrentSinger(-1);
                mw->rotDelegate.setCurrentSinger(-1);
            }
            mw->rotModel.singerDelete(singerId);
        });
        mw->ui->tableViewRotation->clearSelection();
        mw->ui->tableViewQueue->clearSelection();
    });

    scutDeleteSong = new QShortcut(QKeySequence(QKeySequence::Delete), mw->ui->tableViewQueue, nullptr, nullptr,
                                   Qt::WidgetShortcut);
    connect(scutDeleteSong, &QShortcut::activated, [mw]() {
        auto indexes = mw->ui->tableViewQueue->selectionModel()->selectedRows(0);
        bool containsUnplayed{false};
        std::vector<int> songIds;
        std::for_each(indexes.begin(), indexes.end(), [&](auto index) {
            songIds.emplace_back(index.data().toInt());
            if (!mw->qModel.getPlayed(index.data().toInt()))
                containsUnplayed = true;
        });
        if ((settings.showQueueRemovalWarning()) && containsUnplayed) {
            QMessageBox msgBox(mw);
            auto *cb = new QCheckBox("Show this warning in the future");
            cb->setChecked(settings.showQueueRemovalWarning());
            msgBox.setIcon(QMessageBox::Warning);
            msgBox.setText("Removing un-played song from queue");
            msgBox.setInformativeText("This song has not been played yet, are you sure you want to remove it?");
            QPushButton *yesButton = msgBox.addButton(QMessageBox::Yes);
            msgBox.addButton(QMessageBox::Cancel);
            msgBox.setCheckBox(cb);
            connect(cb, &QCheckBox::toggled, &settings, &Settings::setShowQueueRemovalWarning);
            msgBox.exec();
            if (msgBox.clickedButton() != yesButton)
                return;
        }
        std::for_each(songIds.begin(), songIds.end(), [mw](auto songId) {
            mw->qModel.remove(songId);
        });
    });

    scutDeletePlSong = new QShortcut(QKeySequence(QKeySequence::Delete), mw->ui->tableViewBmPlaylist, nullptr, nullptr,
                                     Qt::WidgetShortcut);
    connect(scutDeletePlSong, &QShortcut::activated, [mw]() {
        auto rows = mw->ui->tableViewBmPlaylist->selectionModel()->selectedRows(0);
        std::vector<int> positions;
        bool curPlayingSelected{false};
        std::for_each(rows.begin(), rows.end(), [&](auto index) {
            positions.emplace_back(index.row());
            if (mw->playlistSongsModel.isCurrentlyPlayingSong(index.data(Qt::UserRole).toInt()))
                curPlayingSelected = true;
        });
        auto state = mw->bmMediaBackend.state();
        if (curPlayingSelected && (state == MediaBackend::PlayingState || state == MediaBackend::PausedState)) {
            QMessageBox msgBox;
            msgBox.setWindowTitle("Unable to remove");
            msgBox.setText("The playlist song you are trying to remove is currently playing and can not be removed.");
            msgBox.exec();
            return;
        }
        std::sort(positions.begin(), positions.end());
        std::reverse(positions.begin(), positions.end());
        std::for_each(positions.begin(), positions.end(), [mw](auto position) {
            mw->playlistSongsModel.deleteSong(position);
        });
        if (curPlayingSelected) {
            mw->playlistSongsModel.setCurrentPosition(-1);
        }
        mw->playlistSongsModel.savePlaylistChanges();
        if (state != MediaBackend::PlayingState && state != MediaBackend::PausedState)
            return;
        if (mw->ui->checkBoxBmBreak->isChecked())
            mw->ui->labelBmNext->setText("None - Breaking after current song");
        else {
            auto nextSong = mw->playlistSongsModel.getNextPlSong();
            if (nextSong.has_value())
                mw->ui->labelBmNext->setText(nextSong->get().artist + " - " + nextSong->get().title);
            else
                mw->ui->labelBmNext->setText("None - Breaking after current song");
        }
    });

    connect(&settings, &Settings::shortcutsChanged, mw, &MainWindow::shortcutsUpdated);
}

void ShortcutManager::updateShortcuts()
{
    scutKSelectNextSinger->setKey(settings.loadShortcutKeySequence("kSelectNextSinger"));
    scutKPlayNextUnsung->setKey(settings.loadShortcutKeySequence("kPlayNextUnsung"));
    scutAddSinger->setKey(settings.loadShortcutKeySequence("addSinger"));
    scutBFfwd->setKey(settings.loadShortcutKeySequence("bFfwd"));
    scutBPause->setKey(settings.loadShortcutKeySequence("bPause"));
    scutBRestartSong->setKey(settings.loadShortcutKeySequence("bRestartSong"));
    scutBRwnd->setKey(settings.loadShortcutKeySequence("bRwnd"));
    scutBStop->setKey(settings.loadShortcutKeySequence("bStop"));
    scutBVolDn->setKey(settings.loadShortcutKeySequence("bVolDn"));
    scutBVolMute->setKey(settings.loadShortcutKeySequence("bVolMute"));
    scutBVolUp->setKey(settings.loadShortcutKeySequence("bVolUp"));
    scutJumpToSearch->setKey(settings.loadShortcutKeySequence("jumpToSearch"));
    scutKFfwd->setKey(settings.loadShortcutKeySequence("kFfwd"));
    scutKPause->setKey(settings.loadShortcutKeySequence("kPause"));
    scutKRestartSong->setKey(settings.loadShortcutKeySequence("kRestartSong"));
    scutKRwnd->setKey(settings.loadShortcutKeySequence("kRwnd"));
    scutKStop->setKey(settings.loadShortcutKeySequence("kStop"));
    scutKVolDn->setKey(settings.loadShortcutKeySequence("kVolDn"));
    scutKVolMute->setKey(settings.loadShortcutKeySequence("kVolMute"));
    scutKVolUp->setKey(settings.loadShortcutKeySequence("kVolUp"));
    scutLoadRegularSinger->setKey(settings.loadShortcutKeySequence("loadRegularSinger"));
    scutRequests->setKey(settings.loadShortcutKeySequence("showIncomingRequests"));
    scutToggleSingerWindow->setKey(settings.loadShortcutKeySequence("toggleSingerWindow"));
}
