#include "common/providers.hpp"
#include "common/player.hpp"
#include "common/app.hpp"

#include <QDesktopServices>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QFontDialog>
#include <QMessageBox>
#include <QKeyEvent>
#include <QMenuBar>

#include <QWebEngineProfile>

PLAYER_NS_BEGIN

App::App(QWidget *p):QMainWindow(p){
    video_chooser = new VideoChooser(this);
    status_bar = new QStatusBar(this);
    menu_bar = new QMenuBar(this);

    setCentralWidget(video_chooser);
    setStatusBar(status_bar);
    setMenuBar(menu_bar);

    setMinimumSize(640,480);

    showMaximized();

    //--Connect to the signal
#ifndef NDEBUG
    connect(video_chooser,&VideoChooser::userOpenVideo,[](const QUrl &url){
        qDebug() << "Open Video URL:" << url;
    });
#endif
    connect(video_chooser,&VideoChooser::userLinkHovered,[this](const QUrl &link){
        status_bar->showMessage(link.toString());
    });
    connect(video_chooser,&VideoChooser::userOpenVideo,this,&App::userOpenVideo);
    //Add some menu / action
    QAction *back_action = menu_bar->addAction("回到主页面");
    connect(back_action,&QAction::triggered,[this](){
        video_chooser->load(QUrl("https://www.bilibili.com/anime"));
    });

    QMenu *config_menu = menu_bar->addMenu("设置");
    QAction *report_menu = config_menu->addAction("报告问题");
    connect(report_menu,&QAction::triggered,[this](){
        //Open URL
        auto url = QUrl("https://github.com/BusyStudent/QBilibiliPlayer/issues/new");
        auto ret = QDesktopServices::openUrl(url);
        if(!ret){
            //Let user open it by himself
            QMessageBox::information(this,"打开失败","请手动打开链接：" + url.toString());
        }
    });
    QAction *about_action = config_menu->addAction("关于");
    connect(about_action,&QAction::triggered,[this](){
        QMessageBox::about(nullptr,"关于",R"(QBilibili Player author: BusyStduent)");
    });
}
App::~App(){

}
//--Handle the signal
void App::userOpenVideo(const QUrl &url){
    fetchEpisodeInfo(url);
}
void App::userEpisodeInfoReady(const SeasonInfo &video_info){
    //Receive it from the signal we can use it
    VideoBroswer *video_broswer = new VideoBroswer(this,video_info);
    video_broswer->show();
}

//--Video Chooser
VideoChooser *VideoChooser::createWindow(QWebEnginePage::WebWindowType type){
    if(hovered_url.toString().startsWith("https://www.bilibili.com/bangumi/play/")){
        //User open video
        emit userOpenVideo(hovered_url);
    }
    else{
        //To the page
        load(hovered_url);
    }
    return nullptr;
}
void VideoChooser::linkHovered(const QUrl &link){
    hovered_url = link;
    emit userLinkHovered(link);
}

//--Video resolver
void App::replyFinished(QNetworkReply *reply){
    reply->deleteLater();

    qDebug() << "Reply Finished:" << reply->url();
    if(reply->error()){
        //Notify the user
        qDebug() << "Error:" << reply->errorString();
        return;
    }
    QJsonDocument json_doc = QJsonDocument::fromJson(reply->readAll());
    if(json_doc.isNull()){
        //Notify the user
        qDebug() << "Error: Invalid JSON";
        return;
    }

    //OK Parse the JSON
    if(json_doc["code"].toInt() != 0){
        //Notify the user
        qDebug() << "Error:" << json_doc["message"].toString();
        return;
    }
    QJsonValue data = json_doc["result"];
    if(data.isNull()){
        //Notify the user
        qDebug() << "Error: Invalid JSON";
        return;
    }

    int type = data["type"].toInt();
    if(type != int(VideoType::Anime)){
        //Unsupported video type
        qDebug() << "Error: Unsupported video type";
        return;
    }

    SeasonInfo sinfo;

    sinfo.season_title = data["season_title"].toString();
    sinfo.season_id = data["season_id"].toInt();

    qDebug() << "Season:" << sinfo.season_title << sinfo.season_id;

    for(auto _ep : data["episodes"].toArray()){
        auto ep = _ep.toObject();

        EpisodeInfo info;
        info.aid = ep["aid"].toInt();
        info.cid = ep["cid"].toInt();
        info.title = ep["title"].toString();
        info.subtitle = ep["subtitle"].toString();
        info.cover_image = ep["cover"].toString();
        info.need_pay = ep["badge"].toString() == "会员";
        info.badge = ep["badge"].toString();
        sinfo.episodes.append(info);

        qDebug() << "Episode:" << info.aid << info.cid << info.title << info.need_pay;
    }
    //Notify the user
    userEpisodeInfoReady(sinfo);
}
void App::fetchEpisodeInfo(const QUrl &url){
    //First extra id from url
    QString id = url.toString().split("/").last();
    //Remove ? if it exists
    id = id.split("?").first();

    qDebug() << "Fetch Video Info:" << id;

    QNetworkRequest request;
    QUrl req_url;

    if(id.startsWith("ep")){
        //Episode
        req_url = QUrl("https://api.bilibili.com/pgc/view/web/season?ep_id=" + id.right(id.length() - 2));
    }
    else{
        //season
        req_url = QUrl("https://api.bilibili.com/pgc/view/web/season?season_id=" + id.right(id.length() - 2));
    }
    request.setUrl(req_url);
    //Set User-Agent and Referer to cheat the server
    request.setRawHeader("User-Agent",PLAYER_USERAGENT);
    request.setRawHeader("Referer",PLAYER_BILIREFERER);

    qDebug() << "Request URL:" << req_url;

    //Send it and connect to the signal
    auto reply = manager.get(request);
    
    connect(reply,&QNetworkReply::finished,[this,reply](){
        replyFinished(reply);
    });
}
QList<VideoProvider*> App::createProviderList(){
    QList<VideoProvider*> providers;
    providers.push_back(new BilibiliProvider(&manager));
    providers.push_back(new AngelProvider(&manager));
    providers.push_back(new YsjdmProvider(&manager));
    providers.push_back(new LocalProvider(&manager));
    return providers;
}

