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

#include <qglobal.h>
#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QMessageBox>
// (QDesktopWidget removed — no longer used; use QGuiApplication::screens() instead)
#include <QMenu>
#include <QInputDialog>
#include <QFileDialog>
#include <QImageReader>
#include <QDesktopServices>
#include <QScroller>
#include <QCheckBox>
#include <QComboBox>
#include <QSpinBox>
#include "mzarchive.h"
#include "tagreader.h"
#include "dlgeditsong.h"
#include "soundfxbutton.h"
#include "src/models/tableviewtooltipfilter.h"
#include <tickernew.h>
#include "dbupdatethread.h"
#include "okjutil.h"
#include <algorithm>
#include "dlgaddsong.h"
#include "databasemanager.h"
#include "theme.h"

#ifdef _MSC_VER
#define NOMINMAX

#include <Windows.h>

#endif

extern QString altDataDir;
extern Settings settings;
OKJSongbookAPI *songbookApi;

// for some reason clang-tidy is choking on this function
#pragma clang diagnostic push
#pragma ide diagnostic ignored "OCDFAInspection"

void MainWindow::addSfxButton(const QString &filename, const QString &label, const bool &reset) {
    static int numButtons = 0;
    if (reset)
        numButtons = 0;
    qInfo() << "sfxButtonGrid contains " << numButtons << " children";
    int col = 0;
    if (numButtons % 2) {
        col = 1;
    }
    int row = numButtons / 2;
    qInfo() << "Adding button " << label << "at row: " << row << " col: " << col;
    auto *button = new SoundFxButton();
    button->setButtonData(filename);
    button->setText(label);
    ui->sfxButtonGrid->addWidget(button, row, col);
    connect(button, &SoundFxButton::clicked, this, &MainWindow::sfxButtonPressed);
    connect(button, &SoundFxButton::customContextMenuRequested, this,
            &MainWindow::sfxButton_customContextMenuRequested);
    numButtons++;
}

#pragma clang diagnostic pop

void MainWindow::refreshSfxButtons() {
    QLayoutItem *item;
    while ((item = ui->sfxButtonGrid->takeAt(0)) != nullptr) {
        delete item->widget();
        delete item;
    }
    SfxEntryList list = settings.getSfxEntries();
    bool first = true;
    qInfo() << "SfxEntryList size: " << list.size();
            foreach (SfxEntry entry, list) {
            if (first) {
                first = false;
                addSfxButton(entry.path, entry.name, true);
                continue;
            }
            addSfxButton(entry.path, entry.name);
        }
}

void MainWindow::updateIcons() {
    QString thm = (settings.theme() == 1) ? ":/theme/Icons/okjbreeze-dark/" : ":/theme/Icons/okjbreeze/";
    ui->buttonClearRotation->setIcon(QIcon(thm + "actions/22/edit-clear-all.svg"));
    ui->buttonAddSinger->setIcon(QIcon(thm + "actions/22/list-add-user.svg"));
    ui->buttonRegulars->setIcon(QIcon(thm + "actions/22/user-others.svg"));
    ui->btnRotTop->setIcon(QIcon(thm + "actions/22/go-top.svg"));
    ui->btnRotUp->setIcon(QIcon(thm + "actions/22/go-up.svg"));
    ui->btnRotBottom->setIcon(QIcon(thm + "actions/22/go-bottom.svg"));
    ui->btnRotDown->setIcon(QIcon(thm + "actions/22/go-down.svg"));
    ui->pushButton->setIcon(QIcon(thm + "actions/22/edit-find.svg"));
    ui->buttonClearQueue->setIcon(QIcon(thm + "actions/22/edit-clear-all.svg"));
    ui->btnQTop->setIcon(QIcon(thm + "actions/22/go-top.svg"));
    ui->btnQUp->setIcon(QIcon(thm + "actions/22/go-up.svg"));
    ui->btnQBottom->setIcon(QIcon(thm + "actions/22/go-bottom.svg"));
    ui->btnQDown->setIcon(QIcon(thm + "actions/22/go-down.svg"));
    ui->buttonPause->setIcon(QIcon(thm + "actions/22/media-playback-pause.svg"));
    ui->buttonStop->setIcon(QIcon(thm + "actions/22/media-playback-stop.svg"));
    ui->labelVolume->setPixmap(QPixmap(thm + "actions/16/player-volume.svg"));
    ui->pushButtonTempoDn->setIcon(QIcon(thm + "actions/22/downindicator.svg"));
    ui->pushButtonTempoUp->setIcon(QIcon(thm + "actions/22/upindicator.svg"));
    ui->pushButtonKeyDn->setIcon(QIcon(thm + "actions/22/downindicator.svg"));
    ui->pushButtonKeyUp->setIcon(QIcon(thm + "actions/22/upindicator.svg"));
    ui->btnSfxStop->setIcon(QIcon(thm + "actions/22/media-playback-stop.svg"));

    requestsDialog->updateIcons();
    connect(&settings, &Settings::treatAllSingersAsRegsChanged, this, &MainWindow::treatAllSingersAsRegsChanged);
    treatAllSingersAsRegsChanged(settings.treatAllSingersAsRegs());
}

void MainWindow::setupShortcuts() {
    m_shortcutManager = new ShortcutManager(this, this);
    m_shortcutManager->setup();
}

void MainWindow::shortcutsUpdated() {
    if (m_shortcutManager)
        m_shortcutManager->updateShortcuts();
}

void MainWindow::treatAllSingersAsRegsChanged(bool enabled) {
    if (enabled) {
        ui->tableViewRotation->hideColumn(TableModelRotation::COL_REGULAR);
        if (ui->tabWidgetQueue->count() == 1)
            ui->tabWidgetQueue->addTab(historyTabWidget, "History");
    } else {
        ui->tableViewRotation->showColumn(TableModelRotation::COL_REGULAR);
        int curSelSingerId{-1};
        if (ui->tableViewRotation->selectionModel()->selectedRows().count() > 1) {
            curSelSingerId = ui->tableViewRotation->selectionModel()->selectedRows(0).at(
                    TableModelRotation::COL_ID).data(Qt::UserRole).toInt();
        }
        if (!rotModel.singerIsRegular(curSelSingerId) && ui->tabWidgetQueue->count() == 2)
            ui->tabWidgetQueue->removeTab(1);
    }
    resizeRotation();
}

MainWindow::MainWindow(QWidget *parent) :
        QMainWindow(parent),
        ui(new Ui::MainWindow) {
#ifdef _MSC_VER
    timeBeginPeriod(1);
#endif
    debugDialog = new DlgDebugOutput(this);
    debugDialog->setVisible(settings.logShow());
    QCoreApplication::setOrganizationName("OpenKJ");
    QCoreApplication::setOrganizationDomain("OpenKJ.org");
    QCoreApplication::setApplicationName("OpenKJ");
    ui->setupUi(this);
    setMouseTracking(true);
    historyTabWidget = ui->tabWidgetQueue->widget(1);
    ui->actionShow_Debug_Log->setChecked(settings.logShow());
#ifdef Q_OS_WIN
    ui->sliderProgress->setMaximumHeight(12);
#endif
    QDir okjDataDir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation));
    if (!okjDataDir.exists()) {
        okjDataDir.mkpath(okjDataDir.absolutePath());
    }
    if (settings.theme() != 0) {
        ui->pushButtonIncomingRequests->setStyleSheet("");
        update();
    }
    ui->pushButtonKeyDn->setEnabled(false);
    ui->pushButtonKeyUp->setEnabled(false);
    ui->pushButtonTempoDn->setEnabled(false);
    ui->pushButtonTempoUp->setEnabled(false);
    songbookApi = new OKJSongbookAPI(this);
    int initialKVol = settings.audioVolume();
    qInfo() << "Initial volumes - K: " << initialKVol;
    QTimer::singleShot(250, [&]() {
        settings.restoreWindowState(this);
    });
    dbInit(okjDataDir);
    setupShortcuts();
    karaokeSongsModel.loadData();
    rotModel.loadData();
    ui->comboBoxHistoryDblClick->addItems(QStringList{"Adds to queue", "Plays song"});
    ui->comboBoxHistoryDblClick->setCurrentIndex(settings.historyDblClickAction());
    ui->tabWidgetQueue->setCurrentIndex(0);
    connect(ui->comboBoxHistoryDblClick, QOverload<int>::of(&QComboBox::currentIndexChanged), &settings,
            &Settings::setHistoryDblClickAction);
    ui->tableViewHistory->setModel(&historySongsModel);
    ui->tableViewHistory->hideColumn(0);
    ui->tableViewHistory->hideColumn(1);
    ui->tableViewHistory->hideColumn(2);
    ui->tableViewHistory->sortByColumn(3, Qt::AscendingOrder);
    ui->comboBoxSearchType->addItems({QString("All"), QString("Artist"), QString("Title")});
    ui->tableViewDB->hideColumn(TableModelKaraokeSongs::COL_ID);
    ui->tableViewDB->hideColumn(TableModelKaraokeSongs::COL_FILENAME);
    ui->tableViewRotation->setModel(&rotModel);
    ui->tableViewRotation->setItemDelegate(&rotDelegate);
    ui->tableViewRotation->hideColumn(TableModelRotation::COL_ADDTS);
    ui->tableViewRotation->hideColumn(TableModelRotation::COL_POSITION);
    if (settings.treatAllSingersAsRegs())
        ui->tableViewRotation->hideColumn(TableModelRotation::COL_REGULAR);
    if (settings.rotationShowNextSong()) {
        ui->tableViewRotation->horizontalHeader()->setSectionResizeMode(TableModelRotation::COL_NAME,
                                                                        QHeaderView::Interactive);
        ui->tableViewRotation->horizontalHeader()->setSectionResizeMode(TableModelRotation::COL_NEXT_SONG,
                                                                        QHeaderView::Stretch);
    } else {
        ui->tableViewRotation->horizontalHeader()->setSectionResizeMode(TableModelRotation::COL_NAME,
                                                                        QHeaderView::Stretch);
        ui->tableViewRotation->hideColumn(TableModelRotation::COL_NEXT_SONG);
    }
    ui->tableViewRotation->horizontalHeader()->setSectionResizeMode(TableModelRotation::COL_ID,
                                                                    QHeaderView::ResizeToContents);
    ui->tableViewRotation->horizontalHeader()->setSectionResizeMode(TableModelRotation::COL_DELETE,
                                                                    QHeaderView::ResizeToContents);
    ui->tableViewRotation->horizontalHeader()->setSectionResizeMode(TableModelRotation::COL_REGULAR,
                                                                    QHeaderView::ResizeToContents);
    if (settings.rotationShowNextSong())
        settings.restoreColumnWidths(ui->tableViewRotation);
    ui->tableViewQueue->setModel(&qModel);
    ui->tableViewQueue->setItemDelegate(&qDelegate);
    ui->tableViewQueue->viewport()->installEventFilter(new TableViewToolTipFilter(ui->tableViewQueue));
    ui->labelNoSinger->setVisible(true);
    autosizeQueue();
    ui->tabWidgetQueue->setVisible(false);
    khTmpDir = new QTemporaryDir();
    dbDialog = new DlgDatabase(database, this);
    dlgKeyChange = new DlgKeyChange(&qModel, this);
    requestsDialog = new DlgRequests(&rotModel);
    requestsDialog->setModal(false);
    dlgBookCreator = new DlgBookCreator(this);
    dlgEq = new DlgEq(this);
    dlgAddSinger = new DlgAddSinger(&rotModel, this);
    connect(dlgAddSinger, &DlgAddSinger::newSingerAdded, [&](auto pos) {
        ui->tableViewRotation->selectRow(pos);
        ui->lineEdit->setFocus();
    });
    ui->tableViewDB->setModel(&karaokeSongsModel);
    ui->tableViewDB->viewport()->installEventFilter(new TableViewToolTipFilter(ui->tableViewDB));
    kMediaBackend.setUseFader(settings.audioUseFader());
    if (!MediaBackend::canPitchShift()) {
        ui->spinBoxKey->hide();
        ui->lblKey->hide();
        ui->tableViewQueue->hideColumn(7);
    }
    if (!MediaBackend::canChangeTempo()) {
        ui->spinBoxTempo->hide();
        ui->lblTempo->hide();
    }
    ui->videoPreview->setFillOnPaint(true);
    cdgWindow = new DlgCdg(&kMediaBackend, this, Qt::Window);
    settings.restoreWindowState(cdgWindow);

    connect(&rotModel, &TableModelRotation::songDroppedOnSinger, this, &MainWindow::songDroppedOnSinger);
    connect(dbDialog, &DlgDatabase::databaseUpdateComplete, this, &MainWindow::databaseUpdated);
    connect(dbDialog, &DlgDatabase::databaseSongAdded, &karaokeSongsModel, &TableModelKaraokeSongs::loadData);
    connect(dbDialog, &DlgDatabase::databaseSongAdded, requestsDialog, &DlgRequests::databaseSongAdded);
    connect(dbDialog, &DlgDatabase::databaseCleared, this, &MainWindow::databaseCleared);
    connect(&kMediaBackend, &MediaBackend::volumeChanged, ui->sliderVolume, &QSlider::setValue);
    connect(&kMediaBackend, &MediaBackend::positionChanged, this, &MainWindow::karaokeMediaBackend_positionChanged);
    connect(&kMediaBackend, &MediaBackend::durationChanged, this, &MainWindow::karaokeMediaBackend_durationChanged);
    connect(&kMediaBackend, &MediaBackend::stateChanged, this, &MainWindow::karaokeMediaBackend_stateChanged);
    connect(&kMediaBackend, &MediaBackend::hasActiveVideoChanged, [=](const bool &isActive) {
        m_kHasActiveVideo = isActive;
        hasActiveVideoChanged();
    });
    connect(&kMediaBackend, &MediaBackend::pitchChanged, ui->spinBoxKey, &QSpinBox::setValue);
    connect(&kMediaBackend, &MediaBackend::audioError, this, &MainWindow::audioError);
    connect(&kMediaBackend, &MediaBackend::silenceDetected, this, &MainWindow::silenceDetectedKar);
    connect(&sfxMediaBackend, &MediaBackend::positionChanged, this, &MainWindow::sfxAudioBackend_positionChanged);
    connect(&sfxMediaBackend, &MediaBackend::durationChanged, this, &MainWindow::sfxAudioBackend_durationChanged);
    connect(&sfxMediaBackend, &MediaBackend::stateChanged, this, &MainWindow::sfxAudioBackend_stateChanged);
    connect(&rotModel, &TableModelRotation::rotationModified, this, &MainWindow::rotationDataChanged);
    connect(&settings, &Settings::tickerOutputModeChanged, this, &MainWindow::rotationDataChanged);
    kMediaBackend.setUseFader(settings.audioUseFader());

    kMediaBackend.setUseSilenceDetection(settings.audioDetectSilence());
    connect(ui->pushButtonTempoDn, &QPushButton::clicked, ui->spinBoxTempo, &QSpinBox::stepDown);
    connect(ui->pushButtonTempoUp, &QPushButton::clicked, ui->spinBoxTempo, &QSpinBox::stepUp);
    connect(ui->pushButtonKeyDn, &QPushButton::clicked, ui->spinBoxKey, &QSpinBox::stepDown);
    connect(ui->pushButtonKeyUp, &QPushButton::clicked, ui->spinBoxKey, &QSpinBox::stepUp);

    kMediaBackend.setDownmix(settings.audioDownmix());
    connect(requestsDialog, &DlgRequests::addRequestSong, &qModel, &TableModelQueueSongs::songAddSlot);
    connect(&settings, &Settings::tickerCustomStringChanged, this, &MainWindow::rotationDataChanged);

    settings.restoreWindowState(requestsDialog);
    settings.restoreWindowState(dbDialog);
    settings.restoreSplitterState(ui->splitter);
    settings.restoreSplitterState(ui->splitter_2);
    rotationDataChanged();
    ui->tableViewDB->hideColumn(TableModelKaraokeSongs::COL_ID);
    ui->tableViewDB->hideColumn(TableModelKaraokeSongs::COL_FILENAME);
    ui->tableViewQueue->hideColumn(TableModelQueueSongs::COL_ID);
    ui->tableViewQueue->hideColumn(TableModelQueueSongs::COL_DBSONGID);
    if (!MediaBackend::canPitchShift()) {
        ui->tableViewQueue->hideColumn(TableModelQueueSongs::COL_KEY);
    }
    rotModel.setHeaderData(0, Qt::Horizontal, "");
    rotModel.setHeaderData(1, Qt::Horizontal, "Singer");
    rotModel.setHeaderData(3, Qt::Horizontal, "");
    rotModel.setHeaderData(4, Qt::Horizontal, "");
    //ui->tableViewRotation->hideColumn(2);
    //ui->tableViewRotation->hideColumn(5);
    qInfo() << "Adding singer count to status bar";
    ui->statusBar->addWidget(&labelSingerCount);
    ui->statusBar->addWidget(&labelRotationDuration);


    settings.restoreSplitterState(ui->splitter_3);


    ui->sliderVolume->setValue(initialKVol);
    kMediaBackend.setVolume(initialKVol);
    if (settings.mplxMode() == Multiplex_Normal)
        ui->pushButtonMplxBoth->setChecked(true);
    else if (settings.mplxMode() == Multiplex_LeftChannel)
        ui->pushButtonMplxLeft->setChecked(true);
    else if (settings.mplxMode() == Multiplex_RightChannel)
        ui->pushButtonMplxRight->setChecked(true);
    connect(&m_timerKaraokeAA, &QTimer::timeout, this, &MainWindow::karaokeAATimerTimeout);
    ui->actionAutoplay_mode->setChecked(settings.karaokeAutoAdvance());
    connect(ui->actionAutoplay_mode, &QAction::toggled, &settings, &Settings::setKaraokeAutoAdvance);
    connect(&settings, &Settings::karaokeAutoAdvanceChanged, ui->actionAutoplay_mode, &QAction::setChecked);


    // todo - athom: what's this?
    // isaac: this is the AV offset between audio and video to compensate for any downstream signal processing
    // delay on either the video or audio side (hdmi decoder lag on TVs or audio effects hardware and such).
    // It's just badly named because it existed when OpenK only supported cdg files and not mp4s and such.
    // Not sure why it's just there alone, though, as it returns an offset in milliseconds lol
    settings.cdgDisplayOffset();

    connect(&settings, &Settings::eqKBypassChanged, &kMediaBackend, &MediaBackend::setEqBypass);
    connect(&settings, &Settings::eqKLevelChanged, &kMediaBackend, &MediaBackend::setEqLevel);


    connect(&settings, &Settings::enforceAspectRatioChanged, &kMediaBackend, &MediaBackend::setEnforceAspectRatio);
    connect(&settings, &Settings::mplxModeChanged, &kMediaBackend, &MediaBackend::setMplxMode);
    connect(&settings, &Settings::videoOffsetChanged, [&](auto offsetMs) {
        kMediaBackend.setVideoOffset(offsetMs);
    });

    kMediaBackend.setEnforceAspectRatio(settings.enforceAspectRatio());

    kMediaBackend.setEqBypass(settings.eqKBypass());
    for (int band = 0; band < 10; band++) {
        kMediaBackend.setEqLevel(band, settings.getEqKLevel(band));
    }

    connect(ui->lineEdit, &CustomLineEdit::escapePressed, ui->lineEdit, &CustomLineEdit::clear);
    connect(&qModel, &TableModelQueueSongs::songDroppedWithoutSinger, this, &MainWindow::songDropNoSingerSel);
    connect(ui->splitter_3, &QSplitter::splitterMoved, [&]() { autosizeViews(); });
    checker = new UpdateChecker(this);
    connect(checker, &UpdateChecker::newVersionAvailable, this, &MainWindow::newVersionAvailable);
    checker->checkForUpdates();
    connect(&m_timerButtonFlash, &QTimer::timeout, this, &MainWindow::timerButtonFlashTimeout);
    m_timerButtonFlash.start(1000);
    ui->pushButtonIncomingRequests->setVisible(settings.requestServerEnabled());
    connect(&settings, &Settings::requestServerEnabledChanged, ui->pushButtonIncomingRequests,
            &QPushButton::setVisible);
    qInfo() << "Initial UI stup complete";
    connect(&qModel, &TableModelQueueSongs::filesDroppedOnSinger, this, &MainWindow::filesDroppedOnQueue);
    connect(&settings, &Settings::applicationFontChanged, this, &MainWindow::appFontChanged);
    QApplication::processEvents();
    QApplication::processEvents();
    appFontChanged(settings.applicationFont());
    QTimer::singleShot(500, [&]() {
        autosizeViews();
    });
    connect(songbookApi, &OKJSongbookAPI::alertRecieved, this, &MainWindow::showAlert);
    connect(&settings, &Settings::cdgShowCdgWindowChanged, this, &MainWindow::cdgVisibilityChanged);
    connect(&settings, &Settings::rotationShowNextSongChanged, [&]() { resizeRotation(); });
    connect(&m_dlgRegularSingers.historySingersModel(), &TableModelHistorySingers::historySingersModified, [&]() {
        historySongsModel.refresh();
    });
    connect(&qModel, &TableModelQueueSongs::queueModified, &m_dlgRegularSingers, &DlgRegularSingers::regularsChanged);
    m_dlgRegularSingers.regularsChanged();
    m_dlgRegularSingers.setModal(false);
    SfxEntryList list = settings.getSfxEntries();
    qInfo() << "SfxEntryList size: " << list.size();
            foreach (SfxEntry entry, list) {
            addSfxButton(entry.path, entry.name);
        }
    connect(ui->tableViewRotation->selectionModel(), &QItemSelectionModel::currentChanged, this,
            &MainWindow::tableViewRotationCurrentChanged);
    rotModel.setCurrentSinger(settings.currentRotationPosition());
    rotDelegate.setCurrentSinger(settings.currentRotationPosition());
    updateRotationDuration();
    connect(&m_timerSlowUiUpdate, &QTimer::timeout, this, &MainWindow::updateRotationDuration);
    m_timerSlowUiUpdate.start(10000);
    connect(&qModel, &TableModelQueueSongs::queueModified, [&]() {
        updateRotationDuration();
        rotModel.layoutChanged();
    });
    connect(&settings, &Settings::rotationDurationSettingsModified, this, &MainWindow::updateRotationDuration);
    lazyDurationUpdater = new LazyDurationUpdateController(this);
    connect(lazyDurationUpdater, &LazyDurationUpdateController::gotDuration, &karaokeSongsModel,
            &TableModelKaraokeSongs::setSongDuration);
    if (settings.dbLazyLoadDurations())
        lazyDurationUpdater->getDurations();
    ui->btnToggleCdgWindow->setChecked(settings.showCdgWindow());
    connect(ui->tableViewRotation->selectionModel(), &QItemSelectionModel::selectionChanged, this,
            &MainWindow::rotationSelectionChanged);
    ui->groupBoxNowPlaying->setVisible(settings.showMainWindowNowPlaying());
    ui->groupBoxSoundClips->setVisible(settings.showMainWindowSoundClips());
    if (settings.showMainWindowSoundClips()) {
        ui->groupBoxSoundClips->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        ui->scrollAreaSoundClips->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        ui->scrollAreaWidgetContents->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        ui->verticalSpacerRtPanel->changeSize(0, 20, QSizePolicy::Ignored, QSizePolicy::Ignored);
        ui->groupBoxSoundClips->setVisible(true);
    } else {
        ui->groupBoxSoundClips->setVisible(false);
        ui->groupBoxSoundClips->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Ignored);
        ui->verticalSpacerRtPanel->changeSize(0, 20, QSizePolicy::Ignored, QSizePolicy::Expanding);
    }
    ui->videoPreview->setVisible(settings.showMainWindowVideo());
    ui->actionNow_Playing->setChecked(settings.showMainWindowNowPlaying());
    ui->actionSound_Clips->setChecked(settings.showMainWindowSoundClips());
    ui->actionVideo_Output_2->setChecked(settings.showMainWindowVideo());
    ui->actionMultiplex_Controls->setChecked(settings.showMplxControls());
    ui->widgetMplxControls->setVisible(settings.showMplxControls());
    switch (settings.mainWindowVideoSize()) {
        case Settings::Small:
            on_actionVideoSmall_triggered();
            break;
        case Settings::Medium:
            on_actionVideoMedium_triggered();
            break;
        case Settings::Large:
            on_actionVideoLarge_triggered();
            break;
    }
    ui->labelVolume->setPixmap(QIcon::fromTheme("player-volume").pixmap(QSize(22, 22)));
    updateIcons();
    ui->menuTesting->menuAction()->setVisible(settings.testingEnabled());

    // Apply Modern theme and touch-friendly mode based on saved settings
    applyAppTheme(settings.appTheme());
    applyTouchFriendly(settings.touchFriendlyEnabled());
    connect(&settings, &Settings::appThemeChanged, this, &MainWindow::applyAppTheme);
    connect(&settings, &Settings::touchFriendlyEnabledChanged, this, &MainWindow::applyTouchFriendly);

    connect(ui->tableViewQueue->selectionModel(), &QItemSelectionModel::selectionChanged, [&]() {
        if (ui->tableViewQueue->selectionModel()->selectedRows().empty()) {
            ui->btnQBottom->setEnabled(false);
            ui->btnQDown->setEnabled(false);
            ui->btnQTop->setEnabled(false);
            ui->btnQUp->setEnabled(false);
        } else if (ui->tableViewQueue->selectionModel()->selectedRows().size() == 1) {
            ui->btnQBottom->setEnabled(true);
            ui->btnQDown->setEnabled(true);
            ui->btnQTop->setEnabled(true);
            ui->btnQUp->setEnabled(true);
        } else {
            ui->btnQBottom->setEnabled(true);
            ui->btnQDown->setEnabled(false);
            ui->btnQTop->setEnabled(true);
            ui->btnQUp->setEnabled(false);
        }
    });


    connect(ui->tableViewRotation->selectionModel(), &QItemSelectionModel::selectionChanged, [&]() {
        if (ui->tableViewRotation->selectionModel()->selectedRows().empty()) {
            ui->btnRotBottom->setEnabled(false);
            ui->btnRotDown->setEnabled(false);
            ui->btnRotTop->setEnabled(false);
            ui->btnRotUp->setEnabled(false);
            ui->tabWidgetQueue->hide();
            ui->labelNoSinger->show();
        } else if (ui->tableViewRotation->selectionModel()->selectedRows().size() == 1) {
            ui->btnRotBottom->setEnabled(true);
            ui->btnRotDown->setEnabled(true);
            ui->btnRotTop->setEnabled(true);
            ui->btnRotUp->setEnabled(true);
        } else {
            ui->btnRotBottom->setEnabled(true);
            ui->btnRotDown->setEnabled(false);
            ui->btnRotTop->setEnabled(true);
            ui->btnRotUp->setEnabled(false);
        }
    });

    connect(&qModel, &TableModelQueueSongs::qSongsMoved, [&](auto startRow, auto startCol, auto endRow, auto endCol) {
        auto topLeft = ui->tableViewQueue->model()->index(startRow, startCol);
        auto bottomRight = ui->tableViewQueue->model()->index(endRow, endCol);
        ui->tableViewQueue->clearSelection();
        ui->tableViewQueue->selectionModel()->select(QItemSelection(topLeft, bottomRight), QItemSelectionModel::Select);
    });
    connect(&rotModel, &TableModelRotation::singersMoved, [&](auto startRow, auto startCol, auto endRow, auto endCol) {
        if (startRow == endRow) {
            ui->tableViewRotation->clearSelection();
            ui->tableViewRotation->selectRow(startRow);
            return;
        }
        auto topLeft = ui->tableViewRotation->model()->index(startRow, startCol);
        auto bottomRight = ui->tableViewRotation->model()->index(endRow, endCol);
        ui->tableViewRotation->clearSelection();
        ui->tableViewRotation->selectionModel()->select(QItemSelection(topLeft, bottomRight),
                                                        QItemSelectionModel::Select);
    });
    std::vector<QWidget *> videoWidgets{cdgWindow->getVideoDisplay(), ui->videoPreview};
    kMediaBackend.setVideoOutputWidgets(videoWidgets);
    settings.setStartupOk(true);
    m_initialUiSetupDone = true;

}

