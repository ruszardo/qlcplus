/*
  Q Light Controller Plus
  videoprovider.h

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

#ifndef VIDEOPROVIDER_H
#define VIDEOPROVIDER_H

#include <QMediaPlayer>
#include <QObject>
#include <QHash>
#include <QRect>

#include "video.h"

class Doc;
class QVideoWidget;

class VideoWidget final : public QObject
{
    Q_OBJECT

public:
    VideoWidget(Video *video, QObject *parent = NULL);

protected slots:
    void slotSourceUrlChanged(QString url);
    void slotTotalTimeChanged(qint64 duration);
    void slotStatusChanged(QMediaPlayer::MediaStatus status);
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    void slotMetaDataChanged(QString key, QVariant data);
#else
    void slotMetaDataChanged();
#endif
    void slotPlaybackVideo();
    void slotSetPause(bool enable);
    void slotStopVideo();
    void slotBrightnessVolumeAdjust(qreal value);

private:
    int getScreenCount() const;

    /** Remember the current geometry of the video window,
     *  if it is visible and not in fullscreen mode */
    void storeWindowGeometry();

    /** Return a string describing the current screens setup,
     *  used to detect changes since the window geometry was stored */
    QString screenLayoutSignature() const;

protected:
    /** Catch the video window being closed by the user */
    bool eventFilter(QObject *obj, QEvent *event) override;

    /** reference to the actual Video Function */
    Video *m_video;
    /** output interface to render video data */
    QMediaPlayer *m_videoPlayer;
    /** Qt widget that actually displays the video */
    QVideoWidget *m_videoWidget;
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    QAudioOutput *m_audioOutput;
#endif
    /** Last geometry of the video window in windowed mode.
     *  Runtime only. An invalid rect means default geometry */
    QRect m_windowGeometry;
    /** Whether the video window was maximized when m_windowGeometry was stored */
    bool m_windowMaximized;
    /** Default placement of the video window (frame position and client size),
     *  invalid when the remembered geometry has been applied */
    QRect m_defaultGeometry;
    /** Target screen index of the last windowed playback */
    int m_windowScreen;
    /** Screens setup when m_windowGeometry was stored */
    QString m_windowScreenLayout;

private:
    FunctionParent functionParent() const;
};

class VideoProvider final : public QObject
{
    Q_OBJECT
public:
    VideoProvider(Doc *doc, QObject *parent);
    ~VideoProvider();

protected slots:
    void slotFunctionAdded(quint32 id);
    void slotFunctionRemoved(quint32 id);

private:
    Doc *m_doc;
    QHash<quint32, VideoWidget *> m_videoMap;
};

#endif // VIDEOPROVIDER_H
