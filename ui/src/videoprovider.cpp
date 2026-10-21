/*
  Q Light Controller Plus
  videoprovider.cpp

  Copyright (c) Massimo Callegari

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.
*/

#include <QVersionNumber>
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
 #include <QMediaMetaData>
 #include <QAudioOutput>
#endif
#include <QApplication>
#include <QMediaPlayer>
#include <QVideoWidget>
#include <QScreen>
#include <QEvent>

#include "videoprovider.h"
#include "qlcfile.h"
#include "doc.h"

VideoProvider::VideoProvider(Doc *doc, QObject *parent)
    : QObject(parent)
    , m_doc(doc)
{
    Q_ASSERT(doc != NULL);
    connect(m_doc, SIGNAL(functionAdded(quint32)),
            this, SLOT(slotFunctionAdded(quint32)));
    connect(m_doc, SIGNAL(functionRemoved(quint32)),
            this, SLOT(slotFunctionRemoved(quint32)));
}

VideoProvider::~VideoProvider()
{
    m_videoMap.clear();
}

void VideoProvider::slotFunctionAdded(quint32 id)
{
    Function *func = m_doc->function(id);
    if (func == NULL)
        return;

    if (func->type() == Function::VideoType)
    {
        VideoWidget *vWidget = new VideoWidget(qobject_cast<Video *>(func));
        m_videoMap[id] = vWidget;
    }
}

void VideoProvider::slotFunctionRemoved(quint32 id)
{
    if (m_videoMap.contains(id))
    {
        VideoWidget *vw = m_videoMap.take(id);
        delete vw;
    }
}

/*********************************************************************
 * VideoWidget class implementation
 *********************************************************************/

VideoWidget::VideoWidget(Video *video, QObject *parent)
    : QObject(parent)
    , m_video(video)
    , m_videoPlayer(NULL)
    , m_videoWidget(NULL)
    , m_windowMaximized(false)
    , m_windowScreen(-1)
{
    Q_ASSERT(video != NULL);

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    m_videoPlayer = new QMediaPlayer(this, QMediaPlayer::VideoSurface);
#else
    m_videoPlayer = new QMediaPlayer(this);
    m_audioOutput = new QAudioOutput(this);
    m_videoPlayer->setAudioOutput(m_audioOutput);
#endif
    m_videoPlayer->moveToThread(QCoreApplication::instance()->thread());

    if (QLCFile::getQtRuntimeVersion() >= 50700 && m_videoWidget == NULL)
    {
        m_videoWidget = new QVideoWidget;
        m_videoWidget->setStyleSheet("background-color:black;");
        m_videoWidget->installEventFilter(this);
        m_videoPlayer->setVideoOutput(m_videoWidget);
    }

    connect(m_videoPlayer, SIGNAL(mediaStatusChanged(QMediaPlayer::MediaStatus)),
            this, SLOT(slotStatusChanged(QMediaPlayer::MediaStatus)));
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    connect(m_videoPlayer, SIGNAL(metaDataChanged(QString,QVariant)),
            this, SLOT(slotMetaDataChanged(QString,QVariant)));
#else
    connect(m_videoPlayer, SIGNAL(metaDataChanged()),
            this, SLOT(slotMetaDataChanged()));
#endif
    connect(m_videoPlayer, SIGNAL(durationChanged(qint64)),
            this, SLOT(slotTotalTimeChanged(qint64)));

    connect(m_video, SIGNAL(sourceChanged(QString)),
            this, SLOT(slotSourceUrlChanged(QString)));
    connect(m_video, SIGNAL(requestPlayback()),
            this, SLOT(slotPlaybackVideo()));
    connect(m_video, SIGNAL(requestPause(bool)),
            this, SLOT(slotSetPause(bool)));
    connect(m_video, SIGNAL(requestStop()),
            this, SLOT(slotStopVideo()));
    connect(m_video, SIGNAL(requestBrightnessVolumeAdjust(qreal)),
            this, SLOT(slotBrightnessVolumeAdjust(qreal)));

    QString sourceURL = m_video->sourceUrl();
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    if (sourceURL.contains("://"))
        m_videoPlayer->setMedia(QUrl(sourceURL));
    else
        m_videoPlayer->setMedia(QUrl::fromLocalFile(sourceURL));
#else
    if (sourceURL.contains("://"))
        m_videoPlayer->setSource(QUrl(sourceURL));
    else
        m_videoPlayer->setSource(QUrl::fromLocalFile(sourceURL));
#endif
    qDebug() << "Video source URL:" << sourceURL;
}

