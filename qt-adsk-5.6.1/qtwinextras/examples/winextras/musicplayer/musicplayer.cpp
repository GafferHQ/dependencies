/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the examples of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:BSD$
** You may use this file under the terms of the BSD license as follows:
**
** "Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions are
** met:
**   * Redistributions of source code must retain the above copyright
**     notice, this list of conditions and the following disclaimer.
**   * Redistributions in binary form must reproduce the above copyright
**     notice, this list of conditions and the following disclaimer in
**     the documentation and/or other materials provided with the
**     distribution.
**   * Neither the name of The Qt Company Ltd nor the names of its
**     contributors may be used to endorse or promote products derived
**     from this software without specific prior written permission.
**
**
** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
** "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
** LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
** A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
** OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
** SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
** LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
** OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE."
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "musicplayer.h"
#include "volumebutton.h"

#include <QtWidgets>
#include <QtWinExtras>

MusicPlayer::MusicPlayer(QWidget *parent) : QWidget(parent),
    taskbarButton(0), taskbarProgress(0), thumbnailToolBar(0),
    playToolButton(0), forwardToolButton(0), backwardToolButton(0),
    mediaPlayer(0), playButton(0), volumeButton(0),
    positionSlider(0), positionLabel(0), infoLabel(0)
{
    createWidgets();
    createShortcuts();
    createJumpList();
    createTaskbar();
    createThumbnailToolBar();

    connect(&mediaPlayer, &QMediaPlayer::positionChanged, this, &MusicPlayer::updatePosition);
    connect(&mediaPlayer, &QMediaPlayer::durationChanged, this, &MusicPlayer::updateDuration);
    connect(&mediaPlayer, &QMediaObject::metaDataAvailableChanged, this, &MusicPlayer::updateInfo);

    typedef void(QMediaPlayer::*ErrorSignal)(QMediaPlayer::Error);
    connect(&mediaPlayer, static_cast<ErrorSignal>(&QMediaPlayer::error),
            this, &MusicPlayer::handleError);
    connect(&mediaPlayer, &QMediaPlayer::stateChanged,
            this, &MusicPlayer::updateState);

    stylize();
}

void MusicPlayer::openFile()
{
    const QStringList musicPaths = QStandardPaths::standardLocations(QStandardPaths::MusicLocation);
    const QString filePath =
        QFileDialog::getOpenFileName(this, tr("Open File"),
                                     musicPaths.isEmpty() ? QDir::homePath() : musicPaths.first(),
                                     tr("MP3 files (*.mp3);;All files (*.*)"));
    if (!filePath.isEmpty())
        playFile(filePath);
}

void MusicPlayer::playFile(const QString &filePath)
{
    playButton->setEnabled(true);
    infoLabel->setText(QFileInfo(filePath).fileName());

    mediaPlayer.setMedia(QUrl::fromLocalFile(filePath));
    mediaPlayer.play();
}

void MusicPlayer::togglePlayback()
{
    if (mediaPlayer.mediaStatus() == QMediaPlayer::NoMedia)
        openFile();
    else if (mediaPlayer.state() == QMediaPlayer::PlayingState)
        mediaPlayer.pause();
    else
        mediaPlayer.play();
}

void MusicPlayer::seekForward()
{
    positionSlider->triggerAction(QSlider::SliderPageStepAdd);
}

void MusicPlayer::seekBackward()
{
    positionSlider->triggerAction(QSlider::SliderPageStepSub);
}

//! [0]
bool MusicPlayer::event(QEvent *event)
{
    if (event->type() == QWinEvent::CompositionChange || event->type() == QWinEvent::ColorizationChange)
        stylize();
    return QWidget::event(event);
}
//! [0]

void MusicPlayer::mousePressEvent(QMouseEvent *event)
{
    offset = event->globalPos() - pos();
    event->accept();
}

void MusicPlayer::mouseMoveEvent(QMouseEvent *event)
{
    move(event->globalPos() - offset);
    event->accept();
}

void MusicPlayer::mouseReleaseEvent(QMouseEvent *event)
{
    offset = QPoint();
    event->accept();
}

//! [1]
void MusicPlayer::stylize()
{
    if (QtWin::isCompositionEnabled()) {
        QtWin::extendFrameIntoClientArea(this, -1, -1, -1, -1);
        setAttribute(Qt::WA_TranslucentBackground, true);
        setAttribute(Qt::WA_NoSystemBackground, false);
        setStyleSheet("MusicPlayer { background: transparent; }");
    } else {
        QtWin::resetExtendedFrame(this);
        setAttribute(Qt::WA_TranslucentBackground, false);
        setStyleSheet(QString("MusicPlayer { background: %1; }").arg(QtWin::realColorizationColor().name()));
    }
    volumeButton->stylize();
}
//! [1]