void MainWindow::dbInit(const QDir &okjDataDir) {
    database = DatabaseManager::initDatabase(
            okjDataDir,
            altDataDir,
            [this](const QString &singerName, const QString &filepath,
                   const QString &artist, const QString &title,
                   const QString &songid, int keychange) {
                historySongsModel.saveSong(singerName, filepath, artist, title, songid, keychange);
            }
    );
}

void MainWindow::play(const QString &karaokeFilePath, const bool &k2k) {
    khTmpDir->remove();
    delete khTmpDir;
    khTmpDir = new QTemporaryDir();
    if (kMediaBackend.state() != MediaBackend::PausedState) {
        qInfo() << "Playing file: " << karaokeFilePath;
        if (kMediaBackend.state() == MediaBackend::PlayingState) {
            if (settings.karaokeAutoAdvance()) {
                kAASkip = true;
                cdgWindow->showAlert(false);
            }
            kMediaBackend.stop();
            if (k2kTransition && settings.rotationAltSortOrder())
                rotModel.singerMove(0, rotModel.rowCount() - 1);
            ui->spinBoxTempo->setValue(100);
        }
        if (karaokeFilePath.endsWith(".zip", Qt::CaseInsensitive)) {
            MzArchive archive(karaokeFilePath);
            if ((archive.checkCDG()) && (archive.checkAudio())) {
                if (archive.checkAudio()) {
                    if (!archive.extractAudio(khTmpDir->path(), "tmp" + archive.audioExtension())) {
                        m_timerTest.stop();
                        QMessageBox::warning(this, tr("Bad karaoke file"), tr("Failed to extract audio file."),
                                             QMessageBox::Ok);
                        return;
                    }
                    if (!archive.extractCdg(khTmpDir->path(), "tmp.cdg")) {
                        m_timerTest.stop();
                        QMessageBox::warning(this, tr("Bad karaoke file"), tr("Failed to extract CDG file."),
                                             QMessageBox::Ok);
                        return;
                    }
                    QString audioFile = khTmpDir->path() + QDir::separator() + "tmp" + archive.audioExtension();
                    QString cdgFile = khTmpDir->path() + QDir::separator() + "tmp.cdg";
                    qInfo() << "Extracted audio file size: " << QFileInfo(audioFile).size();
                    qInfo() << "Setting karaoke backend source file to: " << audioFile;
                    kMediaBackend.setMediaCdg(cdgFile, audioFile);
                    qInfo() << "Beginning playback of file: " << audioFile;
                    QApplication::setOverrideCursor(Qt::WaitCursor);
                    kMediaBackend.play();
                    QApplication::restoreOverrideCursor();
                    kMediaBackend.fadeInImmediate();
                }
            } else {
                QMessageBox::warning(this, tr("Bad karaoke file"),
                                     tr("Zip file does not contain a valid karaoke track.  CDG or audio file missing or corrupt."),
                                     QMessageBox::Ok);
                return;
            }
        } else if (karaokeFilePath.endsWith(".cdg", Qt::CaseInsensitive)) {
            QString cdgTmpFile = "tmp.cdg";
            QString audTmpFile = "tmp.mp3";
            QFile cdgFile(karaokeFilePath);
            if (!cdgFile.exists()) {
                m_timerTest.stop();
                QMessageBox::warning(this, tr("Bad karaoke file"), tr("CDG file missing."), QMessageBox::Ok);
                return;
            } else if (cdgFile.size() == 0) {
                m_timerTest.stop();
                QMessageBox::warning(this, tr("Bad karaoke file"), tr("CDG file contains no data"), QMessageBox::Ok);
                return;
            }
            QString audiofn = findMatchingAudioFile(karaokeFilePath);
            if (audiofn == "") {
                m_timerTest.stop();
                QMessageBox::warning(this, tr("Bad karaoke file"), tr("Audio file missing."), QMessageBox::Ok);
                return;
            }
            QFile audioFile(audiofn);
            if (audioFile.size() == 0) {
                m_timerTest.stop();
                QMessageBox::warning(this, tr("Bad karaoke file"), tr("Audio file contains no data"), QMessageBox::Ok);
                return;
            }
            cdgFile.copy(khTmpDir->path() + QDir::separator() + cdgTmpFile);
            QFile::copy(audiofn, khTmpDir->path() + QDir::separator() + audTmpFile);
            kMediaBackend.setMediaCdg(khTmpDir->path() + QDir::separator() + cdgTmpFile,
                                      khTmpDir->path() + QDir::separator() + audTmpFile);
            QApplication::setOverrideCursor(Qt::WaitCursor);
            kMediaBackend.play();
            QApplication::restoreOverrideCursor();
            kMediaBackend.fadeInImmediate();
        } else {
            // Close CDG if open to avoid double video playback
            qInfo() << "Playing non-CDG video file: " << karaokeFilePath;
            QString tmpFileName = khTmpDir->path() + QDir::separator() + "tmpvid." + karaokeFilePath.right(4);
            QFile::copy(karaokeFilePath, tmpFileName);
            qInfo() << "Playing temporary copy to avoid bad filename stuff w/ gstreamer: " << tmpFileName;
            kMediaBackend.setMedia(tmpFileName);
            kMediaBackend.play();
            kMediaBackend.fadeInImmediate();
        }
        kMediaBackend.setTempo(ui->spinBoxTempo->value());


    } else if (kMediaBackend.state() == MediaBackend::PausedState) {
        kMediaBackend.play();
        kMediaBackend.fadeIn(false);
    }
    k2kTransition = false;
    if (settings.karaokeAutoAdvance())
        kAASkip = false;
}

MainWindow::~MainWindow() {
    m_shuttingDown = true;
    cdgWindow->stopTicker();
#ifdef _MSC_VER
    timeEndPeriod(1);
#endif
    lazyDurationUpdater->stopWork();
    settings.setAudioVolume(ui->sliderVolume->value());
    qInfo() << "Saving volumes - K: " << settings.audioVolume();
    qInfo() << "Saving window and widget sizing and positioning info";
    settings.saveSplitterState(ui->splitter);
    settings.saveSplitterState(ui->splitter_2);
    settings.saveSplitterState(ui->splitter_3);
    settings.saveColumnWidths(ui->tableViewDB);
    settings.saveColumnWidths(ui->tableViewRotation);
    settings.saveColumnWidths(ui->tableViewQueue);
    settings.saveWindowState(requestsDialog);
    settings.saveWindowState(dbDialog);
    settings.saveWindowState(this);
    settings.sync();
    qInfo() << "Deleting non-owned objects";
    delete ui;
    delete khTmpDir;
    delete requestsDialog;
    qInfo() << "OpenKJ mainwindow destructor complete";
}

void MainWindow::search() {
    karaokeSongsModel.search(ui->lineEdit->text());
}

