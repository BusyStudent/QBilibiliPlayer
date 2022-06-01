#include <QHttpMultiPart>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QFileDialog>
#include <QDomDocument>

#include <QTextCodec>
#include <QInputDialog>

#define LXML_NO_EXCEPTIONS

#include "common/providers.hpp"
#include "libs/lxml.hpp"

#include <libxml/HTMLparser.h>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <libxml/xpath.h>

PLAYER_NS_BEGIN

//Shit code :( Need to be refactored

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
            providerDebug() << "Error:" << reply->errorString();
            emit error(reply->errorString());
            return;
        }
        //Parse json 
        QJsonDocument json_doc = QJsonDocument::fromJson(reply->readAll());
        if(json_doc.isNull()){
            providerDebug() << "Error: Invalid JSON";
            emit error("Invalid JSON");
            return;
        }
        if(json_doc["code"].toInt() != 0){
            providerDebug() << "Error:" << json_doc["message"].toString();
            emit error(json_doc["message"].toString());
            return;
        }
        //Get the video url
        QJsonValue data = json_doc["data"];
        if(data.isNull()){
            providerDebug() << "Error: Invalid JSON";
            emit error("Invalid JSON");
            return;
        }
        QJsonArray durl = data["durl"].toArray();
        providerDebug() << durl;


        VideoResource res;
        for(auto item : durl){
            auto u = item.toObject()["url"].toString();
            //Pack to QMediaContent
            QNetworkRequest request2;
            request2.setUrl(u);
            request2.setRawHeader("User-Agent",PLAYER_USERAGENT);
            request2.setRawHeader("Referer",PLAYER_BILIREFERER);

            res.videos.push_back(QMediaContent(request2));
        }
        
        res.duration = -1;
        res.single_video = true;

        emit videoReady(res);
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

        providerDebug() << data["accept_quality"].toArray();

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
    VideoResource res;
    res.videos.push_back(QMediaContent(ret));
    res.duration = -1;
    res.single_video = true;

    emit videoReady(res);
}




