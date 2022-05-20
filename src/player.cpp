#include "common/player.hpp"


#include <QTextCharFormat>
#include <QTextDocument>
#include <QTextCursor>
#include <QDomDocument>
#include <QTimerEvent>
#include <algorithm>

PLAYER_NS_BEGIN

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
    connect(&player,&QMediaPlayer::durationChanged,this,&Player::durationChanged);
    connect(&player,&QMediaPlayer::positionChanged,this,&Player::positionChanged);
    connect(&player,&QMediaPlayer::stateChanged,this,&Player::stateChanged);
    connect(vitem,&QGraphicsVideoItem::nativeSizeChanged,this,&Player::nativeSizeChanged);

}
Player::~Player(){

}

void Player::durationChanged(qint64 duration){
    qDebug() << "Got " << duration << "ms";
    video_duration = duration;

    //Try to play danmaku
    danmakuPlay();
}
void Player::positionChanged(qint64 pos){
    qDebug() << "Got " << pos << "ms";
    if(!danmaku_started){
        return;
    }
    if(danmaku_prev_time < pos){
        qDebug() << "Danmaku Seek To" << pos;
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
            danmakuClear();
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
    qDebug() << "Got " << size;
    native_size = size;
    fit();
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
    danmaku_timer = startTimer(
        1000 / danmaku_fps,
        Qt::TimerType::PreciseTimer
    );
    if(danmaku_timer == 0){
        //Fail to start timer
        qDebug() << "Fail to start danmaku timer";
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
        qDebug() << "Seek to " << danmaku_iter->position << "Text:" << danmaku_iter->text;
    }
    else{
        qDebug() << "Seek to end";
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
                    qDebug() << "Collision";
                    x += dan->boundingRect().width();
                }
                break;
            }
            case Danmaku::Bottom:{
                qDebug() << "Bottom danmaku";
                x = s.width() / 2.0 - dan_size.width() / 2.0;
                y = s.height() - dan_size.height();
                //Find a position 
                while(danmaku_group->contains(QPointF(x,y))){
                    y -= dan_size.height();
                }
                break;
            }
            case Danmaku::Top:{
                qDebug() << "Top danmaku";
                x = s.width() / 2.0 - dan_size.width() / 2.0;
                y = 0;

                //Find a position 
                while(danmaku_group->contains(QPointF(x,y))){
                    y += dan_size.height();
                }
                break;
            }
            case Danmaku::Reserve:{
                qDebug() << "Reserve danmaku";
                x = -dan_size.width();
                y = rand() % qint32(s.height() - dan_size.height());
                break;
            }
            default:{
                qDebug() << "FIXME : Unsupported danmaku type";
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