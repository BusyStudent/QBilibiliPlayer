#include "common/player.hpp"


#include <QTextCharFormat>
#include <QTextDocument>
#include <QTextCursor>
#include <QDomDocument>
#include <QTimerEvent>
#include <algorithm>

PLAYER_NS_BEGIN

MediaPlayer::MediaPlayer(QObject *parent) : QObject(parent){
    //Create mediaplayer
    QMediaPlayer *arr [] = {
        &player1,
        &player2
    };

    for(auto p : arr){
        //Connect signals
        connect(p,SIGNAL(error(QMediaPlayer::Error)),this,SLOT(_playerError(QMediaPlayer::Error)));
        connect(p,SIGNAL(durationChanged(qint64)),this,SLOT(_playerDurationChanged(qint64)));
        connect(p,SIGNAL(positionChanged(qint64)),this,SLOT(_playerPositionChanged(qint64)));
        connect(p,SIGNAL(stateChanged(QMediaPlayer::State)),this,SLOT(_playerStateChanged(QMediaPlayer::State)));
        connect(p,SIGNAL(mediaStatusChanged(QMediaPlayer::MediaStatus)),this,SLOT(_playerMediaStatusChanged(QMediaPlayer::MediaStatus)));
        connect(p,SIGNAL(bufferStatusChanged(int)),this,SLOT(_playerBufferStatusChanged(int)));
        //Make mute

        p->setMuted(true);
    }

}
MediaPlayer::~MediaPlayer(){
    //Stop all players
    player1.stop();
    player2.stop();
}
void MediaPlayer::setVideoOutput(QGraphicsVideoItem *item){
    video_item = item;
    //Set video output
}
void MediaPlayer::setMedia(VideoResource *res){
    resource = res;

    QMediaPlayer *arr [] = {
        &player1,
        &player2
    };
    //Stop all players
    for(auto p : arr){
        p->stop();
        p->setMuted(true);
        p->setVideoOutput(&empty_item);
    }
    currentPlayer = &player1;
    cur_segment = 0;
    //Set media
    player1.setMedia(resource->videos[0]);
    player1.setVideoOutput(video_item);
    player1.setMuted(false);

    if(!resource->single_video){
        //Begin caching
        player2.setMedia(resource->videos[1]);
        player2.play();
    }
}
void MediaPlayer::play(){
    if(currentPlayer->state() == QMediaPlayer::PlayingState){
        return;
    }
    currentPlayer->play();
}

void MediaPlayer::_playerError(QMediaPlayer::Error e){
    //Emit error
    emit error(e);
}
void MediaPlayer::_playerDurationChanged(qint64 duration){
    //Emit duration changed
    if(resource->single_video){
        emit durationChanged(duration);
    }
    else{
        //Means this video is ready to play
        if(sender() == currentPlayer){
            emit durationChanged(resource->duration);
        }
        else if(sender() == _nextPlayer() && duration != 0){
            //Cached done,pause
            mplayerDebug() << _nameOfPlayer(sender()) << "Cached done,pause";
            
            _nextPlayer()->pause();
            _nextPlayer()->setPosition(0);
        }
    }
    // emit durationChanged(duration);
}
void MediaPlayer::_playerPositionChanged(qint64 position){
    //Emit position changed
    if(resource->single_video){
        //Just forward
        emit positionChanged(position);
        return;
    }
    if(sender() == currentPlayer){
        emit positionChanged(resource->segments[cur_segment].start + position);
    }
}
void MediaPlayer::_playerStateChanged(QMediaPlayer::State state){
    //Emit state changed
    // emit stateChanged(state);
    if(resource->single_video){
        //Just forward
        emit stateChanged(state);
        return;        
    }
}
void MediaPlayer::_playerMediaStatusChanged(QMediaPlayer::MediaStatus status){
    //Emit media status changed
    // emit mediaStatusChanged(status);
    mplayerDebug() << _nameOfPlayer(sender()) <<"Media status changed" << status;
    if(resource->single_video){
        return;
    }
    if(status == QMediaPlayer::EndOfMedia && sender() == currentPlayer){
        //Switch to next segment
        mplayerDebug() << _nameOfPlayer(sender()) << "Switch to next segment";
        if(cur_segment < resource->segments.size() - 1){
            cur_segment++;

            mplayerDebug() << "From " << _currentPlayerName() << " to " << _nextPlayerName();

            currentPlayer->setMuted(true);
            currentPlayer->setVideoOutput(&empty_item);
            currentPlayer->stop();
            currentPlayer = _nextPlayer();

            //Switch to next segment
            currentPlayer->setVideoOutput(video_item);
            currentPlayer->setMuted(false);
            currentPlayer->play();

            //Let next player to cache if needed
            if(cur_segment < resource->segments.size() - 1){
                _nextPlayer()->setMedia(resource->videos[cur_segment + 1]);
                _nextPlayer()->play();
            }
        
        }
        else{
            //Stop
            currentPlayer->stop();
        }
    }
}
void MediaPlayer::_playerBufferStatusChanged(int percent){
    //Emit buffer status changed
    // emit bufferStatusChanged(percent);
    // mplayerDebug() << "Buffer status changed" << percent;
}