//--Angel Provider
void AngelProvider::matchVideoPages(const QString &_name){
    providerDebug() << "Searching for " << _name;
    // QJsonObject obj;
    // obj["searchword"] = name;
    // obj["submit"] = "搜索";

    // QJsonDocument doc(obj);
    // QByteArray data = doc.toJson();

    // QString d(data);
    // providerDebug() << d;
    // //Convert to gbk
    //Create a post request
    QString name = _name.split(" ").first();
    //Improve the search

    auto codec = QTextCodec::codecForName("gbk");
    QByteArray enc_name = codec->fromUnicode(name);
    QByteArray enc_sub = codec->fromUnicode("搜索");

    QByteArray data = "searchword=" + enc_name + "&submit=" + enc_sub;
    //Do url encode by hand
    for(int i = 0;i < enc_name.size();i++){
        if(enc_name[i] == ' '){
            data.append('+');
        }else{
            data.append(enc_name[i]);
        }
    }


    QNetworkRequest request;
    request.setUrl(QUrl(search_url));
    request.setHeader(QNetworkRequest::ContentTypeHeader,"application/x-www-form-urlencoded");
    //Use utf8
    request.setRawHeader("Accept-Charset","utf-8");
    request.setRawHeader("User-Agent",PLAYER_USERAGENT);
    request.setRawHeader("Referer",search_url.toUtf8());

    auto reply = manager->post(request,data);
    //Connect the reply
    connect(reply,&QNetworkReply::finished,[this,reply](){
        const auto &data = reply->readAll();
        reply->deleteLater();
        if(reply->error()){
            emit error(reply->errorString());
            return;
        }
        auto codec = QTextCodec::codecForHtml(data);
        auto html = codec->toUnicode(data);
        providerDebug() << html;

        //Save the html
        // QFile file("/tmp/angel.html");0
        // file.open(QIODevice::WriteOnly);
        // file.write(data);

        //XPath expression to get the video url
        const auto xpath = R"(//div[@class="intro"]/h6/a)";
        //Using libxml2
        auto doc = htmlReadDoc(BAD_CAST html.toStdString().c_str(),nullptr,"utf8",
            XML_PARSE_NOBLANKS | XML_PARSE_NONET | XML_PARSE_NOERROR | XML_PARSE_NOWARNING
        );
        if(doc == nullptr){
            emit error("Invalid HTML");
        }
        //Compile xpath
        auto xpath_obj = xmlXPathNewContext(doc);
        auto xpath_res = xmlXPathEvalExpression(BAD_CAST xpath,xpath_obj);
        xmlXPathFreeContext(xpath_obj);
        if(xpath_res == nullptr){
            xmlFreeDoc(doc);
            emit error("Invalid XPath");
            return;
        }
        //Get the result
        if(xmlXPathNodeSetIsEmpty(xpath_res->nodesetval)){
            xmlXPathFreeObject(xpath_res);
            xmlFreeDoc(doc);
            emit error("Search : No result");
            return;
        }
        //For node
        xmlNodeSetPtr nodeset = xpath_res->nodesetval;

        QList<QPair<QString,QString>> results;

        for(int n = 0;n < nodeset->nodeNr;n ++){
            auto node = nodeset->nodeTab[n];
            //Get attribute herf
            auto url = xmlGetProp(node,BAD_CAST "href");
            auto url_str = QString::fromUtf8((char*)url);
            xmlFree(url);

            //Get children
            auto child = xmlGetLastChild(node);
            QString name;
            //Get the alt
            if(child != nullptr){
                auto alt = xmlNodeGetContent(child);
                auto alt_str = QString::fromUtf8((char*)alt);
                name = alt_str;
                xmlFree(alt);
            }
            //Push to list
            results.push_back(QPair<QString,QString>(name,url_str));
        }
        //Free it
        xmlXPathFreeObject(xpath_res);
        xmlFreeDoc(doc);

        providerDebug() << results;
        QString url_of;

        if(results.size() > 1){
            //Show the dialog,let user choose
            QInputDialog dialog;
            dialog.setLabelText("哎呀 搜索返回多个视频 请选择");
            QStringList items;
            for(auto pair : results){
                items.push_back(pair.first);
            }
            dialog.setComboBoxItems(items);
            dialog.setWindowTitle("Select Video");
            dialog.exec();
            auto index = dialog.textValue();
            if(index.isEmpty()){
                emit error("No video selected");
                return;
            }
            //Find the url by it
            for(auto pair : results){
                if(pair.first == index){
                    url_of = pair.second;
                    break;
                }
            }
        }
        else{
            url_of = results[0].second;
        }
        url_of.prepend(url_prefix);

        //Make a new request of it :(
        QNetworkRequest request;
        request.setUrl(QUrl(url_of));
        request.setRawHeader("User-Agent",PLAYER_USERAGENT);
        request.setRawHeader("Referer",url_of.toUtf8());

        auto rpl = manager->get(request);
        connect(rpl,&QNetworkReply::finished,[rpl,this](){
            videoPagesReady(rpl);
        });
    });
}
//--Handle receive video pages
void AngelProvider::videoPagesReady(QNetworkReply *reply){
    reply->deleteLater();
    if(reply->error()){
        emit error(reply->errorString());
        return;
    }
    //Get the data
    const auto &data = reply->readAll();
    //Todo:Parse detail page
    auto codec = QTextCodec::codecForHtml(data);
    auto html = codec->toUnicode(data);

    const auto xpath = R"(//div[@class="bfdz"]/ul/ul/li/ul/li/a)";
    auto doc = htmlReadDoc(BAD_CAST html.toStdString().c_str(),nullptr,"utf8",
        XML_PARSE_NOBLANKS | XML_PARSE_NONET | XML_PARSE_NOERROR | XML_PARSE_NOWARNING
    );
    if(doc == nullptr){
        emit error("Invalid HTML");
    }
    //Compile xpath
    auto xpath_obj = xmlXPathNewContext(doc);
    auto xpath_res = xmlXPathEvalExpression(BAD_CAST xpath,xpath_obj);
    xmlXPathFreeContext(xpath_obj);
    if(xpath_res == nullptr){
        xmlFreeDoc(doc);
        emit error("Invalid XPath");
        return;
    }
    //Get the result
    if(xmlXPathNodeSetIsEmpty(xpath_res->nodesetval)){
        xmlXPathFreeObject(xpath_res);
        xmlFreeDoc(doc);
        emit error("VideoPage : No result");
        return;
    }
    //For node
    xmlNodeSetPtr nodeset = xpath_res->nodesetval;
    for(int n = 0;n < nodeset->nodeNr;n ++){
        auto node = nodeset->nodeTab[n];

        //Get title / href prop
        auto title = xmlGetProp(node,BAD_CAST "title");
        auto href = xmlGetProp(node,BAD_CAST "href");

        //To QString
        auto title_str = QString::fromUtf8((char*)title);
        auto href_str = QString::fromUtf8((char*)href);

        //Free it
        xmlFree(title);
        xmlFree(href);

        //Check href is begin with '/'
        if(!href_str.startsWith("/")){
            continue;
        }
        //Cehck title_str is begin with '第'
        if(!title_str.startsWith("第")){
            continue;
        }
        //The string is 第numberString
        //Get number
        auto number_str = title_str.mid(1).toStdString();

        int number = std::atoi(number_str.c_str());

        providerDebug () << number << title_str << href_str;

        video_pages[number] = url_prefix + href_str;
    }
    providerDebug() << video_pages;

    //OK Done
    matched = true;

    //Create an empty to notify the player
    VideoInfo info;
    info.need_pay.push_back(false);
    info.resolutions.push_back("");
    info.resolutions_name.push_back("Auto");
    emit videoInfoReady(info);
}