//--Video broswer
VideoBroswer::VideoBroswer(App *w,const SeasonInfo &info):QMainWindow(w),season(info){
    ui = new Ui::BroswerUI();
    ui->setupUi(this);

    setWindowTitle(season.season_title);

    int n = 0;
    for(auto &video : season.episodes){
        QListWidgetItem *item = new QListWidgetItem(ui->listWidget);
        item->setText(QString("%1 %2").arg(video.title).arg(video.badge));
        item->setData(Qt::UserRole,n);//< The index of the video
        ui->listWidget->addItem(item);

        n += 1;
    }
    //--Get provider
    providers = w->createProviderList();
    //--Get manager
    manager = w->networkManager();

    for(auto p : providers){
        //Add to combo box
        ui->providerBox->addItem(p->name(),QVariant::fromValue(p));
        //Connect to the signal
        connect(p,&VideoProvider::videoInfoReady,this,&VideoBroswer::videoInfoReady);
        connect(p,&VideoProvider::videoReady,this,&VideoBroswer::videoReady);
        connect(p,&VideoProvider::error,this,&VideoBroswer::videoError);
    }

    //--Add player
    video_widget = new Player(this);
    ui->playerBox->insertWidget(0,video_widget);
    //--Connect to the signal
    connect(video_widget->mediaPlayer(),&MediaPlayer::mediaStatusChanged,this,&VideoBroswer::mediaStatusChanged);
    connect(video_widget->mediaPlayer(),&MediaPlayer::bufferStatusChanged,this,&VideoBroswer::bufferStatusChanged);
    connect(video_widget->mediaPlayer(),&MediaPlayer::positionChanged,this,&VideoBroswer::positionChanged);
    connect(video_widget->mediaPlayer(),&MediaPlayer::durationChanged,this,&VideoBroswer::durationChanged);
    //--Connect control buttons
    connect(ui->fullscreenButton,&QPushButton::clicked,this,&VideoBroswer::doFullScreen);
    connect(ui->playButton,&QPushButton::clicked,this,&VideoBroswer::doPlayButton);
    connect(ui->volumeButton,&QPushButton::clicked,this,&VideoBroswer::doVolumeButton);
    //--set it
    connect(ui->progressSilder,&QSlider::sliderPressed,[this](){
        doPause();
    });
    connect(ui->progressSilder,&QSlider::sliderMoved,[this](int position){
        ui->timeLabel->setText(QTime(0,0,0).addMSecs(position).toString("hh:mm:ss"));
        ui->progressSilder->setValue(position);
    });
    connect(ui->progressSilder,&QSlider::sliderReleased,[this](){
        video_widget->setPosition(ui->progressSilder->value());
        //Back to playing
        doPause();
    });

    //status bar
    status_bar = new QStatusBar(this);
    setStatusBar(status_bar);
    //menubar
    menu_bar = new QMenuBar(this);
    setMenuBar(menu_bar);

    QMenu *config_menu = menu_bar->addMenu("设置");
    QAction *font_config_action = config_menu->addAction("弹幕字体设置");
    connect(font_config_action,&QAction::triggered,[this](){
        QFontDialog dialog;
        dialog.setWindowTitle("选择一个用于弹幕的字体");
        dialog.exec();
        QFont font = dialog.selectedFont();
        video_widget->setFont(font);
    });
    //--Layout done

    // BilibiliProvider provider;
    // auto video = provider.get(season,0);
    // playlist->clear();
    // playlist->addMedia(video);

    // player->play();

    //--Connect to the signal
    connect(video_widget,SIGNAL(error(QMediaPlayer::Error)),this,SLOT(playerError(QMediaPlayer::Error)));
    connect(ui->listWidget,&QListWidget::itemDoubleClicked,this,&VideoBroswer::listItemClicked);
    connect(ui->listWidget,&QListWidget::currentItemChanged,[this](QListWidgetItem *cur,QListWidgetItem *){
        if(cur == nullptr) return;

        int index = cur->data(Qt::UserRole).toInt();
        //Get the video resloution
        auto provider = ui->providerBox->currentData().value<VideoProvider*>();
        ui->resolutionBox->clear();

        provider->fetchInfo(season,index);
    });
}