void VideoWidget::slotSourceUrlChanged(QString url)
{
    qDebug() << "Video source URL changed:" << url;

    // a different video gets the default window geometry
    m_windowGeometry = QRect();

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    if (url.contains("://"))
        m_videoPlayer->setMedia(QUrl(url));
    else
        m_videoPlayer->setMedia(QUrl::fromLocalFile(url));
#else
    if (url.contains("://"))
        m_videoPlayer->setSource(QUrl(url));
    else
        m_videoPlayer->setSource(QUrl::fromLocalFile(url));
#endif
}

void VideoWidget::slotTotalTimeChanged(qint64 duration)
{
    qDebug() << "Video duration: " << duration;
    m_video->setTotalDuration(duration);
}

void VideoWidget::slotStatusChanged(QMediaPlayer::MediaStatus status)
{
    qDebug() << Q_FUNC_INFO << status;
    switch (status)
    {
        case QMediaPlayer::NoMedia:
        case QMediaPlayer::LoadedMedia:
        case QMediaPlayer::BufferingMedia:
        case QMediaPlayer::BufferedMedia:
            //setStatusInfo(QString());
        break;
        case QMediaPlayer::LoadingMedia:
            //setStatusInfo(tr("Loading..."));
        break;
        case QMediaPlayer::StalledMedia:
            //setStatusInfo(tr("Media Stalled"));
        break;
        case QMediaPlayer::EndOfMedia:
        {
            if (m_videoPlayer != NULL)
                m_videoPlayer->stop();

            if (m_video->runOrder() == Video::Loop)
            {
                m_videoPlayer->play();
                break;
            }

            if (m_videoWidget != NULL)
            {
                storeWindowGeometry();
                m_videoWidget->hide();
            }

            m_video->stop(functionParent());
            break;
        }
        default:
        case QMediaPlayer::InvalidMedia:
            //displayErrorMessage();
        break;
    }
}

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
void VideoWidget::slotMetaDataChanged(QString key, QVariant data)
{
    if (m_video == NULL)
        return;

    qDebug() << Q_FUNC_INFO << "Got meta data:" << key;
    if (key == "Resolution")
        m_video->setResolution(data.toSize());
    else if (key == "VideoCodec")
        m_video->setVideoCodec(data.toString());
    else if (key == "AudioCodec")
        m_video->setAudioCodec(data.toString());
}
#else
void VideoWidget::slotMetaDataChanged()
{
    if (m_video == NULL)
        return;

    QMediaMetaData md = m_videoPlayer->metaData();
    foreach (QMediaMetaData::Key k, md.keys())
    {
        qDebug() << "[Metadata]" << md.metaDataKeyToString(k) << ":" << md.stringValue(k);
        switch (k)
        {
            case QMediaMetaData::Resolution:
                m_video->setResolution(md.value(k).toSize());
            break;
            case QMediaMetaData::VideoCodec:
                m_video->setVideoCodec(md.stringValue(k));
            break;
            case QMediaMetaData::AudioCodec:
                m_video->setAudioCodec(md.stringValue(k));
            break;
            default:
            break;
        }
    }
}
#endif

void VideoWidget::slotPlaybackVideo()
{
    int screen = m_video->screen();
    QList<QScreen*> screens = QGuiApplication::screens();
    QScreen *scr = screens.count() > screen ? screens.at(screen) : screens.first();
    QRect rect = scr->availableGeometry();

    if (QLCFile::getQtRuntimeVersion() < 50700 && m_videoWidget == NULL)
    {
        m_videoWidget = new QVideoWidget;
        m_videoWidget->setStyleSheet("background-color:black;");
        m_videoWidget->installEventFilter(this);
        m_videoPlayer->setVideoOutput(m_videoWidget);
    }

    m_videoWidget->setWindowFlags(m_videoWidget->windowFlags() | Qt::WindowStaysOnTopHint);

    if (m_video->fullscreen() == false)
    {
        QSize resolution = m_video->resolution();
        m_videoWidget->setFullScreen(false);

        // forget the last window geometry if the screens setup
        // or the Video target screen have changed in the meantime
        if (m_windowScreen != screen || m_windowScreenLayout != screenLayoutSignature())
            m_windowGeometry = QRect();
        m_windowScreen = screen;

        if (m_windowGeometry.isValid())
        {
            m_defaultGeometry = QRect();
            m_videoWidget->setGeometry(m_windowGeometry);
            if (m_windowMaximized)
                m_videoWidget->setWindowState(m_videoWidget->windowState() | Qt::WindowMaximized);
        }
        else
        {
            if (resolution.isEmpty())
                m_videoWidget->setGeometry(0, 50, 640, 480);
            else
                m_videoWidget->setGeometry(0, 50, resolution.width(), resolution.height());
            m_videoWidget->move(rect.topLeft());
            m_defaultGeometry = QRect(rect.topLeft(), m_videoWidget->size());
        }
    }
    else
    {
#if defined(WIN32) || defined(Q_OS_WIN)
        m_videoWidget->setFullScreen(true);
        m_videoWidget->setGeometry(rect);
#else
        m_videoWidget->setGeometry(rect);
        m_videoWidget->setFullScreen(true);
#endif
    }

    if (m_videoPlayer->isSeekable())
        m_videoPlayer->setPosition(m_video->elapsed());
    else
        m_videoPlayer->setPosition(0);

    m_videoWidget->show();
    m_videoPlayer->play();
}

