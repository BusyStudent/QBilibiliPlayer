#include "common/player.hpp"
#include "common/app.hpp"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QMessageBox>
#include <QFileDialog>
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
    ui->playerBox->addWidget(video_widget);

    //status bar
    status_bar = new QStatusBar(this);
    setStatusBar(status_bar);
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
    qDebug() << "Video Broswer Destroyed";

    for(auto p : providers){
        delete p;
    }

    delete ui;
}

void VideoBroswer::playerError(QMediaPlayer::Error error){
    if(error){

    }
    qDebug() << "Player Error:" << error;
}
void VideoBroswer::listItemClicked(QListWidgetItem *item){
    qDebug() << "List Item Clicked:" << item->data(Qt::UserRole).toInt();

    if(!ui->resolutionBox->currentData().isValid()){
        qDebug() << "No resolution selected";
        return;
    }

    int index = item->data(Qt::UserRole).toInt();
    VideoProvider *provider = ui->providerBox->currentData().value<VideoProvider*>();
    //Waiting for the signal
    status_bar->showMessage("Waiting for the video...");
    // BilibiliProvider provider;
    provider->fetchVideo(season,index,ui->resolutionBox->currentData().toString());
}
void VideoBroswer::videoReady(const QMediaContent &videos,const QMediaContent &audios){
    qDebug() << "Video Ready";
    video_widget->play(videos,audios);
    status_bar->showMessage("VideoReady playing...");
    //Begin fetch danmaku
    fetchDanmaku(ui->listWidget->currentItem()->data(Qt::UserRole).toInt());
}
void VideoBroswer::videoInfoReady(const VideoInfo &info){
    qDebug() << "Video Info Ready";
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
    qDebug() << "Video Error:" << error;
    QMessageBox msg;
    
    status_bar->showMessage("Video Error:" + error);

    msg.setWindowTitle("来自蒙古上单的提示");
    msg.setText(error);
    msg.setIcon(QMessageBox::Critical);
    msg.exec();

}
void VideoBroswer::fetchDanmaku(int n){
    qDebug() << "Fetch Danmaku for video:" << n;
    int cid = season.episodes[n].cid;
    //Get danmaku from bilibili
    QUrl req(QString("http://comment.bilibili.com/%1.xml").arg(cid));
    QNetworkRequest request(req);
    request.setRawHeader("User-Agent",PLAYER_USERAGENT);
    request.setRawHeader("Referer",PLAYER_BILIREFERER);
    auto reply = manager->get(request);

    //Connect
    connect(reply,&QNetworkReply::finished,[this,reply](){
        if(reply->error()){
            qDebug() << "Fetch Danmaku Error:" << reply->errorString();
            status_bar->showMessage("Fetch Danmaku Error:" + reply->errorString());
            return;
        }
        qDebug() << "Fetch Danmaku Success";
        status_bar->showMessage("Fetch Danmaku Success");
        QString ret = reply->readAll();
        //Process it
        playDanmaku(ret);
    });
}
void VideoBroswer::playDanmaku(const QString &d){
    video_widget->setDanmaku(d);
}

void VideoBroswer::closeEvent(QCloseEvent *event){
    deleteLater();
}
void VideoBroswer::keyPressEvent(QKeyEvent *event){
    if(event->key() == Qt::Key_F11){
        //Check is already fullscreen
        bool visible = isFullScreen();
        if(isFullScreen()){
            showNormal();
        }
        else{
            showFullScreen();
        }
        ui->listWidget->setVisible(visible);
        ui->label->setVisible(visible);
        ui->providerBox->setVisible(visible);
        ui->providerLabel->setVisible(visible);
        ui->resolutionBox->setVisible(visible);
        ui->resolutionLabel->setVisible(visible);
        status_bar->setVisible(visible);
    }
}

