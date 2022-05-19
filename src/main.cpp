#include "common/defs.hpp"
#include "common/app.hpp"

#include <csignal>

using namespace PLAYER_NS;

int main(int argc,char **argv){
    QApplication a(argc,argv);
    App w;
    w.show();

    return a.exec();
}