void MusicPlayer::updateState(QMediaPlayer::State state)
{
    if (state == QMediaPlayer::PlayingState) {
        playButton->setToolTip(tr("Pause"));
        playButton->setIcon(style()->standardIcon(QStyle::SP_MediaPause));
    } else {
        playButton->setToolTip(tr("Play"));
        playButton->setIcon(style()->standardIcon(QStyle::SP_MediaPlay));
    }
}

void MusicPlayer::updatePosition(qint64 position)
{
    positionSlider->setValue(position);

    QTime duration(0, position / 60000, qRound((position % 60000) / 1000.0));
    positionLabel->setText(duration.toString(tr("mm:ss")));
}

void MusicPlayer::updateDuration(qint64 duration)
{
    positionSlider->setRange(0, duration);
    positionSlider->setEnabled(duration > 0);
    positionSlider->setPageStep(duration / 10);
}

void MusicPlayer::setPosition(int position)
{
    // avoid seeking when the slider value change is triggered from updatePosition()
    if (qAbs(mediaPlayer.position() - position) > 99)
        mediaPlayer.setPosition(position);
}

void MusicPlayer::updateInfo()
{
    QStringList info;
    QString author = mediaPlayer.metaData("Author").toString();
    if (!author.isEmpty())
        info += author;
    QString title = mediaPlayer.metaData("Title").toString();
    if (!title.isEmpty())
        info += title;
    if (!info.isEmpty())
        infoLabel->setText(info.join(tr(" - ")));
}

void MusicPlayer::handleError()
{
    playButton->setEnabled(false);
    infoLabel->setText(tr("Error: %1").arg(mediaPlayer.errorString()));
}

//! [2]
void MusicPlayer::updateTaskbar()
{
    switch (mediaPlayer.state()) {
    case QMediaPlayer::PlayingState:
        taskbarButton->setOverlayIcon(style()->standardIcon(QStyle::SP_MediaPlay));
        taskbarProgress->show();
        taskbarProgress->resume();
        break;
    case QMediaPlayer::PausedState:
        taskbarButton->setOverlayIcon(style()->standardIcon(QStyle::SP_MediaPause));
        taskbarProgress->show();
        taskbarProgress->pause();
        break;
    case QMediaPlayer::StoppedState:
        taskbarButton->setOverlayIcon(style()->standardIcon(QStyle::SP_MediaStop));
        taskbarProgress->hide();
        break;
    }
}
//! [2]

//! [3]
void MusicPlayer::updateThumbnailToolBar()
{
    playToolButton->setEnabled(mediaPlayer.duration() > 0);
    backwardToolButton->setEnabled(mediaPlayer.position() > 0);
    forwardToolButton->setEnabled(mediaPlayer.position() < mediaPlayer.duration());

    if (mediaPlayer.state() == QMediaPlayer::PlayingState) {
        playToolButton->setToolTip(tr("Pause"));
        playToolButton->setIcon(style()->standardIcon(QStyle::SP_MediaPause));
    } else {
        playToolButton->setToolTip(tr("Play"));
        playToolButton->setIcon(style()->standardIcon(QStyle::SP_MediaPlay));
    }
}
//! [3]

void MusicPlayer::createWidgets()
{
    playButton = new QToolButton(this);
    playButton->setEnabled(false);
    playButton->setToolTip(tr("Play"));
    playButton->setIcon(style()->standardIcon(QStyle::SP_MediaPlay));
    connect(playButton, &QAbstractButton::clicked, this, &MusicPlayer::togglePlayback);

    QAbstractButton *openButton = new QToolButton(this);
    openButton->setText(tr("..."));
    openButton->setToolTip(tr("Open a file..."));
    openButton->setFixedSize(playButton->sizeHint());
    connect(openButton, &QAbstractButton::clicked, this, &MusicPlayer::openFile);

    volumeButton = new VolumeButton(this);
    volumeButton->setToolTip(tr("Adjust volume"));
    volumeButton->setVolume(mediaPlayer.volume());
    connect(volumeButton, &VolumeButton::volumeChanged, &mediaPlayer, &QMediaPlayer::setVolume);

    positionSlider = new QSlider(Qt::Horizontal, this);
    positionSlider->setEnabled(false);
    positionSlider->setToolTip(tr("Seek"));
    connect(positionSlider, &QAbstractSlider::valueChanged, this, &MusicPlayer::setPosition);

    infoLabel = new QLabel(this);
    positionLabel = new QLabel(tr("00:00"), this);
    positionLabel->setMinimumWidth(positionLabel->sizeHint().width());

    QBoxLayout *controlLayout = new QHBoxLayout;
    controlLayout->setMargin(0);
    controlLayout->addWidget(openButton);
    controlLayout->addWidget(playButton);
    controlLayout->addWidget(positionSlider);
    controlLayout->addWidget(positionLabel);
    controlLayout->addWidget(volumeButton);

    QBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->addWidget(infoLabel);
    mainLayout->addLayout(controlLayout);
}