void AngelProvider::fetchInfo(const SeasonInfo &season,int idx){
    const auto &episode = season.episodes[idx];

    if(matched){
        //Create an empty to notify the player
        VideoInfo info;
        info.need_pay.push_back(false);
        info.resolutions.push_back("");
        info.resolutions_name.push_back("Auto");
        emit videoInfoReady(info);
        return;
    }

    //Get name
    QString name = season.season_title;
    //Try match page first
    matchVideoPages(name);
}
void AngelProvider::fetchVideo(const SeasonInfo &season,int n,QStringView){
    //Begin fetch video by info
    if(!matched){
        emit error("Still wating for matched video page");
    }
    //real idx
    int idx = season.episodes[n].title.toInt();
    
    //Get the url
    auto &ep = season.episodes[n];
    providerDebug() << "Fetch video For : " << ep.title;

    auto url_iter = video_pages.find(ep.title.toInt());
    if(url_iter == video_pages.end()){
        emit error("No video page found");
        return;
    }


    //OK Begin match video
    QString url = url_iter.value();
    QNetworkRequest request;
    request.setUrl(QUrl(url));
    request.setRawHeader("User-Agent",PLAYER_USERAGENT);
    request.setRawHeader("Referer",search_url.toUtf8());

    auto rpl = manager->get(request);
    connect(rpl,&QNetworkReply::finished,[rpl,url,idx,this](){
        //Process here
        rpl->deleteLater();
        if(rpl->error()){
            emit error(rpl->errorString());
            return;
        }
        //Get the data
        const auto &data = rpl->readAll();
        auto codec = QTextCodec::codecForHtml(data);
        auto html = codec->toUnicode(data);

        //Find the video url javascript
        //"/playdata/167/40103.js?47149.34"
        //Use regex
        QRegularExpression re("/playdata/[0-9]+/[0-9]+\\.js\\?[0-9]+\\.[0-9]+");
        auto match = re.match(html);
        if(!match.hasMatch()){
            emit error("No video url found");
            return;
        }
        providerDebug() << "Video url : " << match.captured();

        QString js_url = url_prefix + match.captured();
        providerDebug() << "JS url : " << js_url;

        //Make a request
        QNetworkRequest request;
        request.setUrl(QUrl(js_url));
        request.setRawHeader("User-Agent",PLAYER_USERAGENT);
        request.setRawHeader("Referer",url.toUtf8());

        auto reply = manager->get(request);
        connect(reply,&QNetworkReply::finished,[reply,this,idx](){
            videoJsReady(reply,idx);
        });
    });
}
void AngelProvider::videoJsReady(QNetworkReply *reply,int idx){
    //Process here
    reply->deleteLater();
    if(reply->error()){
        emit error(reply->errorString());
        return;
    }
    //Get the data
    const auto &data = reply->readAll();
    QString js = QString::fromUtf8(data);
    //Match stirng like https://yun.66dm.net/xxx/xxx.m3u8
    QRegularExpression re("https://yun.66dm.net/[^/]+/[^/]+\\.m3u8");
    auto i = re.globalMatch(js);
    if(!i.hasNext()){
        emit error("No video url found");
        return;
    }
    QStringList list;
    while(i.hasNext()){
        list.push_back(i.next().captured());
    }
    providerDebug() << "Video url list : " << list;

    //Get url by index
    QString url = list[idx - 1];
    //0 is begin
    providerDebug() << "Video url : " << url;


    QNetworkRequest request;
    request.setUrl(QUrl(url));
    request.setRawHeader("User-Agent",PLAYER_USERAGENT);
    request.setRawHeader("Referer",reply->url().toString().toUtf8());

    //Got m3u8
    auto rpy = manager->get(request);
    connect(rpy,&QNetworkReply::finished,[rpy,this](){
        providerDebug() << "Got m3u8";
        rpy->deleteLater();
        if(rpy->error()){
            emit error(rpy->errorString());
            return;
        }
        //Get the data
        const auto &data = rpy->readAll();
        QString m3u8 = QString::fromUtf8(data);
        VideoResource res;

        if(parseEncrypedM3U8(m3u8,res)){
            emit videoReady(res);
        }
    });
}