void MainWindow::databaseUpdated() {
    karaokeSongsModel.loadData();
    search();
    settings.restoreColumnWidths(ui->tableViewDB);
    requestsDialog->databaseUpdateComplete();
    autosizeViews();
    lazyDurationUpdater->stopWork();
    lazyDurationUpdater->deleteLater();
    lazyDurationUpdater = new LazyDurationUpdateController(this);
    connect(lazyDurationUpdater, &LazyDurationUpdateController::gotDuration, &karaokeSongsModel,
            &TableModelKaraokeSongs::setSongDuration);
    lazyDurationUpdater->getDurations();
}

void MainWindow::databaseCleared() {
    lazyDurationUpdater->stopWork();
    karaokeSongsModel.loadData();
    rotModel.loadData();
    qModel.loadSinger(-1);
    ui->tableViewQueue->reset();
    autosizeViews();
    rotationDataChanged();


}

void MainWindow::on_buttonStop_clicked() {
    if (kMediaBackend.state() == MediaBackend::PlayingState) {
        if (settings.showSongPauseStopWarning()) {
            QMessageBox msgBox(this);
            auto *cb = new QCheckBox("Show warning on pause/stop in the future");
            cb->setChecked(settings.showSongPauseStopWarning());
            msgBox.setIcon(QMessageBox::Warning);
            msgBox.setText("Stop currenly playing karaoke song?");
            msgBox.setInformativeText(
                    "There is currently a karaoke song playing.  If you continue, the current song will be stopped.  Are you sure?");
            QPushButton *yesButton = msgBox.addButton(QMessageBox::Yes);
            msgBox.addButton(QMessageBox::Cancel);
            msgBox.setCheckBox(cb);
            connect(cb, &QCheckBox::toggled, &settings, &Settings::setShowSongPauseStopWarning);
            msgBox.exec();
            if (msgBox.clickedButton() != yesButton) {
                return;
            }
        }
    }
    kAASkip = true;
    cdgWindow->showAlert(false);
    kMediaBackend.stop();
//    ipcClient->send_MessageToServer(KhIPCClient::CMD_FADE_IN);
}

void MainWindow::on_buttonPause_clicked() {
    if (kMediaBackend.state() == MediaBackend::PausedState) {
        kMediaBackend.play();
    } else if (kMediaBackend.state() == MediaBackend::PlayingState) {
        if (settings.showSongPauseStopWarning()) {
            QMessageBox msgBox(this);
            auto *cb = new QCheckBox("Show warning on pause/stop in the future");
            cb->setChecked(settings.showSongPauseStopWarning());
            msgBox.setIcon(QMessageBox::Warning);
            msgBox.setText("Pause currenly playing karaoke song?");
            msgBox.setInformativeText(
                    "There is currently a karaoke song playing.  If you continue, the current song will be paused.  Are you sure?");
            QPushButton *yesButton = msgBox.addButton(QMessageBox::Yes);
            msgBox.addButton(QMessageBox::Cancel);
            msgBox.setCheckBox(cb);
            connect(cb, &QCheckBox::toggled, &settings, &Settings::setShowSongPauseStopWarning);
            msgBox.exec();
            if (msgBox.clickedButton() != yesButton) {
                return;
            }
        }
        kMediaBackend.pause();
    }
}

void MainWindow::on_lineEdit_returnPressed() {
    ui->tableViewDB->scrollToTop();
    search();
}

void MainWindow::on_tableViewDB_doubleClicked(const QModelIndex &index) {
    if (settings.dbDoubleClickAddsSong()) {
        auto addSongDlg = new DlgAddSong(&rotModel, &qModel, index.sibling(index.row(), 0).data().toInt(), this);
        connect(addSongDlg, &DlgAddSong::newSingerAdded, [&](auto pos) {
            ui->tableViewRotation->selectRow(pos);
            ui->lineEdit->setFocus();
        });
        addSongDlg->setModal(true);
        addSongDlg->show();
        return;
    }
    if (qModel.getSingerId() >= 0) {
        qModel.add(index.sibling(index.row(), 0).data().toInt());
        updateRotationDuration();
    } else {
        QMessageBox msgBox;
        msgBox.setText("No singer selected.  You must select a singer before you can double-click to add to a queue.");
        msgBox.exec();
    }
}

void MainWindow::on_buttonAddSinger_clicked() {
    dlgAddSinger->show();
}

void MainWindow::on_tableViewRotation_doubleClicked(const QModelIndex &index) {
    if (index.column() <= 3) {
        k2kTransition = false;
        int singerId = index.data(Qt::UserRole).toInt();
        QString nextSongPath = rotModel.nextSongPath(singerId);
        if (nextSongPath != "") {
            if ((kMediaBackend.state() == MediaBackend::PlayingState) && (settings.showSongInterruptionWarning())) {
                QMessageBox msgBox(this);
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
                k2kTransition = true;
            }
            if (kMediaBackend.state() == MediaBackend::PausedState) {
                if (settings.karaokeAutoAdvance()) {
                    kAASkip = true;
                    cdgWindow->showAlert(false);
                }
                kMediaBackend.stop(true);
            }
            //           play(nextSongPath);
            //           kAudioBackend.setPitchShift(rotModel.nextSongKeyChg(singerId));

            curSinger = rotModel.getSingerName(singerId);
            curArtist = rotModel.nextSongArtist(singerId);
            curTitle = rotModel.nextSongTitle(singerId);
            QString curSongId = rotModel.nextSongSongId(singerId);
            int curKeyChange = rotModel.nextSongKeyChg(singerId);

            karaokeSongsModel.updateSongHistory(karaokeSongsModel.getIdForPath(nextSongPath));
            play(nextSongPath, k2kTransition);
            ui->labelArtist->setText(curArtist);
            ui->labelTitle->setText(curTitle);
            ui->labelSinger->setText(curSinger);
            if (settings.treatAllSingersAsRegs() || rotModel.singerIsRegular(singerId))
                historySongsModel.saveSong(curSinger, nextSongPath, curArtist, curTitle, curSongId, curKeyChange);
            kMediaBackend.setPitchShift(curKeyChange);
            qModel.setPlayed(rotModel.nextSongQueueId(singerId));
            rotDelegate.setCurrentSinger(singerId);
            rotModel.setCurrentSinger(singerId);
            if (settings.rotationAltSortOrder()) {
                auto curSingerPos = rotModel.getSingerPosition(singerId);
                m_curSingerOriginalPosition = curSingerPos;
                if (curSingerPos != 0) {
                    rotModel.singerMove(curSingerPos, 0);
                    ui->tableViewRotation->clearSelection();
                    ui->tableViewRotation->selectRow(0);
                }
            }
        }
    }
}

void MainWindow::on_tableViewRotation_clicked(const QModelIndex &index) {
    if (index.column() == TableModelRotation::COL_DELETE) {
        if (settings.showSingerRemovalWarning()) {
            QMessageBox msgBox(this);
            auto *cb = new QCheckBox("Show this warning in the future");
            cb->setChecked(settings.showSingerRemovalWarning());
            msgBox.setIcon(QMessageBox::Warning);
            msgBox.setText("Are you sure you want to remove this singer?");
            msgBox.setInformativeText(
                    "Unless this singer is a tracked regular, you will be unable retrieve any queue data for this singer once they are deleted.");
            QPushButton *yesButton = msgBox.addButton(QMessageBox::Yes);
            msgBox.addButton(QMessageBox::Cancel);
            msgBox.setCheckBox(cb);
            connect(cb, &QCheckBox::toggled, &settings, &Settings::setShowSingerRemovalWarning);
            msgBox.exec();
            if (msgBox.clickedButton() != yesButton) {
                return;
            }
        }
        int singerId = index.data(Qt::UserRole).toInt();
        qInfo() << "Singer id selected: " << singerId;
        qModel.loadSinger(-1);
        if (rotModel.currentSinger() == singerId) {
            rotModel.setCurrentSinger(-1);
            rotDelegate.setCurrentSinger(-1);
        }
        rotModel.singerDelete(singerId);
        ui->tableViewRotation->clearSelection();
        ui->tableViewQueue->clearSelection();
        return;

    }
    if (index.column() == TableModelRotation::COL_REGULAR) {
        if (!rotModel.singerIsRegular(index.data(Qt::UserRole).toInt())) {
            QString name = index.sibling(index.row(), TableModelRotation::COL_NAME).data().toString();
            if (rotModel.historySingerExists(name)) {
                auto answer = QMessageBox::question(this,
                                                    "A regular singer with this name already exists!",
                                                    "There is already a regular singer saved with this name. Would you like to load "
                                                    "the matching regular singer's history for this singer?",
                                                    QMessageBox::StandardButtons(
                                                            QMessageBox::Yes | QMessageBox::Cancel),
                                                    QMessageBox::Cancel
                );
                if (answer == QMessageBox::Yes)
                    rotModel.singerMakeRegular(rotModel.getSingerId(name));
            } else
                rotModel.singerMakeRegular(index.data(Qt::UserRole).toInt());
        } else {
            QMessageBox msgBox(this);
            msgBox.setText("Are you sure you want to disable regular tracking for this singer?");
            msgBox.setInformativeText(
                    "Doing so will not remove the regular singer entry, but it will prevent any changes made to the singer's queue from being saved to the regular singer until the regular singer is either reloaded or the rotation singer is re-merged with the regular singer.");
            QPushButton *yesButton = msgBox.addButton(QMessageBox::Yes);
            msgBox.addButton(QMessageBox::Cancel);
            msgBox.exec();
            if (msgBox.clickedButton() == yesButton) {
                rotModel.singerDisableRegularTracking(index.data(Qt::UserRole).toInt());
            }
        }
    }
}