qint64 MediaPlayer::position() const {
    if(resource->single_video){
        return currentPlayer->position();
    }
    else{
        return resource->segments[cur_segment].start + currentPlayer->position();
    }
}


Player::Player(QWidget *parent) : QGraphicsView(parent){
    //Configure
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setAlignment(Qt::AlignLeft | Qt::AlignTop);

    setScene(&scene);
    setStyleSheet("border: 0px;");
    setBackgroundBrush(Qt::black);
    //Add video item
    vitem = new QGraphicsVideoItem;
    player.setVideoOutput(vitem);
    scene.addItem(vitem);

    //Add Danmaku
    danmaku_group = new QGraphicsItemGroup;
    scene.addItem(danmaku_group);
    //ADD CONTROL
    control_group = new QGraphicsItemGroup;
    scene.addItem(control_group);

    //--Connect signals and slots
    connect(&player,SIGNAL(error(QMediaPlayer::Error)),this,SLOT(forwardError(QMediaPlayer::Error)));
    connect(&player,&MediaPlayer::durationChanged,this,&Player::durationChanged);
    connect(&player,&MediaPlayer::positionChanged,this,&Player::positionChanged);
    connect(&player,&MediaPlayer::stateChanged,this,&Player::stateChanged);
    connect(vitem,&QGraphicsVideoItem::nativeSizeChanged,this,&Player::nativeSizeChanged);

}
Player::~Player(){

}