bool TsdmProvider::parseEncrypedM3U8(const QString &m3u8,VideoResource &res){
    //Split the m3u8
    QStringList lines = m3u8.split("\n");
    //Get the video url

    qreal duration = 0;
    qreal seg_duration = 0;

    for(auto iter = lines.begin();iter != lines.end();){
        if(iter->startsWith("#EXTINF:")){
            //Get the duration
            QStringList list = iter->split(",");
            if(list.size() < 2){
                emit error("No duration found");
                return false;
            }
            seg_duration = list[0].mid(8).toDouble();
            duration += seg_duration;

            //Find url next
            ++iter;
            if(iter == lines.cend()){
                emit error("No url found");
                return false;
            }
            //Get the url
            QNetworkRequest request;
            request.setUrl(QUrl(*iter));
            request.setRawHeader("User-Agent",PLAYER_USERAGENT);
            // request.setRawHeader("Referer",rpy->url().toString().toUtf8());
            //Set range
            request.setRawHeader("Range","bytes=" + QByteArray::number(offset) + "-");
            res.videos.push_back(request);

            //Create a segment
            VideoResource::Segment seg;
            //Cast to ms
            seg.duration = seg_duration * 1000;
            seg.start = (duration - seg_duration) * 1000;
            res.segments.push_back(seg);

            //
        }
        else{
            //Skip
            ++iter;
        }
    }
    res.duration = duration * 1000;
    // for(auto &url : lines){
    //     if(url.startsWith("#")){
    //         continue;
    //     }
    //     //Get the url,create the request
    //     if(url.isEmpty()){
    //         continue;
    //     }
    //     QNetworkRequest request;
    //     request.setUrl(QUrl(url));
    //     request.setRawHeader("User-Agent",PLAYER_USERAGENT);
    //     request.setRawHeader("Referer",rpy->url().toString().toUtf8());
    //     //Set range
    //     request.setRawHeader("Range","bytes=" + QByteArray::number(offset) + "-");

    //     res.videos.push_back(request);
    // }
    res.single_video = false;
    return true;
}