void MainWindow::on_tableViewQueue_doubleClicked(const QModelIndex &index) {
    k2kTransition = false;
    if (kMediaBackend.state() == MediaBackend::PlayingState) {
        if (settings.showSongInterruptionWarning()) {
            QMessageBox msgBox(this);
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
        k2kTransition = true;
    }
    if (kMediaBackend.state() == MediaBackend::PausedState) {
        if (settings.karaokeAutoAdvance()) {
            kAASkip = true;
            cdgWindow->showAlert(false);
        }
        kMediaBackend.stop(true);
    }
    int curSingerId = qModel.getSingerId();
    curSinger = rotModel.getSingerName(curSingerId);
    curArtist = index.sibling(index.row(), TableModelQueueSongs::COL_ARTIST).data().toString();
    curTitle = index.sibling(index.row(), TableModelQueueSongs::COL_TITLE).data().toString();
    QString curSongId = index.sibling(index.row(), TableModelQueueSongs::COL_SONGID).data().toString();
    QString filePath = index.sibling(index.row(), TableModelQueueSongs::COL_PATH).data().toString();
    int curKeyChange = index.sibling(index.row(), TableModelQueueSongs::COL_KEY).data().toInt();
    ui->labelSinger->setText(curSinger);
    ui->labelArtist->setText(curArtist);
    ui->labelTitle->setText(curTitle);
    karaokeSongsModel.updateSongHistory(karaokeSongsModel.getIdForPath(filePath));
    play(filePath, k2kTransition);
    if (settings.treatAllSingersAsRegs() || rotModel.singerIsRegular(curSingerId))
        historySongsModel.saveSong(curSinger, filePath, curArtist, curTitle, curSongId, curKeyChange);
    kMediaBackend.setPitchShift(curKeyChange);
    qModel.setPlayed(index.sibling(index.row(), TableModelQueueSongs::COL_ID).data().toInt());

    rotModel.setCurrentSinger(curSingerId);
    rotDelegate.setCurrentSinger(curSingerId);
    if (settings.rotationAltSortOrder()) {
        auto curSingerPos = rotModel.getSingerPosition(curSingerId);
        m_curSingerOriginalPosition = curSingerPos;
        if (curSingerPos != 0) {
            rotModel.singerMove(curSingerPos, 0);
            ui->tableViewRotation->clearSelection();
            ui->tableViewRotation->selectRow(0);
        }
    }
}

void MainWindow::on_actionManage_DB_triggered() {
    dbDialog->showNormal();
}

void MainWindow::on_actionExport_Regulars_triggered() {
    auto exportdlg = new DlgRegularExport(karaokeSongsModel, this);
    exportdlg->setModal(true);
    exportdlg->show();
}

void MainWindow::on_actionImport_Regulars_triggered() {
    auto iDialog = new DlgRegularImport(karaokeSongsModel, this);
    iDialog->setModal(true);
    iDialog->show();
}

void MainWindow::on_actionSettings_triggered() {
    auto settingsDialog = new DlgSettings(&kMediaBackend, this);
    settingsDialog->setModal(true);
    connect(settingsDialog, &DlgSettings::audioUseFaderChanged, &kMediaBackend, &MediaBackend::setUseFader);
    connect(settingsDialog, &DlgSettings::audioSilenceDetectChanged, &kMediaBackend,
            &MediaBackend::setUseSilenceDetection);
    connect(settingsDialog, &DlgSettings::audioDownmixChanged, &kMediaBackend, &MediaBackend::setDownmix);
    settingsDialog->show();
}

void MainWindow::on_actionRegulars_triggered() {
    on_buttonRegulars_clicked();
}

void MainWindow::on_actionIncoming_Requests_triggered() {
    requestsDialog->show();
}

void MainWindow::songDroppedOnSinger(const int &singerId, const int &songId, const int &dropRow) {
    qModel.loadSinger(singerId);
    qModel.add(songId);
    ui->tableViewRotation->clearSelection();
    QItemSelectionModel *selmodel = ui->tableViewRotation->selectionModel();
    QModelIndex topLeft;
    QModelIndex bottomRight;
    topLeft = rotModel.index(dropRow, 0, QModelIndex());
    bottomRight = rotModel.index(dropRow, 4, QModelIndex());
    QItemSelection selection(topLeft, bottomRight);
    selmodel->select(selection, QItemSelectionModel::Select);
}

void MainWindow::on_pushButton_clicked() {
    search();
}

void MainWindow::on_tableViewQueue_clicked(const QModelIndex &index) {
    if (index.column() == TableModelQueueSongs::COL_PATH) {
        if ((settings.showQueueRemovalWarning()) &&
            (!qModel.getPlayed(index.sibling(index.row(), TableModelQueueSongs::COL_ID).data().toInt()))) {
            QMessageBox msgBox(this);
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
            if (msgBox.clickedButton() == yesButton) {
                qModel.remove(index.sibling(index.row(), TableModelQueueSongs::COL_ID).data().toInt());
            }
        } else {
            qModel.remove(index.sibling(index.row(), TableModelQueueSongs::COL_ID).data().toInt());
        }
    }
}

void MainWindow::on_buttonClearRotation_clicked() {
    if (m_testMode) {
        settings.setCurrentRotationPosition(-1);
        rotModel.clearRotation();
        rotDelegate.setCurrentSinger(-1);
        qModel.loadSinger(-1);
        return;
    }
    QMessageBox msgBox;
    msgBox.setText("Are you sure?");
    msgBox.setInformativeText(
            "This action will clear all rotation singers and queues. This operation can not be undone.");
    msgBox.setIcon(QMessageBox::Warning);
    msgBox.addButton(QMessageBox::Cancel);
    QPushButton *yesButton = msgBox.addButton(QMessageBox::Yes);
    msgBox.exec();
    if (msgBox.clickedButton() == yesButton) {
        settings.setCurrentRotationPosition(-1);
        rotModel.clearRotation();
        rotDelegate.setCurrentSinger(-1);
        qModel.loadSinger(-1);
    }
}

void MainWindow::on_buttonClearQueue_clicked() {
    if (m_testMode) {
        qModel.removeAll();
        return;
    }
    QMessageBox msgBox;
    msgBox.setText("Are you sure?");
    msgBox.setInformativeText(
            "This action will clear all queued songs for the selected singer.  If the singer is a regular singer, it will delete their saved regular songs as well! This operation can not be undone.");
    msgBox.setIcon(QMessageBox::Warning);
    msgBox.addButton(QMessageBox::Cancel);
    QPushButton *yesButton = msgBox.addButton(QMessageBox::Yes);
    msgBox.exec();
    if (msgBox.clickedButton() == yesButton) {
        qModel.removeAll();
    }
}

void MainWindow::on_spinBoxKey_valueChanged(const int &arg1) {
    kMediaBackend.setPitchShift(arg1);
    if (arg1 > 0)
        ui->spinBoxKey->setPrefix("+");
    else
        ui->spinBoxKey->setPrefix("");
    QTimer::singleShot(20, [&]() {
        ui->spinBoxKey->findChild<QLineEdit *>()->deselect();
    });
}

void MainWindow::karaokeMediaBackend_positionChanged(const qint64 &position) {
    if (kMediaBackend.state() == MediaBackend::PlayingState) {
        if (!sliderPositionPressed) {
            ui->sliderProgress->setMaximum(kMediaBackend.duration());
            ui->sliderProgress->setValue(position);
        }
        ui->labelElapsedTime->setText(MediaBackend::msToMMSS(position));
        ui->labelRemainTime->setText(MediaBackend::msToMMSS(kMediaBackend.duration() - position));
        rotModel.setCurRemainSecs((kMediaBackend.duration() - position) / 1000);
    }
}

void MainWindow::karaokeMediaBackend_durationChanged(const qint64 &duration) {
    ui->labelTotalTime->setText(MediaBackend::msToMMSS(duration));
}

void MainWindow::karaokeMediaBackend_stateChanged(const MediaBackend::State &state) {
    if (m_shuttingDown)
        return;
    //qInfo() << "MainWindow - audioBackend_stateChanged(" << state << ") triggered";
    if (state == MediaBackend::StoppedState) {
        qInfo() << "MainWindow - audio backend state is now STOPPED";
        if (ui->labelTotalTime->text() == "0:00") {
            //qInfo() << "MainWindow - UI is already reset, bailing out";
            return;
        }
        qInfo() << "KAudio entered StoppedState";
        if (k2kTransition)
            return;
        ui->labelArtist->setText("None");
        ui->labelTitle->setText("None");
        ui->labelSinger->setText("None");
        ui->labelElapsedTime->setText("0:00");
        ui->labelRemainTime->setText("0:00");
        ui->labelTotalTime->setText("0:00");
        ui->sliderProgress->setValue(0);
        ui->spinBoxTempo->setValue(100);
        ui->spinBoxKey->setValue(0);
        ui->pushButtonKeyDn->setEnabled(false);
        ui->pushButtonKeyUp->setEnabled(false);
        ui->pushButtonTempoDn->setEnabled(false);
        ui->pushButtonTempoUp->setEnabled(false);
        if (state == m_lastAudioState)
            return;
        m_lastAudioState = state;
        if (settings.karaokeAutoAdvance()) {
            qInfo() << " - Karaoke Autoplay is enabled";
            if (kAASkip) {
                kAASkip = false;
                qInfo() << " - Karaoke Autoplay set to skip, bailing out";
            } else {
                int nextSinger = -1;
                QString nextSongPath;
                bool empty = false;

                int curSingerId = rotModel.currentSinger();

                int curPos = rotModel.getSingerPosition(curSingerId);
                if (settings.rotationAltSortOrder())
                    curPos = m_curSingerOriginalPosition;
                if (curSingerId == -1)
                    curPos = rotModel.rowCount() - 1;
                int loops = 0;
                while ((nextSongPath == "") && (!empty)) {
                    if (loops > rotModel.rowCount()) {
                        empty = true;
                    } else {
                        if (++curPos >= rotModel.rowCount()) {
                            curPos = 0;
                        }
                        nextSinger = rotModel.singerIdAtPosition(curPos);
                        nextSongPath = rotModel.nextSongPath(nextSinger);
                        loops++;
                    }
                }
                if (empty)
                    qInfo() << "KaraokeAA - No more songs to play, giving up";
                else {
                    kAANextSinger = nextSinger;
                    kAANextSongPath = nextSongPath;
                    qInfo() << "KaraokeAA - Will play: " << rotModel.getSingerName(nextSinger) << " - " << nextSongPath;
                    qInfo() << "KaraokeAA - Starting " << settings.karaokeAATimeout() << " second timer";
                    m_timerKaraokeAA.start(settings.karaokeAATimeout() * 1000);
                    cdgWindow->setNextSinger(rotModel.getSingerName(nextSinger));
                    cdgWindow->setNextSong(
                            rotModel.nextSongArtist(nextSinger) + " - " + rotModel.nextSongTitle(nextSinger));
                    cdgWindow->setCountdownSecs(settings.karaokeAATimeout());
                    cdgWindow->showAlert(true);
                }
            }
        }
        if (settings.rotationAltSortOrder()) {
            rotModel.singerMove(0, rotModel.rowCount() - 1);
            rotModel.setCurrentSinger(-1);
            rotDelegate.setCurrentSinger(-1);
            ui->tableViewRotation->clearSelection();
            ui->tableViewRotation->selectRow(0);
            rotModel.setCurRemainSecs(0);
        }
    }
    if (state == MediaBackend::EndOfMediaState) {
        qInfo() << "KAudio entered EndOfMediaState";
//        ipcClient->send_MessageToServer(KhIPCClient::CMD_FADE_IN);
        kMediaBackend.stop(true);
    }
    if (state == MediaBackend::PausedState) {
        qInfo() << "KAudio entered PausedState";
    }
    if (state == MediaBackend::PlayingState) {
        qInfo() << "KAudio entered PlayingState";
        m_lastAudioState = state;
        ui->pushButtonKeyUp->setEnabled(true);
        ui->pushButtonKeyDn->setEnabled(true);
        ui->pushButtonTempoDn->setEnabled(true);
        ui->pushButtonTempoUp->setEnabled(true);
    }
    if (state == MediaBackend::UnknownState) {
        qInfo() << "KAudio entered UnknownState";
    }
    rotationDataChanged();
}

void MainWindow::sfxAudioBackend_positionChanged(const qint64 &position) {
    ui->sliderSfxPos->setValue(position);
}

void MainWindow::sfxAudioBackend_durationChanged(const qint64 &duration) {
    ui->sliderSfxPos->setMaximum(duration);
}

void MainWindow::sfxAudioBackend_stateChanged(const MediaBackend::State &state) {
    if (state == MediaBackend::EndOfMediaState) {
        ui->sliderSfxPos->setValue(0);
        sfxMediaBackend.stop();
    }
    if (state == MediaBackend::StoppedState || state == MediaBackend::UnknownState)
        ui->sliderSfxPos->setValue(0);
}

void MainWindow::hasActiveVideoChanged() {
    ui->videoPreview->setHasActiveVideo(m_kHasActiveVideo);
    cdgWindow->getVideoDisplay()->setHasActiveVideo(m_kHasActiveVideo);
    if (m_timerKaraokeAA.isActive() && settings.karaokeAAAlertEnabled())
        return;
    cdgWindow->getVideoDisplay()->show();
    ui->videoPreview->show();
}

void MainWindow::on_buttonRegulars_clicked() {
    m_dlgRegularSingers.setVisible(!m_dlgRegularSingers.isVisible());
}

void MainWindow::rotationDataChanged() {
    if (m_shuttingDown)
        return;
    if (settings.rotationShowNextSong())
        resizeRotation();
    updateRotationDuration();
    QString sep = "•";
    requestsDialog->rotationChanged();
    QString statusBarText = "Singers: ";
    statusBarText += QString::number(rotModel.rowCount());
    labelSingerCount.setText(statusBarText);
    QString tickerText;
    if (settings.tickerCustomString() != "") {
        tickerText += settings.tickerCustomString() + " " + sep + " ";
        QString cs = rotModel.getSingerName(rotModel.currentSinger());
        int nsPos;
        if (cs == "") {
            cs = rotModel.getSingerName(rotModel.singerIdAtPosition(0));
            if (cs == "")
                cs = "[nobody]";
            nsPos = 0;
        } else
            nsPos = rotModel.getSingerPosition(rotModel.currentSinger());
        QString ns = "[nobody]";
        if (rotModel.rowCount() > 0) {
            if (nsPos + 1 < rotModel.rowCount())
                nsPos++;
            else
                nsPos = 0;
            ns = rotModel.getSingerName(rotModel.singerIdAtPosition(nsPos));
        }
        tickerText.replace("%cs", cs);
        tickerText.replace("%ns", ns);
        tickerText.replace("%rc", QString::number(rotModel.rowCount()));
        if (ui->labelArtist->text() == "None" && ui->labelTitle->text() == "None")
            tickerText.replace("%curSong", "None");
        else
            tickerText.replace("%curSong", ui->labelArtist->text() + " - " + ui->labelTitle->text());
        tickerText.replace("%curArtist", ui->labelArtist->text());
        tickerText.replace("%curTitle", ui->labelTitle->text());
        tickerText.replace("%curSinger", cs);
        tickerText.replace("%nextSinger", ns);

    }
    if (settings.tickerShowRotationInfo()) {
        tickerText += "Singers: ";
        tickerText += QString::number(rotModel.rowCount());
        tickerText += " " + sep + " Current: ";
        int displayPos;
        QString curSingerName = rotModel.getSingerName(rotModel.currentSinger());
        if (curSingerName != "") {
            tickerText += curSingerName;
            displayPos = rotModel.getSingerPosition(rotModel.currentSinger());
        } else {
            tickerText += "None ";
            displayPos = -1;
        }
        int listSize;
        if (settings.tickerFullRotation() || (rotModel.rowCount() < settings.tickerShowNumSingers())) {
            if (curSingerName == "")
                listSize = rotModel.rowCount();
            else
                listSize = rotModel.rowCount() - 1;
            if (listSize > 0)
                tickerText += " " + sep + " Upcoming: ";
        } else {
            listSize = settings.tickerShowNumSingers();
            tickerText += " " + sep + " Next ";
            tickerText += QString::number(settings.tickerShowNumSingers());
            tickerText += " Singers: ";
        }
        for (int i = 0; i < listSize; i++) {
            if (displayPos + 1 < rotModel.rowCount())
                displayPos++;
            else
                displayPos = 0;
            tickerText += QString::number(i + 1);
            tickerText += ") ";
            tickerText += rotModel.getSingerName(rotModel.singerIdAtPosition(displayPos));
            if (i < listSize - 1)
                tickerText += " ";
        }
        // tickerText += "|";
    }
    cdgWindow->setTickerText(tickerText);
}

void MainWindow::silenceDetectedKar() {
    qInfo() << "Karaoke music silence detected";
    kMediaBackend.rawStop();
    if (settings.karaokeAutoAdvance())
        kAASkip = false;
//        ipcClient->send_MessageToServer(KhIPCClient::CMD_FADE_IN);
}


void MainWindow::on_tableViewDB_customContextMenuRequested(const QPoint &pos) {
    QModelIndex index = ui->tableViewDB->indexAt(pos);
    if (index.isValid()) {
        dbRtClickFile = karaokeSongsModel.getPath(
                index.sibling(index.row(), TableModelKaraokeSongs::COL_ID).data().toInt());
        QMenu contextMenu(this);
        contextMenu.addAction("Preview", this, &MainWindow::previewCdg);
        contextMenu.addSeparator();
        contextMenu.addAction("Edit", this, &MainWindow::editSong);
        contextMenu.addAction("Mark bad", this, &MainWindow::markSongBad);
        contextMenu.exec(QCursor::pos());
    }
}

void MainWindow::on_tableViewRotation_customContextMenuRequested(const QPoint &pos) {
    QModelIndex index = ui->tableViewRotation->indexAt(pos);
    if (index.isValid()) {
        m_rtClickRotationSingerId = index.data(Qt::UserRole).toInt();
        QMenu contextMenu(this);
        if (ui->tableViewRotation->selectionModel()->selectedRows().size() > 1) {
            contextMenu.addAction("Delete", m_shortcutManager->deleteSingerShortcut(), &QShortcut::activated);
        } else {
            contextMenu.addAction("Rename", this, &MainWindow::renameSinger);
        }
        contextMenu.exec(QCursor::pos());
    }
}

void MainWindow::sfxButton_customContextMenuRequested([[maybe_unused]]const QPoint &pos) {
    auto *btn = (SoundFxButton *) sender();
    lastRtClickedSfxBtn.path = btn->buttonData().toString();
    lastRtClickedSfxBtn.name = btn->text();
    QMenu contextMenu(this);
    contextMenu.addAction("Remove", this, &MainWindow::removeSfxButton);
    contextMenu.exec(QCursor::pos());
}

void MainWindow::renameSinger() {
    bool ok;
    QString currentName = rotModel.getSingerName(m_rtClickRotationSingerId);
    QString name = QInputDialog::getText(this, "Rename singer", "New name:", QLineEdit::Normal, currentName, &ok);
    if (ok && !name.isEmpty()) {
        if ((name.toLower() == currentName.toLower() && name != currentName) || !rotModel.singerExists(name)) {
            rotModel.singerSetName(m_rtClickRotationSingerId, name);
        } else if (rotModel.singerExists(name)) {
            QMessageBox::warning(this, "Singer exists!", "A singer named " + name +
                                                         " already exists. Please choose a unique name and try again. The operation has been cancelled.",
                                 QMessageBox::Ok);
        }

    }
}


void MainWindow::on_tableViewQueue_customContextMenuRequested(const QPoint &pos) {
    int selCount = ui->tableViewQueue->selectionModel()->selectedRows().size();
    if (selCount == 1) {
        QModelIndex index = ui->tableViewQueue->indexAt(pos);
        if (index.isValid()) {
            dbRtClickFile = index.sibling(index.row(), TableModelQueueSongs::COL_PATH).data().toString();
            m_rtClickQueueSongId = index.sibling(index.row(), 0).data().toInt();
            dlgKeyChange->setActiveSong(m_rtClickQueueSongId);
            QMenu contextMenu(this);
            contextMenu.addAction("Preview", this, &MainWindow::previewCdg);
            contextMenu.addSeparator();
            contextMenu.addAction("Set Key Change", this, &MainWindow::setKeyChange);
            contextMenu.addAction("Toggle played", this, &MainWindow::toggleQueuePlayed);
            contextMenu.addSeparator();
            contextMenu.addAction("Delete", m_shortcutManager->deleteSongShortcut(), &QShortcut::activated);
            contextMenu.exec(QCursor::pos());
        }
    } else if (selCount > 1) {
        QMenu contextMenu(this);
        contextMenu.addAction("Set Played", this, &MainWindow::setMultiPlayed);
        contextMenu.addAction("Set Unplayed", this, &MainWindow::setMultiUnplayed);
        contextMenu.addSeparator();
        contextMenu.addAction("Delete", m_shortcutManager->deleteSongShortcut(), &QShortcut::activated);

        contextMenu.exec(QCursor::pos());
    }
}

void MainWindow::on_sliderProgress_sliderPressed() {
    sliderPositionPressed = true;
}

void MainWindow::on_sliderProgress_sliderReleased() {
    kMediaBackend.setPosition(ui->sliderProgress->value());
    sliderPositionPressed = false;
}

void MainWindow::setKeyChange() {
    dlgKeyChange->show();
}

void MainWindow::toggleQueuePlayed() {
    qModel.setPlayed(m_rtClickQueueSongId, !qModel.getPlayed(m_rtClickQueueSongId));
    updateRotationDuration();
}

void MainWindow::previewCdg() {
    if (!QFile::exists(dbRtClickFile)) {
        QMessageBox::warning(this, tr("Missing File!"),
                             "Specified karaoke file missing, preview aborted!\n\n" + dbRtClickFile, QMessageBox::Ok);
        return;
    }
    auto *videoPreview = new DlgVideoPreview(dbRtClickFile, this);
    videoPreview->setAttribute(Qt::WA_DeleteOnClose);
    videoPreview->show();
//    DlgCdgPreview *cdgPreviewDialog = new DlgCdgPreview(this);
//    cdgPreviewDialog->setAttribute(Qt::WA_DeleteOnClose);
//    cdgPreviewDialog->setSourceFile(dbRtClickFile);
//    cdgPreviewDialog->preview();
}

void MainWindow::editSong() {
    bool isCdg = false;
    if (QFileInfo(dbRtClickFile).suffix().toLower() == "cdg")
        isCdg = true;
    QString mediaFile;
    if (isCdg)
        mediaFile = DbUpdateThread::findMatchingAudioFile(dbRtClickFile);
    TableModelKaraokeSourceDirs model;
    SourceDir srcDir = model.getDirByPath(dbRtClickFile);
    int rowId;
    QString artist;
    QString title;
    QString songId;
    QSqlQuery query;
    query.prepare("SELECT songid,artist,title,discid FROM dbsongs WHERE path = :path LIMIT 1");
    query.bindValue(":path", dbRtClickFile);
    query.exec();
    if (!query.next())
        qInfo() << "Unable to find song in db!";
    artist = query.value("artist").toString();
    title = query.value("title").toString();
    songId = query.value("discid").toString();
    rowId = query.value("songid").toInt();
    qInfo() << "db song match: " << rowId << ": " << artist << " - " << title << " - " << songId;
    bool allowRename = true;
    bool showSongId = true;
    if (srcDir.getPattern() == SourceDir::AT || srcDir.getPattern() == SourceDir::TA)
        showSongId = false;
    if (srcDir.getPattern() == SourceDir::CUSTOM || srcDir.getPattern() == SourceDir::METADATA)
        allowRename = false;
    if (srcDir.getIndex() == -1) {
        allowRename = false;
        QMessageBox msgBoxErr;
        msgBoxErr.setText("Unable to find a configured source path containing the file.");
        msgBoxErr.setInformativeText(
                "You won't be able to rename the file.  To fix this, ensure that a source directory is configured in the database settings which contains this file.");
        msgBoxErr.setStandardButtons(QMessageBox::Ok);
        msgBoxErr.exec();
    }
    DlgEditSong dlg(artist, title, songId, showSongId, allowRename, this);
    int result = dlg.exec();
    if (result != QDialog::Accepted)
        return;
    if (artist == dlg.artist() && title == dlg.title() && songId == dlg.songId())
        return;
    if (dlg.renameFile()) {
        if (!QFileInfo(dbRtClickFile).isWritable()) {
            QMessageBox msgBoxErr;
            msgBoxErr.setText("Unable to rename file");
            msgBoxErr.setInformativeText(
                    "Unable to rename file, your user does not have write permissions. Operation cancelled.");
            msgBoxErr.setStandardButtons(QMessageBox::Ok);
            msgBoxErr.exec();
            return;
        }
        if (isCdg) {
            if (!QFileInfo(mediaFile).isWritable()) {
                QMessageBox msgBoxErr;
                msgBoxErr.setText("Unable to rename file");
                msgBoxErr.setInformativeText(
                        "Unable to rename file, your user does not have write permissions. Operation cancelled.");
                msgBoxErr.setStandardButtons(QMessageBox::Ok);
                msgBoxErr.exec();
                return;
            }
        }
        QString newFn;
        QString newMediaFn;
        bool unsupported = false;
        switch (srcDir.getPattern()) {
            case SourceDir::SAT:
                newFn = dlg.songId() + " - " + dlg.artist() + " - " + dlg.title() + "." +
                        QFileInfo(dbRtClickFile).suffix();
                if (isCdg)
                    newMediaFn = dlg.songId() + " - " + dlg.artist() + " - " + dlg.title() + "." +
                                 QFileInfo(mediaFile).suffix();
                break;
            case SourceDir::STA:
                newFn = dlg.songId() + " - " + dlg.title() + " - " + dlg.artist() + "." +
                        QFileInfo(dbRtClickFile).suffix();
                if (isCdg)
                    newMediaFn = dlg.songId() + " - " + dlg.title() + " - " + dlg.artist() + "." +
                                 QFileInfo(mediaFile).suffix();
                break;
            case SourceDir::ATS:
                newFn = dlg.artist() + " - " + dlg.title() + " - " + dlg.songId() + "." +
                        QFileInfo(dbRtClickFile).suffix();
                if (isCdg)
                    newMediaFn = dlg.artist() + " - " + dlg.title() + " - " + dlg.songId() + "." +
                                 QFileInfo(mediaFile).suffix();
                break;
            case SourceDir::TAS:
                newFn = dlg.title() + " - " + dlg.artist() + " - " + dlg.songId() + "." +
                        QFileInfo(dbRtClickFile).suffix();
                if (isCdg)
                    newMediaFn = dlg.title() + " - " + dlg.artist() + " - " + dlg.songId() + "." +
                                 QFileInfo(mediaFile).suffix();
                break;
            case SourceDir::S_T_A:
                newFn = dlg.songId() + "_" + dlg.title() + "_" + dlg.artist() + "." +
                        QFileInfo(dbRtClickFile).suffix();
                if (isCdg)
                    newMediaFn = dlg.songId() + "_" + dlg.title() + "_" + dlg.artist() + "." +
                                 QFileInfo(mediaFile).suffix();
                break;
            case SourceDir::AT:
                newFn = dlg.artist() + " - " + dlg.title() + "." + QFileInfo(dbRtClickFile).suffix();
                if (isCdg)
                    newMediaFn = dlg.artist() + " - " + dlg.title() + "." + QFileInfo(mediaFile).suffix();
                break;
            case SourceDir::TA:
                newFn = dlg.title() + " - " + dlg.artist() + "." + QFileInfo(dbRtClickFile).suffix();
                if (isCdg)
                    newMediaFn = dlg.title() + " - " + dlg.artist() + "." + QFileInfo(mediaFile).suffix();
                break;
            case SourceDir::CUSTOM:
            case SourceDir::METADATA:
                unsupported = true;
                break;
        }
        if (unsupported) {
            QMessageBox msgBoxErr;
            msgBoxErr.setText("Unable to rename file");
            msgBoxErr.setInformativeText(
                    "Unable to rename file, renaming custom and metadata based source files is not supported. Operation cancelled.");
            msgBoxErr.setStandardButtons(QMessageBox::Ok);
            msgBoxErr.exec();
            return;
        }
        if (QFile::exists(newFn)) {
            QMessageBox msgBoxErr;
            msgBoxErr.setText("Unable to rename file");
            msgBoxErr.setInformativeText(
                    "Unable to rename file, a file by that name already exists in the same directory. Operation cancelled.");
            msgBoxErr.setStandardButtons(QMessageBox::Ok);
            msgBoxErr.exec();
            return;
        }
        if (isCdg) {
            if (QFile::exists(newMediaFn)) {
                QMessageBox msgBoxErr;
                msgBoxErr.setText("Unable to rename file");
                msgBoxErr.setInformativeText(
                        "Unable to rename file, a file by that name already exists in the same directory. Operation cancelled.");
                msgBoxErr.setStandardButtons(QMessageBox::Ok);
                msgBoxErr.exec();
                return;
            }
        }
        QString newFilePath = QFileInfo(dbRtClickFile).absoluteDir().absolutePath() + "/" + newFn;
        if (newFilePath != dbRtClickFile) {
            if (!QFile::rename(dbRtClickFile, QFileInfo(dbRtClickFile).absoluteDir().absolutePath() + "/" + newFn)) {
                QMessageBox msgBoxErr;
                msgBoxErr.setText("Error while renaming file!");
                msgBoxErr.setInformativeText("An unknown error occurred while renaming the file. Operation cancelled.");
                msgBoxErr.setStandardButtons(QMessageBox::Ok);
                msgBoxErr.exec();
                return;
            }
            if (isCdg) {
                if (!QFile::rename(mediaFile,
                                   QFileInfo(dbRtClickFile).absoluteDir().absolutePath() + "/" + newMediaFn)) {
                    QMessageBox msgBoxErr;
                    msgBoxErr.setText("Error while renaming file!");
                    msgBoxErr.setInformativeText(
                            "An unknown error occurred while renaming the file. Operation cancelled.");
                    msgBoxErr.setStandardButtons(QMessageBox::Ok);
                    msgBoxErr.exec();
                    return;
                }
            }
        }
        qInfo() << "New filename: " << newFn;
        query.prepare(
                "UPDATE dbsongs SET artist = :artist, title = :title, discid = :songid, path = :path, filename = :filename, searchstring = :searchstring WHERE songid = :rowid");
        QString newArtist = dlg.artist();
        QString newTitle = dlg.title();
        QString newSongId = dlg.songId();
        QString newPath = QFileInfo(dbRtClickFile).absoluteDir().absolutePath() + "/" + newFn;
        QString newSearchString =
                QFileInfo(newPath).completeBaseName() + " " + newArtist + " " + newTitle + " " + newSongId;
        query.bindValue(":artist", newArtist);
        query.bindValue(":title", newTitle);
        query.bindValue(":songid", newSongId);
        query.bindValue(":path", newPath);
        query.bindValue(":filename", newFn);
        query.bindValue(":searchstring", newSearchString);
        query.bindValue(":rowid", rowId);
        query.exec();
        qInfo() << query.lastError();
        if (!query.lastError().isValid()) {
            query.prepare(
                    "UPDATE mem.dbsongs SET artist = :artist, title = :title, discid = :songid, path = :path, filename = :filename, searchstring = :searchstring WHERE songid = :rowid");
            query.bindValue(":artist", newArtist);
            query.bindValue(":title", newTitle);
            query.bindValue(":songid", newSongId);
            query.bindValue(":path", newPath);
            query.bindValue(":filename", newFn);
            query.bindValue(":searchstring", newSearchString);
            query.bindValue(":rowid", rowId);
            query.exec();
            QMessageBox msgBoxInfo;
            msgBoxInfo.setText("Edit successful");
            msgBoxInfo.setInformativeText("The file has been renamed and the database has been updated successfully.");
            msgBoxInfo.setStandardButtons(QMessageBox::Ok);
            msgBoxInfo.exec();
            karaokeSongsModel.loadData();
            return;
        } else {
            QMessageBox msgBoxErr;
            msgBoxErr.setText("Error while updating the database!");
            msgBoxErr.setInformativeText(query.lastError().text());
            msgBoxErr.setStandardButtons(QMessageBox::Ok);
            msgBoxErr.exec();
            return;
            //QFile::rename(QFileInfo(dbRtClickFile).absoluteDir().absolutePath() + "/" + newFn, dbRtClickFile);
        }
    } else {
        query.prepare(
                "UPDATE dbsongs SET artist = :artist, title = :title, discid = :songid, searchstring = :searchstring WHERE songid = :rowid");
        QString newArtist = dlg.artist();
        QString newTitle = dlg.title();
        QString newSongId = dlg.songId();
        QString newSearchString =
                QFileInfo(dbRtClickFile).completeBaseName() + " " + newArtist + " " + newTitle + " " + newSongId;
        query.bindValue(":artist", newArtist);
        query.bindValue(":title", newTitle);
        query.bindValue(":songid", newSongId);
        query.bindValue(":searchstring", newSearchString);
        query.bindValue(":rowid", rowId);
        query.exec();
        qInfo() << query.lastError();
        if (!query.lastError().isValid()) {
            query.prepare(
                    "UPDATE mem.dbsongs SET artist = :artist, title = :title, discid = :songid, searchstring = :searchstring WHERE songid = :rowid");
            query.bindValue(":artist", newArtist);
            query.bindValue(":title", newTitle);
            query.bindValue(":songid", newSongId);
            query.bindValue(":searchstring", newSearchString);
            query.bindValue(":rowid", rowId);
            query.exec();
            QMessageBox msgBoxInfo;
            msgBoxInfo.setText("Edit successful");
            msgBoxInfo.setInformativeText("The database has been updated successfully.");
            msgBoxInfo.setStandardButtons(QMessageBox::Ok);
            msgBoxInfo.exec();
            karaokeSongsModel.loadData();
            return;
        } else {
            QMessageBox msgBoxErr;
            msgBoxErr.setText("Error while updating the database!");
            msgBoxErr.setInformativeText(query.lastError().text());
            msgBoxErr.setStandardButtons(QMessageBox::Ok);
            msgBoxErr.exec();
            return;
        }
    }

}

void MainWindow::markSongBad() {
    QMessageBox msgBox;
    QMessageBox msgBoxResult;
    msgBox.setText("Marking song as bad");
    msgBox.setIcon(QMessageBox::Question);
    msgBox.setInformativeText("Would you like mark the file as bad in the DB, or remove it from disk permanently?");
    auto markBadButton = msgBox.addButton(tr("Mark Bad"), QMessageBox::ActionRole);
    auto removeFileButton = msgBox.addButton(tr("Remove File"), QMessageBox::ActionRole);
    auto cancelButton = msgBox.addButton(QMessageBox::Cancel);
    msgBox.exec();
    if (msgBox.clickedButton() == markBadButton) {
        karaokeSongsModel.markSongBad(dbRtClickFile);
        msgBoxResult.setText("File marked as bad and will no longer show up in searches.");
        msgBoxResult.setIcon(QMessageBox::Information);
        msgBoxResult.exec();
    } else if (msgBox.clickedButton() == removeFileButton) {
        bool isCdg = false;
        if (QFileInfo(dbRtClickFile).suffix().toLower() == "cdg")
            isCdg = true;
        QString mediaFile;
        if (isCdg)
            mediaFile = DbUpdateThread::findMatchingAudioFile(dbRtClickFile);
        QFile file(dbRtClickFile);
        auto ret = karaokeSongsModel.removeBadSong(dbRtClickFile);
        switch (ret) {
            case TableModelKaraokeSongs::DELETE_OK:
                msgBoxResult.setText("File removed successfully");
                msgBoxResult.setIcon(QMessageBox::Information);
                msgBoxResult.exec();
                break;
            case TableModelKaraokeSongs::DELETE_FAIL:
                msgBoxResult.setText("Error while removing file");
                msgBoxResult.setIcon(QMessageBox::Warning);
                msgBoxResult.setInformativeText(
                        "Unable to remove the file.  Please check file permissions.\nOperation cancelled.");
                msgBoxResult.exec();
                break;
            case TableModelKaraokeSongs::DELETE_CDG_AUDIO_FAIL:
                msgBoxResult.setText("Error while removing file");
                msgBoxResult.setIcon(QMessageBox::Warning);
                msgBoxResult.setInformativeText(
                        "The cdg file was deleted, but there was an error while deleting the matching media file.  You will need to manually remove the file.");
                msgBoxResult.exec();
                break;
        }
    }
}

void MainWindow::karaokeAATimerTimeout() {
    qInfo() << "KaraokeAA - timer timeout";
    m_timerKaraokeAA.stop();
    cdgWindow->showAlert(false);
    if (kAASkip) {
        qInfo() << "KaraokeAA - Aborted via stop button";
        kAASkip = false;
    } else {
        curSinger = rotModel.getSingerName(kAANextSinger);
        curArtist = rotModel.nextSongArtist(kAANextSinger);
        curTitle = rotModel.nextSongTitle(kAANextSinger);
        ui->labelArtist->setText(curArtist);
        ui->labelTitle->setText(curTitle);
        ui->labelSinger->setText(curSinger);
        if (settings.treatAllSingersAsRegs() || rotModel.singerIsRegular(kAANextSinger)) {
            historySongsModel.saveSong(
                    curSinger,
                    kAANextSongPath,
                    curArtist,
                    curTitle,
                    rotModel.nextSongSongId(kAANextSinger),
                    rotModel.nextSongKeyChg(kAANextSinger)
            );
        }
        karaokeSongsModel.updateSongHistory(karaokeSongsModel.getIdForPath(kAANextSongPath));
        play(kAANextSongPath);
        kMediaBackend.setPitchShift(rotModel.nextSongKeyChg(kAANextSinger));
        qModel.setPlayed(rotModel.nextSongQueueId(kAANextSinger));
        rotModel.setCurrentSinger(kAANextSinger);
        rotDelegate.setCurrentSinger(kAANextSinger);
        if (settings.rotationAltSortOrder()) {
            auto curSingerPos = rotModel.getSingerPosition(kAANextSinger);
            m_curSingerOriginalPosition = curSingerPos;
            if (curSingerPos != 0)
                rotModel.singerMove(curSingerPos, 0);
        }
    }
}

void MainWindow::timerButtonFlashTimeout() {
    static QString normalSS = " \
        QPushButton { \
            border: 2px solid #8f8f91; \
            border-radius: 6px; \
            background-color: qlineargradient(x1: 0, y1: 0, x2: 0, y2: 1, stop: 0 #f6f7fa, stop: 1 #dadbde); \
            min-width: 80px; \
            padding-left: 5px; \
            padding-right: 5px; \
            padding-top: 3px; \
            padding-bottom: 3px; \
        } \
        QPushButton:pressed { \
            background-color: qlineargradient(x1: 0, y1: 0, x2: 0, y2: 1, stop: 0 #dadbde, stop: 1 #f6f7fa); \
        } \
        QPushButton:flat { \
            border: none; /* no border for a flat push button */ \
        } \
        QPushButton:default { \
            border-color: navy; /* make the default button prominent */ \
        }";

    static QString blinkSS = " \
        QPushButton { \
            border: 2px solid #8f8f91; \
            border-radius: 6px; \
            background-color: qlineargradient(x1: 0, y1: 0, x2: 0, y2: 1, stop: 0 #f6f700, stop: 1 #dadb00); \
            min-width: 80px; \
            padding-left: 5px; \
            padding-right: 5px; \
            padding-top: 3px; \
            padding-bottom: 3px; \
        } \
        QPushButton:pressed { \
            background-color: qlineargradient(x1: 0, y1: 0, x2: 0, y2: 1, stop: 0 #dadbde, stop: 1 #f6f7fa); \
        } \
        QPushButton:flat { \
            border: none; /* no border for a flat push button */ \
        } \
        QPushButton:default { \
            border-color: navy; /* make the default button prominent */ \
        } \
    ";

    if (requestsDialog->numRequests() > 0) {
        static bool flashed = false;
        if (settings.theme() != 0) {
            auto normal = this->palette().button().color();
            auto blink = QColor("yellow");
            auto blinkTxt = QColor("black");
            auto normalTxt = this->palette().buttonText().color();
            auto palette = QPalette(ui->pushButtonIncomingRequests->palette());
            palette.setColor(QPalette::Button, (flashed) ? normal : blink);
            palette.setColor(QPalette::ButtonText, (flashed) ? normalTxt : blinkTxt);
            ui->pushButtonIncomingRequests->setPalette(palette);
            ui->pushButtonIncomingRequests->setText(
                    " Requests (" + QString::number(requestsDialog->numRequests()) + ") ");
            flashed = !flashed;
        } else {
            ui->pushButtonIncomingRequests->setText(
                    " Requests (" + QString::number(requestsDialog->numRequests()) + ") ");
            ui->pushButtonIncomingRequests->setStyleSheet((flashed) ? normalSS : blinkSS);
            flashed = !flashed;
        }
        update();
    } else if (ui->pushButtonIncomingRequests->text() != "Requests") {
        if (settings.theme() != 0) {
            ui->pushButtonIncomingRequests->setPalette(this->palette());
            ui->pushButtonIncomingRequests->setText("Requests");
        } else {
            ui->pushButtonIncomingRequests->setStyleSheet(normalSS);
            ui->pushButtonIncomingRequests->setText(" Requests ");
        }
        update();
    }
}



















void MainWindow::on_actionShow_Debug_Log_toggled(const bool &arg1) {
    debugDialog->setVisible(arg1);
    settings.setLogVisible(arg1);
}

void MainWindow::on_actionManage_Karaoke_DB_triggered() {
    dbDialog->showNormal();
}






void MainWindow::on_actionAbout_triggered() {
    QString title;
    QString text;
    QString date = QString::fromLocal8Bit(__DATE__) + " " + QString(__TIME__);
    title = "About OpenKJ";
    text = "OpenKJ\n\nVersion: " + QString(OKJ_VERSION_STRING) + " " + QString(OKJ_VERSION_BRANCH) + "\nBuilt: " +
           date + "\nLicense: GPL v3+";
    QMessageBox::about(this, title, text);
}

void MainWindow::on_pushButtonMplxLeft_toggled(const bool &checked) {
    if (checked)
        settings.setMplxMode(Multiplex_LeftChannel);
}

void MainWindow::on_pushButtonMplxBoth_toggled(const bool &checked) {
    if (checked)
        settings.setMplxMode(Multiplex_Normal);
}

void MainWindow::on_pushButtonMplxRight_toggled(const bool &checked) {
    if (checked)
        settings.setMplxMode(Multiplex_RightChannel);
}

void MainWindow::on_lineEdit_textChanged(const QString &arg1) {
    if (!settings.progressiveSearchEnabled())
        return;
    ui->tableViewDB->scrollToTop();
    static QString lastVal;
    if (arg1.trimmed() != lastVal) {
        karaokeSongsModel.search(arg1);
        lastVal = arg1.trimmed();
    }
}

void MainWindow::setMultiPlayed() {
    QModelIndexList indexes = ui->tableViewQueue->selectionModel()->selectedIndexes();
    QModelIndex index;

            foreach(index, indexes) {
            if (index.column() == 0) {
                int queueId = index.sibling(index.row(), 0).data().toInt();
                qInfo() << "Selected row: " << index.row() << " queueId: " << queueId;
                qModel.setPlayed(queueId);
            }
        }
}

void MainWindow::setMultiUnplayed() {
    QModelIndexList indexes = ui->tableViewQueue->selectionModel()->selectedIndexes();
    QModelIndex index;

            foreach(index, indexes) {
            if (index.column() == 0) {
                int queueId = index.sibling(index.row(), 0).data().toInt();
                qInfo() << "Selected row: " << index.row() << " queueId: " << queueId;
                qModel.setPlayed(queueId, false);
            }
        }
}

void MainWindow::on_spinBoxTempo_valueChanged(const int &arg1) {
    kMediaBackend.setTempo(arg1);
    QTimer::singleShot(20, [&]() {
        ui->spinBoxTempo->findChild<QLineEdit *>()->deselect();
    });
}

void MainWindow::on_actionSongbook_Generator_triggered() {
    dlgBookCreator->show();
}

void MainWindow::on_actionEqualizer_triggered() {
    dlgEq->show();
}

void MainWindow::audioError(const QString &msg) {
    QMessageBox msgBox;
    msgBox.setTextFormat(Qt::RichText);
    msgBox.setText("Audio playback error! - " + msg);
    msgBox.setIcon(QMessageBox::Warning);
    msgBox.exec();
}

void MainWindow::closeEvent(QCloseEvent *event) {
    if (kMediaBackend.state() == MediaBackend::PlayingState) {
        QMessageBox msgBox(this);
        msgBox.setIcon(QMessageBox::Warning);
        msgBox.setText("Are you sure you want to exit?");
        msgBox.setInformativeText(
                "There is currently a karaoke song playing.  If you continue, the current song will be stopped. Are you sure?");
        QPushButton *yesButton = msgBox.addButton(QMessageBox::Yes);
        msgBox.addButton(QMessageBox::Cancel);
        msgBox.exec();
        if (msgBox.clickedButton() != yesButton) {
            event->ignore();
            return;
        }
    }
    if (!settings.cdgWindowFullscreen())
        settings.saveWindowState(cdgWindow);
    settings.setShowCdgWindow(cdgWindow->isVisible());
    cdgWindow->setVisible(false);
    requestsDialog->setVisible(false);
    event->accept();
}

void MainWindow::on_sliderVolume_valueChanged(int value) {
    kMediaBackend.setVolume(value);
    kMediaBackend.fadeInImmediate();
}


void MainWindow::songDropNoSingerSel() {
    QMessageBox msgBox;
    msgBox.setText("No singer selected.  You must select a singer before you can add songs to a queue.");
    msgBox.setIcon(QMessageBox::Warning);
    msgBox.exec();
}

void MainWindow::newVersionAvailable(const QString &version) {
    QMessageBox msgBox;
    msgBox.setTextFormat(Qt::RichText);
    msgBox.setText("New version of OpenKJ is available: " + version);
    msgBox.setIcon(QMessageBox::Information);
    if (checker->getOS() == "Linux") {
        msgBox.setInformativeText(
                "To install the update, please use your distribution's package manager or download and build the current source.");
    }
    if (checker->getOS() == "Win32" || checker->getOS() == "Win64") {
        msgBox.setInformativeText(
                "You can download the new version at <a href=https://openkj.org/software>https://openkj.org/software</a>");
    }
    if (checker->getOS() == "MacOS") {
        msgBox.setInformativeText(
                "You can download the new version at <a href=https://openkj.org/software>https://openkj.org/software</a>");
    }
    msgBox.exec();
}

void MainWindow::on_pushButtonIncomingRequests_clicked() {
    requestsDialog->show();
}


void MainWindow::filesDroppedOnQueue(const QList<QUrl> &urls, const int &singerId, const int &position) {
            foreach (QUrl url, urls) {
            QString file = url.toLocalFile();
            if (QFile(file).exists()) {
                if (file.endsWith(".zip", Qt::CaseInsensitive)) {
                    MzArchive archive(file);
                    if (!archive.isValidKaraokeFile()) {
                        QMessageBox msgBox;
                        msgBox.setWindowTitle("Invalid karaoke file!");
                        msgBox.setText("Invalid karaoke file dropped on queue");
                        msgBox.setIcon(QMessageBox::Warning);
                        msgBox.setInformativeText(file);
                        msgBox.exec();
                        continue;
                    }
                } else if (file.endsWith(".cdg", Qt::CaseInsensitive)) {
                    if (TableModelKaraokeSongs::findCdgAudioFile(file) == QString()) {
                        QMessageBox msgBox;
                        msgBox.setWindowTitle("Invalid karaoke file!");
                        msgBox.setIcon(QMessageBox::Warning);
                        msgBox.setText("CDG file dropped on queue has no matching audio file");
                        msgBox.setInformativeText(file);
                        msgBox.exec();
                        continue;
                    }
                } else if (!file.endsWith(".mp4", Qt::CaseInsensitive) && !file.endsWith(".mkv", Qt::CaseInsensitive) &&
                           !file.endsWith(".avi", Qt::CaseInsensitive) && !file.endsWith(".m4v", Qt::CaseInsensitive)) {
                    QMessageBox msgBox;
                    msgBox.setWindowTitle("Invalid karaoke file!");
                    msgBox.setText(
                            "Unsupported file type dropped on queue.  Supported file types: mp3+g zip, cdg, mp4, mkv, avi");
                    msgBox.setIcon(QMessageBox::Warning);
                    msgBox.setInformativeText(file);
                    msgBox.exec();
                    continue;
                }
                qInfo() << "Karaoke file dropped. Singer: " << singerId << " Pos: " << position << " Path: " << file;
                QFileInfo dFileInfo(file);

                KaraokeSong droppedSong{
                        -1,
                        "--Dropped Song--",
                        "--dropped song--",
                        dFileInfo.completeBaseName(),
                        dFileInfo.completeBaseName().toLower(),
                        "!!DROPPED!!",
                        "!!dropped!!",
                        0,
                        dFileInfo.fileName(),
                        file,
                        "",
                        0,
                        QDateTime()
                };
                int songId = karaokeSongsModel.addSong(droppedSong);
                qInfo() << "addSong returned songid: " << songId;
                if (songId == -1)
                    continue;
                qModel.insert(songId, position);
            }
        }
}

void MainWindow::appFontChanged(const QFont &font) {
    auto smallerFont = font;
    smallerFont.setPointSize(font.pointSize() - 2);
    auto smallerFontBold = smallerFont;
    smallerFontBold.setBold(true);
    ui->labelTotal->setFont(smallerFont);
    ui->labelTotalTime->setFont(smallerFont);
    ui->labelElapsed->setFont(smallerFont);
    ui->labelElapsedTime->setFont(smallerFont);
    ui->labelRemain->setFont(smallerFont);
    ui->labelRemainTime->setFont(smallerFont);
    ui->labelArtistHeader->setFont(smallerFontBold);
    ui->labelArtist->setFont(smallerFont);
    ui->labelTitleHeader->setFont(smallerFontBold);
    ui->labelTitle->setFont(smallerFont);
    ui->labelSingerHeader->setFont(smallerFontBold);
    ui->labelSinger->setFont(smallerFont);

    QApplication::setFont(font, "QWidget");
    setFont(font);
    QFontMetrics fm(settings.applicationFont());
#if (QT_VERSION >= QT_VERSION_CHECK(5, 11, 0))
    int cvwWidth = std::max(300, fm.horizontalAdvance("Total: 00:00  Current:00:00  Remain: 00:00"));
#else
    int cvwWidth = std::max(300, fm.width("Total: 00:00  Current:00:00  Remain: 00:00"));
#endif
    qInfo() << "Resizing videoPreview to width: " << cvwWidth;
//    ui->cdgFrame->setMinimumWidth(cvwWidth);
//    ui->cdgFrame->setMaximumWidth(cvwWidth);
//    ui->mediaFrame->setMinimumWidth(cvwWidth);
//    ui->mediaFrame->setMaximumWidth(cvwWidth);

    QSize mcbSize(fm.height(), fm.height());
    if (mcbSize.width() < 32) {
        mcbSize.setWidth(32);
        mcbSize.setHeight(32);
    }
//    ui->buttonStop->resize(mcbSize);
//    ui->buttonPause->resize(mcbSize);
//    ui->buttonStop->setIconSize(mcbSize);
//    ui->buttonPause->setIconSize(mcbSize);
//    ui->buttonStop->setIcon(style()->standardIcon(QStyle::SP_MediaStop));
//    ui->buttonPause->setIcon(style()->standardIcon(QStyle::SP_MediaPause));


//    ui->pushButton->resize(mcbSize);
//    ui->pushButton->setIcon(QIcon(QPixmap(":/Icons/system-search2.png").scaled(mcbSize)));
//    ui->pushButton->setIconSize(mcbSize);


//    ui->buttonAddSinger->resize(mcbSize);
//    ui->buttonAddSinger->setIcon(QIcon(QPixmap(":/Icons/breeze-dark/list-add-user.svg").scaled(mcbSize)));
//    ui->buttonAddSinger->setIconSize(mcbSize);

//    ui->buttonClearRotation->resize(mcbSize);
//    ui->buttonClearRotation->setIcon(QIcon(QPixmap(":/Icons/breeze-dark/edit-delete.svg").scaled(mcbSize)));
//    ui->buttonClearRotation->setIconSize(mcbSize);

//    ui->buttonClearQueue->resize(mcbSize);
//    ui->buttonClearQueue->setIcon(QIcon(QPixmap(":/Icons/breeze-dark/edit-delete.svg").scaled(mcbSize)));
//    ui->buttonClearQueue->setIconSize(mcbSize);

//    ui->buttonRegulars->resize(mcbSize);
//    ui->buttonRegulars->setIcon(QIcon(QPixmap(":/Icons/breeze-dark/user-others.svg").scaled(mcbSize)));
//    ui->buttonRegulars->setIconSize(mcbSize);

    autosizeViews();

}

void MainWindow::resizeRotation() {
    if (settings.rotationShowNextSong()) {
        ui->tableViewRotation->showColumn(TableModelRotation::COL_NEXT_SONG);
        ui->tableViewRotation->horizontalHeader()->setSectionResizeMode(TableModelRotation::COL_NAME,
                                                                        QHeaderView::Interactive);
        ui->tableViewRotation->horizontalHeader()->setSectionResizeMode(TableModelRotation::COL_NEXT_SONG,
                                                                        QHeaderView::Stretch);
    } else {
        ui->tableViewRotation->horizontalHeader()->setSectionResizeMode(TableModelRotation::COL_NAME,
                                                                        QHeaderView::Stretch);
        ui->tableViewRotation->hideColumn(TableModelRotation::COL_NEXT_SONG);
    }
}

void MainWindow::autosizeViews() {
#if (QT_VERSION >= QT_VERSION_CHECK(5, 11, 0))
    int durationColSize = QFontMetrics(settings.applicationFont()).horizontalAdvance(" Duration ");
    int songidColSize = QFontMetrics(settings.applicationFont()).horizontalAdvance(" AA0000000-0000 ");
    int lastPlayColSize = QFontMetrics(settings.applicationFont()).horizontalAdvance("_00/00/00 00:00 MM_");
#else
    int durationColSize = QFontMetrics(settings.applicationFont()).width(" Duration ");
    int songidColSize = QFontMetrics(settings.applicationFont()).width(" AA0000000-0000 ");
    int lastPlayColSize = QFontMetrics(settings.applicationFont()).width("_00/00/00 00:00 MM_");
#endif
    int remainingSpace = ui->tableViewDB->width() - durationColSize - songidColSize;
    int artistColSize = (remainingSpace / 2) - 120;
    int titleColSize = (remainingSpace / 2) + 100;
    ui->tableViewDB->horizontalHeader()->resizeSection(TableModelKaraokeSongs::COL_ARTIST, artistColSize);
    ui->tableViewDB->horizontalHeader()->resizeSection(TableModelKaraokeSongs::COL_TITLE, titleColSize);
    ui->tableViewDB->horizontalHeader()->resizeSection(TableModelKaraokeSongs::COL_DURATION, durationColSize);
    ui->tableViewDB->horizontalHeader()->setSectionResizeMode(TableModelKaraokeSongs::COL_DURATION, QHeaderView::Fixed);
    ui->tableViewDB->horizontalHeader()->resizeSection(TableModelKaraokeSongs::COL_SONGID, songidColSize);
    ui->tableViewDB->horizontalHeader()->resizeSection(TableModelKaraokeSongs::COL_LASTPLAY, lastPlayColSize);
    resizeRotation();
    autosizeQueue();
//    ui->tableViewQueue->horizontalHeader()->resizeSection(7, playsColSize);
//    ui->tableViewQueue->horizontalHeader()->setSectionResizeMode(7, QHeaderView::Fixed);

}

void MainWindow::autosizeQueue() {
    auto curTab = ui->tabWidgetQueue->currentIndex();

    ui->tabWidgetQueue->setCurrentIndex(0);
    QApplication::processEvents();
    int fH = QFontMetrics(settings.applicationFont()).height();
    int iconWidth = fH + fH;
#if (QT_VERSION >= QT_VERSION_CHECK(5, 11, 0))
    int keyColSize = QFontMetrics(settings.applicationFont()).horizontalAdvance("Key") + iconWidth;
    int durationColSize = QFontMetrics(settings.applicationFont()).horizontalAdvance("Duration") + iconWidth;
    int songidColSize = QFontMetrics(settings.applicationFont()).horizontalAdvance(" AA0000000-0000 ");
    int lastPlayedColSize = QFontMetrics(settings.applicationFont()).horizontalAdvance("00-00-0000") + iconWidth;
    int playsColSize = QFontMetrics(settings.applicationFont()).horizontalAdvance("Plays") + iconWidth;
#else
    int lastPlayedColSize = QFontMetrics(settings.applicationFont()).width("00-00-0000") + iconWidth;
    int playsColSize = QFontMetrics(settings.applicationFont()).width("Plays") + iconWidth;
    int durationColSize = QFontMetrics(settings.applicationFont()).width("Duration") + iconWidth;
    int keyColSize = QFontMetrics(settings.applicationFont()).width("Key") + iconWidth;
    int songidColSize = QFontMetrics(settings.applicationFont()).width(" AA0000000-0000 ");
#endif
    int remainingSpace = ui->tableViewQueue->width() - iconWidth - keyColSize - durationColSize - songidColSize - 16;
    int artistColSize = (remainingSpace / 2);
    int titleColSize = (remainingSpace / 2);
    ui->tableViewQueue->horizontalHeader()->resizeSection(TableModelQueueSongs::COL_ARTIST, artistColSize);
    ui->tableViewQueue->horizontalHeader()->resizeSection(TableModelQueueSongs::COL_TITLE, titleColSize);
    ui->tableViewQueue->horizontalHeader()->resizeSection(TableModelQueueSongs::COL_SONGID, songidColSize);
    ui->tableViewQueue->horizontalHeader()->resizeSection(TableModelQueueSongs::COL_DURATION, durationColSize);
    ui->tableViewQueue->horizontalHeader()->resizeSection(TableModelQueueSongs::COL_PATH, iconWidth);
    ui->tableViewQueue->horizontalHeader()->setSectionResizeMode(TableModelQueueSongs::COL_PATH, QHeaderView::Fixed);
    ui->tableViewQueue->horizontalHeader()->resizeSection(TableModelQueueSongs::COL_KEY, keyColSize);
    ui->tableViewQueue->horizontalHeader()->setSectionResizeMode(TableModelQueueSongs::COL_KEY, QHeaderView::Fixed);

    ui->tabWidgetQueue->setCurrentIndex(1);
    QApplication::processEvents();
    remainingSpace = ui->tableViewHistory->width() - keyColSize - songidColSize - lastPlayedColSize - playsColSize - 16;
    artistColSize = (remainingSpace / 2);
    titleColSize = (remainingSpace / 2);
    ui->tableViewHistory->horizontalHeader()->resizeSection(3, artistColSize);
    ui->tableViewHistory->horizontalHeader()->resizeSection(4, titleColSize);
    ui->tableViewHistory->horizontalHeader()->resizeSection(5, songidColSize);
    ui->tableViewHistory->horizontalHeader()->resizeSection(6, keyColSize);
    ui->tableViewHistory->horizontalHeader()->setSectionResizeMode(6, QHeaderView::Fixed);
    ui->tableViewHistory->horizontalHeader()->resizeSection(7, playsColSize);
    ui->tableViewHistory->horizontalHeader()->setSectionResizeMode(7, QHeaderView::Fixed);
    QApplication::processEvents();
    ui->tabWidgetQueue->setCurrentIndex(curTab);
}


void MainWindow::resizeEvent(QResizeEvent *event) {
    if (!m_initialUiSetupDone)
        return;
    QMainWindow::resizeEvent(event);
    autosizeViews();
    if (ui->tabWidget->currentIndex() == 0) {
        autosizeViews();
        kNeedAutoSize = false;
    }
    if (ui->tabWidget->currentIndex() == 1) {
        kNeedAutoSize = true;
    }
    settings.saveWindowState(this);
}

void MainWindow::on_tabWidget_currentChanged(const int &index) {
    if (kNeedAutoSize && index == 0) {
        autosizeViews();
        kNeedAutoSize = false;
    }
}





void MainWindow::sfxButtonPressed() {
    auto *btn = (SoundFxButton *) sender();
    qInfo() << "SfxButton pressed: " << btn->buttonData();
    sfxMediaBackend.setMedia(btn->buttonData().toString());
    sfxMediaBackend.setVolume(ui->sliderVolume->value());
    sfxMediaBackend.play();
}

void MainWindow::on_btnAddSfx_clicked() {
//    QString name = "a button";
//    QString path = "a path";
    QString path = QFileDialog::getOpenFileName(this, QString("Select audio file"),
                                                QStandardPaths::writableLocation(QStandardPaths::MusicLocation),
                                                QString("Audio (*.mp3 *.ogg *.wav *.wma)"), nullptr, QFileDialog::DontUseNativeDialog);
    if (path != "") {
        bool ok;
        QString name = QInputDialog::getText(this, tr("Button Text"), tr("Enter button text:"), QLineEdit::Normal,
                                             QString(), &ok);
        if (!ok || name.isEmpty())
            return;
        SfxEntry entry;
        entry.name = name;
        entry.path = path;
        settings.addSfxEntry(entry);
        addSfxButton(path, name);
    }

}

void MainWindow::on_btnSfxStop_clicked() {
    sfxMediaBackend.stop(true);
}

void MainWindow::removeSfxButton() {
    SfxEntryList entries = settings.getSfxEntries();
    SfxEntryList newEntries;
            foreach (SfxEntry entry, entries) {
            if (entry.name == lastRtClickedSfxBtn.name && entry.path == lastRtClickedSfxBtn.path)
                continue;
            newEntries.push_back(entry);
        }
    settings.setSfxEntries(newEntries);
    refreshSfxButtons();
}

void MainWindow::showAlert(const QString &title, const QString &message) {
    QMessageBox msgBox;
    msgBox.setWindowTitle(title);
    msgBox.setText(message);
    msgBox.exec();
}

void MainWindow::tableViewRotationCurrentChanged(const QModelIndex &cur, const QModelIndex &prev) {
    Q_UNUSED(prev)
    qModel.loadSinger(cur.data(Qt::UserRole).toInt());
    historySongsModel.loadSinger(rotModel.getSingerName(cur.data(Qt::UserRole).toInt()));
    if (!settings.treatAllSingersAsRegs() && !cur.sibling(cur.row(), TableModelRotation::COL_REGULAR).data().toBool())
        ui->tabWidgetQueue->removeTab(1);
    else {
        if (ui->tabWidgetQueue->count() == 1)
            ui->tabWidgetQueue->addTab(historyTabWidget, "History");
    }
    ui->gbxQueue->setTitle(
            QString("Song Queue - " + cur.sibling(cur.row(), TableModelRotation::COL_NAME).data().toString()));
    if (!ui->tabWidgetQueue->isVisible()) {
        ui->tabWidgetQueue->setVisible(true);
        ui->labelNoSinger->setVisible(false);
        QApplication::processEvents();
        autosizeQueue();
    }
}

void MainWindow::updateRotationDuration() {
    QString text;
    int secs = rotModel.rotationDuration();
    if (secs > 0) {
        int hours = 0;
        int minutes = secs / 60;
        int seconds = secs % 60;
        if (seconds > 0)
            minutes++;
        if (minutes > 60) {
            hours = minutes / 60;
            minutes = minutes % 60;
            if (hours > 1)
                text = " Rotation Duration: " + QString::number(hours) + " hours " + QString::number(minutes) + " min";
            else
                text = " Rotation Duration: " + QString::number(hours) + " hour " + QString::number(minutes) + " min";
        } else
            text = " Rotation Duration: " + QString::number(minutes) + " min";
    } else
        text = " Rotation Duration: 0 min";
    labelRotationDuration.setText(text);
}

void MainWindow::cdgVisibilityChanged() {
    ui->btnToggleCdgWindow->setChecked(cdgWindow->isVisible());
}

void MainWindow::rotationSelectionChanged(const QItemSelection &sel, const QItemSelection &desel) {
    if (sel.empty()) {
        qInfo() << "Rotation Selection Cleared!";
        qModel.loadSinger(-1);
        ui->tableViewRotation->reset();
        ui->gbxQueue->setTitle("Song Queue");
        ui->tabWidgetQueue->setVisible(false);
        ui->labelNoSinger->setVisible(true);
    }

    qInfo() << "Rotation Selection Changed";

}


void MainWindow::on_btnRotTop_clicked() {
    auto indexes = ui->tableViewRotation->selectionModel()->selectedRows();
    std::vector<int> singerIds;
    std::for_each(indexes.begin(), indexes.end(), [&](QModelIndex index) {
        singerIds.emplace_back(index.data(Qt::UserRole).toInt());
    });
    std::for_each(singerIds.rbegin(), singerIds.rend(), [&](auto singerId) {
        rotModel.singerMove(rotModel.getSingerPosition(singerId), 0);
    });
    auto topLeft = ui->tableViewRotation->model()->index(0, 0);
    auto bottomRight = ui->tableViewRotation->model()->index(singerIds.size() - 1, rotModel.columnCount() - 1);
    ui->tableViewRotation->clearSelection();
    ui->tableViewRotation->selectionModel()->select(QItemSelection(topLeft, bottomRight), QItemSelectionModel::Select);
    rotationDataChanged();
}

void MainWindow::on_btnRotUp_clicked() {
    if (ui->tableViewRotation->selectionModel()->selectedRows().count() < 1)
        return;
    int curPos = ui->tableViewRotation->selectionModel()->selectedRows().at(0).row();
    if (curPos == 0)
        return;
    rotModel.singerMove(curPos, curPos - 1);
    ui->tableViewRotation->selectRow(curPos - 1);
    rotationDataChanged();
}

void MainWindow::on_btnRotDown_clicked() {
    if (ui->tableViewRotation->selectionModel()->selectedRows().count() < 1)
        return;
    int curPos = ui->tableViewRotation->selectionModel()->selectedRows().at(0).row();
    if (curPos == rotModel.rowCount() - 1)
        return;
    rotModel.singerMove(curPos, curPos + 1);
    ui->tableViewRotation->selectRow(curPos + 1);
    rotationDataChanged();
}

void MainWindow::on_btnRotBottom_clicked() {
    auto indexes = ui->tableViewRotation->selectionModel()->selectedRows();
    std::vector<int> singerIds;
    std::for_each(indexes.begin(), indexes.end(), [&](QModelIndex index) {
        singerIds.emplace_back(index.data(Qt::UserRole).toInt());
    });
    std::for_each(singerIds.begin(), singerIds.end(), [&](auto songId) {
        rotModel.singerMove(rotModel.getSingerPosition(songId), rotModel.rowCount() - 1);
    });
    auto topLeft = ui->tableViewRotation->model()->index(rotModel.rowCount() - singerIds.size(), 0);
    auto bottomRight = ui->tableViewRotation->model()->index(rotModel.rowCount() - 1, rotModel.columnCount() - 1);
    ui->tableViewRotation->clearSelection();
    ui->tableViewRotation->selectionModel()->select(QItemSelection(topLeft, bottomRight), QItemSelectionModel::Select);
    rotationDataChanged();
}

void MainWindow::on_btnQTop_clicked() {
    auto indexes = ui->tableViewQueue->selectionModel()->selectedRows();
    std::vector<int> songIds;
    std::for_each(indexes.begin(), indexes.end(), [&](QModelIndex index) {
        songIds.emplace_back(index.data().toInt());
    });
    std::for_each(songIds.rbegin(), songIds.rend(), [&](auto songId) {
        qModel.moveSongId(songId, 0);
    });
    auto topLeft = ui->tableViewQueue->model()->index(0, 0);
    auto bottomRight = ui->tableViewQueue->model()->index(songIds.size() - 1, qModel.columnCount() - 1);
    ui->tableViewQueue->selectionModel()->select(QItemSelection(topLeft, bottomRight), QItemSelectionModel::Select);
    rotationDataChanged();
}

void MainWindow::on_btnQUp_clicked() {
    if (ui->tableViewQueue->selectionModel()->selectedRows().count() < 1)
        return;
    int curPos = ui->tableViewQueue->selectionModel()->selectedRows().at(0).row();
    if (curPos == 0)
        return;
    qModel.move(curPos, curPos - 1);
    ui->tableViewQueue->selectRow(curPos - 1);
    rotationDataChanged();
}

void MainWindow::on_btnQDown_clicked() {
    if (ui->tableViewQueue->selectionModel()->selectedRows().count() < 1)
        return;
    int curPos = ui->tableViewQueue->selectionModel()->selectedRows().at(0).row();
    if (curPos == ui->tableViewQueue->model()->rowCount() - 1)
        return;
    qModel.move(curPos, curPos + 1);
    ui->tableViewQueue->selectRow(curPos + 1);
    rotationDataChanged();
}

void MainWindow::on_btnQBottom_clicked() {
    auto indexes = ui->tableViewQueue->selectionModel()->selectedRows();
    std::vector<int> songIds;
    std::for_each(indexes.begin(), indexes.end(), [&](QModelIndex index) {
        songIds.emplace_back(index.data().toInt());
    });
    std::for_each(songIds.begin(), songIds.end(), [&](auto songId) {
        qModel.moveSongId(songId, qModel.rowCount() - 1);
    });
    auto topLeft = ui->tableViewQueue->model()->index(qModel.rowCount() - songIds.size(), 0);
    auto bottomRight = ui->tableViewQueue->model()->index(qModel.rowCount() - 1, qModel.columnCount() - 1);
    ui->tableViewQueue->selectionModel()->select(QItemSelection(topLeft, bottomRight), QItemSelectionModel::Select);
    rotationDataChanged();
}






void MainWindow::on_actionSound_Clips_triggered(const bool &checked) {
    if (checked) {
        ui->groupBoxSoundClips->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        ui->scrollAreaSoundClips->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        ui->scrollAreaWidgetContents->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        ui->verticalSpacerRtPanel->changeSize(0, 20, QSizePolicy::Ignored, QSizePolicy::Ignored);
    } else {
        ui->groupBoxSoundClips->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Ignored);
        ui->verticalSpacerRtPanel->changeSize(0, 20, QSizePolicy::Ignored, QSizePolicy::Expanding);
    }
    ui->groupBoxSoundClips->setVisible(checked);
    settings.setShowMainWindowSoundClips(checked);
}