void Player::durationChanged(qint64 duration){
    playerDebug() << "durationChanged " << duration << "ms";
    //Ready to play danmaku
    video_ready = true; 
    video_duration = duration;

    //Try to play danmaku
    danmakuPlay();
}
void Player::positionChanged(qint64 pos){
    playerDebug() << "positionChanged " << pos << "ms";
    if(!danmaku_started){
        return;
    }
    if(danmaku_prev_time < pos){
        playerDebug() << "Danmaku Seek To" << pos;
        danmakuSeek(pos);
    }
}
void Player::stateChanged(QMediaPlayer::State s){
    if(!danmaku_started){
        return;
    }
    switch(s){
        case QMediaPlayer::StoppedState:
            //Stoped state,stop danmaku
            playerDebug() << "Stoped State";
            danmakuClear();
            video_ready = false;
            break;
        case QMediaPlayer::PausedState:{
            //Kill timer
            killTimer(danmaku_timer);
            danmaku_timer = 0;
            danmaku_paused = true;
            break;
        }
        case QMediaPlayer::PlayingState:{
            //Start timer
            danmakuPlay();
            break;
        }
    }
}
//Fit the screen
void Player::fit(){
    scene.setSceneRect(0,0,width(),height());
    auto nt_size = vitem->nativeSize();
    auto size = this->size();//< Widget size

    //Calc the x and y / w /h to keep aspect ratio
    qreal tex_w = nt_size.width();
    qreal tex_h = nt_size.height();
    qreal win_w = size.width();
    qreal win_h = size.height();

    qreal x,y,w,h;

    QRectF rect;
    if(tex_w * win_h > tex_h * win_w){
        w = win_w;
        h = tex_h * win_w / tex_w;
    }
    else{
        w = tex_w * win_h / tex_h;
        h = win_h;
    }
    x = (win_w - w) / 2;
    y = (win_h - h) / 2;

    vitem->setPos(x,y);
    vitem->setSize(QSizeF(w,h));

    //Configure danmaku
    danmaku_group->setPos(0,0);
    // danmaku_group->setSize(size);
}
void Player::nativeSizeChanged(const QSizeF &size){
    playerDebug() << "Got " << size;
    native_size = size;
    fit();
}
void Player::play(const VideoResource &res){
    danmakuClear();//< Stop danmaku / clear danmaku list
    video_ready = false;//< Means not ready

    // playlist.clear();
    // playlist.addMedia(res.videos);
    video_res = res;
    player.setMedia(&video_res);
    player.play();
}
void Player::setDanmaku(const QString &str){
    QDomDocument doc;
    doc.setContent(str);
    QDomElement root = doc.documentElement();
    QDomNodeList nodes = root.elementsByTagName("d");

    //Color map
    // QMap<int,QColor> color_map = {
    //     {16646914,Qt::red},
    //     {16740868,QColor("FF7204")},
    //     {16755202,QColor("FFAA02")},
    //     {16765698,QColor("FFD302")},
    //     {16776960,Qt::yellow},
    //     {10546688,QColor("A0EE00")},
    //     {52480,Qt::green},
    //     {104601,QColor("019899")},
    //     {4351678,QColor("4266BE")},
    //     {9022215,QColor("89D5FF")},
    //     {13369971,QColor("CC0273")},
    //     {2236962,Qt::black},
    //     {10197915,QColor("9B9B9B")},
    //     {16777215,Qt::white},
    // };

    for(int i = 0;i < nodes.size();i++){
        QDomElement e = nodes.at(i).toElement();
        auto list = e.attribute("p").splitRef(',');

        Danmaku danmaku;

        danmaku.position = list[0].toDouble();
        danmaku.type = Danmaku::Type(list[1].toInt());
        danmaku.size = Danmaku::Size(list[2].toInt());
        //Color is RRGGBB in decimal
        auto c_num = list[3].toInt();
        QColor c('#' + QString::number(c_num,16));

        danmaku.color = c;
        danmaku.pool = Danmaku::Pool(list[5].toInt());
        danmaku.level = list[8].toInt();

        danmaku.text = e.text();

        danmaku_list.push_back(danmaku);

        // playerDebug() << list << danmaku.text;
    }
    if(nodes.size() == 0){
        playerDebug() << "No danmaku";
        playerDebug() << str;
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
    killTimer(danmaku_timer);

    danmaku_list.clear();
    danmaku_started = false;
    danmaku_paused = false;

    //Remove all children from group
    qDeleteAll(danmaku_group->childItems());
}
void Player::danmakuPause(){
    //Pause the danmaku

}
void Player::danmakuPlay(){
    if(danmaku_list.empty() || danmaku_started || (!video_ready)){
        playerDebug() << "Try to play danmaku, but not ready";
        playerDebug() << danmaku_list.empty() << danmaku_started << video_ready;
        return;
    }
    playerDebug() << "Start playing danmaku";
    //ready
    danmaku_started = true;
    danmaku_paused = false;

    danmakuSeek(player.position());

    //Config timer and start
    danmaku_timer = startTimer(
        1000 / danmaku_fps,
        Qt::TimerType::PreciseTimer
    );
    if(danmaku_timer == 0){
        //Fail to start timer
        playerDebug() << "Fail to start danmaku timer";
    }
}
void Player::danmakuSeek(qint64 _pos){

    qreal pos = _pos/1000.0;
    danmaku_prev_time = pos;

    //TODO : Add seek forwards and backwards

    while(danmaku_iter != danmaku_list.cend() && danmaku_iter->position < pos){
        ++danmaku_iter;
    }
    if(danmaku_iter != danmaku_list.cend()){
        playerDebug() << "Seek to " << danmaku_iter->position << "Text:" << danmaku_iter->text;
    }
    else{
        playerDebug() << "Seek to end";
    }
}

void Player::forwardError(QMediaPlayer::Error err){
    emit error(err);
}

void Player::resizeEvent(QResizeEvent *event){
    QGraphicsView::resizeEvent(event);
    //Set Item size keep aspect ratio    
    if(native_size != QSizeF(-1,-1)){
        //Is ready
        fit();
    }
}
void Player::keyPressEvent(QKeyEvent *event){
    QGraphicsView::keyPressEvent(event);
}
void Player::timerEvent(QTimerEvent *event){
    QGraphicsView::timerEvent(event);

    if(event->timerId() != danmaku_timer){
        return;
    }

    //Add danmaku and translate
    qreal cur_time = player.position() / 1000.0;
    QSizeF s = size();//< Current screen size

    while(danmaku_iter != danmaku_list.cend() && danmaku_iter->position < cur_time){
        //Add danmaku
        auto f = font();
        f.setBold(true);
        f.setPixelSize(danmaku_iter->size * 0.9);
        //Bilibili default use 0.8 as font size

        auto dan = new DanmakuItem();

        //Make color
#if 1
        auto doc = new QTextDocument(dan);
        QTextCharFormat format;
        format.setTextOutline(outpen);
        format.setForeground(danmaku_iter->color);
        QTextCursor text_cursor(doc);
        text_cursor.insertText(danmaku_iter->text,format);
        dan->setDocument(doc);
#else
        dan->setPlainText(danmaku_iter->text);
        dan->setDefaultTextColor(danmaku_iter->color);
#endif

        dan->setFont(f);
        dan->info = &*danmaku_iter;
        dan->deadtime = cur_time + alive_time;

        auto dan_size = dan->boundingRect().size();

        qreal x,y;

        switch(danmaku_iter->type){
            case Danmaku::Regular1:
            case Danmaku::Regular2:
            case Danmaku::Regular3:{
                //Set the position
                x = s.width();
                y = rand() % qint32(s.height() - dan_size.height());

                if(danmaku_group->contains(QPointF(x,y))){
                    //
                    // playerDebug() << "Regular Collision";
                    x += dan->boundingRect().width();
                }
                break;
            }
            case Danmaku::Bottom:{
                playerDebug() << "Bottom danmaku";
                x = s.width() / 2.0 - dan_size.width() / 2.0;
                y = s.height() - dan_size.height();
                //Find a position 
                while(danmaku_group->contains(QPointF(x,y))){
                    playerDebug() << "Bottom Collision";
                    y -= dan_size.height();
                }
                break;
            }
            case Danmaku::Top:{
                playerDebug() << "Top danmaku";
                x = s.width() / 2.0 - dan_size.width() / 2.0;
                y = 0;

                //Find a position 
                while(danmaku_group->contains(QPointF(x,y))){
                    playerDebug() << "Top Collision";
                    y += dan_size.height();
                }
                break;
            }
            case Danmaku::Reserve:{
                playerDebug() << "Reserve danmaku";
                x = -dan_size.width();
                y = rand() % qint32(s.height() - dan_size.height());
                break;
            }
            default:{
                playerDebug() << "FIXME : Unsupported danmaku type";
                x = -dan_size.width();
                y = -1;
                break;
            }
        }
        //Configure it
        dan->setPos(x,y);

        //Add to scene
        danmaku_group->addToGroup(dan);

        ++danmaku_iter;
    }

    //Move danmaku
    auto items = danmaku_group->childItems();
    for(auto item : items){
        auto dan = static_cast<DanmakuItem*>(item);
        auto text_size = dan->boundingRect().size();
        qreal step = (s.width() + text_size.width()) / alive_time / danmaku_fps;

        //We need to move it
        if(dan->info->isRegular()){
            dan->setPos(dan->x() - step,dan->y());
            //Move done check it
            if(dan->x() + text_size.width() < - 10){
                danmaku_group->removeFromGroup(dan);
                delete dan;
            }
        }
        else if(dan->info->isReserve()){
            dan->setPos(dan->x() + step,dan->y());
            //Check
            if(dan->x() > s.width() + 10){
                danmaku_group->removeFromGroup(dan);
                delete dan;
            }
        }
        else if(!dan->info->isMoveable()){
            if(cur_time >= dan->deadtime){
                danmaku_group->removeFromGroup(dan);
                delete dan;
            }
        }
    }

    danmaku_prev_time = cur_time;
}

PLAYER_NS_END