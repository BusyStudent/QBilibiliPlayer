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

class VideoProvider;

//Console text red macro
#define CONSOLE_RED(x) "\033[31m" << x << "\033[0m"
#define CONSOLE_YELLOW(x) "\033[33m" << x << "\033[0m"
#define CONSOLE_GREEN(x) "\033[32m" << x << "\033[0m"
#define CONSOLE_BLUE(x) "\033[34m" << x << "\033[0m"

#define mplayerDebug() qDebug() << CONSOLE_RED("[Player::Internal]")
#define playerDebug() qDebug() << CONSOLE_YELLOW("[Player]")
#define providerDebug() qDebug() << CONSOLE_GREEN("[Provider]")
#define vbrowserDebug() qDebug() << CONSOLE_BLUE("[VideoBrowser]")

PLAYER_NS_END