void MainWindow::on_actionNow_Playing_triggered(const bool &checked) {
    ui->groupBoxNowPlaying->setVisible(checked);
    settings.setShowMainWindowNowPlaying(checked);
}

void MainWindow::on_actionVideoSmall_triggered() {
    ui->videoPreview->setMinimumSize(QSize(256, 144));
    ui->videoPreview->setMaximumSize(QSize(256, 144));
    ui->mediaFrame->setMaximumWidth(300);
    ui->mediaFrame->setMinimumWidth(300);
    settings.setMainWindowVideoSize(Settings::Small);
    QTimer::singleShot(15, [&]() { autosizeViews(); });
}

void MainWindow::on_actionVideoMedium_triggered() {
    ui->videoPreview->setMinimumSize(QSize(384, 216));
    ui->videoPreview->setMaximumSize(QSize(384, 216));
    ui->mediaFrame->setMaximumWidth(430);
    ui->mediaFrame->setMinimumWidth(430);
    settings.setMainWindowVideoSize(Settings::Medium);
    QTimer::singleShot(15, [&]() { autosizeViews(); });
}

void MainWindow::on_actionVideoLarge_triggered() {
    ui->videoPreview->setMinimumSize(QSize(512, 288));
    ui->videoPreview->setMaximumSize(QSize(512, 288));
    ui->mediaFrame->setMaximumWidth(560);
    ui->mediaFrame->setMinimumWidth(560);
    settings.setMainWindowVideoSize(Settings::Large);
    QTimer::singleShot(15, [&]() { autosizeViews(); });
}