void YsjdmProvider::matchVideoPages(const QString &str){
    QUrl url = QUrl(search_url.arg(str));
    QNetworkRequest request;
    request.setUrl(url);
    request.setRawHeader("User-Agent",PLAYER_USERAGENT);
    request.setRawHeader("Referer",url.toString().toUtf8());

    auto reply = manager->get(request);
    connect(reply,&QNetworkReply::finished,[reply,this](){
        reply->deleteLater();
        if(reply->error()){
            emit error(reply->errorString());
            return;
        }
        //Get the data
        const auto &data = reply->readAll();
        auto codec = QTextCodec::codecForHtml(data);
        QString html = codec->toUnicode(data);

        //Get search result
        auto doc = LXml::HtmlDocument::Parse(html.toStdString());
        auto nodes = doc.root_node().xpath(R"(//li[@class="searchlist_item"]/div/a)");

        QList<QPair<QString,QString>> results;
        for(auto node : nodes){
            QString title = node.attribute("title").c_str();
            QString url = node.attribute("href").c_str();
            results.push_back(QPair<QString,QString>(title,url));
        }
        providerDebug() << "Search result : " << results;
        if(results.empty()){
            emit error("No result found");
            return;
        }

        //Check if the result is more than one
        QString url_of;
        if(results.size() > 1){
            //Show the dialog,let user choose
            QInputDialog dialog;
            dialog.setLabelText("哎呀 搜索返回多个视频 请选择");
            QStringList items;
            for(auto pair : results){
                items.push_back(pair.first);
            }
            dialog.setComboBoxItems(items);
            dialog.setWindowTitle("Select Video");
            dialog.exec();
            auto index = dialog.textValue();
            if(index.isEmpty()){
                emit error("No video selected");
                return;
            }
            //Find the url by it
            for(auto pair : results){
                if(pair.first == index){
                    url_of = pair.second;
                    break;
                }
            }
        }
        else{
            url_of = results[0].second;
        }
        url_of.prepend(prefix);
        providerDebug() << "Fetch :" << url_of;
        
        //OK Send it
        QNetworkRequest request;
        request.setUrl(QUrl(url_of));
        request.setRawHeader("User-Agent",PLAYER_USERAGENT);
        request.setRawHeader("Referer",url_of.toUtf8());

        auto rpy = manager->get(request);
        connect(rpy,&QNetworkReply::finished,[rpy,this](){
            videoPagesReady(rpy);
        });
    });
}

