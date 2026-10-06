// SPDX-License-Identifier: MIT
#include "window.h"
#include "wallpaper_gallery.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QFrame>
#include <QGridLayout>
#include <QSignalBlocker>
#include <QFileDialog>
#include <QScrollArea>
#include <QStandardPaths>
#include <QUrl>
#include <QScreen>
#include <QGuiApplication>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QJsonDocument>

static QScreen *screenForDesktop(const QJsonObject &desktop) {
    auto g=desktop["geometry"].toObject();
    QRect geometry(g["x"].toInt(),g["y"].toInt(),g["width"].toInt(),g["height"].toInt());
    QScreen *match=nullptr;
    for(auto screen:QGuiApplication::screens())if(screen->geometry()==geometry){if(match)return nullptr;match=screen;}
    return match;
}
static QString displayLabel(const QJsonObject &desktop) {
    if(auto screen=screenForDesktop(desktop))return Studio::monitorDisplayName(screen->manufacturer(),screen->model(),screen->name());
    return QString("Display %1 · identity unavailable").arg(desktop["screen"].toInt()+1);
}
using namespace Studio;
static QLabel *text(const QString &s,const QString &name={}) {auto l=new QLabel(s);l->setWordWrap(true);l->setObjectName(name);return l;}
static QFrame *box() {auto b=new QFrame;b->setObjectName("card");return b;}
QWidget *StudioWindow::desktopPage() {
    auto root=new QWidget;auto v=new QVBoxLayout(root);v->setContentsMargins(0,0,0,0);v->setSpacing(12);
    desktopSelect=appearanceDisplay;wallpaperScope=appearanceScope;wallpaperTheme=themeSelect;wallpaperGallery=appearanceGallery;
    wallpaperScope->addItem("Selected display","single");wallpaperScope->addItem("Same wallpaper on enabled displays","all");wallpaperScope->addItem("Span across enabled displays","span");wallpaperScope->setCurrentIndex(1);
    v->addWidget(text("Local wallpaper and Plasma widgets","section"));
    v->addWidget(text("Widgets use the selected display. Theme wallpaper choices and monitor placement are above.","subtitle"));
    wallpaperPreview=new QLabel("Choose a wallpaper from your pictures");wallpaperPreview->setAlignment(Qt::AlignCenter);wallpaperPreview->setMinimumHeight(140);wallpaperPreview->setMaximumHeight(180);v->addWidget(wallpaperPreview);
    auto row=new QHBoxLayout;auto pick=new QPushButton("Choose wallpaper…");auto clear=new QPushButton("Clear wallpaper choice");row->addWidget(pick);row->addWidget(clear);v->addLayout(row);
    wallpaperEnabled=new QCheckBox("Include this wallpaper in the trial");wallpaperEnabled->setEnabled(false);v->addWidget(wallpaperEnabled);
    wallpaperColors=new QCheckBox("Match the theme to the wallpaper");wallpaperColors->setChecked(true);v->addWidget(wallpaperColors);
    connect(pick,&QPushButton::clicked,this,[this]{
        if(busy){error("Finish the current trial before choosing another wallpaper.");return;}
        auto path=QFileDialog::getOpenFileName(this,"Choose a local wallpaper",QStandardPaths::writableLocation(QStandardPaths::PicturesLocation),"Images (*.png *.jpg *.jpeg *.webp *.bmp)");if(path.isEmpty())return;
        selectWallpaper(path,wallpaperColors->isChecked());
    });
    connect(clear,&QPushButton::clicked,this,[this]{if(busy)return;wallpaperPath.clear();selectedWallpaper={};derivedPalette={};wallpaperEnabled->setChecked(false);wallpaperEnabled->setEnabled(false);wallpaperPreview->setText("Choose a wallpaper from your pictures");if(preset=="wallpaper")choosePreset("omarchy-osaka-jade");updatePreview();});
    connect(wallpaperColors,&QCheckBox::toggled,this,[this](bool on){if(on&&!wallpaperPath.isEmpty())choosePreset("wallpaper");else if(preset=="wallpaper")choosePreset("omarchy-osaka-jade");});
    connect(wallpaperEnabled,&QCheckBox::toggled,this,&StudioWindow::updatePreview);
    auto widgets=box();auto wv=new QVBoxLayout(widgets);wv->addWidget(text("Plasma widget collection","section"));
    wv->addWidget(text("Choose installed KDE widgets to add to this desktop. Existing widgets stay in place; duplicates are skipped. Drag and resize kept widgets using Plasma’s Edit Mode.","subtitle"));
    const QStringList names{"Clock","Media player","CPU monitor","Memory monitor","Application dashboard","Calendar","Volume","Network"};
    auto widgetGrid=new QGridLayout;wv->addLayout(widgetGrid);
    auto types=safeWidgetTypes();for(int i=0;i<types.size();i++){auto check=new QCheckBox(names[i]);check->setProperty("plugin",types[i]);widgetChoices.append(check);widgetGrid->addWidget(check,i/2,i%2);}
    v->addWidget(widgets);
    auto applyDesktop=new QPushButton("Apply & try desktop choices");applyDesktop->setObjectName("primary");connect(applyDesktop,&QPushButton::clicked,this,&StudioWindow::beginTrial);desktopApply=applyDesktop;v->addWidget(applyDesktop);
    v->addWidget(text("Uses the selected Appearance palette and panel settings too. No downloads or third-party widget code. Wallpaper trials support KDE’s Image wallpaper type.","subtitle"));v->addStretch();return root;
}
void StudioWindow::refreshDesktopTools() {
    auto old=desktopSelect->currentData();desktopSelect->clear();
    QJsonObject data;
    try {if(demoMode)data={{"desktops",QJsonArray{QJsonObject{{"id",1},{"screen",0}}}},{"types",QJsonArray::fromStringList(safeWidgetTypes())}};else data=desktopInventory();}
    catch(const std::exception &e){desktopApply->setEnabled(false);desktopSelect->setToolTip(e.what());return;}
    for(const auto &value:data["desktops"].toArray()){auto d=value.toObject();if(d["screen"].toInt(-1)<0||!d["active"].toBool(true))continue;desktopSelect->addItem(displayLabel(d),d["id"].toInt());}
    if(desktopSelect->findData(old)>=0)desktopSelect->setCurrentIndex(desktopSelect->findData(old));
    refreshMonitorControls(data["desktops"].toArray());
    if(!demoMode&&!monitorChanging)refreshPrimaryMonitor();
    for(auto choice:widgetChoices){bool available=data["types"].toArray().contains(choice->property("plugin").toString());choice->setEnabled(available);if(!available){choice->setChecked(false);choice->setToolTip("This KDE widget is not installed.");}}
    desktopApply->setEnabled(apply->isEnabled()&&desktopSelect->count()>0);
}
void StudioWindow::selectWallpaper(const QString &path,bool matchColors) {
    try {auto image=loadWallpaper(path);themeWallpaperEnabled->setChecked(false);wallpaperPath=QFileInfo(path).canonicalFilePath();selectedWallpaper=image;derivedPalette=wallpaperPalette(wallpaperPath);
        wallpaperPreview->setPixmap(QPixmap::fromImage(image).scaled(700,180,Qt::KeepAspectRatio,Qt::SmoothTransformation));wallpaperEnabled->setEnabled(true);wallpaperEnabled->setChecked(true);if(wallpaperGallery)wallpaperGallery->setSelected(wallpaperPath);if(appearanceGallery)appearanceGallery->setSelected(wallpaperPath);if(matchColors)choosePreset("wallpaper");updatePreview();
    }catch(const std::exception &e){error(e.what());}
}
void StudioWindow::refreshWallpaperGallery() {
    if(!wallpaperTheme||!wallpaperGallery)return;
    int index=wallpaperTheme->findData(preset);if(index>=0){QSignalBlocker block(wallpaperTheme);wallpaperTheme->setCurrentIndex(index);}
    wallpaperGallery->setWallpapers(themeWallpapers(wallpaperTheme->currentData().toString()));wallpaperGallery->setSelected(wallpaperPath);if(appearanceGallery){appearanceGallery->setWallpapers(themeWallpapers(wallpaperTheme->currentData().toString()));appearanceGallery->setSelected(wallpaperPath);}
}

