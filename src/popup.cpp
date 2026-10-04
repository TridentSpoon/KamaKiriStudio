// SPDX-License-Identifier: MIT
#include "popup.h"
#include "core.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QPushButton>
#include <QTabWidget>
#include <QCalendarWidget>
#include <QPainter>
#include <QPainterPath>
#include <QKeyEvent>
#include <QDateTime>
#include <QStorageInfo>
#include <QDir>
#include <QFile>
#include <QApplication>
#include <QMessageBox>
#include <QSignalBlocker>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusMessage>
#include <QDBusArgument>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <KConfig>
#include <KConfigGroup>
#include <KWindowEffects>
#include <KIO/ApplicationLauncherJob>
#include <algorithm>
#include <QSettings>
#include <QFontDatabase>
#include <QTextCharFormat>
#include <QToolButton>
static QColor configuredColor(const KConfigGroup &group,const char *key,const QColor &fallback) {
    auto parts=group.readEntry(key,QString()).split(',');bool ok[3]={};
    if(parts.size()<3)return fallback;
    int r=parts[0].toInt(&ok[0]),g=parts[1].toInt(&ok[1]),b=parts[2].toInt(&ok[2]);
    if(!ok[0]||!ok[1]||!ok[2]||r<0||g<0||b<0||r>255||g>255||b>255)return fallback;
    return QColor(r,g,b);
}
static QLabel *plainLabel(const QString &text,const QString &name={}) {auto label=new QLabel(text);label->setTextFormat(Qt::PlainText);label->setWordWrap(true);label->setObjectName(name);return label;}
StudioPopup::StudioPopup(Mode m,bool demo,QWidget *parent,const QString &style,const QString &appearance):QWidget(parent,Qt::Tool|Qt::FramelessWindowHint),demoMode(demo),mode(m) {
    if(QIcon::themeName().isEmpty())QIcon::setThemeName("breeze");
    QIcon::setFallbackThemeName("breeze");
    setAttribute(Qt::WA_TranslucentBackground);setWindowTitle(m==Launcher?"KamaKiri launcher":"KamaKiri dashboard");
    KConfig globals(Studio::configFile(),KConfig::SimpleConfig);
    background=configuredColor(globals.group("Colors:Window"),"BackgroundNormal",palette().color(QPalette::Window));
    foreground=configuredColor(globals.group("Colors:Window"),"ForegroundNormal",palette().color(QPalette::WindowText));
    accent=configuredColor(globals.group("General"),"AccentColor",configuredColor(globals.group("Colors:Selection"),"BackgroundNormal",palette().color(QPalette::Highlight)));
    QSettings settings;QString visualStyle=style.isEmpty()?settings.value("popup/style","rounded").toString():style;
    QString mode=appearance.isEmpty()?settings.value("popup/mode","desktop").toString():appearance;
    bool fluent=visualStyle=="fluent";cornerRadius=fluent?12:24;
    bool isLight=mode=="light"||(mode!="dark"&&background.lightnessF()>=.5);
    if(fluent||mode!="desktop") {background=QColor(isLight?"#f3f3f3":"#202020");foreground=QColor(isLight?"#1b1b1b":"#f5f5f5");}
    surface=background.lightnessF()<.5?background.lighter(145):background.darker(106);
    muted=foreground;muted.setAlpha(175);
    QPalette colors=palette();for(auto role:{QPalette::Window,QPalette::Base,QPalette::Button})colors.setColor(role,background);
    for(auto role:{QPalette::WindowText,QPalette::Text,QPalette::ButtonText})colors.setColor(role,foreground);
    colors.setColor(QPalette::Highlight,accent);colors.setColor(QPalette::HighlightedText,accent.lightnessF()>.55?QColor("#111418"):QColor("#ffffff"));setPalette(colors);
    auto css=QString(R"CSS(
        QWidget {font-family:Sans Serif;color:%1;font-size:13px;background:transparent;}
        QLabel#title {font-size:20px;font-weight:650;}
        QLabel#clock {font-size:38px;font-weight:600;}
        QLabel#muted {color:%2;font-size:12px;}
        QFrame#tile {background:%3;border-radius:16px;}
        QPushButton {background:%3;border:0;border-radius:12px;padding:9px 13px;}
        QPushButton:hover {background:%4;color:%5;}
        QPushButton:disabled {color:%2;}
        QListWidget {border:0;background:transparent;outline:0;}
        QListWidget::item {padding:10px 12px;margin:2px 0;border-radius:16px;}
        QListWidget::item:selected {background:%3;}
        QLineEdit {background:%3;border:0;border-radius:16px;padding:12px 16px;selection-background-color:%4;}
        QTabWidget::pane {border:0;} QTabBar::tab {background:transparent;padding:10px 30px;border-bottom:2px solid transparent;}
        QTabBar::tab:selected {border-bottom:2px solid %4;}
        QCalendarWidget QWidget {background:transparent;} QCalendarWidget QToolButton {color:%1;background:transparent;border:0;}
        QCalendarWidget QAbstractItemView {background-color:%3;alternate-background-color:%3;color:%1;selection-background-color:%4;selection-color:%5;font-size:12px;border:0;}
        QComboBox {background:%3;border:0;border-radius:10px;padding:8px;}
        QComboBox QAbstractItemView {background:%3;color:%1;}
        QProgressBar {border:0;border-radius:7px;background:%3;text-align:center;min-height:22px;}
        QProgressBar::chunk {background:%4;border-radius:7px;}
        QScrollBar:vertical {background:transparent;width:6px;} QScrollBar::handle:vertical {background:%3;min-height:20px;border-radius:3px;}
        QScrollBar::add-line:vertical,QScrollBar::sub-line:vertical {height:0;}
    )CSS").arg(foreground.name(),muted.name(QColor::HexArgb),surface.name(),accent.name(),colors.color(QPalette::HighlightedText).name());
    if(fluent){css.replace("border-radius:16px","border-radius:8px");css.replace("border-radius:12px","border-radius:6px");css.replace("font-family:Sans Serif",QFontDatabase::families().contains("Segoe UI")?"font-family:Segoe UI":"font-family:Sans Serif");}
    setStyleSheet(css);
    auto v=new QVBoxLayout(this);v->setContentsMargins(25,20,25,24);v->setSpacing(12);
    auto header=new QHBoxLayout;header->addWidget(plainLabel(m==Launcher?"Applications":"Your desktop","title"),1);auto close=new QPushButton("×");close->setAccessibleName("Close popup");close->setFixedSize(32,32);header->addWidget(close);connect(close,&QPushButton::clicked,this,&QWidget::close);v->addLayout(header);
    if(m==Launcher) {
        resize(520,590);applications=KService::allServices();
        applications.erase(std::remove_if(applications.begin(),applications.end(),[](const KService::Ptr &s){return !s->isApplication()||s->noDisplay()||!s->isValid();}),applications.end());
        if(demoMode)applications.erase(std::remove_if(applications.begin(),applications.end(),[](const KService::Ptr &s){return !s->desktopEntryName().startsWith("org.kde.")&&!s->name().startsWith("KamaKiri");}),applications.end());
        std::sort(applications.begin(),applications.end(),[](const KService::Ptr &a,const KService::Ptr &b){return QString::localeAwareCompare(a->name(),b->name())<0;});
        appList=new QListWidget;appList->setObjectName("applications");appList->setIconSize(QSize(30,30));v->addWidget(appList,1);
        search=new QLineEdit;search->setObjectName("applicationSearch");search->setPlaceholderText("Search installed apps…");search->setClearButtonEnabled(true);v->addWidget(search);
        v->addWidget(plainLabel(demo?"Preview · launching is disabled":"Enter to open · Escape to dismiss","muted"));
        connect(search,&QLineEdit::textChanged,this,&StudioPopup::filterApplications);connect(search,&QLineEdit::returnPressed,this,&StudioPopup::launchSelected);connect(appList,&QListWidget::itemActivated,this,[this]{launchSelected();});
        filterApplications();
    } else {
        resize(760,390);auto tabs=new QTabWidget;v->addWidget(tabs,1);
        auto dashboard=new QWidget;auto row=new QHBoxLayout(dashboard);row->setContentsMargins(0,8,0,0);row->setSpacing(12);
        auto clockTile=new QFrame;clockTile->setObjectName("tile");auto cv=new QVBoxLayout(clockTile);cv->setContentsMargins(22,20,22,20);clock=plainLabel("","clock");date=plainLabel("","muted");system=plainLabel("","muted");cv->addWidget(clock);cv->addWidget(date);cv->addStretch();cv->addWidget(system);row->addWidget(clockTile,1);
        auto calendar=new QCalendarWidget;calendar->setPalette(colors);QTextCharFormat headerFormat;headerFormat.setForeground(foreground);headerFormat.setBackground(surface);calendar->setHeaderTextFormat(headerFormat);
        for(const auto &name:{"qt_calendar_prevmonth","qt_calendar_nextmonth"})if(auto arrow=calendar->findChild<QToolButton*>(name)){QPixmap pixmap(20,20);pixmap.fill(Qt::transparent);QPainter painter(&pixmap);painter.setRenderHint(QPainter::Antialiasing);painter.setPen(Qt::NoPen);painter.setBrush(foreground);bool previous=QString(name).contains("prev");painter.drawPolygon(QPolygonF{QPointF(previous?12:8,5),QPointF(previous?7:13,10),QPointF(previous?12:8,15)});arrow->setIcon(QIcon(pixmap));}
        calendar->setGridVisible(false);calendar->setVerticalHeaderFormat(QCalendarWidget::NoVerticalHeader);calendar->setMaximumWidth(390);row->addWidget(calendar,2);tabs->addTab(dashboard,"Dashboard");
        auto media=new QWidget;auto mv=new QVBoxLayout(media);players=new QComboBox;mv->addWidget(players);track=plainLabel("Open a media player to use its controls here.","title");track->setAlignment(Qt::AlignCenter);mv->addWidget(track,1);
        auto buttons=new QHBoxLayout;for(const auto &entry:QList<QStringList>{{"Previous","Previous"},{"Play / pause","PlayPause"},{"Next","Next"}}){auto button=new QPushButton(entry[0]);mediaButtons.append(button);button->setEnabled(false);buttons->addWidget(button);connect(button,&QPushButton::clicked,this,[this,method=entry[1]]{mediaAction(method);});}mv->addLayout(buttons);tabs->addTab(media,"Media");
        auto performance=new QWidget;auto grid=new QGridLayout(performance);grid->setSpacing(16);
        auto meter=[&](const QString &name,int x,int y){auto tile=new QFrame;tile->setObjectName("tile");auto layout=new QVBoxLayout(tile);layout->setContentsMargins(18,14,18,14);layout->addWidget(plainLabel(name,"title"));auto bar=new QProgressBar;bar->setRange(0,100);layout->addWidget(bar);grid->addWidget(tile,x,y);return bar;};
        cpu=meter("CPU",0,0);memory=meter("Memory",0,1);disk=meter("Disk",1,0);network=plainLabel("Network —","title");grid->addWidget(network,1,1);tabs->addTab(performance,"Performance");
        v->addWidget(plainLabel("Plasma + KWin · Escape to dismiss","muted"));time.start();refresh.setInterval(1500);connect(&refresh,&QTimer::timeout,this,&StudioPopup::updateDashboard);refresh.start();updateDashboard();
    }
}
void StudioPopup::paintEvent(QPaintEvent *) {
    QPainter p(this);p.setRenderHint(QPainter::Antialiasing);QRectF body=rect().adjusted(8,8,-8,-8);
    for(int i=7;i>=1;i--){QColor shadow(0,0,0,4);p.setPen(Qt::NoPen);p.setBrush(shadow);p.drawRoundedRect(body.adjusted(-i,-i,i,i),cornerRadius+i,cornerRadius+i);}
    QColor tint=background;tint.setAlpha(cornerRadius==12?235:238);p.setBrush(tint);QColor border=foreground;border.setAlpha(22);p.setPen(border);p.drawRoundedRect(body,cornerRadius,cornerRadius);
}
void StudioPopup::showEvent(QShowEvent *event) {
    QWidget::showEvent(event);QPainterPath path;path.addRoundedRect(QRectF(rect().adjusted(8,8,-8,-8)),cornerRadius,cornerRadius);
    if(windowHandle())KWindowEffects::enableBlurBehind(windowHandle(),true,QRegion(path.toFillPolygon().toPolygon()));
    if(search)QTimer::singleShot(0,search,[this]{search->setFocus();});
}
void StudioPopup::keyPressEvent(QKeyEvent *event) {
    if(event->key()==Qt::Key_Escape){close();return;}
    if(search && (event->key()==Qt::Key_Down||event->key()==Qt::Key_Up)) {
        int step=event->key()==Qt::Key_Down?1:-1;appList->setCurrentRow(qBound(0,appList->currentRow()+step,qMax(0,appList->count()-1)));return;
    }
    QWidget::keyPressEvent(event);
}
bool StudioPopup::event(QEvent *event) {
    if(event->type()==QEvent::WindowDeactivate&&!demoMode)QTimer::singleShot(0,this,[this]{if(!isActiveWindow()&&!QApplication::activeModalWidget())close();});
    return QWidget::event(event);
}
void StudioPopup::filterApplications() {
    appList->clear();auto query=search->text().trimmed();
    for(int i=0;i<applications.size();i++){auto service=applications[i];if(!query.isEmpty()&&!QString(service->name()+" "+service->genericName()).contains(query,Qt::CaseInsensitive))continue;
        auto icon=QIcon::fromTheme(service->icon(),QIcon::fromTheme("application-x-executable"));
        if(icon.isNull()){QPixmap pixmap(32,32);pixmap.fill(Qt::transparent);QPainter painter(&pixmap);painter.setRenderHint(QPainter::Antialiasing);QColor tint=accent;tint.setAlpha(50);painter.setBrush(tint);painter.setPen(Qt::NoPen);painter.drawRoundedRect(pixmap.rect(),8,8);painter.setPen(foreground);painter.drawText(pixmap.rect(),Qt::AlignCenter,service->name().left(1).toUpper());icon=QIcon(pixmap);}
        auto item=new QListWidgetItem(icon,service->name()+(service->genericName().isEmpty()?QString():"\n"+service->genericName()),appList);item->setData(Qt::UserRole,i);item->setToolTip(service->comment());}
    if(appList->count())appList->setCurrentRow(0);
}
int StudioPopup::visibleApplications() const {return appList?appList->count():0;}
void StudioPopup::setSearch(const QString &query) {if(search)search->setText(query);}
void StudioPopup::launchSelected() {
    if(demoMode||!appList->currentItem())return;
    auto service=applications.value(appList->currentItem()->data(Qt::UserRole).toInt());if(!service)return;
    auto job=new KIO::ApplicationLauncherJob(service,this);
    connect(job,&KJob::result,this,[this,job]{if(job->error())QMessageBox::warning(this,"Could not open app",job->errorString());else close();});job->start();
}
void StudioPopup::updateDashboard() {
    auto now=QDateTime::currentDateTime();clock->setText(now.toString("HH:mm"));date->setText(now.toString("dddd\nd MMMM yyyy"));
    QFile mem("/proc/meminfo");quint64 total=0,available=0;if(mem.open(QIODevice::ReadOnly))for(const auto &line:mem.readAll().split('\n')){auto parts=line.simplified().split(' ');if(parts.size()>1){if(parts[0]=="MemTotal:")total=parts[1].toULongLong();if(parts[0]=="MemAvailable:")available=parts[1].toULongLong();}}
    memory->setValue(total?int(100*(total-available)/total):0);
    auto storage=QStorageInfo(QDir::homePath());disk->setValue(storage.bytesTotal()>0?int(100*(storage.bytesTotal()-storage.bytesAvailable())/storage.bytesTotal()):0);
    system->setText(QString("Memory %1 / %2 GB\nDisk %3 GB free").arg((total-available)/1048576.,0,'f',1).arg(total/1048576.,0,'f',1).arg(storage.bytesAvailable()/1073741824.,0,'f',1));
    QFile stats("/proc/stat");if(stats.open(QIODevice::ReadOnly)){auto parts=stats.readLine().simplified().split(' ');quint64 sum=0,idle=0;for(int i=1;i<qMin(9,int(parts.size()));i++){sum+=parts[i].toULongLong();if(i==4||i==5)idle+=parts[i].toULongLong();}if(lastCpuTotal&&sum>lastCpuTotal)cpu->setValue(qBound(0,int(100*(sum-lastCpuTotal-idle+lastCpuIdle)/(sum-lastCpuTotal)),100));lastCpuTotal=sum;lastCpuIdle=idle;}
    QFile net("/proc/net/dev");quint64 bytes=0;if(net.open(QIODevice::ReadOnly))for(const auto &line:net.readAll().split('\n')){int colon=line.indexOf(':');if(colon<0||line.left(colon).trimmed()=="lo")continue;auto parts=line.mid(colon+1).simplified().split(' ');if(parts.size()>8)bytes+=parts[0].toULongLong()+parts[8].toULongLong();}
    qint64 elapsed=time.elapsed();network->setText(lastTime&&elapsed>lastTime&&bytes>=lastNet?QString("Network\n%1 KB/s").arg((bytes-lastNet)*1000./(elapsed-lastTime)/1024.,0,'f',1):"Network\nCollecting…");lastTime=elapsed;lastNet=bytes;
    if(demoMode){track->setText("Media controls follow your active player.");return;}
    auto bus=QDBusConnection::sessionBus();auto names=bus.interface()->registeredServiceNames();if(!names.isValid())return;
    auto selected=players->currentData().toString();QSignalBlocker block(players);players->clear();for(const auto &name:names.value())if(name.startsWith("org.mpris.MediaPlayer2."))players->addItem(name.mid(23),name);int index=players->findData(selected);if(index>=0)players->setCurrentIndex(index);
    for(auto button:mediaButtons)button->setEnabled(false);
    auto service=players->currentData().toString();if(service.isEmpty()){track->setText("Open a media player to use its controls here.");return;}
    auto message=QDBusMessage::createMethodCall(service,"/org/mpris/MediaPlayer2","org.freedesktop.DBus.Properties","GetAll");message<<"org.mpris.MediaPlayer2.Player";auto reply=bus.call(message,QDBus::Block,250);if(reply.type()==QDBusMessage::ErrorMessage)return;
    auto values=qdbus_cast<QVariantMap>(reply.arguments().value(0));auto metadata=qdbus_cast<QVariantMap>(values["Metadata"]);track->setText(metadata["xesam:title"].toString()+"\n"+metadata["xesam:artist"].toStringList().join(", "));
    mediaButtons[0]->setEnabled(values["CanGoPrevious"].toBool());mediaButtons[1]->setEnabled(values["CanControl"].toBool()&&(values["CanPlay"].toBool()||values["CanPause"].toBool()));mediaButtons[2]->setEnabled(values["CanGoNext"].toBool());
}
void StudioPopup::mediaAction(const QString &method) {
    auto service=players->currentData().toString();if(!service.startsWith("org.mpris.MediaPlayer2."))return;
    auto message=QDBusMessage::createMethodCall(service,"/org/mpris/MediaPlayer2","org.mpris.MediaPlayer2.Player",method);
    auto watcher=new QDBusPendingCallWatcher(QDBusConnection::sessionBus().asyncCall(message,1000),this);connect(watcher,&QDBusPendingCallWatcher::finished,this,[this,watcher]{QDBusPendingReply<> reply=*watcher;if(reply.isError())track->setText("The player refused that action.");watcher->deleteLater();updateDashboard();});
}
