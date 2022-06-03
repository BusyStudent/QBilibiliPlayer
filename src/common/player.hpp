#pragma once

#include <QtMultimediaWidgets/QGraphicsVideoItem>
#include <QtMultimedia/QMediaPlayer>
#include <QtMultimedia/QMediaPlaylist>

#include <QGraphicsTextItem>
#include <QGraphicsScene>
#include <QGraphicsView>

#include "defs.hpp"
#include "app.hpp"

//We cached 2 resource at once time
#define PLAYER_CACHES_SIZE 2

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
        
        enum Size {
            Small = 18,
            Medium = 25,
            Large = 32,
        }size;

        QColor color;
        QString text;
        qreal position;//< Which second the danmaku appears
        uint32_t level;//< Level from 1 to 10

        bool isRegular() const {
            return type == Regular1 || type == Regular2 || type == Regular3;
        }
        bool isReserve() const {
            return type == Reserve;
        }
        bool isMoveable() const{
            return isRegular() || type == Reserve;
        }
};

class DanmakuItem : public QGraphicsTextItem {
    public:
        using QGraphicsTextItem::QGraphicsTextItem;

        const Danmaku *info;
        qreal deadtime;
};

/**
 * @brief The Interface to play a list of resource like one single resource
 * 
 */
class MediaPlayer : public QObject {
    Q_OBJECT

    signals:
        void error(QMediaPlayer::Error error);
        void durationChanged(qint64 duration);
        void positionChanged(qint64 position);
        void stateChanged(QMediaPlayer::State state);
        void mediaStatusChanged(QMediaPlayer::MediaStatus status);
        void bufferStatusChanged(int percentFilled);
        void volumeChanged(int volume);

        void nativeSizeChanged(const QSizeF &size);
    private slots:
        //Map cached resource to the player
        void _playerDurationChanged(qint64 duration);
        void _playerPositionChanged(qint64 position);
        void _playerStateChanged(QMediaPlayer::State state);
        void _playerError(QMediaPlayer::Error error);
        void _playerMediaStatusChanged(QMediaPlayer::MediaStatus status);
        void _playerBufferStatusChanged(int percent);
        void _playerVolumeChanged(int volume);
        void _itemNativeSizeChanged(const QSizeF &size);
    private:
        QMediaPlayer *_currentPlayer(){
            return currentPlayer;
        }
        QMediaPlayer *_nextPlayer(){
            if(&player1 == _currentPlayer())
                return &player2;
            else
                return &player1;
        }
        QGraphicsVideoItem *_currentVideoItem(){
            if(&player1 == _currentPlayer())
                return player1_item;
            else
                return player2_item;
        }
        QGraphicsVideoItem *_nextVideoItem(){
            if(&player1 == _currentPlayer())
                return player2_item;
            else
                return player1_item;
        }

        void _setCurrentPlayer(QMediaPlayer *player);
        const char *_currentPlayerName(){
            if(&player1 == _currentPlayer())
                return "player1";
            else
                return "player2";
        }
        const char *_nextPlayerName(){
            if(&player1 == _currentPlayer())
                return "player2";
            else
                return "player1";
        }
        const char *_nameOfPlayer(QObject *player){
            if(&player1 == player)
                return "player1";
            else
                return "player2";
        }
    public:
        MediaPlayer(QObject *parent = nullptr);
        ~MediaPlayer();

        void setVideoOutput(QGraphicsScene *scene);
        void setOutputRect(qreal x,qreal y,qreal w,qreal h);

        void setMedia(VideoResource *res);

        void pause();
        void play();
        void setPosition(qint64 pos);

        qint64 position() const;
        qint64 duration() const;
        qint64 volume() const;
        
        QSizeF nativeSize() {
            return _currentVideoItem()->nativeSize();
        }

        bool isPaused() const {
            return currentPlayer->state() == QMediaPlayer::PausedState;
        }
        bool hasMedia() const {
            return resource != nullptr;
        }
        void setVolume(int vol) {
            player1.setVolume(vol);
            player2.setVolume(vol);
        }
    private:
        //Switch between 2 players
        QMediaPlayer player1;
        QMediaPlayer player2;
        QGraphicsVideoItem *player1_item;
        QGraphicsVideoItem *player2_item;

        QMediaPlayer *currentPlayer = &player1;

        VideoResource *resource;
        QGraphicsVideoItem  empty_item;
        size_t cur_segment = 0;
};

class Player : public QGraphicsView {
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
         */
        void play(const VideoResource &res);
        void pause() {
            mediaPlayer()->pause();
        }
        void resume() {
            mediaPlayer()->play();
            danmakuPlay();
        }
        void stop();
        //position in ms
        qint64 position() {
            return mediaPlayer()->position();
        }
        void setPosition(qint64 pos) {
            danmakuSeek(pos);
            mediaPlayer()->setPosition(pos);
            qDeleteAll(danmaku_group->childItems());
        }
        //Danmaku variables
        void setDanmakuVisible(bool visible) {
            danmaku_group->setVisible(visible);
        }
        bool isDanmakuVisible() {
            return danmaku_group->isVisible();
        }

        MediaPlayer *mediaPlayer() {
            return &player;
        }
    private slots:
        void forwardError(QMediaPlayer::Error error);
        // void paused();
        // void stopped();
    private:
        void durationChanged(qint64 duration);
        void positionChanged(qint64 position);
        void stateChanged(QMediaPlayer::State);

        //Event
        void resizeEvent(QResizeEvent *event) override;
        void timerEvent(QTimerEvent *event) override;
        void keyPressEvent(QKeyEvent *event) override;

        //Player set size
        void nativeSizeChanged(const QSizeF &size);
        void fit();

        //Danmaku
        void danmakuSeek(qint64 position);
        void danmakuPlay();
        void danmakuPause();
        void danmakuClear();

        //Screen size
        QSizeF native_size = QSizeF(-1,-1);

        QGraphicsScene scene;
        QGraphicsItemGroup *danmaku_group;//< Danmaku group alwasy is screen size
        QGraphicsItemGroup *control_group;//< Control group alwasy is screen size

        QList<Danmaku>::const_iterator danmaku_iter;//< Current position of danmaku
        QList<Danmaku> danmaku_list;

        //Resource
        VideoResource video_res;
        
        bool video_ready = false;

        MediaPlayer player;

        int danmaku_timer = 0;

        int alive_time = 7;// Single danmaku alive time
        qint64 video_duration = -1;

        qreal danmaku_prev_time;//< Last position danmaku update
        qint64 danmaku_fps = 60;//< TODO : Detect FPS
        bool danmaku_started = false;

        //Outline
        QPen outpen = QPen(Qt::black,0.5,Qt::SolidLine);
};

PLAYER_NS_END