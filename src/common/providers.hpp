//--Ext providers
#pragma once

#include <QtMultimedia/QMediaContent>
#include <QtMultimedia/QMediaPlaylist>

#include "app.hpp"

PLAYER_NS_BEGIN


/**
 * @brief Provider from Sukura anime
 * 
 */
class SukuraProvider : public VideoProvider {
    public:
        void fetchVideo(const SeasonInfo &season,int idx,QStringView resolution) override;
        void fetchInfo(const SeasonInfo &season,int idx) override;
    private:
        QString search_url = "https://www.yhdmp.cc/s_all?ex=1&kw=";
        QWebEngineView *view = nullptr;//< Maybe we use WebEngine to parse the javascript to get the video URL
};

/**
 * @brief Abstract Interface for 66dm.net
 * 
 */
class TsdmProvider : public VideoProvider {
    public:
        using VideoProvider::VideoProvider;

        bool parseEncrypedM3U8(const QString &m3u8,VideoResource &out);
        int offset = 113;//< They add a png file before the real video
};
class YsjdmProvider : public TsdmProvider {
    public:
        YsjdmProvider(QNetworkAccessManager *m) : TsdmProvider(m){
            provider_name = "异世界动漫";
        }
        void fetchVideo(const SeasonInfo &season,int idx,QStringView resolution) override;
        void fetchInfo(const SeasonInfo &season,int idx) override;
    private:
        void videoPagesReady(QNetworkReply *n);
        void videoPageReady(QNetworkReply *n);
        void m3u8Ready(QNetworkReply *n);

        void matchVideoPages(const QString &name);

        bool matched = false;
        QString search_url = "https://www.ysjdm.net/index.php/vod/search.html?wd=%1&submit=";
        QString prefix = "https://www.ysjdm.net";
        QMap<int,QString> video_pages;

};
/**
 * @brief Provider from Angle anime
 * 
 */
class AngelProvider : public TsdmProvider {
    Q_OBJECT
    signals:
        void videoPagesReady();
    public:
        AngelProvider(QNetworkAccessManager *m) : TsdmProvider(m){
            provider_name = "Angel";
        }

        void fetchVideo(const SeasonInfo &season,int idx,QStringView resolution) override;
        void fetchInfo(const SeasonInfo &season,int idx) override;
    private:
        //Map Bilibili ID to URL

        void matchVideoPages(const QString &name);
        void videoPagesReady(QNetworkReply *n);
        void videoJsReady(QNetworkReply *n,int idx);

        bool matched = false;
        QString video_page ;//< Content of the video page
        QString search_url = "http://tv.66dm.net/search.asp";
        QString url_prefix = "http://tv.66dm.net";

        QMap<int,QString> video_pages;//< Map Bilibili ID to URL
};

PLAYER_NS_END