void MainWindow::on_actionVideo_Output_2_triggered(const bool &checked) {
    ui->videoPreview->setVisible(checked);
    settings.setShowMainWindowVideo(checked);
}

void MainWindow::on_actionKaraoke_torture_triggered() {
#if (QT_VERSION >= QT_VERSION_CHECK(5, 10, 0))
    connect(&m_timerTest, &QTimer::timeout, [&]() {
        QApplication::beep();
        static int runs = 0;
        qInfo() << "Karaoke torture test timer timeout";
        qInfo() << "num songs in db: " << karaokeSongsModel.rowCount(QModelIndex());
        ui->tableViewDB->scrollToBottom();
        ui->tableViewDB->scrollToBottom();
        ui->tableViewDB->scrollToBottom();
        ui->tableViewDB->scrollToBottom();
        ui->tableViewDB->scrollToBottom();
        ui->tableViewDB->scrollToBottom();
        ui->tableViewDB->scrollToBottom();
        ui->tableViewDB->scrollToBottom();
        ui->tableViewDB->scrollToBottom();
        ui->tableViewDB->scrollToBottom();
        ui->tableViewDB->scrollToBottom();
        ui->tableViewDB->scrollToBottom();
        ui->tableViewDB->scrollToBottom();
        ui->tableViewDB->scrollToBottom();
        ui->tableViewDB->scrollToBottom();
        ui->tableViewDB->scrollToBottom();
        int randno = QRandomGenerator::global()->bounded(0, karaokeSongsModel.rowCount(QModelIndex()) - 1);
        randno = 1;
        qInfo() << "randno: " << randno;
        ui->tableViewDB->selectRow(randno);
        ui->tableViewDB->scrollTo(ui->tableViewDB->selectionModel()->selectedRows().at(0));
        play(karaokeSongsModel.getPath(
                ui->tableViewDB->selectionModel()->selectedRows(TableModelKaraokeSongs::COL_ID).at(0).data().toInt()),
             true);
        ui->labelSinger->setText("Torture run (" + QString::number(++runs) + ")");
//       QSqlQuery query;
//       query.prepare("SELECT songid,artist,title,discid,path FROM dbsongs WHERE songid >= (abs(random()) % (SELECT max(songid) FROM dbsongs))LIMIT 1");
//       query.exec();
//       if (!query.next())
//           qInfo() << "Unable to find song in db!";
//       auto artist = query.value("artist").toString();
//       auto title = query.value("title").toString();
//       auto songId = query.value("discid").toString();
//       auto path = query.value("path").toString();
//       qInfo() << "Torture test playing: " << path;
    });
    m_timerTest.start(4000);
#endif
}

