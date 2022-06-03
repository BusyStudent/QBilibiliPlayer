#pragma once

#include <QNetworkReply>
#include <QNetworkRequest>
#include <QNetworkAccessManager>
#include <QWebEngineView>
#include <QMainWindow>
#include <QStatusBar>
#include <QUrl>

#include <QtMultimedia/QMediaContent>
#include <QtMultimedia/QMediaPlayer>

#include "ui_broswer.h"
#include "defs.hpp"

PLAYER_NS_BEGIN

class Player;
/**
 * @brief Single Episode info
 */
class EpisodeInfo {
    public:
        int aid;
        int cid;
        QString title;
        QString subtitle;
        QString cover_image;//< Url of the image
        QString badge;//< "会员" or ""
        bool need_pay;//< Need VIP to play,if true,We use Provider to get the video URL
};
class SeasonInfo {
    public:
        int season_id;
        QString season_title;//< Season title(we can use it to get URL from another site by this)
        QString cover_image;//< Url of the image
        QList<EpisodeInfo> episodes;
};

class VideoInfo {
    public:
        QStringList resolutions_name;//< From highest to lowest(descriptions)
        QStringList resolutions;//< The string you need to pass to fetchVideo
        QList<bool> need_pay;//< Need VIP or extra condition to play in this resolution
};

class VideoResource {
    public:
        struct Segment {
            qint64 start;//< Start time of it
            qint64 duration;//< The duration of the segment
        };
        
        QList<QMediaContent> videos;
        QList<QMediaContent> audios;

        QList<Segment> segments;//< The Segment of the video

        qint64 duration;//< The whole duration of the video in seconds
        bool single_video;//< If the video is a single video,duration is not needed
};

/**
 * @brief For Get Video URL from Bilibili
 * 
 */
class VideoProvider : public QObject {
    Q_OBJECT
    signals:
        void videoReady(const VideoResource &res);
        void videoInfoReady(const VideoInfo &info);
        void error(const QString &msg);
    public:
        /**
         * @brief Construct a new Video Broswer object
         * 
         * @param manager The manager to use
         */
        VideoProvider(QNetworkAccessManager *manager) : manager(manager){}

        QString name() const {
            return provider_name;
        }
        /**
         * @brief Get the video URL 
         * 
         * @param season 
         * @param idx 
         * @param resolution The resolution to get(string from fetchInfo)
         */
        virtual void fetchVideo(const SeasonInfo &season,int idx,QStringView resolution) = 0;
        /**
         * @brief Get the video info 
         * 
         * @param season 
         * @param idx 
         */
        virtual void fetchInfo(const SeasonInfo &season,int idx) = 0;
    protected:
        QString provider_name = "Undefined";
        QNetworkAccessManager *manager;
};
/**
 * @brief Provider from Bilibili
 * 
 */
class BilibiliProvider : public VideoProvider {
    public:
        BilibiliProvider(QNetworkAccessManager *m) : VideoProvider(m){
            provider_name = "Bilibili";
        }

        void fetchVideo(const SeasonInfo &season,int idx,QStringView resolution) override;
        void fetchInfo(const SeasonInfo &season,int idx) override;
};
/**
 * @brief Provider from local file
 * 
 */
class LocalProvider : public VideoProvider {
    public:
        LocalProvider(QNetworkAccessManager *m) : VideoProvider(m){
            provider_name = "Local";
        }

        void fetchVideo(const SeasonInfo &season,int idx,QStringView resolution) override;
        void fetchInfo(const SeasonInfo &season,int idx) override;
};

class VideoChooser : public QWebEngineView {
    Q_OBJECT

    public:
        VideoChooser(QWidget *parent = nullptr) : QWebEngineView(parent) {
            setUrl(QUrl("https://www.bilibili.com/anime"));
            connect(page(),&QWebEnginePage::linkHovered,this,&VideoChooser::linkHovered);
        }

        VideoChooser *createWindow(QWebEnginePage::WebWindowType type) override;

        void          linkHovered(const QUrl &link);
    signals:
        void userOpenVideo(const QUrl &url);
        void userLinkHovered(const QUrl &link);
    private:
        QUrl hovered_url;
};

class App : public QMainWindow {
    Q_OBJECT

    public:
        App(QWidget *parent = 0);
        ~App();

        /**
         * @brief Try to get the video url from the given url
         * 
         * @param url 
         */
        void fetchEpisodeInfo(const QUrl &url);
        void replyFinished(QNetworkReply *reply);
        /**
         * @brief Create a Provider List object
         * 
         * @return QList<VideoProvider> 
         */
        QList<VideoProvider*> createProviderList();
        QNetworkAccessManager *networkManager(){
            return &manager;
        }
    private:
        void userOpenVideo(const QUrl &url);
        void userEpisodeInfoReady(const SeasonInfo &);

        VideoChooser *video_chooser;
        QStatusBar *status_bar;
        QMenuBar *menu_bar;
        QNetworkAccessManager manager;
};

/**
 * @brief Play the video Control Widget
 * 
 */
class VideoBroswer : public QMainWindow {
    Q_OBJECT
    public:
        VideoBroswer(App *app,const SeasonInfo &info);
        ~VideoBroswer();

        /**
         * @brief Fetch the video danmaku
         * 
         * @param n 
         */
        void fetchDanmaku(int n);
        void playDanmaku(const QString &danmaku);
    private slots:
        void playerError(QMediaPlayer::Error);
        void listItemClicked(QListWidgetItem *item);
        void danmakuReceived(QNetworkReply *reply);
        //From MediaPlayer
        void mediaStatusChanged(QMediaPlayer::MediaStatus);
        void bufferStatusChanged(int);
        //For VideoPlayer
        void durationChanged(qint64);
        void positionChanged(qint64);
    private:
        void videoReady(const VideoResource &res);
        void videoInfoReady(const VideoInfo &info);
        void videoError(const QString &msg);

        void userOpenVideo(const QUrl &url);
        void userEpisodeInfoReady(const SeasonInfo &);
        void closeEvent(QCloseEvent *event) override;
        void keyPressEvent(QKeyEvent *event) override;

        void doFullScreen();
        void doPlayButton();//< Play or pause the video
        void doPause();//< Try to pause or resume the video
        void doVolumeButton();//< Open the volume control dialog

        SeasonInfo season;
        QMenuBar *menu_bar;
        QStatusBar *status_bar;
        Player *video_widget;
        QNetworkAccessManager *manager;

        QList<VideoProvider*> providers;

        Ui::BroswerUI *ui;
}; 

PLAYER_NS_END