// SPDX-License-Identifier: MIT
#include "desktop_frame.h"
#include "core.h"
#include <QCryptographicHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSettings>
#include <QProcess>
#include <QElapsedTimer>
#include <QThread>
#include <QCoreApplication>
#include <QSet>
#include <QFile>
#include <stdexcept>
namespace Studio {
QString desktopFrameHash(const QString &config){return QString::fromLatin1(QCryptographicHash::hash(config.toUtf8(),QCryptographicHash::Sha256).toHex());}
QPainterPath desktopFramePath(const QRectF &bounds,const QJsonObject &monitor){
    QRectF outer=bounds;const auto edge=monitor["edge"].toString();int panel=monitor["panelSize"].toInt();
    if(edge=="left")outer.adjust(panel,0,0,0);
    if(edge=="right")outer.adjust(0,0,-panel,0);
    if(edge=="top")outer.adjust(0,panel,0,0);
    if(edge=="bottom")outer.adjust(0,0,0,-panel);
    QPainterPath path;path.setFillRule(Qt::OddEvenFill);path.addRect(outer);path.addRoundedRect(outer.adjusted(8,8,-8,-8),24,24);return path;
}
void validateDesktopFrame(const QJsonObject &config){
    if(config.isEmpty())return;
    auto bad=[] {throw std::runtime_error("Invalid desktop frame configuration.");};
    if(config["version"].toInt()!=1||!QColor(config["color"].toString()).isValid())bad();
    auto screens=config["screens"].toArray();if(screens.isEmpty()||screens.size()>16)bad();QSet<QString> names;
    for(const auto &v:screens){auto s=v.toObject();auto name=s["name"].toString();auto edge=s["edge"].toString();int size=s["panelSize"].toInt(-1);
        if((edge=="none"&&size!=0)||(edge!="none"&&size<24))bad();
        if(name.isEmpty()||name.size()>128||names.contains(name)||!QStringList{"none","left","right","top","bottom"}.contains(edge)||size<0||size>96)bad();names.insert(name);
    }
}
void syncDesktopFrame(bool requireAllScreens,const QString &executable){
    QSettings settings;settings.sync();const auto config=settings.value("frame/config").toString();
    auto doc=QJsonDocument::fromJson(config.toUtf8());if(!config.isEmpty()&&!doc.isObject())throw std::runtime_error("Invalid saved desktop frame settings.");validateDesktopFrame(doc.object());
    QFile::remove(stateRoot()+"/frame-status.json");
    if(!QProcess::startDetached(executable.isEmpty()?QCoreApplication::applicationFilePath():executable,{"--desktop-frame"}))throw std::runtime_error("The desktop frame could not start. Your trial will be restored.");
    QElapsedTimer timer;timer.start();
    while(timer.elapsed()<7000){auto status=readJson(stateRoot()+"/frame-status.json");if(status["hash"]==desktopFrameHash(config)){
        if((requireAllScreens&&status["ready"].toBool())||(!requireAllScreens&&status["availableReady"].toBool()))return;
        if(!status["error"].toString().isEmpty())throw std::runtime_error(status["error"].toString().toStdString());
    }QThread::msleep(50);}
    throw std::runtime_error("The desktop frame did not become ready. Check the Wayland session and restart KamaKiriStudio. Your recovery snapshot has been preserved.");
}
}