void VideoWidget::slotSetPause(bool enable)
{
    if (enable)
        m_videoPlayer->pause();
    else
        m_videoPlayer->play();
}

void VideoWidget::slotStopVideo()
{
    if (m_videoPlayer != NULL)
        m_videoPlayer->stop();

    if (m_videoWidget != NULL)
    {
        // must be done before leaving fullscreen mode
        storeWindowGeometry();

        if (m_video->fullscreen())
            m_videoWidget->setFullScreen(false);
        m_videoWidget->hide();
    }

    m_video->stop(functionParent());
}

void VideoWidget::slotBrightnessVolumeAdjust(qreal value)
{
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    int brightness = -100 + (int)(qreal(100.0) * value);
    int volume = 100 * (int)QAudio::convertVolume(value, QAudio::LogarithmicVolumeScale, QAudio::LinearVolumeScale);
    if (m_videoWidget)
        m_videoWidget->setBrightness(brightness);
    if (m_videoPlayer)
        m_videoPlayer->setVolume(volume);
#else
    qreal linearVolume = QAudio::convertVolume(value, QAudio::LogarithmicVolumeScale, QAudio::LinearVolumeScale);
    if (m_audioOutput)
        m_audioOutput->setVolume(linearVolume);
#endif
}

void VideoWidget::storeWindowGeometry()
{
    if (m_videoWidget == NULL || m_videoWidget->isVisible() == false ||
        m_videoWidget->isFullScreen())
        return;

    // the user has not moved or resized the window placed by default,
    // so keep using the default (e.g. the video resolution might be known later)
    if (m_defaultGeometry.isValid() &&
        m_videoWidget->frameGeometry().topLeft() == m_defaultGeometry.topLeft() &&
        m_videoWidget->size() == m_defaultGeometry.size())
        return;

    // geometry() and setGeometry() both refer to the client area,
    // so the window frame does not shift the position at each playback
    m_windowMaximized = m_videoWidget->isMaximized();
    if (m_windowMaximized)
        m_windowGeometry = m_videoWidget->normalGeometry();
    else
        m_windowGeometry = m_videoWidget->geometry();
    m_windowScreenLayout = screenLayoutSignature();
}

QString VideoWidget::screenLayoutSignature() const
{
    QString signature;
    QScreen *primary = QGuiApplication::primaryScreen();

    foreach (QScreen *scr, QGuiApplication::screens())
    {
        QRect geom = scr->geometry();
        signature.append(QString("%1,%2,%3,%4,%5,%6;")
                         .arg(geom.x()).arg(geom.y())
                         .arg(geom.width()).arg(geom.height())
                         .arg(scr->devicePixelRatio())
                         .arg(scr == primary ? 1 : 0));
    }

    return signature;
}

bool VideoWidget::eventFilter(QObject *obj, QEvent *event)
{
    // the user closed the video window. Close is received
    // before the window is hidden, so its geometry is still valid
    if (obj == m_videoWidget && event->type() == QEvent::Close)
        storeWindowGeometry();

    return QObject::eventFilter(obj, event);
}

int VideoWidget::getScreenCount() const
{
    int screenCount = QGuiApplication::screens().count();

    return screenCount;
}

FunctionParent VideoWidget::functionParent() const
{
    return FunctionParent::master();
}
