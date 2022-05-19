#pragma once

#include <QtMultimedia/QMediaPlayer>
#include <QtMultimedia/QMediaPlaylist>
#include <QtMultimediaWidgets/QVideoWidget>

#include <QLinkedList>
#include <QLabel>
#include <QTimer>

#include "defs.hpp"


PLAYER_NS_BEGIN

class Danmaku {
    public:
        enum Type {
            Regular1 = 1,
            Regular2 = 2,
            Regular3 = 3,
            Bottom = 4,
            Top = 5,
            Reserve = 6,
            Advanced = 7,
            Code = 8,
            Bas = 9,
        }type;
        enum Pool {
            RegularPool = 1,
            SubtitlePool = 2,
            SpecialPool = 3,
        }pool;
        QColor color;
        QString text;
        qreal position;//< Which second the danmaku appears
        uint32_t size;//< Size in px
        uint32_t level;//< Level from 1 to 10


        bool isRegular() const {
            return type == Regular1 || type == Regular2 || type == Regular3;
        }
};
class PlayingDanmaku {
    public:
        qreal x,y;// from 0 to 1
        qreal xadv;//< Advance normalized
        qreal disappear_time;//< Disappear time
        const Danmaku *danmaku;
};

class Player : public QVideoWidget {
    Q_OBJECT

    signals:
        void error(QMediaPlayer::Error error);
    public:
        Player(QWidget *parent = nullptr);
        ~Player();

        void setDanmaku(const QString &danmaku);
        /**
         * @brief Play the video
         * 
         * @param video 
         * @param audio 
         */
        void play(const QMediaContent &video,const QMediaContent &audio);
        void pause();
        void resume();
        void stop();
    private slots:
        void forwardError(QMediaPlayer::Error error);
        void positionChanged(qint64 position);
        // void paused();
        // void stopped();
    private:
        void durationChanged(qint64 duration);

        //Danmaku
        void danmakuSeek(qint64 position);
        void danmakuPlay();
        void danmakuPause();
        void danmakuClear();

        void doUpdate(){

        }

        QLinkedList<PlayingDanmaku> playing_danmaku_list;//< Danmaku list that is being displayed
        QList<Danmaku>::const_iterator danmaku_iter;//< Current position of danmaku
        QList<Danmaku> danmaku_list;

        QMediaPlayer player;
        QTimer timer;

        int alive_time = 5;// Single danmaku alive time
        qint64 video_duration = -1;

        qreal last_added;//< Last position danmaku update
        qint64 update_ticks = 30;
        bool danmaku_started = false;
        bool danmaku_paused = false;
};

PLAYER_NS_END