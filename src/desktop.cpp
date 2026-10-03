// SPDX-License-Identifier: MIT
#include "core.h"
#include <QJsonDocument>
#include <QFileInfo>
#include <QImageReader>
#include <QUrl>
#include <QSet>
#include <QStandardPaths>
#include <QRegularExpression>
#include <stdexcept>
namespace Studio {
static void bad(const QString &s) {throw std::runtime_error(s.toStdString());}
QJsonObject desktopInventory() {
    auto text=plasmaScript(R"JS(print(JSON.stringify({types:knownWidgetTypes,panelWidgets:panels().map(function(d){return {id:d.id,widgets:d.widgets().map(function(w){w.currentConfigGroup=['General'];return {id:w.id,type:w.type,owner:w.readConfig('KamaKiriTransaction','')};})};}),desktops:desktops().map(function(d){d.currentConfigGroup=['Wallpaper',d.wallpaperPlugin,'General'];return {id:d.id,screen:d.screen,plugin:d.wallpaperPlugin,image:d.readConfig('Image',''),widgets:d.widgets().map(function(w){w.currentConfigGroup=['General'];return {id:w.id,type:w.type,owner:w.readConfig('KamaKiriTransaction','')};})};})})))JS");
    auto doc=QJsonDocument::fromJson(text.trimmed().toUtf8());
    if(!doc.isObject()) bad("Cannot read desktop wallpaper and widgets.");
    return doc.object();
}
QStringList safeWidgetTypes() {return {"org.kde.plasma.digitalclock","org.kde.plasma.mediacontroller","org.kde.plasma.systemmonitor.cpu","org.kde.plasma.systemmonitor.memory","org.kde.plasma.kickerdash","org.kde.plasma.calendar","org.kde.plasma.volume","org.kde.plasma.networkmanagement"};}
QImage loadWallpaper(const QString &path) {
    QFileInfo file(path);
    if(!file.isAbsolute()||!file.isFile()||file.size()>50*1024*1024) bad("Choose a local image smaller than 50 MB.");
    QImageReader reader(path); reader.setAutoTransform(true);
    auto format=reader.format().toLower();
    if(!QList<QByteArray>{"png","jpeg","jpg","webp","bmp"}.contains(format)) bad("Choose a PNG, JPEG, WebP or BMP image.");
    auto size=reader.size();
    if(size.width()<=0||size.height()<=0||qint64(size.width())*size.height()>64000000) bad("Wallpaper dimensions must be below 64 million pixels.");
    reader.setScaledSize(size.scaled(640,360,Qt::KeepAspectRatio));
    auto image=reader.read(); if(image.isNull()) bad("The wallpaper image could not be read.");
    return image;
}
QJsonObject wallpaperPalette(const QString &path) {
    auto image=loadWallpaper(path); double scores[36]={};
    for(int y=0;y<image.height();y+=2) for(int x=0;x<image.width();x+=2) {
        auto c=image.pixelColor(x,y); if(c.alpha()<128||c.hslHueF()<0) continue;
        scores[qMin(35,int(c.hslHueF()*36))]+=c.hslSaturationF()*(0.3+1.0-qAbs(c.lightnessF()-0.5)*2);
    }
    int best=0; for(int i=1;i<36;i++)if(scores[i]>scores[best])best=i;
    double hue=(best+0.5)/36;
    auto color=[&](double saturation,double lightness){return QColor::fromHslF(hue,saturation,lightness).name();};
    return {{"background",color(.18,.105)},{"dark_background",color(.18,.075)},{"lighter_background",color(.18,.17)},
        {"foreground",color(.15,.91)},{"dark_foreground",color(.12,.66)},{"accent",color(.55,.68)},{"selection",color(.35,.29)},
        {"magenta","#cba6f7"},{"red","#f38ba8"},{"orange","#fab387"},{"green","#a6e3a1"},{"mode","dark"}};
}
QString wallpaperScript(int id,const QString &image) {
    if(id<0||image.size()>8192)bad("Invalid wallpaper target.");
    return QString("try {var d=desktopById(%1);if(!d||d.wallpaperPlugin!=='org.kde.image')throw new Error('Image wallpaper target unavailable');d.currentConfigGroup=['Wallpaper','org.kde.image','General'];d.writeConfig('Image',%2);print('STUDIO_OK');}catch(e){print('STUDIO_ERROR:'+e);}").arg(id).arg(jsonString(image));
}
QString addWidgetsScript(int id,const QJsonArray &types,const QString &owner) {
    if(id<0||!QRegularExpression("^[a-f0-9-]{36}$").match(owner).hasMatch())bad("Invalid widget target.");
    QSet<QString> seen;
    for(const auto &t:types){if(!t.isString()||!safeWidgetTypes().contains(t.toString())||seen.contains(t.toString()))bad("Unsupported widget choice.");seen.insert(t.toString());}
    QString data=QString::fromUtf8(QJsonDocument(types).toJson(QJsonDocument::Compact));
    return QString(R"JS(try {var d=desktopById(%1);if(!d)throw new Error('Desktop unavailable');var choices=%2;choices.forEach(function(type,i){if(knownWidgetTypes.indexOf(type)<0)throw new Error('Widget unavailable');if(d.widgets(type).length===0){var w;try{w=d.addWidget(type,30+i*310,40,280,180);w.currentConfigGroup=['General'];w.writeConfig('KamaKiriTransaction',%3);if(w.readConfig('KamaKiriTransaction','')!==%3)throw new Error('Cannot mark new widget');}catch(e){if(w)w.remove();throw e;}}});print('STUDIO_OK');}catch(e){print('STUDIO_ERROR:'+e);})JS").arg(id).arg(data).arg(jsonString(owner));
}
QString removeTrialWidgetsScript(const QString &owner) {
    if(!QRegularExpression("^[a-f0-9-]{36}$").match(owner).hasMatch())bad("Invalid widget recovery marker.");
    return QString(R"JS(try {desktops().concat(panels()).forEach(function(d){d.widgets().forEach(function(w){w.currentConfigGroup=['General'];if(w.readConfig('KamaKiriTransaction','')===%1)w.remove();});});print('STUDIO_OK');}catch(e){print('STUDIO_ERROR:'+e);})JS").arg(jsonString(owner));
}
QString panelPopupActionsScript(int id,const QString &owner) {
    if(id<0||!QRegularExpression("^[a-f0-9-]{36}$").match(owner).hasMatch())bad("Invalid popup panel target.");
    QJsonArray urls;
    for(const auto &name:{"kamakiri-launcher.desktop","kamakiri-dashboard.desktop"}) {
        auto path=QStandardPaths::locate(QStandardPaths::GenericDataLocation,"applications/"+QString(name));
        if(path.isEmpty())bad("Install KamaKiriStudio before adding its panel buttons.");
        urls.append(QUrl::fromLocalFile(path).toString());
    }
    auto data=QString::fromUtf8(QJsonDocument(urls).toJson(QJsonDocument::Compact));
    return QString(R"JS(try {var p=panelById(%1);if(!p||knownWidgetTypes.indexOf('org.kde.plasma.icon')<0)throw new Error('Panel icon widget unavailable');var urls=%2;urls.forEach(function(url){var exists=p.widgets('org.kde.plasma.icon').some(function(w){w.currentConfigGroup=['General'];return w.readConfig('url','')===url;});if(!exists){var w;try{w=p.addWidget('org.kde.plasma.icon');w.currentConfigGroup=['General'];w.writeConfig('KamaKiriTransaction',%3);w.writeConfig('url',url);if(w.readConfig('KamaKiriTransaction','')!==%3||w.readConfig('url','')!==url)throw new Error('Could not configure panel button');}catch(e){if(w)w.remove();throw e;}}});print('STUDIO_OK');}catch(e){print('STUDIO_ERROR:'+e);})JS").arg(id).arg(data).arg(Studio::jsonString(owner));
}

}