void MainWindow::on_actionK_B_torture_triggered() {
#if (QT_VERSION >= QT_VERSION_CHECK(5, 10, 0))
    connect(&m_timerTest, &QTimer::timeout, [&]() {
        QApplication::beep();
        static bool playing = false;
        static int runs = 0;
        if (playing) {
            on_buttonStop_clicked();
            playing = false;
            ui->labelSinger->setText("Torture run (" + QString::number(runs) + ")");
            return;
        }
        qInfo() << "Karaoke torture test timer timeout";
        qInfo() << "num songs in db: " << karaokeSongsModel.rowCount(QModelIndex());
        ui->tableViewDB->scrollToBottom();
        ui->tableViewDB->scrollToBottom();
        ui->tableViewDB->scrollToBottom();
        ui->tableViewDB->scrollToBottom();
        ui->tableViewDB->scrollToBottom();
        ui->tableViewDB->scrollToBottom();
        ui->tableViewDB->scrollToBottom();
        ui->tableViewDB->scrollToBottom();
        ui->tableViewDB->scrollToBottom();
        ui->tableViewDB->scrollToBottom();
        ui->tableViewDB->scrollToBottom();
        ui->tableViewDB->scrollToBottom();
        ui->tableViewDB->scrollToBottom();
        ui->tableViewDB->scrollToBottom();
        ui->tableViewDB->scrollToBottom();
        ui->tableViewDB->scrollToBottom();
        int randno = QRandomGenerator::global()->bounded(0, karaokeSongsModel.rowCount(QModelIndex()) - 1);
        qInfo() << "randno: " << randno;
        ui->tableViewDB->selectRow(randno);
        ui->tableViewDB->scrollTo(ui->tableViewDB->selectionModel()->selectedRows().at(0));
        play(ui->tableViewDB->selectionModel()->selectedRows(5).at(0).data().toString(), false);
        ui->labelSinger->setText("Torture run (" + QString::number(++runs) + ")");
        playing = true;
//       QSqlQuery query;
//       query.prepare("SELECT songid,artist,title,discid,path FROM dbsongs WHERE songid >= (abs(random()) % (SELECT max(songid) FROM dbsongs))LIMIT 1");
//       query.exec();
//       if (!query.next())
//           qInfo() << "Unable to find song in db!";
//       auto artist = query.value("artist").toString();
//       auto title = query.value("title").toString();
//       auto songId = query.value("discid").toString();
//       auto path = query.value("path").toString();
//       qInfo() << "Torture test playing: " << path;
    });
    m_timerTest.start(2000);
#endif
}