void MusicPlayer::createShortcuts()
{
    QShortcut *quitShortcut = new QShortcut(QKeySequence::Quit, this);
    connect(quitShortcut, &QShortcut::activated, QCoreApplication::quit);

    QShortcut *openShortcut = new QShortcut(QKeySequence::Open, this);
    connect(openShortcut, &QShortcut::activated, this, &MusicPlayer::openFile);

    QShortcut *toggleShortcut = new QShortcut(Qt::Key_Space, this);
    connect(toggleShortcut, &QShortcut::activated, this, &MusicPlayer::togglePlayback);

    QShortcut *forwardShortcut = new QShortcut(Qt::Key_Right, this);
    connect(forwardShortcut, &QShortcut::activated, this, &MusicPlayer::seekForward);

    QShortcut *backwardShortcut = new QShortcut(Qt::Key_Left, this);
    connect(backwardShortcut, &QShortcut::activated, this, &MusicPlayer::seekBackward);

    QShortcut *increaseShortcut = new QShortcut(Qt::Key_Up, this);
    connect(increaseShortcut, &QShortcut::activated, volumeButton, &VolumeButton::increaseVolume);

    QShortcut *decreaseShortcut = new QShortcut(Qt::Key_Down, this);
    connect(decreaseShortcut, &QShortcut::activated, volumeButton, &VolumeButton::descreaseVolume);
}

//! [4]
void MusicPlayer::createJumpList()
{
    QWinJumpList jumplist;
    jumplist.recent()->setVisible(true);
}
//! [4]

//! [5]
void MusicPlayer::createTaskbar()
{
    taskbarButton = new QWinTaskbarButton(this);
    taskbarButton->setWindow(windowHandle());

    taskbarProgress = taskbarButton->progress();
    connect(positionSlider, &QAbstractSlider::valueChanged, taskbarProgress, &QWinTaskbarProgress::setValue);
    connect(positionSlider, &QAbstractSlider::rangeChanged, taskbarProgress, &QWinTaskbarProgress::setRange);

    connect(&mediaPlayer, &QMediaPlayer::stateChanged, this, &MusicPlayer::updateTaskbar);
}
//! [5]

//! [6]
void MusicPlayer::createThumbnailToolBar()
{
    thumbnailToolBar = new QWinThumbnailToolBar(this);
    thumbnailToolBar->setWindow(windowHandle());

    playToolButton = new QWinThumbnailToolButton(thumbnailToolBar);
    playToolButton->setEnabled(false);
    playToolButton->setToolTip(tr("Play"));
    playToolButton->setIcon(style()->standardIcon(QStyle::SP_MediaPlay));
    connect(playToolButton, &QWinThumbnailToolButton::clicked, this, &MusicPlayer::togglePlayback);

    forwardToolButton = new QWinThumbnailToolButton(thumbnailToolBar);
    forwardToolButton->setEnabled(false);
    forwardToolButton->setToolTip(tr("Fast forward"));
    forwardToolButton->setIcon(style()->standardIcon(QStyle::SP_MediaSeekForward));
    connect(forwardToolButton, &QWinThumbnailToolButton::clicked, this, &MusicPlayer::seekForward);

    backwardToolButton = new QWinThumbnailToolButton(thumbnailToolBar);
    backwardToolButton->setEnabled(false);
    backwardToolButton->setToolTip(tr("Rewind"));
    backwardToolButton->setIcon(style()->standardIcon(QStyle::SP_MediaSeekBackward));
    connect(backwardToolButton, &QWinThumbnailToolButton::clicked, this, &MusicPlayer::seekBackward);

    thumbnailToolBar->addButton(backwardToolButton);
    thumbnailToolBar->addButton(playToolButton);
    thumbnailToolBar->addButton(forwardToolButton);

    connect(&mediaPlayer, &QMediaPlayer::positionChanged, this, &MusicPlayer::updateThumbnailToolBar);
    connect(&mediaPlayer, &QMediaPlayer::durationChanged, this, &MusicPlayer::updateThumbnailToolBar);
    connect(&mediaPlayer, &QMediaPlayer::stateChanged, this, &MusicPlayer::updateThumbnailToolBar);
}
//! [6]