void YsjdmProvider::videoPagesReady(QNetworkReply *reply){
    reply->deleteLater();

    if(reply->error()){
        emit error(reply->errorString());
        return;
    }
    //Check status code
    if(reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() != 200){
        emit error("Status code not 200");
        return;
    }
    //Get the data
    const auto &data = reply->readAll();
    auto codec = QTextCodec::codecForHtml(data);
    QString html = codec->toUnicode(data);
    
    auto doc = LXml::HtmlDocument::Parse(html.toStdString());
    auto nodes = doc.root_node().xpath(R"(//div[@class="playlist_full"]/ul/li/a)");
    if(nodes.as_nodeset().size() == 0){
        emit error("No video found");
        return;
    }
    for(auto node : nodes){
        QString title = node.content().c_str();
        QString url = node.attribute("href").c_str();
        providerDebug() << title << url;
        //Check href is begin with '/'
        if(!url.startsWith("/")){
            continue;
        }
        //Cehck title_str is begin with '第'
        if(!title.startsWith("第")){
            continue;
        }
        //The string is 第numberString
        //Get number
        auto number_str = title.mid(1).toStdString();
        int number = 0;
        number = std::atoi(number_str.c_str());
        //Insert
        video_pages.insert(number,url);
    }
    providerDebug() << "Video pages : " << video_pages;
    matched = true;

    //Create an empty to notify the player
    VideoInfo info;
    info.need_pay.push_back(false);
    info.resolutions.push_back("");
    info.resolutions_name.push_back("Auto");
    emit videoInfoReady(info);
}
void YsjdmProvider::fetchInfo(const SeasonInfo &season,int idx){
    if(matched){
        //Create an empty to notify the player
        VideoInfo info;
        info.need_pay.push_back(false);
        info.resolutions.push_back("");
        info.resolutions_name.push_back("Auto");
        emit videoInfoReady(info);
        return;
    }
    //Begin match
    matchVideoPages(season.season_title);
}
void YsjdmProvider::fetchVideo(const SeasonInfo &season,int n,QStringView){
    if(!matched){
        emit error("Still waiting for matching result");
        return;
    }
    //Get the url
    //real idx
    int idx = season.episodes[n].title.toInt();
    //Try to find the url
    auto it = video_pages.find(idx);
    if(it == video_pages.end()){
        emit error("No video found");
        return;
    }
    QString url = it.value();
    url.prepend(prefix);
    providerDebug() << "Fetch :" << url;

    QNetworkRequest request;
    request.setUrl(QUrl(url));
    request.setRawHeader("User-Agent",PLAYER_USERAGENT);
    request.setRawHeader("Referer",url.toUtf8());

    auto rpy = manager->get(request);
    connect(rpy,&QNetworkReply::finished,[rpy,this](){
        videoPageReady(rpy);
    });
}
void YsjdmProvider::videoPageReady(QNetworkReply *reply){
    reply->deleteLater();

    if(reply->error()){
        emit error(reply->errorString());
        return;
    }
    //Check status code
    if(reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() != 200){
        emit error("Status code not 200");
        return;
    }
    //Get the data
    const auto &data = reply->readAll();
    auto codec = QTextCodec::codecForHtml(data);
    QString html = codec->toUnicode(data);

    auto doc = LXml::HtmlDocument::Parse(html.toStdString());
    //Use xpath to get all; javascript 
    auto nodes = doc.root_node().xpath(R"(//script[@type="text/javascript"])");
    if(nodes.as_nodeset().size() == 0){
        emit error("No URL found");
        return;
    }
    //Get the url
    QString content;
    for(auto node : nodes){
        QString js = node.content().c_str();
        providerDebug() << js;
        if(js.startsWith("var player_aaaa=")){
            content = js.mid(16);
            break;
        }
    }
    if(content.isEmpty()){
        emit error("No URL found");
        return;
    }
    providerDebug() << "Got content : " << content;
    //To json
    QJsonDocument js = QJsonDocument::fromJson(content.toUtf8());
    if(js.isNull()){
        emit error("Invalid json");
        return;
    }
    providerDebug() << "Got json : " << js;

    auto url = js.object()["url"].toString();
    if(url.isEmpty()){
        emit error("No URL found");
        return;
    }
    providerDebug() << "Got url : " << url;
    if(url.endsWith("m3u8")){
        //Oh, it is a m3u8,we need to process it
        //Get the m3u8
        QNetworkRequest request;
        request.setUrl(QUrl(url));
        request.setRawHeader("User-Agent",PLAYER_USERAGENT);
        request.setRawHeader("Referer",reply->url().toString().toUtf8());

        //Send it
        auto rpy = manager->get(request);
        connect(rpy,&QNetworkReply::finished,[rpy,this](){
            m3u8Ready(rpy);
        });
    }
    else{
        //Just play it
        VideoResource res;
        QNetworkRequest request;
        request.setUrl(QUrl(url));
        request.setRawHeader("User-Agent",PLAYER_USERAGENT);
        request.setRawHeader("Referer",reply->url().toString().toUtf8());
        res.videos.push_back(request);
        res.single_video = true;
        res.duration = -1;
        emit videoReady(res);
    }
}
void YsjdmProvider::m3u8Ready(QNetworkReply *reply){
    reply->deleteLater();
    if(reply->error()){
        emit error(reply->errorString());
        return;
    }
    //Check status code
    if(reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() != 200){
        emit error("Status code not 200");
        return;
    }
    //Get the data
    const auto &data = reply->readAll();
    QString m3u8 = QString::fromUtf8(data);

    VideoResource res;
    if(parseEncrypedM3U8(m3u8,res)){
        providerDebug() << "Got video : ";
        emit videoReady(res);
    }
    else{
        emit error("Invalid m3u8");
    }
}

PLAYER_NS_END