void MainWindow::on_actionBurn_in_triggered() {
#if (QT_VERSION >= QT_VERSION_CHECK(5, 10, 0))
    m_testMode = true;
    emit ui->buttonClearRotation->clicked();
    for (auto i = 0; i < 21; i++) {
        auto singerName = "Test Singer " + QString::number(i);
        rotModel.singerAdd(singerName);
        // rotModel.regularDelete(singerName);
    }
    connect(&m_timerTest, &QTimer::timeout, [&]() {
        QApplication::beep();
        static bool playing = false;
        static int runs = 0;
        if (playing) {
            on_buttonStop_clicked();
            playing = false;
            ui->labelSinger->setText("Torture run (" + QString::number(runs) + ")");
            return;
        }

        rotModel.singerMove(QRandomGenerator::global()->bounded(0, 19), QRandomGenerator::global()->bounded(0, 19));
        ui->tableViewRotation->selectRow(QRandomGenerator::global()->bounded(0, 19));
        int randno{0};
        if (karaokeSongsModel.rowCount(QModelIndex()) > 1)
            randno = QRandomGenerator::global()->bounded(0, karaokeSongsModel.rowCount(QModelIndex()) - 1);
        qInfo() << "randno: " << randno;
        ui->tableViewDB->selectRow(randno);
        ui->tableViewDB->scrollTo(ui->tableViewDB->selectionModel()->selectedRows().at(0));
        emit ui->tableViewDB->doubleClicked(ui->tableViewDB->selectionModel()->selectedRows().at(0));
        ui->tableViewQueue->selectRow(qModel.rowCount() - 1);
        ui->tableViewQueue->scrollTo(ui->tableViewQueue->selectionModel()->selectedRows().at(0));
        if (qModel.rowCount() > 2) {
            auto newPos = QRandomGenerator::global()->bounded(0, qModel.rowCount() - 1);
            qModel.move(qModel.rowCount() - 1, newPos);
            ui->tableViewQueue->selectRow(newPos);
            ui->tableViewQueue->scrollTo(ui->tableViewQueue->selectionModel()->selectedRows().at(0));
        }
        auto idx = ui->tableViewQueue->selectionModel()->selectedRows().at(0);
        emit ui->tableViewQueue->doubleClicked(idx);
        ui->tableViewQueue->selectRow(idx.row());
        playing = true;
        qInfo() << "Burn in test cycle: " << ++runs;
    });
    m_timerTest.start(3000);
#endif
}

void MainWindow::on_actionMultiplex_Controls_triggered(bool checked) {
    ui->widgetMplxControls->setVisible(checked);
    settings.setShowMplxControls(checked);
}

void MainWindow::on_actionCDG_Decode_Torture_triggered() {
#if (QT_VERSION >= QT_VERSION_CHECK(5, 10, 0))
    connect(&m_timerTest, &QTimer::timeout, [&]() {
        QApplication::beep();
        static int runs = 0;
        qInfo() << "Karaoke torture test timer timeout";
        qInfo() << "num songs in db: " << karaokeSongsModel.rowCount(QModelIndex());
        ui->tableViewDB->scrollToBottom();
        ui->tableViewDB->scrollToBottom();
        ui->tableViewDB->scrollToBottom();
        ui->tableViewDB->scrollToBottom();
        ui->tableViewDB->scrollToBottom();
        ui->tableViewDB->scrollToBottom();
        ui->tableViewDB->scrollToBottom();
        ui->tableViewDB->scrollToBottom();
        ui->tableViewDB->scrollToBottom();
        ui->tableViewDB->scrollToBottom();
        ui->tableViewDB->scrollToBottom();
        ui->tableViewDB->scrollToBottom();
        ui->tableViewDB->scrollToBottom();
        ui->tableViewDB->scrollToBottom();
        ui->tableViewDB->scrollToBottom();
        ui->tableViewDB->scrollToBottom();
        int randno = QRandomGenerator::global()->bounded(0, karaokeSongsModel.rowCount(QModelIndex()) - 1);
        qInfo() << "randno: " << randno;
        ui->tableViewDB->selectRow(randno);
        ui->tableViewDB->scrollTo(ui->tableViewDB->selectionModel()->selectedRows().at(0));
        QString karaokeFilePath = ui->tableViewDB->selectionModel()->selectedRows(5).at(0).data().toString();
        if (!karaokeFilePath.endsWith("zip", Qt::CaseInsensitive) &&
            !karaokeFilePath.endsWith("cdg", Qt::CaseInsensitive)) {
            return;
        }
        if (karaokeFilePath.endsWith(".zip", Qt::CaseInsensitive)) {
            MzArchive archive(karaokeFilePath);
            if ((archive.checkCDG()) && (archive.checkAudio())) {
                if (archive.checkAudio()) {
                    if (!archive.extractAudio(khTmpDir->path(), "tmp" + archive.audioExtension())) {
                        return;
                    }
                    if (!archive.extractCdg(khTmpDir->path(), "tmp.cdg")) {
                        return;
                    }
                    QString audioFile = khTmpDir->path() + QDir::separator() + "tmp" + archive.audioExtension();
                    QString cdgFile = khTmpDir->path() + QDir::separator() + "tmp.cdg";
                    qInfo() << "Extracted audio file size: " << QFileInfo(audioFile).size();
                    qInfo() << "Setting karaoke backend source file to: " << audioFile;
                    kMediaBackend.setMediaCdg(cdgFile, audioFile);
                    //kMediaBackend.testCdgDecode(); // todo: andth
                }
            } else {
                return;
            }
        } else if (karaokeFilePath.endsWith(".cdg", Qt::CaseInsensitive)) {
            QString cdgTmpFile = "tmp.cdg";
            QString audTmpFile = "tmp.mp3";
            QFile cdgFile(karaokeFilePath);
            if (!cdgFile.exists() || cdgFile.size() == 0) {
                return;
            }
            QString audiofn = findMatchingAudioFile(karaokeFilePath);
            if (audiofn == "") {
                return;
            }
            QFile audioFile(audiofn);
            if (audioFile.size() == 0) {
                return;
            }
            cdgFile.copy(khTmpDir->path() + QDir::separator() + cdgTmpFile);
            QFile::copy(audiofn, khTmpDir->path() + QDir::separator() + audTmpFile);
            kMediaBackend.setMediaCdg(khTmpDir->path() + QDir::separator() + cdgTmpFile,
                                      khTmpDir->path() + QDir::separator() + audTmpFile);
            // kMediaBackend.testCdgDecode(); // todo: andth
        }
        ui->labelSinger->setText("Torture run (" + QString::number(++runs) + ")");
    });
    m_timerTest.start(2000);
#endif
}

void MainWindow::on_actionWrite_Gstreamer_pipeline_dot_files_triggered() {
    QString outputFolder = QStandardPaths::standardLocations(QStandardPaths::PicturesLocation).at(0);
    kMediaBackend.writePipelinesGraphToFile(outputFolder);
    sfxMediaBackend.writePipelinesGraphToFile(outputFolder);
}

void MainWindow::on_comboBoxSearchType_currentIndexChanged(int index) {
    switch (index) {
        case 1:
            karaokeSongsModel.setSearchType(TableModelKaraokeSongs::SEARCH_TYPE_ARTIST);
            break;
        case 2:
            karaokeSongsModel.setSearchType(TableModelKaraokeSongs::SEARCH_TYPE_TITLE);
            break;
        default:
            karaokeSongsModel.setSearchType(TableModelKaraokeSongs::SEARCH_TYPE_ALL);
            break;
    }
}

void MainWindow::on_actionDocumentation_triggered() {
    QDesktopServices::openUrl(QUrl("https://docs.openkj.org"));
}

void MainWindow::on_btnToggleCdgWindow_clicked(bool checked) {
    if (!checked) {
        cdgWindow->hide();
    } else {
        cdgWindow->show();
    }
}

void MainWindow::on_pushButtonHistoryPlay_clicked() {
    auto selRows = ui->tableViewHistory->selectionModel()->selectedRows();
    if (selRows.empty())
        return;
    auto index = selRows.at(0);
    k2kTransition = false;
    if (kMediaBackend.state() == MediaBackend::PlayingState) {
        if (settings.showSongInterruptionWarning()) {
            QMessageBox msgBox(this);
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
        k2kTransition = true;
    }
    if (kMediaBackend.state() == MediaBackend::PausedState) {
        if (settings.karaokeAutoAdvance()) {
            kAASkip = true;
            cdgWindow->showAlert(false);
        }
        kMediaBackend.stop(true);
    }
    int curSingerId = rotModel.getSingerId(historySongsModel.currentSingerName());
    curSinger = rotModel.getSingerName(curSingerId);
    curArtist = index.sibling(index.row(), 3).data().toString();
    curTitle = index.sibling(index.row(), 4).data().toString();
    QString curSongId = index.sibling(index.row(), 5).data().toString();
    QString filePath = index.sibling(index.row(), 2).data().toString();
    int curKeyChange = index.sibling(index.row(), 6).data().toInt();
    ui->labelSinger->setText(curSinger);
    ui->labelArtist->setText(curArtist);
    ui->labelTitle->setText(curTitle);
    karaokeSongsModel.updateSongHistory(karaokeSongsModel.getIdForPath(filePath));
    play(filePath, k2kTransition);
    if (settings.treatAllSingersAsRegs() || rotModel.singerIsRegular(curSingerId))
        historySongsModel.saveSong(curSinger, filePath, curArtist, curTitle, curSongId, curKeyChange);
    kMediaBackend.setPitchShift(curKeyChange);
    rotModel.setCurrentSinger(curSingerId);
    rotDelegate.setCurrentSinger(curSingerId);
    if (settings.rotationAltSortOrder()) {
        auto curSingerPos = rotModel.getSingerPosition(curSingerId);
        m_curSingerOriginalPosition = curSingerPos;
        if (curSingerPos != 0)
            rotModel.singerMove(curSingerPos, 0);
    }
}

void MainWindow::on_pushButtonHistoryToQueue_clicked() {
    auto selRows = ui->tableViewHistory->selectionModel()->selectedRows();
    if (selRows.empty())
        return;

    std::for_each(selRows.begin(), selRows.end(), [&](auto index) {
        auto path = index.sibling(index.row(), 2).data().toString();
        int curSingerId = rotModel.getSingerId(historySongsModel.currentSingerName());
        int key = index.sibling(index.row(), 6).data().toInt();
        int dbSongId = karaokeSongsModel.getIdForPath(path);
        if (dbSongId == -1) {
            QMessageBox::warning(this,
                                 "Unable to add history song",
                                 "Unable to find a matching song in the database for the selected "
                                 "history song.  This usually happens when a file that was once in "
                                 "OpenKJ's database has been removed or renamed outside of OpenKJ\n\n"
                                 "Song: " + path
            );
            return;
        }
        qModel.songAddSlot(dbSongId, curSingerId, key);
    });
    ui->tableViewHistory->clearSelection();
    ui->tabWidgetQueue->setCurrentIndex(0);
}

void MainWindow::on_tableViewHistory_doubleClicked([[maybe_unused]]const QModelIndex &index) {
    switch (ui->comboBoxHistoryDblClick->currentIndex()) {
        case 0:
            on_pushButtonHistoryToQueue_clicked();
            break;
        case 1:
            on_pushButtonHistoryPlay_clicked();
            break;
    }
}

void MainWindow::on_tableViewHistory_customContextMenuRequested(const QPoint &pos) {
    int selCount = ui->tableViewHistory->selectionModel()->selectedRows().size();
    if (selCount == 1) {
        QModelIndex index = ui->tableViewHistory->indexAt(pos);
        if (index.isValid()) {
            QMenu contextMenu(this);
            contextMenu.addAction("Preview", [&]() {
                QString filename = index.sibling(index.row(), 2).data().toString();
                if (!QFile::exists(filename)) {
                    QMessageBox::warning(this, tr("Missing File!"),
                                         "Specified karaoke file missing, preview aborted!\n\n" + dbRtClickFile,
                                         QMessageBox::Ok);
                    return;
                }
                auto *videoPreview = new DlgVideoPreview(filename, this);
                videoPreview->setAttribute(Qt::WA_DeleteOnClose);
                videoPreview->show();
            });
            contextMenu.addAction("Play", this, &MainWindow::on_pushButtonHistoryPlay_clicked);
            contextMenu.addAction("Add to queue", this, &MainWindow::on_pushButtonHistoryToQueue_clicked);
            contextMenu.addSeparator();
            contextMenu.addAction("Delete", [&]() {
                QMessageBox msgBox;
                msgBox.setText("Delete song from singer history?");
                msgBox.setIcon(QMessageBox::Warning);
                msgBox.setInformativeText("Are you sure you want to remove this song from the singer's history?");
                msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::Cancel);
                msgBox.setDefaultButton(QMessageBox::Cancel);
                int ret = msgBox.exec();
                if (ret == QMessageBox::Cancel)
                    return;
                int songIndex = index.sibling(index.row(), 0).data().toInt();
                historySongsModel.deleteSong(songIndex);
                ui->tableViewHistory->clearSelection();
            });
            contextMenu.exec(QCursor::pos());
        }
    } else if (selCount > 1) {
        QMenu contextMenu(this);
        contextMenu.addAction("Add to queue", this, &MainWindow::on_pushButtonHistoryToQueue_clicked);
        contextMenu.addSeparator();
        contextMenu.addAction("Delete", [&]() {
            QMessageBox msgBox;
            msgBox.setText("Delete songs from singer history?");
            msgBox.setIcon(QMessageBox::Warning);
            msgBox.setInformativeText("Are you sure you want to remove these songs from the singer's history?");
            msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::Cancel);
            msgBox.setDefaultButton(QMessageBox::Cancel);
            int ret = msgBox.exec();
            if (ret == QMessageBox::Cancel)
                return;
            auto indexes = ui->tableViewHistory->selectionModel()->selectedRows();
            std::for_each(indexes.rbegin(), indexes.rend(), [&](QModelIndex index) {
                historySongsModel.deleteSong(index.data().toInt());
            });
            ui->tableViewHistory->clearSelection();
        });

        contextMenu.exec(QCursor::pos());
    }
}




void MainWindow::mouseMoveEvent(QMouseEvent *event) {
    qInfo() << "Mouse move event: " << event->pos();
    QMainWindow::mouseMoveEvent(event);
}

void MainWindow::on_actionBurn_in_EOS_Jump_triggered() {
#if (QT_VERSION >= QT_VERSION_CHECK(5, 10, 0))
    m_testMode = true;
    emit ui->buttonClearRotation->clicked();
    for (auto i = 0; i < 21; i++) {
        auto singerName = "Test Singer " + QString::number(i);
        rotModel.singerAdd(singerName);
        // rotModel.regularDelete(singerName);
    }
    connect(&m_timerTest, &QTimer::timeout, [&]() {
        QApplication::beep();
        static bool playing = false;
        static int runs = 0;
        rotModel.singerMove(QRandomGenerator::global()->bounded(0, 19), QRandomGenerator::global()->bounded(0, 19));
        ui->tableViewRotation->selectRow(QRandomGenerator::global()->bounded(0, 19));
        int randno{0};
        if (karaokeSongsModel.rowCount(QModelIndex()) > 1)
            randno = QRandomGenerator::global()->bounded(0, karaokeSongsModel.rowCount(QModelIndex()) - 1);
        qInfo() << "randno: " << randno;
        ui->tableViewDB->selectRow(randno);
        ui->tableViewDB->scrollTo(ui->tableViewDB->selectionModel()->selectedRows().at(0));
        emit ui->tableViewDB->doubleClicked(ui->tableViewDB->selectionModel()->selectedRows().at(0));
        ui->tableViewQueue->selectRow(qModel.rowCount() - 1);
        ui->tableViewQueue->scrollTo(ui->tableViewQueue->selectionModel()->selectedRows().at(0));
        if (qModel.rowCount() > 2) {
            auto newPos = QRandomGenerator::global()->bounded(0, qModel.rowCount() - 1);
            qModel.move(qModel.rowCount() - 1, newPos);
            ui->tableViewQueue->selectRow(newPos);
            ui->tableViewQueue->scrollTo(ui->tableViewQueue->selectionModel()->selectedRows().at(0));
        }
        auto idx = ui->tableViewQueue->selectionModel()->selectedRows().at(0);
        emit ui->tableViewQueue->doubleClicked(idx);
        ui->tableViewQueue->selectRow(idx.row());
        playing = true;
        QTimer::singleShot(500, [&]() {
            auto duration = kMediaBackend.duration();
            auto jumpPoint = duration - 10000;
            kMediaBackend.setPosition(jumpPoint);
        });
        qInfo() << "Burn in test cycle: " << ++runs;
        ui->labelSinger->setText("Torture run (" + QString::number(runs) + ")");
    });
    m_timerTest.start(13000);
#endif
}


void MainWindow::applyAppTheme(int themeIndex)
{
    AppTheme::applyTheme(themeIndex);
}

void MainWindow::applyTouchFriendly(bool enabled)
{
    // List of all main table/list views to configure for touch-friendly mode.
    const QList<QAbstractItemView *> views {
        ui->tableViewRotation,
        ui->tableViewDB,
        ui->tableViewQueue,
        ui->tableViewHistory,
    };

    constexpr int kTouchRowHeight = 44;
    constexpr int kNormalRowHeight = 22;
    constexpr int kTouchBtnMinHeight = 44;
    constexpr int kTouchBtnMinWidth  = 44;

    for (auto *view : views) {
        auto *tableView = qobject_cast<QTableView *>(view);
        const int rowHeight = enabled ? kTouchRowHeight : kNormalRowHeight;
        if (tableView)
            tableView->verticalHeader()->setDefaultSectionSize(rowHeight);
        if (enabled)
            QScroller::grabGesture(view, QScroller::TouchGesture);
        else
            QScroller::ungrabGesture(view);
    }

    const auto buttons = findChildren<QPushButton *>();
    for (auto *btn : buttons) {
        btn->setMinimumHeight(enabled ? kTouchBtnMinHeight : 0);
        btn->setMinimumWidth(enabled ? kTouchBtnMinWidth : 0);
    }
}

