// SPDX-License-Identifier: MIT
#include "desktop_frame.h"
#include "core.h"
#include <LayerShellQt/window.h>
#include <QApplication>
#include <QWidget>
#include <QWindow>
#include <QScreen>
#include <QPainter>
#include <QPainterPath>
#include <QSettings>
#include <QJsonDocument>
#include <QJsonArray>
#include <QLockFile>
#include <QTimer>
#include <QFile>
#include <memory>
namespace Studio {
class FrameSurface:public QWidget {
    QColor color;QJsonObject monitor;bool reserve;bool painted=false;
public:
    FrameSurface(QScreen *screen,const QJsonObject &config,const QColor &c,int edge=-1):color(c),monitor(config),reserve(edge>=0){
        setWindowFlags(Qt::FramelessWindowHint|Qt::Tool|Qt::WindowTransparentForInput|Qt::WindowDoesNotAcceptFocus);
        setAttribute(Qt::WA_TranslucentBackground);setAttribute(Qt::WA_ShowWithoutActivating);
        setAttribute(Qt::WA_TransparentForMouseEvents);setFocusPolicy(Qt::NoFocus);
        winId();auto window=windowHandle();window->setScreen(screen);
        auto layer=LayerShellQt::Window::get(window);layer->setScope("kamakiri-desktop-frame");layer->setScreen(screen);
        layer->setLayer(reserve?LayerShellQt::Window::LayerBottom:LayerShellQt::Window::LayerTop);layer->setKeyboardInteractivity(LayerShellQt::Window::KeyboardInteractivityNone);layer->setActivateOnShow(false);
        using L=LayerShellQt::Window;
        if(reserve){
            const L::Anchor edges[]={L::AnchorTop,L::AnchorRight,L::AnchorBottom,L::AnchorLeft};
            layer->setAnchors(edge%2==0?(L::Anchors(edges[edge])|L::AnchorLeft|L::AnchorRight):(L::Anchors(edges[edge])|L::AnchorTop|L::AnchorBottom));
            layer->setExclusiveZone(32);layer->setExclusiveEdge(edges[edge]);
            layer->setDesiredSize(edge%2==0?QSize(0,1):QSize(1,0));resize(edge%2==0?QSize(screen->size().width(),1):QSize(1,screen->size().height()));
        }else{
            layer->setAnchors(L::Anchors(L::AnchorTop)|L::AnchorBottom|L::AnchorLeft|L::AnchorRight);layer->setExclusiveZone(-1);resize(screen->size());
        }
        show();
    }
    bool ready()const{return painted&&windowHandle()->isVisible();}
    void paintEvent(QPaintEvent *)override{
        painted=true;
        if(reserve)return;
        QPainter painter(this);painter.setRenderHint(QPainter::Antialiasing);
        painter.fillPath(desktopFramePath(rect(),monitor),color);
    }
};
int runDesktopFrame(){
    if(QGuiApplication::platformName()!="wayland")return 1;
    QLockFile lock(stateRoot()+"/desktop-frame.lock");lock.setStaleLockTime(0);if(!lock.tryLock(0))return 0;
    QList<FrameSurface*> surfaces;QString signature;QString currentHash;QJsonObject lastStatus;
    auto clear=[&]{qDeleteAll(surfaces);surfaces.clear();};
    auto update=[&]{
        QSettings settings;settings.sync();auto configText=settings.value("frame/config").toString();auto config=QJsonDocument::fromJson(configText.toUtf8()).object();currentHash=desktopFrameHash(configText);
        QString topology;for(auto screen:QGuiApplication::screens())topology+=screen->name()+QString::number(screen->geometry().x())+":"+QString::number(screen->geometry().y())+":"+QString::number(screen->size().width())+":"+QString::number(screen->size().height())+":"+QString::number(screen->devicePixelRatio());
        auto next=currentHash+topology;
        try{
            validateDesktopFrame(config);
            if(next!=signature){clear();signature=next;
                for(const auto &v:config["screens"].toArray()){auto spec=v.toObject();for(auto screen:QGuiApplication::screens())if(screen->name()==spec["name"].toString()){
                    for(int edge=0;edge<4;++edge)surfaces.append(new FrameSurface(screen,spec,QColor(config["color"].toString()),edge));
                    surfaces.append(new FrameSurface(screen,spec,QColor(config["color"].toString())));
                }}
            }
            bool exposed=true;QJsonArray windows;
            for(auto surface:surfaces){exposed&=surface->ready();windows.append(QJsonObject{{"painted",surface->ready()},{"width",surface->width()},{"height",surface->height()}});}
            // Disconnected outputs have no surfaces; new trials require every selected output.
            const bool complete=surfaces.size()==config["screens"].toArray().size()*5;
            QJsonObject status{{"protocol",2},{"pid",qApp->applicationPid()},{"hash",currentHash},{"ready",exposed&&complete},{"availableReady",exposed},{"surfaces",surfaces.size()},{"windows",windows},{"error",complete?"":"A desktop frame monitor disconnected. Refresh displays and try again."}};
            if(status!=lastStatus||!QFile::exists(stateRoot()+"/frame-status.json")){writeJson(stateRoot()+"/frame-status.json",status);lastStatus=status;}
        }catch(const std::exception &e){clear();signature.clear();writeJson(stateRoot()+"/frame-status.json",{{"hash",currentHash},{"ready",false},{"error",QString::fromUtf8(e.what())}});}
    };
    QObject::connect(qApp,&QGuiApplication::screenRemoved,qApp,[&](QScreen*){clear();signature.clear();});
    QTimer timer;QObject::connect(&timer,&QTimer::timeout,qApp,update);timer.start(250);update();
    int result=qApp->exec();clear();QFile::remove(stateRoot()+"/frame-status.json");return result;
}
}
