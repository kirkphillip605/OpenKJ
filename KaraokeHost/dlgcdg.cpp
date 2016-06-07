/*
 * Copyright (c) 2013-2016 Thomas Isaac Lightburn
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

#include "dlgcdg.h"
#include "ui_dlgcdg.h"
#include <QDebug>
#include <QApplication>
#include <QDesktopWidget>

extern KhSettings *settings;


DlgCdg::DlgCdg(QWidget *parent, Qt::WindowFlags f) :
    QDialog(parent, f),
    ui(new Ui::DlgCdg)
{
    ui->setupUi(this);
//    settings = new KhSettings(this);
//    canvas = new QGLCanvas(this);
    canvas = new CdgVideoWidget(this);
    //    ui->verticalLayout->addWidget(canvas);
    ui->verticalLayout_2->addWidget(canvas);
    canvas->repaint();
    m_fullScreen = false;
    m_lastSize.setWidth(300);
    m_lastSize.setHeight(216);
    ticker = new ScrollText(this);
    ticker->setFont(settings->tickerFont());
    ticker->setMinimumHeight(settings->tickerHeight());
    ticker->setMaximumHeight(settings->tickerHeight());
    ticker->setSpeed(settings->tickerSpeed());
    QPalette palette = ticker->palette();
    palette.setColor(ticker->foregroundRole(), settings->tickerTextColor());
    ticker->setPalette(palette);
    palette = this->palette();
    palette.setColor(QPalette::Background, settings->tickerBgColor());
    this->setPalette(palette);
    ticker->setText("This is some text to scroll - This is some text to scroll - This is some text to scroll - This is some text to scroll - This is some text to scroll - This is some text to scroll - This is some text to scroll - This is some text to scroll");
    ui->verticalLayout_2->addWidget(ticker);

    connect(settings, SIGNAL(tickerFontChanged()), this, SLOT(tickerFontChanged()));
    connect(settings, SIGNAL(tickerHeightChanged(int)), this, SLOT(tickerHeightChanged()));
    connect(settings, SIGNAL(tickerSpeedChanged()), this, SLOT(tickerSpeedChanged()));
    connect(settings, SIGNAL(tickerTextColorChanged()), this, SLOT(tickerTextColorChanged()));
    connect(settings, SIGNAL(tickerBgColorChanged()), this, SLOT(tickerBgColorChanged()));
    connect(settings, SIGNAL(tickerEnableChanged()), this, SLOT(tickerEnableChanged()));
    canvas->videoSurface()->start();
}

DlgCdg::~DlgCdg()
{
    delete ui;
}

void DlgCdg::updateCDG(QImage image, bool overrideVisibleCheck)
{
    if ((isVisible()) || (overrideVisibleCheck))
    {
        canvas->videoSurface()->present(QVideoFrame(image));
//        canvas->setImage(image);
//        canvas->repaint();
    }
}

void DlgCdg::makeFullscreen()
{
    m_lastSize.setHeight(height());
    m_lastSize.setWidth(width());
    Qt::WindowFlags flags;
    flags |= Qt::Window;
    flags |= Qt::FramelessWindowHint;
    flags |= Qt::WindowStaysOnTopHint;
    setWindowFlags(flags);
    move(QApplication::desktop()->screenGeometry(settings->cdgWindowFullScreenMonitor()).topLeft());
    QRect screenDimensions = QApplication::desktop()->screenGeometry(settings->cdgWindowFullScreenMonitor());
    resize(screenDimensions.width(),screenDimensions.height());
    show();
    QImage cdgBg;
    if (settings->cdgDisplayBackgroundImage() != "")
    {
        qDebug() << "Attempting to load CDG background: " << settings->cdgDisplayBackgroundImage();
        if (!cdgBg.load(settings->cdgDisplayBackgroundImage()))
        {
            qDebug() << "Failed to load, loading default resource";
            cdgBg.load(":/icons/Icons/openkjlogo1.png");
        }
        else
            qDebug() << "Loaded OK";
    }
    else
    {
        cdgBg.load(":/icons/Icons/openkjlogo1.png");
        qDebug() << "No CDG background image specified, loading default resource";
    }
    updateCDG(cdgBg);
    m_fullScreen = true;
}

void DlgCdg::makeWindowed()
{
    setWindowFlags(Qt::Window | Qt::WindowStaysOnTopHint);
    resize(300, 216);
    settings->saveWindowState(this);
    show();
    canvas->repaint();
    m_fullScreen = false;
}

void DlgCdg::setTickerText(QString text)
{
    ticker->setText(text);
}

void DlgCdg::setFullScreen(bool fullscreen)
{
    if (fullscreen)
        makeFullscreen();
    else
        makeWindowed();
}

void DlgCdg::setFullScreenMonitor(int monitor)
{
    Q_UNUSED(monitor);
    makeWindowed();
    makeFullscreen();
}

void DlgCdg::tickerFontChanged()
{
    qDebug() << "tickerFontSettingsChanged() fired";
    ticker->setFont(settings->tickerFont());
    ticker->refresh();
 //   QFontMetrics fm(ticker->font());
    int newHeight = QFontMetrics(ticker->font()).height() * 1.2;
    settings->setTickerHeight(newHeight);
   // ticker->setFixedHeight(QFontMetrics(ticker->font()).height() * 1.2);
}

void DlgCdg::tickerHeightChanged()
{
    ticker->setMinimumHeight(settings->tickerHeight());
    ticker->setMaximumHeight(settings->tickerHeight());
    ticker->refresh();
}

void DlgCdg::tickerSpeedChanged()
{
    ticker->setSpeed(settings->tickerSpeed());
}

void DlgCdg::tickerTextColorChanged()
{
    QPalette palette = ticker->palette();
    palette.setColor(ticker->foregroundRole(), settings->tickerTextColor());
    ticker->setPalette(palette);
}

void DlgCdg::tickerBgColorChanged()
{
    QPalette palette = this->palette();
    palette.setColor(QPalette::Background, settings->tickerBgColor());
    this->setPalette(palette);
}

void DlgCdg::tickerEnableChanged()
{
    ticker->enable(settings->tickerEnabled());
}

void DlgCdg::mouseDoubleClickEvent(QMouseEvent *e)
{
    Q_UNUSED(e);
    if (m_fullScreen)
    {
        makeWindowed();
    }
    else
        makeFullscreen();
}