void StudioWindow::refreshMonitorControls(const QJsonArray &desktops) {
    currentMonitorDesktops=desktops;
    QMap<QString,QPair<bool,QString>> choices;
    for(int i=0;i<monitorEnabled.size();i++)choices[monitorEnabled[i]->property("monitorKey").toString()]={monitorEnabled[i]->isChecked(),monitorEdges[i]->currentData().toString()};
    while(auto item=monitorLayout->takeAt(0)){delete item->widget();delete item;}monitorEnabled.clear();monitorEdges.clear();
    for(const auto &entry:desktops){auto desktop=entry.toObject();int screen=desktop["screen"].toInt(-1);if(screen<0||!desktop["active"].toBool(true))continue;
        auto matchedScreen=screenForDesktop(desktop);QString monitorKey=matchedScreen?matchedScreen->name():QString("desktop:%1").arg(desktop["id"].toInt());
        auto row=new QWidget;auto layout=new QHBoxLayout(row);layout->setContentsMargins(0,0,0,0);
        auto enabled=new QCheckBox(displayLabel(desktop));enabled->setProperty("screen",screen);enabled->setProperty("monitorKey",monitorKey);enabled->setProperty("desktopId",desktop["id"].toInt());enabled->setChecked(choices.contains(monitorKey)?choices[monitorKey].first:true);layout->addWidget(enabled);
        auto edge=new QComboBox;for(const auto &pair:QList<QStringList>{{"None","none"},{"Top","top"},{"Right","right"},{"Bottom","bottom"},{"Left","left"}})edge->addItem(pair[0],pair[1]);
        QJsonObject panel;for(const auto &p:inventory["panels"].toArray())if(p.toObject()["screen"].toInt(0)==screen){panel=p.toObject();break;}
        edge->setProperty("panelId",panel.isEmpty()?-1:panel["id"].toInt());edge->setCurrentIndex(qMax(0,edge->findData(choices.contains(monitorKey)?choices[monitorKey].second:panel["location"].toString("none"))));
        bool primary=demoMode?screen==0:(primaryMonitorReady&&matchedScreen&&matchedScreen->name()==primaryMonitorName);if(primary){enabled->setText(enabled->text()+" · primary");QString selection=edge->currentData().toString();edge->removeItem(0);edge->setCurrentIndex(edge->findData(selection=="none"?"bottom":selection));}
        edge->setToolTip(primary?"The primary monitor keeps a panel.":"None removes this monitor's panel; No restores it during a trial.");
        edge->setEnabled(enabled->isChecked()&&(demoMode||primaryMonitorReady));connect(enabled,&QCheckBox::toggled,this,[this,edge](bool checked){edge->setEnabled(checked&&(demoMode||primaryMonitorReady));});connect(edge,&QComboBox::currentIndexChanged,this,[this,edge,primary]{if(primary&&edge->currentData()!="none")edgeSelect->setCurrentText(edge->currentData().toString());});layout->addWidget(new QLabel("Panel"));layout->addWidget(edge);auto primaryButton=new QPushButton(primary?"Primary monitor":"Set primary monitor");
        primaryButton->setObjectName("setPrimaryMonitor");
        auto output=screenForDesktop(desktop);QString connector=output?output->name():QString();
        primaryButton->setEnabled(primaryMonitorReady&&!demoMode&&!primary&&!connector.isEmpty()&&!QStandardPaths::findExecutable("kscreen-doctor").isEmpty());
        primaryButton->setToolTip("Applies immediately to KDE. Finish any appearance trial first. KDE may move the primary panel.");
        connect(primaryButton,&QPushButton::clicked,this,[this,connector]{setPrimaryMonitor(connector);});
        layout->addWidget(primaryButton);layout->addStretch();monitorLayout->addWidget(row);monitorEnabled.append(enabled);monitorEdges.append(edge);
    }
}