//--Video Provider
void BilibiliProvider::fetchVideo(const SeasonInfo &info,int n,QStringView resolution){
    const auto &episode = info.episodes[n];

    if(episode.need_pay){        
        emit error("Cherry这集让你付钱,当前Provider不支持这个操作");
        return;
    }

    //Get video url from bilibili
    QUrl url("https://api.bilibili.com/x/player/playurl?avid=" + QString::number(episode.aid) 
        + "&cid=" + QString::number(episode.cid) 
        + "&qn=" + resolution.toString() 
    );

    QNetworkRequest request;
    request.setUrl(url);
    request.setRawHeader("User-Agent",PLAYER_USERAGENT);
    request.setRawHeader("Referer",PLAYER_BILIREFERER);

    //Send it
    auto reply = manager->get(request);

    connect(reply,&QNetworkReply::finished,[this,reply](){
        //Check the reply
        reply->deleteLater();
        if(reply->error()){
            qDebug() << "Error:" << reply->errorString();
            emit error(reply->errorString());
            return;
        }
        //Parse json 
        QJsonDocument json_doc = QJsonDocument::fromJson(reply->readAll());
        if(json_doc.isNull()){
            qDebug() << "Error: Invalid JSON";
            emit error("Invalid JSON");
            return;
        }
        if(json_doc["code"].toInt() != 0){
            qDebug() << "Error:" << json_doc["message"].toString();
            emit error(json_doc["message"].toString());
            return;
        }
        //Get the video url
        QJsonValue data = json_doc["data"];
        if(data.isNull()){
            qDebug() << "Error: Invalid JSON";
            emit error("Invalid JSON");
            return;
        }
        QJsonArray durl = data["durl"].toArray();
        qDebug() << durl;

        QMediaPlaylist *video_list = new QMediaPlaylist;
        QMediaPlaylist *audio_list = new QMediaPlaylist;

        for(auto item : durl){
            auto u = item.toObject()["url"].toString();
            //Pack to QMediaContent
            QNetworkRequest request2;
            request2.setUrl(u);
            request2.setRawHeader("User-Agent",PLAYER_USERAGENT);
            request2.setRawHeader("Referer",PLAYER_BILIREFERER);

            video_list->addMedia(QMediaContent(request2));
        }
        //Pack info single content
        QMediaContent videos(video_list,QUrl(),true);
        QMediaContent audios(audio_list,QUrl(),true);

        emit videoReady(videos,audios);
    });
}
void BilibiliProvider::fetchInfo(const SeasonInfo &info,int n){
    const auto &episode = info.episodes[n];

    if(episode.need_pay){        
        emit error("Cherry这集让你付钱,当前Provider不支持这个操作");
        return;
    }

    //Get video url from bilibili
    QUrl url("https://api.bilibili.com/x/player/playurl?avid=" + QString::number(episode.aid) + "&cid=" + QString::number(episode.cid));
    QNetworkRequest request;
    request.setUrl(url);
    request.setRawHeader("User-Agent",PLAYER_USERAGENT);
    request.setRawHeader("Referer",PLAYER_BILIREFERER);

    //Send it
    auto reply = manager->get(request);
    
    connect(reply,&QNetworkReply::finished,[reply,this](){
        reply->deleteLater();
        if(reply->error()){
            emit error(reply->errorString());
            return;
        }
        //Parse json
        QJsonDocument json_doc = QJsonDocument::fromJson(reply->readAll());
        if(json_doc.isNull()){
            emit error("Invalid JSON");
            return;
        }
        if(json_doc["code"].toInt() != 0){
            emit error(json_doc["message"].toString());
            return;
        }
        //Get the video resolution
        QJsonValue data = json_doc["data"];
        QJsonArray arr  = data["accept_quality"].toArray();
        VideoInfo info;

        qDebug() << data["accept_quality"].toArray();

        for(auto item : data["accept_quality"].toArray()){
            info.resolutions.push_back(QString::number(item.toInt()));
            info.need_pay.push_back(item.toInt() > 64);
        }
        for(auto item : data["accept_description"].toArray()){
            info.resolutions_name.push_back(item.toString());
        }
        emit videoInfoReady(info);
    });
}

void LocalProvider::fetchInfo(const SeasonInfo &,int){
    //We didnot need to get info 
    VideoInfo info;
    info.need_pay.push_back(false);
    info.resolutions.push_back("");
    info.resolutions_name.push_back("Auto");

    emit videoInfoReady(info);
}
void LocalProvider::fetchVideo(const SeasonInfo &s,int n,QStringView){
    const auto &episode = s.episodes[n];

    auto ret = QFileDialog::getOpenFileUrl(
        nullptr,
        "Select Vide for " + episode.title + "(" + (n + 1) + ")"
    );

    if(ret.isEmpty()){
        emit error("No file selected");
        return;
    }
    QMediaContent videos(ret);
    QMediaContent audios;
    emit videoReady(videos,audios);
}

PLAYER_NS_END