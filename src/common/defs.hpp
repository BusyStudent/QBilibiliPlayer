#pragma once


#define PLAYER_NS QCherry
#define PLAYER_NS_BEGIN namespace PLAYER_NS {
#define PLAYER_NS_END   }

#define PLAYER_USERAGENT "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/80.0.3987.132 Safari/537.36"
#define PLAYER_BILIREFERER "https://www.bilibili.com/bangumi/play/"

#include <QApplication>
#include <QObject>


PLAYER_NS_BEGIN

enum class VideoType {
    Anime = 1,
};

PLAYER_NS_END

