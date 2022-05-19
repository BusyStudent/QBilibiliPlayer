// #include <QDomDocument>
// #include <QXmlQuery>

// #include "common/app.hpp"

// PLAYER_NS_BEGIN

// #define XPATH_EXP "//div/ul/li/h2/a/@href"

// void SukuraProvider::fetchInfo(const SeasonInfo &,int){
//     //We didnot need to get info 
//     VideoInfo info;
//     info.need_pay.push_back(false);
//     info.resolutions.push_back("");
//     info.resolutions_name.push_back("Auto");

//     emit videoInfoReady(info);
// }
// void SukuraProvider::fetchVideo(const SeasonInfo &info,int n,QStringView){
//     const auto &ep = info.episodes[n];

//     QUrl url(search_url + ep.title);

//     QNetworkRequest request(url);
//     request.setRawHeader("User-Agent",PLAYER_USERAGENT);
//     request.setRawHeader("Referer",search_url.toUtf8());

//     QNetworkReply *reply = manager->get(request);
//     connect(reply,&QNetworkReply::finished,[this,reply](){
//         reply->deleteLater();

//         if(reply->error()){
//             emit error(reply->errorString());
//             return;
//         }
//         QString html = reply->readAll();
//         //Make a XPath query
//         QXmlQuery query;
//         query.setFocus(html);
//         query.setQuery(XPATH_EXP);
//         QStringList list;
//         query.evaluateTo(&list);

//         if(list.size() == 0){
//             emit error("No episode found");
//             return;
//         }
        
//     });
// }


// PLAYER_NS_END