VideoBroswer::~VideoBroswer(){    
    vbrowserDebug() << "Video Broswer Destroyed";

    for(auto p : providers){
        delete p;
    }

    delete ui;
    ui = nullptr;
}

void VideoBroswer::playerError(QMediaPlayer::Error error){
    if(error){

    }
    vbrowserDebug() << "Player Error:" << error;
}
void VideoBroswer::listItemClicked(QListWidgetItem *item){
    vbrowserDebug() << "List Item Clicked:" << item->data(Qt::UserRole).toInt();

    if(!ui->resolutionBox->currentData().isValid()){
        vbrowserDebug() << "No resolution selected";
        return;
    }

    int index = item->data(Qt::UserRole).toInt();
    VideoProvider *provider = ui->providerBox->currentData().value<VideoProvider*>();
    //Waiting for the signal
    status_bar->showMessage("Waiting for the video...");
    // BilibiliProvider provider;
    provider->fetchVideo(season,index,ui->resolutionBox->currentData().toString());
}
void VideoBroswer::videoReady(const VideoResource &res){
    vbrowserDebug() << "Video Ready";
    video_widget->play(res);
    //Configure ui
    status_bar->showMessage("VideoReady playing...");
    ui->playButton->setIcon(QIcon(":/icons/pause.png"));
    //Begin fetch danmaku
    fetchDanmaku(ui->listWidget->currentItem()->data(Qt::UserRole).toInt());
}
void VideoBroswer::videoInfoReady(const VideoInfo &info){
    vbrowserDebug() << "Video Info Ready";
    //Add to the combo box
    for(int n = 0;n < info.resolutions.size();n++){
        ui->resolutionBox->addItem(info.resolutions_name[n],QVariant::fromValue(info.resolutions[n]));
    }
    //Select the first one without need pay
    for(int n = 0;n < info.need_pay.size();n++){
        if(not info.need_pay[n]){
            ui->resolutionBox->setCurrentIndex(n);
            break;
        }
    }

    status_bar->showMessage("VideoInfoReady");
}
void VideoBroswer::videoError(const QString &error){
    vbrowserDebug() << "Video Error:" << error;
    QMessageBox msg;
    
    status_bar->showMessage("Video Error:" + error);

    msg.setWindowTitle("来自蒙古上单的提示");
    msg.setText(error);
    msg.setIcon(QMessageBox::Critical);
    msg.exec();

}
void VideoBroswer::fetchDanmaku(int n){
    vbrowserDebug() << "Fetch Danmaku for video:" << n;
    int cid = season.episodes[n].cid;
    //Get danmaku from bilibili
    QUrl req(QString("https://comment.bilibili.com/%1.xml").arg(cid));
    QNetworkRequest request(req);
    request.setRawHeader("User-Agent",PLAYER_USERAGENT);
    request.setRawHeader("Referer",PLAYER_BILIREFERER);
    auto reply = manager->get(request);

    //Connect
    connect(reply,&QNetworkReply::finished,[reply,this](){
        danmakuReceived(reply);
    });
    vbrowserDebug() << req;
}
void VideoBroswer::danmakuReceived(QNetworkReply *reply){
    reply->deleteLater();        
    if(reply->error()){
        vbrowserDebug() << "Fetch Danmaku Error:" << reply->errorString();
        status_bar->showMessage("Fetch Danmaku Error:" + reply->errorString());
        return;
    }
    if(reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() == 302){
        //Redirect
        vbrowserDebug() << "Redirect";
        QUrl redirect = reply->attribute(QNetworkRequest::RedirectionTargetAttribute).toUrl();
        QNetworkRequest request(redirect);
        request.setRawHeader("User-Agent",PLAYER_USERAGENT);
        request.setRawHeader("Referer",PLAYER_BILIREFERER);
        auto reply = manager->get(request);
        vbrowserDebug() << redirect;
        connect(reply,&QNetworkReply::finished,this,[reply,this](){
            danmakuReceived(reply);
        });
        return;
    }

    vbrowserDebug() << "Fetch Danmaku Success";
    status_bar->showMessage("Fetch Danmaku Success");
    QString ret = reply->readAll();
    //Process it
    playDanmaku(ret);
}
void VideoBroswer::playDanmaku(const QString &d){
    video_widget->setDanmaku(d);
}
void VideoBroswer::bufferStatusChanged(int percent){
    status_bar->showMessage(QString("Buffering %1%").arg(percent));
}
void VideoBroswer::mediaStatusChanged(QMediaPlayer::MediaStatus status){
    if(status == QMediaPlayer::BufferedMedia){
        vbrowserDebug() << "Buffered";
        status_bar->showMessage("Buffered");
    }
    else if(status == QMediaPlayer::StalledMedia){
        vbrowserDebug() << "Begin buffering";
    }
    else if(status == QMediaPlayer::EndOfMedia){
        vbrowserDebug() << "EndofMedia";
        ui->progressSilder->setRange(0,0);
        ui->durationLabel->setText("00:00:00");
        ui->timeLabel->setText("00:00:00");
        ui->playButton->setIcon(QIcon(":/icons/play.png"));
    }
}
void VideoBroswer::durationChanged(qint64 duration){
    if(ui != nullptr){
        ui->progressSilder->setRange(0,duration);
        ui->durationLabel->setText(QTime(0,0,0).addMSecs(duration).toString("hh:mm:ss"));
    }
}
void VideoBroswer::positionChanged(qint64 position){
    if(ui != nullptr){
        ui->progressSilder->setValue(position);
        ui->timeLabel->setText(QTime(0,0,0).addMSecs(position).toString("hh:mm:ss"));
    }
}

