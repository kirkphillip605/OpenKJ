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

#ifndef SHORTCUTMANAGER_H
#define SHORTCUTMANAGER_H

#include <QObject>
#include <QShortcut>

class MainWindow;

// Manages all global application shortcuts for MainWindow.
// Declared as a friend of MainWindow so it can access private members
// during shortcut setup without requiring a large public API surface.
class ShortcutManager : public QObject
{
    Q_OBJECT
public:
    explicit ShortcutManager(MainWindow *mainWindow, QObject *parent = nullptr);

    // Create shortcuts and connect signals.  Must be called once after the
    // MainWindow UI is fully initialised.
    void setup();

    // Re-apply key sequences from settings (called when shortcuts change).
    void updateShortcuts();

private:
    MainWindow *m_mainWindow;

    QShortcut *scutAddSinger{nullptr};
    QShortcut *scutKSelectNextSinger{nullptr};
    QShortcut *scutKPlayNextUnsung{nullptr};
    QShortcut *scutBFfwd{nullptr};
    QShortcut *scutBPause{nullptr};
    QShortcut *scutBRestartSong{nullptr};
    QShortcut *scutBRwnd{nullptr};
    QShortcut *scutBStop{nullptr};
    QShortcut *scutBVolDn{nullptr};
    QShortcut *scutBVolMute{nullptr};
    QShortcut *scutBVolUp{nullptr};
    QShortcut *scutJumpToSearch{nullptr};
    QShortcut *scutKFfwd{nullptr};
    QShortcut *scutKPause{nullptr};
    QShortcut *scutKRestartSong{nullptr};
    QShortcut *scutKRwnd{nullptr};
    QShortcut *scutKStop{nullptr};
    QShortcut *scutKVolDn{nullptr};
    QShortcut *scutKVolMute{nullptr};
    QShortcut *scutKVolUp{nullptr};
    QShortcut *scutLoadRegularSinger{nullptr};
    QShortcut *scutRequests{nullptr};
    QShortcut *scutToggleSingerWindow{nullptr};
    QShortcut *scutDeleteSinger{nullptr};
    QShortcut *scutDeleteSong{nullptr};
    QShortcut *scutDeletePlSong{nullptr};
};

#endif // SHORTCUTMANAGER_H
