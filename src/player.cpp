#include "common/player.hpp"

#include <QDomDocument>
#include <QPainter>
#include <algorithm>

PLAYER_NS_BEGIN

Player::Player(QWidget *parent) : QVideoWidget(parent){
    player.setVideoOutput(this);

    //--Connect signals and slots
    connect(&player,SIGNAL(error(QMediaPlayer::Error)),this,SLOT(forwardError(QMediaPlayer::Error)));
    connect(&player,&QMediaPlayer::durationChanged,this,&Player::durationChanged);
    connect(&timer,&QTimer::timeout,this,&Player::doUpdate);

    //TODO ::Detect fps
    timer.setInterval(30);
}
Player::~Player(){

}

void Player::durationChanged(qint64 duration){
    qDebug() << "Got " << duration << "ms";
    video_duration = duration;

    //Try to play danmaku
    danmakuPlay();
}
void Player::positionChanged(qint64 position){
    if(!danmaku_started) return;
    // danmakuSeek(position/1000.0);
}
void Player::play(const QMediaContent &video,const QMediaContent &audio){
    danmakuClear();//< Stop danmaku / clear danmaku list
    video_duration = -1;//< Means not ready
    
    player.setMedia(video);
    player.play();
}
void Player::setDanmaku(const QString &str){
    QDomDocument doc;
    doc.setContent(str);
    QDomElement root = doc.documentElement();
    QDomNodeList nodes = root.elementsByTagName("d");
    for(int i = 0;i < nodes.size();i++){
        QDomElement e = nodes.at(i).toElement();
        auto list = e.attribute("p").splitRef(',');

        Danmaku danmaku;

        danmaku.position = list[0].toDouble();
        danmaku.type = Danmaku::Type(list[1].toInt());
        danmaku.size = list[2].toInt();
        danmaku.color = QColor(list[3]);

        danmaku.pool = Danmaku::Pool(list[5].toInt());
        danmaku.level = list[8].toInt();

        danmaku.text = e.text();

        danmaku_list.push_back(danmaku);

        // qDebug() << list << danmaku.text;
    }
    //Sort danmaku by position
    std::sort(danmaku_list.begin(),danmaku_list.end(),[](const Danmaku &a,const Danmaku &b){
        return a.position < b.position;
    });
    danmaku_iter = danmaku_list.cbegin();

    //Try play danmaku
    danmakuPlay();
}
void Player::danmakuClear(){
    timer.stop();

    danmaku_list.clear();
    playing_danmaku_list.clear();
    danmaku_started = false;
    danmaku_paused = false;
}
void Player::danmakuPlay(){
    if(danmaku_list.empty() || danmaku_started || video_duration == -1){
        qDebug() << "Try to play danmaku, but not ready";
        return;
    }
    qDebug() << "Start playing danmaku";
    //ready
    danmaku_started = true;
    danmaku_paused = false;

    danmakuSeek(player.position());

    //Config timer and start
    timer.start();
}
void Player::danmakuSeek(qint64 _pos){
    playing_danmaku_list.clear();

    qreal pos = _pos/1000.0;
    last_added = pos;

    while(danmaku_iter != danmaku_list.cend() && danmaku_iter->position < pos){
        ++danmaku_iter;
    }
    if(danmaku_iter != danmaku_list.cend()){
        qDebug() << "Seek to " << danmaku_iter->position << "Text:" << danmaku_iter->text;
    }
}

void Player::forwardError(QMediaPlayer::Error err){
    emit error(err);
}
PLAYER_NS_END