void VideoBroswer::closeEvent(QCloseEvent *event){
    deleteLater();
}
void VideoBroswer::keyPressEvent(QKeyEvent *event){
    if(event->key() == Qt::Key_F11){
        doFullScreen();
    }
    else if(event->key() == Qt::Key_Space){
        doPause();
    }
    else if(event->key() == Qt::Key_Right){
        video_widget->setPosition(video_widget->position() + 5000);
    }
    else if(event->key() == Qt::Key_Left){
        video_widget->setPosition(video_widget->position() - 5000);
    }
}
void VideoBroswer::doPause(){
    if(video_widget->mediaPlayer()->isPaused()){
        qDebug() << "Play the video";
        ui->playButton->setIcon(QIcon(":/icons/pause.png"));
        video_widget->resume();
    }
    else{
        qDebug() << "Pause the video";
        ui->playButton->setIcon(QIcon(":/icons/play.png"));
        video_widget->pause();
    }
}
void VideoBroswer::doFullScreen(){
    //Check is already fullscreen
    bool visible = isFullScreen();
    if(isFullScreen()){
        ui->fullscreenButton->setIcon(QIcon(":/icons/full.png"));
        showNormal();
    }
    else{
        ui->fullscreenButton->setIcon(QIcon(":/icons/min.png"));
        showFullScreen();
    }
    ui->listWidget->setVisible(visible);
    ui->label->setVisible(visible);
    ui->providerBox->setVisible(visible);
    ui->providerLabel->setVisible(visible);
    ui->resolutionBox->setVisible(visible);
    ui->resolutionLabel->setVisible(visible);
    status_bar->setVisible(visible);
    menu_bar->setVisible(visible);
}
void VideoBroswer::doPlayButton(){
    if(!video_widget->mediaPlayer()->hasMedia()){
        //No media nothing to play
        return;
    }
    doPause();
}
void VideoBroswer::doVolumeButton(){
    struct Data : QObjectUserData {
        QSlider *slider;
    };
    Data *data = (Data*)ui->volumeButton->userData(Qt::UserRole);
    if(data == nullptr){
        //Create one
        //TODO volume dialog
        auto slider = new QSlider(this);
        //Get current volume
        slider->setRange(0,100);
        slider->setValue(video_widget->mediaPlayer()->volume());
        //Let it on the upside of volume button
        auto pos = ui->volumeButton->pos();
        pos.setY(pos.y() - slider->height());
        slider->move(pos);
        slider->show();

        data = new Data;
        data->slider = slider;

        ui->volumeButton->setUserData(Qt::UserRole,data);

        //Connect signals
        connect(video_widget->mediaPlayer(),&MediaPlayer::volumeChanged,slider,&QSlider::setValue);
        connect(slider,&QSlider::valueChanged,video_widget->mediaPlayer(),&MediaPlayer::setVolume);
    }
    else{
        //Delete it
        data->slider->deleteLater();
        ui->volumeButton->setUserData(Qt::UserRole,nullptr);
        delete data;
    }
}

PLAYER_NS_END