void StudioWindow::identifyMonitors() {
    if(demoMode){notice->setText("Display identification is available in your Plasma session.");return;}
    auto message=QDBusMessage::createMethodCall("org.kde.KWin","/org/kde/KWin/Effect/OutputLocator1","org.kde.KWin.Effect.OutputLocator1","show");
    auto watcher=new QDBusPendingCallWatcher(QDBusConnection::sessionBus().asyncCall(message,5000),this);
    connect(watcher,&QDBusPendingCallWatcher::finished,this,[this,watcher]{
        QDBusPendingReply<> reply=*watcher;watcher->deleteLater();
        if(reply.isError())error("KDE could not identify the monitors. Open System Settings → Display & Monitor to identify them. "+reply.error().message());
    });
}
void StudioWindow::queryKdeOutputs(std::function<void(QJsonArray,QString)> callback) {
    const QString doctor=QStandardPaths::findExecutable("kscreen-doctor");
    if(doctor.isEmpty()){callback({},"KDE's display tool is missing. Open System Settings → Display & Monitor.");return;}
    auto query=new QProcess(this);auto deadline=new QTimer(query);deadline->setSingleShot(true);
    connect(deadline,&QTimer::timeout,query,&QProcess::kill);deadline->start(10000);
    connect(query,&QProcess::errorOccurred,this,[query,callback](QProcess::ProcessError code){if(code==QProcess::FailedToStart){query->deleteLater();callback({},"KDE's display tool could not start.");}});
    connect(query,&QProcess::finished,this,[query,deadline,callback](int code,QProcess::ExitStatus status){
        deadline->stop();auto bytes=query->readAllStandardOutput();query->deleteLater();
        if(code!=0||status!=QProcess::NormalExit){callback({},"Cannot read KDE display configuration. Use Refresh displays to retry.");return;}
        QJsonParseError parseError;auto document=QJsonDocument::fromJson(bytes,&parseError);
        if(parseError.error!=QJsonParseError::NoError||!document.isObject()||!document.object()["outputs"].isArray()){callback({},"KDE returned unreadable display information. Use Refresh displays to retry.");return;}
        callback(document.object()["outputs"].toArray(),{});
    });
    query->start(doctor,QStringList{"--json"});
}
void StudioWindow::refreshPrimaryMonitor(const QString &expected) {
    if(demoMode||primaryQueryRunning||busy)return;
    primaryQueryRunning=true;
    queryKdeOutputs([this,expected](QJsonArray outputs,QString problem){
        primaryQueryRunning=false;monitorChanging=false;
        if(problem.isEmpty())try{primaryMonitorName=kdePrimaryConnector(outputs);primaryMonitorReady=true;}catch(const std::exception &e){problem=e.what();}
        if(!problem.isEmpty()){
            primaryMonitorReady=false;primaryMonitorName.clear();
            monitorStatus->setText(problem);refreshMonitorControls(currentMonitorDesktops);
            if(!expected.isEmpty())error("The primary-monitor change could not be verified. "+problem);
            return;
        }
        refreshMonitorControls(currentMonitorDesktops);
        QString name=primaryMonitorName;
        for(auto screen:QGuiApplication::screens())if(screen->name()==primaryMonitorName)name=monitorDisplayName(screen->manufacturer(),screen->model(),screen->name());
        if(!expected.isEmpty()&&primaryMonitorName!=expected){
            monitorStatus->setText("KDE still reports "+name+" as primary. The change was not confirmed.");
            error("KDE did not confirm the requested primary monitor. Open System Settings → Display & Monitor or retry.");return;
        }
        monitorStatus->setText("Primary monitor: "+name);
        if(!expected.isEmpty()){notice->setText("Primary monitor confirmed by KDE: "+name);refreshInventory();}
    });
}
void StudioWindow::setPrimaryMonitor(const QString &connector) {
    if(demoMode)return;
    if(busy||monitorChanging||primaryQueryRunning||updateProcess){error("Finish the current trial, display check or update before changing the primary monitor.");return;}
    monitorChanging=true;monitorStatus->setText("Checking connected displays…");
    queryKdeOutputs([this,connector](QJsonArray outputs,QString problem){
        QString argument;
        if(problem.isEmpty())try{argument=primaryMonitorArgument(outputs,connector);}catch(const std::exception &e){problem=e.what();}
        if(!problem.isEmpty()){monitorChanging=false;monitorStatus->setText(problem);error(problem);return;}
        auto change=new QProcess(this);auto timeout=new QTimer(change);timeout->setSingleShot(true);
        connect(timeout,&QTimer::timeout,change,&QProcess::kill);timeout->start(10000);
        monitorStatus->setText("Setting primary monitor…");
        connect(change,&QProcess::errorOccurred,this,[this,change](QProcess::ProcessError error){if(error==QProcess::FailedToStart){change->deleteLater();monitorChanging=false;monitorStatus->setText("KDE's display tool could not start.");this->error(monitorStatus->text());}});
        connect(change,&QProcess::finished,this,[this,change,timeout,connector](int result,QProcess::ExitStatus exit){
            timeout->stop();QString detail=QString::fromUtf8(change->readAllStandardError());change->deleteLater();
            if(result!=0||exit!=QProcess::NormalExit){monitorChanging=false;monitorStatus->setText("KDE could not change the primary monitor.");error(monitorStatus->text()+" "+detail);return;}
            monitorStatus->setText("Verifying primary monitor with KDE…");
            QTimer::singleShot(500,this,[this,connector]{refreshPrimaryMonitor(connector);});
        });
        change->start(QStandardPaths::findExecutable("kscreen-doctor"),QStringList{argument});
    });
}
