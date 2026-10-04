// SPDX-License-Identifier: MIT
#include "window.h"
#include "popup.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QFrame>
#include <QGridLayout>
#include <QSignalBlocker>
#include <QFileDialog>
#include <QScrollArea>
#include <QStorageInfo>
#include <QDir>
#include <QFile>
#include <QDateTime>
#include <QStandardPaths>
#include <QDBusConnectionInterface>
#include <QDBusArgument>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QUrl>
using namespace Studio;
static QLabel *text(const QString &s,const QString &name={}) {auto l=new QLabel(s);l->setWordWrap(true);l->setObjectName(name);return l;}
static QFrame *box() {auto b=new QFrame;b->setObjectName("card");return b;}
QWidget *StudioWindow::desktopPage() {
    auto scroll=new QScrollArea;scroll->setWidgetResizable(true);auto root=new QWidget;scroll->setWidget(root);
    auto v=new QVBoxLayout(root);v->setContentsMargins(0,0,6,0);v->setSpacing(12);
    v->addWidget(text("More than a color scheme.","hero"));
    v->addWidget(text("Wallpapers, matching colors and native Plasma widgets. Preview your choices before applying.","subtitle"));
    desktopSelect=new QComboBox;v->addWidget(text("Desktop target","section"));v->addWidget(desktopSelect);
    wallpaperPreview=new QLabel("Choose a wallpaper from your pictures");wallpaperPreview->setAlignment(Qt::AlignCenter);wallpaperPreview->setMinimumHeight(140);wallpaperPreview->setMaximumHeight(180);v->addWidget(wallpaperPreview);
    auto row=new QHBoxLayout;auto pick=new QPushButton("Choose wallpaper…");auto clear=new QPushButton("Clear wallpaper choice");row->addWidget(pick);row->addWidget(clear);v->addLayout(row);
    wallpaperEnabled=new QCheckBox("Include this wallpaper in the trial");wallpaperEnabled->setEnabled(false);v->addWidget(wallpaperEnabled);
    wallpaperColors=new QCheckBox("Match the theme to the wallpaper");wallpaperColors->setChecked(true);v->addWidget(wallpaperColors);
    connect(pick,&QPushButton::clicked,this,[this]{
        if(busy){error("Finish the current trial before choosing another wallpaper.");return;}
        auto path=QFileDialog::getOpenFileName(this,"Choose a local wallpaper",QStandardPaths::writableLocation(QStandardPaths::PicturesLocation),"Images (*.png *.jpg *.jpeg *.webp *.bmp)");if(path.isEmpty())return;
        try {auto image=loadWallpaper(path);wallpaperPath=QFileInfo(path).canonicalFilePath();selectedWallpaper=image;derivedPalette=wallpaperPalette(wallpaperPath);
            wallpaperPreview->setPixmap(QPixmap::fromImage(image).scaled(700,210,Qt::KeepAspectRatio,Qt::SmoothTransformation));
            wallpaperEnabled->setEnabled(true);wallpaperEnabled->setChecked(true);
            themeWallpaperEnabled->setChecked(false);if(wallpaperColors->isChecked())choosePreset("wallpaper");
            updatePreview();
        }catch(const std::exception &e){error(e.what());}
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
    v->addWidget(text("Uses the selected Appearance palette and panel settings too. No downloads or third-party widget code. Wallpaper trials support KDE’s Image wallpaper type.","subtitle"));v->addStretch();return scroll;
}
QWidget *StudioWindow::dashboardPage() {
    auto root=new QWidget;auto v=new QVBoxLayout(root);v->setContentsMargins(0,0,0,0);v->setSpacing(15);
    v->addWidget(text("Your desktop, at a glance.","hero"));v->addWidget(text("Live widgets and controls, connected to your existing Plasma session.","subtitle"));
    auto clock=box();auto cv=new QVBoxLayout(clock);clockLabel=text("","hero");dateLabel=text("","subtitle");cv->addWidget(clockLabel);cv->addWidget(dateLabel);systemLabel=text("");cv->addWidget(systemLabel);v->addWidget(clock);
    auto popups=new QHBoxLayout;for(const auto &entry:QList<QStringList>{{"Rounded app launcher","launcher"},{"Rounded dashboard","dashboard"}}){auto button=new QPushButton(entry[0]);popups->addWidget(button);connect(button,&QPushButton::clicked,this,[this,mode=entry[1]]{auto popup=new StudioPopup(mode=="launcher"?StudioPopup::Launcher:StudioPopup::Dashboard,demoMode,this,popupStyle->currentData().toString(),popupMode->currentData().toString());popup->setAttribute(Qt::WA_DeleteOnClose);popup->show();});}v->addLayout(popups);
    auto launcher=new QPushButton("KRunner · apps, files and actions");launcher->setObjectName("primary");launcher->setEnabled(!demoMode&&!QStandardPaths::findExecutable("krunner").isEmpty());
    connect(launcher,&QPushButton::clicked,this,[this]{if(!QProcess::startDetached(QStandardPaths::findExecutable("krunner"),{}))error("KRunner could not be opened.");});v->addWidget(launcher);
    auto media=box();auto mv=new QVBoxLayout(media);mv->addWidget(text("Now playing","section"));playerSelect=new QComboBox;mv->addWidget(playerSelect);trackLabel=text("Open a media player to see its controls here.");mv->addWidget(trackLabel);
    auto controls=new QHBoxLayout;for(const auto &entry:QList<QStringList>{{"Previous","Previous"},{"Play / pause","PlayPause"},{"Next","Next"}}){auto b=new QPushButton(entry[0]);mediaButtons.append(b);b->setEnabled(false);controls->addWidget(b);connect(b,&QPushButton::clicked,this,[this,method=entry[1]]{mediaAction(method);});}mv->addLayout(controls);v->addWidget(media);
    connect(playerSelect,&QComboBox::currentIndexChanged,this,[this]{refreshDashboard();});
    auto quick=box();auto qv=new QVBoxLayout(quick);qv->addWidget(text("Quick controls","section"));auto grid=new QGridLayout;qv->addLayout(grid);
    const QList<QStringList> modules{{"Sound","kcm_pulseaudio"},{"Wi-Fi and network","kcm_networkmanagement"},{"Bluetooth","kcm_bluetooth"},{"Displays","kcm_kscreen"},{"Power","kcm_powerdevilprofilesconfig"},{"Shortcuts","kcm_keys"}};
    for(int i=0;i<modules.size();i++){auto b=new QPushButton(modules[i][0]);b->setEnabled(!demoMode&&!QStandardPaths::findExecutable("systemsettings").isEmpty());grid->addWidget(b,i/3,i%3);connect(b,&QPushButton::clicked,this,[this,module=modules[i][1]]{if(!QProcess::startDetached(QStandardPaths::findExecutable("systemsettings"),{module}))error("KDE settings could not be opened.");});}
    v->addWidget(quick);v->addWidget(text("The launcher and settings use KDE’s own interfaces. Add the Application dashboard widget for a desktop app grid.","subtitle"));v->addStretch();
    dashboardTimer.setInterval(1500);connect(&dashboardTimer,&QTimer::timeout,this,[this]{if(pages->currentIndex()==2)refreshDashboard();});dashboardTimer.start();QTimer::singleShot(0,this,&StudioWindow::refreshDashboard);return root;
}
void StudioWindow::refreshDesktopTools() {
    auto old=desktopSelect->currentData();desktopSelect->clear();
    QJsonObject data;
    try {if(demoMode)data={{"desktops",QJsonArray{QJsonObject{{"id",1},{"screen",0}}}},{"types",QJsonArray::fromStringList(safeWidgetTypes())}};else data=desktopInventory();}
    catch(const std::exception &e){desktopApply->setEnabled(false);desktopSelect->setToolTip(e.what());return;}
    for(const auto &value:data["desktops"].toArray()){auto d=value.toObject();if(d["screen"].toInt(-1)<0)continue;desktopSelect->addItem(QString("Screen %1 · desktop %2").arg(d["screen"].toInt()+1).arg(d["id"].toInt()),d["id"].toInt());}
    if(desktopSelect->findData(old)>=0)desktopSelect->setCurrentIndex(desktopSelect->findData(old));
    for(auto choice:widgetChoices){bool available=data["types"].toArray().contains(choice->property("plugin").toString());choice->setEnabled(available);if(!available){choice->setChecked(false);choice->setToolTip("This KDE widget is not installed.");}}
    desktopApply->setEnabled(apply->isEnabled()&&desktopSelect->count()>0);
}
void StudioWindow::refreshDashboard() {
    auto now=QDateTime::currentDateTime();clockLabel->setText(now.toString("HH:mm"));dateLabel->setText(now.toString("dddd, d MMMM yyyy"));
    QFile mem("/proc/meminfo");quint64 total=0,available=0;if(mem.open(QIODevice::ReadOnly)){for(const auto &line:mem.readAll().split('\n')){auto parts=line.simplified().split(' ');if(parts.size()>1){if(parts[0]=="MemTotal:")total=parts[1].toULongLong();if(parts[0]=="MemAvailable:")available=parts[1].toULongLong();}}}
    auto storage=QStorageInfo(QDir::homePath());systemLabel->setText(QString("Memory %1 / %2 GB  ·  Disk %3 GB available").arg((total-available)/1048576.,0,'f',1).arg(total/1048576.,0,'f',1).arg(storage.bytesAvailable()/1073741824.,0,'f',1));
    if(demoMode){trackLabel->setText("Preview mode · live media controls connect in your desktop session.");return;}
    auto bus=QDBusConnection::sessionBus();auto services=bus.interface()->registeredServiceNames();if(!services.isValid())return;
    QString selected=playerSelect->currentData().toString();QSignalBlocker block(playerSelect);playerSelect->clear();for(const auto &name:services.value())if(name.startsWith("org.mpris.MediaPlayer2."))playerSelect->addItem(name.mid(23),name);
    int index=playerSelect->findData(selected);if(index>=0)playerSelect->setCurrentIndex(index);
    QString service=playerSelect->currentData().toString();for(auto b:mediaButtons)b->setEnabled(false);if(service.isEmpty()){trackLabel->setText("Open a media player to see its controls here.");return;}
    auto m=QDBusMessage::createMethodCall(service,"/org/mpris/MediaPlayer2","org.freedesktop.DBus.Properties","GetAll");m<<"org.mpris.MediaPlayer2.Player";
    auto reply=bus.call(m,QDBus::Block,500);if(reply.type()==QDBusMessage::ErrorMessage){trackLabel->setText("Media player is not responding.");return;}
    auto values=qdbus_cast<QVariantMap>(reply.arguments().value(0));auto metadata=qdbus_cast<QVariantMap>(values["Metadata"]);
    trackLabel->setText(metadata["xesam:title"].toString()+"\n"+metadata["xesam:artist"].toStringList().join(", ")+" · "+values["PlaybackStatus"].toString());
    mediaButtons[0]->setEnabled(values["CanGoPrevious"].toBool());mediaButtons[1]->setEnabled(values["CanControl"].toBool()&&(values["CanPlay"].toBool()||values["CanPause"].toBool()));mediaButtons[2]->setEnabled(values["CanGoNext"].toBool());
}
void StudioWindow::mediaAction(const QString &method) {
    auto service=playerSelect->currentData().toString();if(!service.startsWith("org.mpris.MediaPlayer2."))return;
    auto m=QDBusMessage::createMethodCall(service,"/org/mpris/MediaPlayer2","org.mpris.MediaPlayer2.Player",method);
    auto watcher=new QDBusPendingCallWatcher(QDBusConnection::sessionBus().asyncCall(m,1000),this);
    connect(watcher,&QDBusPendingCallWatcher::finished,this,[this,watcher]{QDBusPendingReply<> reply=*watcher;if(reply.isError())error("The media player refused this action.");watcher->deleteLater();refreshDashboard();});
}
