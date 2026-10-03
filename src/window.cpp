// SPDX-License-Identifier: MIT
#include "window.h"
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QFormLayout>
#include <QFrame>
#include <QColorDialog>
#include <QMessageBox>
#include <QButtonGroup>
#include <QCloseEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QDir>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDateTime>
#include <QUuid>
#include <QLockFile>
#include <QScrollArea>
#include <QTimer>
#include <algorithm>
#include <QSettings>
#include <KConfig>
#include <KConfigGroup>
#include "popup.h"
#include <signal.h>
#include <errno.h>

using namespace Studio;
static QLabel *label(const QString &text,const QString &name=QString()) {
    auto l=new QLabel(text); l->setWordWrap(true); if(!name.isEmpty()) l->setObjectName(name); return l;
}
static QFrame *card() {auto f=new QFrame;f->setObjectName("card");return f;}

void DesktopPreview::paintEvent(QPaintEvent *) {
    QPainter p(this); p.setRenderHint(QPainter::Antialiasing);
    QRectF bounds=rect().adjusted(1,1,-1,-1);
    auto palette=customPalette.isEmpty()?omarchyPalette(preset):customPalette;
    QColor bg=light?QColor("#ece9f5"):QColor(preset=="caelestia"?"#181622":"#181b21");
    if (!palette.isEmpty()) bg=QColor(palette["background"].toString());
    QLinearGradient g(bounds.topLeft(),bounds.bottomRight());
    g.setColorAt(0,bg);g.setColorAt(1,palette.isEmpty()?(light?QColor("#dadde8"):QColor("#262234")):QColor(palette["dark_background"].toString()));
    p.setPen(Qt::NoPen);p.setBrush(g);p.drawRoundedRect(bounds,16,16);
    if(!wallpaper.isNull()){p.drawImage(bounds,wallpaper);p.fillRect(bounds,QColor(0,0,0,55));}
    p.setBrush(QColor(accent.red(),accent.green(),accent.blue(),30));
    p.drawEllipse(QPointF(width()*0.79,height()*0.48),width()*0.25,height()*0.65);
    p.setBrush(QColor(accent.red(),accent.green(),accent.blue(),22));
    p.drawEllipse(QPointF(width()*0.46,height()*0.94),width()*0.38,height()*0.75);
    QRectF win(width()*0.13,height()*0.20,width()*0.57,height()*0.59);
    QColor surface=light?QColor("#faf9ff"):QColor("#24222e");
    if (!palette.isEmpty()) surface=QColor(palette["lighter_background"].toString());
    p.setBrush(surface);p.setPen(QColor(light?"#c8c2d5":"#383343"));p.drawRoundedRect(win,12,12);
    p.setPen(Qt::NoPen);p.setBrush(accent);p.drawEllipse(win.topLeft()+QPointF(15,14),3,3);
    p.setBrush(QColor(light?"#d7d1e1":"#514b60"));
    p.drawRoundedRect(QRectF(win.x()+28,win.y()+11,72,5),2,2);
    p.setPen(palette.isEmpty()?QColor(light?"#21212d":"#f4f0ff"):QColor(palette["foreground"].toString()));
    QFont title=font();title.setPointSize(14);title.setBold(true);p.setFont(title);
    p.drawText(win.adjusted(22,27,-16,-18),Qt::AlignTop|Qt::AlignLeft,"Make room for your desktop.");
    p.setFont(font());p.setPen(QColor(light?"#625c70":"#aaa2bc"));
    p.drawText(win.adjusted(22,56,-16,-10),Qt::AlignTop|Qt::AlignLeft,"Your Plasma widgets. Your workflow.");
    p.setPen(Qt::NoPen);p.setBrush(accent);
    p.drawRoundedRect(QRectF(win.x()+22,win.y()+win.height()-32,82,18),8,8);
    bool side=edge=="left"||edge=="right";
    QRectF panel;
    double gap=floating?10:0;
    if(side) panel=QRectF(edge=="left"?gap:width()-34-gap,gap,34,height()-2*gap);
    else panel=QRectF(gap,edge=="top"?gap:height()-28-gap,width()-2*gap,28);
    p.setBrush(surface);p.drawRoundedRect(panel,floating?10:0,floating?10:0);
    for(int i=0;i<5;i++) {
        p.setBrush(i==0?accent:QColor(light?"#bfb8cd":"#5e566d"));
        if(side) p.drawRoundedRect(QRectF(panel.x()+10,panel.y()+12+i*25,14,14),4,4);
        else p.drawRoundedRect(QRectF(panel.x()+12+i*25,panel.y()+7,14,14),4,4);
    }
    if(!side) {p.setPen(light?QColor("#625c70"):QColor("#d3cadf"));p.drawText(panel.adjusted(0,0,-16,0),Qt::AlignRight|Qt::AlignVCenter,"12:35");}
    p.setPen(QColor(light?"#625c70":"#aaa2bc"));
    p.drawText(bounds.adjusted(14,10,-14,-10),Qt::AlignRight|Qt::AlignTop,"STYLE PREVIEW");
}

StudioWindow::StudioWindow(bool demo,QWidget *parent):QMainWindow(parent),demoMode(demo) {
    setWindowTitle(demo?"KamaKiriStudio — Preview mode":"KamaKiriStudio");
    resize(1080,920);setMinimumSize(860,810);
    setStyleSheet(R"(
      QMainWindow,QDialog {background:#15151e;color:#efedf7;}
      QWidget {font-family:Sans Serif;font-size:13px;color:#efedf7;}
      QLabel#hero {font-size:29px;font-weight:700;} QLabel#subtitle {color:#a8a3b8;}
      QLabel#section {font-size:17px;font-weight:600;}
      QFrame#card {background:#20202c;border:1px solid #353344;border-radius:12px;}
      QPushButton {background:#2a2839;border:1px solid #474156;border-radius:8px;padding:9px 14px;}
      QPushButton:hover {background:#373247;} QPushButton:checked {border:2px solid #c4a7ff;background:#302740;}
      QPushButton:disabled {color:#888391;border-color:#35313e;}
      QPushButton#primary {background:#c4a7ff;color:#21192e;border:none;font-weight:700;}
      QPushButton#primary:disabled {background:#655875;color:#c0b7cb;}
      QComboBox,QSpinBox,QListWidget {background:#242330;border:1px solid #474156;border-radius:7px;padding:6px;}
      QComboBox QAbstractItemView {background:#242330;selection-background-color:#51416a;}
      QListWidget::item {padding:13px;} QListWidget::item:selected {background:#423454;border-radius:7px;}
      QCheckBox {spacing:8px;} QTabBar::tab {padding:12px;}
      QScrollArea {border:none;background:transparent;} QScrollArea>QWidget>QWidget {background:transparent;}
    )");
    referenceManagerStyle=styleSheet();
    auto central=new QWidget;setCentralWidget(central);
    auto layout=new QHBoxLayout(central);layout->setContentsMargins(22,22,22,22);layout->setSpacing(25);
    auto side=new QVBoxLayout;side->setSpacing(10);
    side->addWidget(label("KAMAKIRI\nSTUDIO","section"));
    side->addWidget(label("A desktop that feels like you.","subtitle"));side->addSpacing(25);
    pages=new QStackedWidget;
    pages->addWidget(appearancePage());pages->addWidget(desktopPage());pages->addWidget(dashboardPage());pages->addWidget(sessionsPage());pages->addWidget(recoveryPage());pages->addWidget(aboutPage());
    auto navigation=new QButtonGroup(this);navigation->setExclusive(true);
    QStringList names{"Appearance","Wallpaper and widgets","Dashboard","Desktop sessions","Recovery and profiles","About and updates"};
    for(int i=0;i<names.size();i++) {
        auto b=new QPushButton(names[i]);b->setCheckable(true);navigation->addButton(b,i);side->addWidget(b);
        connect(b,&QPushButton::clicked,this,[this,i]{pages->setCurrentIndex(i);});
        if(i==0)b->setChecked(true);
    }
    connect(pages,&QStackedWidget::currentChanged,this,[navigation](int index){if(auto button=navigation->button(index))button->setChecked(true);});
    side->addStretch();
    side->addWidget(label(demo?"PREVIEW MODE\nDesktop settings stay untouched.":"KDE FIRST\nPlasma + KWin stay in place.","subtitle"));
    auto sideWidget=new QWidget;sideWidget->setLayout(side);sideWidget->setFixedWidth(205);layout->addWidget(sideWidget);
    layout->addWidget(pages,1);
    timer.setInterval(200);connect(&timer,&QTimer::timeout,this,&StudioWindow::pollTrial);timer.start();
    qApp->installEventFilter(this);
    refreshInventory();
    updatePreview();
    QTimer::singleShot(0,this,&StudioWindow::recoverPending);
}
QWidget *StudioWindow::appearancePage() {
    auto root=new QWidget;auto v=new QVBoxLayout(root);v->setContentsMargins(0,0,0,0);v->setSpacing(12);
    v->addWidget(label("Your desktop, reimagined.","hero"));
    v->addWidget(label("Choose a complete color palette, then adjust your Plasma panel layout.","subtitle"));
    colorEnabled=new QCheckBox("Include KDE color changes in this trial");colorEnabled->setChecked(true);v->addWidget(colorEnabled);
    themeSelect=new QComboBox;
    themeSelect->addItem("Caelestia inspired · Lavender","caelestia");
    themeSelect->addItem("Ryoku inspired · Graphite and rose","ryoku");
    themeSelect->addItem("Breeze · KDE classic","breeze");
    themeSelect->addItem("From your wallpaper · choose an image first","wallpaper");
    for (const auto &value:omarchyPalettes()) {
        auto p=value.toObject(); themeSelect->addItem("Omarchy · "+p["name"].toString()+" · "+p["mode"].toString(),p["id"].toString());
    }
    v->addWidget(label("Color theme","section")); v->addWidget(themeSelect);
    connect(themeSelect,&QComboBox::currentIndexChanged,this,[this]{choosePreset(themeSelect->currentData().toString());});
    preview=new DesktopPreview;v->addWidget(preview,1);
    auto options=card();auto form=new QFormLayout(options);form->setContentsMargins(16,12,16,12);form->setVerticalSpacing(10);
    accentButton=new QPushButton("Accent · #c4a7ff");
    connect(accentButton,&QPushButton::clicked,this,[this]{auto c=QColorDialog::getColor(accent,this,"Choose an accent");if(c.isValid()){accent=c;colorEnabled->setChecked(true);updatePreview();}});
    light=new QCheckBox("Light surfaces");connect(light,&QCheckBox::toggled,this,&StudioWindow::updatePreview);
    auto paletteRow=new QHBoxLayout;paletteRow->addWidget(accentButton);paletteRow->addWidget(light);paletteRow->addStretch();form->addRow("Palette",paletteRow);
    panelEnabled=new QCheckBox("Adjust one existing panel");panelEnabled->setChecked(true);
    connect(panelEnabled,&QCheckBox::toggled,this,[this](bool enabled){panelSelect->setEnabled(enabled);edgeSelect->setEnabled(enabled);height->setEnabled(enabled);floating->setEnabled(enabled);});
    panelSelect=new QComboBox;auto panelRow=new QHBoxLayout;panelRow->addWidget(panelEnabled);panelRow->addWidget(panelSelect,1);form->addRow("Plasma panel",panelRow);
    edgeSelect=new QComboBox;edgeSelect->addItems({"bottom","top","left","right"});
    height=new QSpinBox;height->setRange(24,96);height->setValue(44);height->setSuffix(" px");
    floating=new QCheckBox("Floating");floating->setChecked(true);
    auto geometry=new QHBoxLayout;geometry->addWidget(edgeSelect);geometry->addWidget(height);geometry->addWidget(floating);geometry->addStretch();form->addRow("Layout",geometry);
    connect(edgeSelect,&QComboBox::currentTextChanged,this,&StudioWindow::updatePreview);
    connect(floating,&QCheckBox::toggled,this,&StudioWindow::updatePreview);
    connect(panelSelect,&QComboBox::currentIndexChanged,this,[this]{
        auto p=panelSelect->currentData().toJsonObject();
        if(!p.isEmpty()){edgeSelect->setCurrentText(p["location"].toString());height->setValue(p["height"].toInt());floating->setChecked(p["floating"].toBool());}
    });
    auto presetLayout=new QComboBox;presetLayout->addItems({"Custom panel layout","Slim left rail · reference look","Floating bottom bar"});form->addRow("Panel preset",presetLayout);
    panelActions=new QCheckBox("Add launcher and dashboard buttons to this panel");form->addRow("Popup access",panelActions);
    popupStyle=new QComboBox;popupStyle->addItem("Reference rounded","rounded");popupStyle->addItem("Fluent inspired","fluent");
    popupMode=new QComboBox;popupMode->addItem("Follow desktop","desktop");popupMode->addItem("Light","light");popupMode->addItem("Dark","dark");
    QSettings popupSettings;popupStyle->setCurrentIndex(qMax(0,popupStyle->findData(popupSettings.value("popup/style","rounded"))));popupMode->setCurrentIndex(qMax(0,popupMode->findData(popupSettings.value("popup/mode","desktop"))));
    connect(popupStyle,&QComboBox::currentIndexChanged,this,[this]{colorEnabled->setChecked(false);refreshManagerLook();});connect(popupMode,&QComboBox::currentIndexChanged,this,[this]{colorEnabled->setChecked(false);refreshManagerLook();});
    refreshManagerLook();
    auto popupRow=new QHBoxLayout;popupRow->addWidget(popupStyle);popupRow->addWidget(popupMode);form->addRow("Popup look",popupRow);
    auto previews=new QHBoxLayout;
    for(const auto &entry:QList<QStringList>{{"Preview launcher","launcher"},{"Preview dashboard","dashboard"}}){auto button=new QPushButton(entry[0]);previews->addWidget(button);connect(button,&QPushButton::clicked,this,[this,kind=entry[1]]{auto popup=new StudioPopup(kind=="launcher"?StudioPopup::Launcher:StudioPopup::Dashboard,demoMode,this,popupStyle->currentData().toString(),popupMode->currentData().toString());popup->setAttribute(Qt::WA_DeleteOnClose);popup->show();});}
    form->addRow("Try popup look",previews);
    connect(presetLayout,&QComboBox::currentIndexChanged,this,[this](int i){if(i==0)return;panelEnabled->setChecked(true);edgeSelect->setCurrentText(i==1?"left":"bottom");height->setValue(i==1?40:48);floating->setChecked(true);panelActions->setChecked(true);colorEnabled->setChecked(false);updatePreview();});
    connect(colorEnabled,&QCheckBox::toggled,this,&StudioWindow::updatePreview);
    v->addWidget(options);
    notice=label("Select a style, then try it on your desktop.","subtitle");notice->setMinimumHeight(34);v->addWidget(notice);
    auto bottom=new QHBoxLayout;
    bottom->addWidget(label("Widgets, shortcuts, and authentication stay yours.","subtitle"),1);
    apply=new QPushButton(demoMode?"Try confirmation flow":"Apply & try");apply->setObjectName("primary");connect(apply,&QPushButton::clicked,this,&StudioWindow::beginTrial);bottom->addWidget(apply);v->addLayout(bottom);
    choosePreset("omarchy-osaka-jade");
    return root;
}
QWidget *StudioWindow::sessionsPage() {
    auto w=new QWidget;auto v=new QVBoxLayout(w);v->setContentsMargins(0,0,0,0);v->setSpacing(15);
    v->addWidget(label("A place for every desktop.","hero"));
    v->addWidget(label("Choose an installed desktop environment or window manager session.","subtitle"));
    auto info=card();auto iv=new QVBoxLayout(info);
    iv->addWidget(label("Sessions switch at the login screen","section"));
    iv->addWidget(label("Save your work first. This opens KDE’s logout confirmation. At the login screen, choose the selected session and sign in. Plasma remains available for your next login.","subtitle"));
    iv->addWidget(label("Wayland window managers run as separate sessions; they cannot replace KWin inside a running Plasma Wayland session.","subtitle"));v->addWidget(info);
    sessionList=new QListWidget;v->addWidget(sessionList,1);
    connect(sessionList,&QListWidget::currentRowChanged,this,[this]{auto item=sessionList->currentItem();switchButton->setEnabled(!demoMode&&!busy&&item&&item->data(Qt::UserRole).toJsonObject()["available"].toBool());});
    auto actions=new QHBoxLayout;auto refresh=new QPushButton("Refresh installed sessions");connect(refresh,&QPushButton::clicked,this,&StudioWindow::refreshInventory);actions->addWidget(refresh);actions->addStretch();
    switchButton=new QPushButton("Log out to switch…");switchButton->setObjectName("primary");switchButton->setEnabled(false);connect(switchButton,&QPushButton::clicked,this,&StudioWindow::logoutToSelectedSession);actions->addWidget(switchButton);v->addLayout(actions);
    v->addWidget(label("Appearance rollback applies to this Plasma session. A session change uses the display manager’s normal login process.","subtitle"));
    return w;
}
QWidget *StudioWindow::recoveryPage() {
    auto w=new QWidget;auto v=new QVBoxLayout(w);v->setContentsMargins(0,0,0,0);v->setSpacing(16);
    v->addWidget(label("Experiment with a way back.","hero"));
    auto explanation=card();auto e=new QVBoxLayout(explanation);
    e->addWidget(label("Keep these changes?","section"));
    e->addWidget(label("No visible countdown. If there is no keyboard, pointer, or touch input during the first 15 seconds, your previous settings return. Once input is detected, the timeout is removed and only your Yes or No decides.","subtitle"));
    e->addWidget(label("You can test other windows while the confirmation stays open. Closing or crashing the manager restores an undecided trial.","subtitle"));v->addWidget(explanation);
    auto profiles=card();auto pl=new QVBoxLayout(profiles);pl->addWidget(label("Portable appearance profiles","section"));
    pl->addWidget(label("Save your choices as data. Importing a profile only updates the preview; use Apply & try to test it.","subtitle"));
    auto buttons=new QHBoxLayout;auto save=new QPushButton("Export profile…"),load=new QPushButton("Import profile…");buttons->addWidget(save);buttons->addWidget(load);buttons->addStretch();pl->addLayout(buttons);v->addWidget(profiles);
    connect(save,&QPushButton::clicked,this,[this]{try {auto path=QFileDialog::getSaveFileName(this,"Export appearance",QString(),"JSON profile (*.json)");if(!path.isEmpty()){auto data=desired();data["format"]="kamakiri-studio-v1";writeJson(path,data);notice->setText("Profile exported.");}}catch(const std::exception &ex){error(ex.what());}});
    connect(load,&QPushButton::clicked,this,[this]{
        if(busy){error("Finish the current trial before importing a profile.");return;}
        auto path=QFileDialog::getOpenFileName(this,"Import appearance",QString(),"JSON profile (*.json)");if(path.isEmpty())return;
        try {auto r=readJson(path);if(r["format"]!="kamakiri-studio-v1")throw std::runtime_error("This is not a KamaKiriStudio profile.");validateRequest(r);
            wallpaperPath=r["wallpaperPath"].toString();selectedWallpaper=wallpaperPath.isEmpty()?QImage():loadWallpaper(wallpaperPath);derivedPalette=wallpaperPath.isEmpty()?QJsonObject():wallpaperPalette(wallpaperPath);wallpaperEnabled->setEnabled(!wallpaperPath.isEmpty());wallpaperEnabled->setChecked(r["changeWallpaper"].toBool());
            if(!wallpaperPath.isEmpty())wallpaperPreview->setPixmap(QPixmap::fromImage(loadWallpaper(wallpaperPath)).scaled(700,210,Qt::KeepAspectRatio,Qt::SmoothTransformation));
            for(auto choice:widgetChoices)choice->setChecked(choice->isEnabled()&&r["widgets"].toArray().contains(choice->property("plugin").toString()));
            choosePreset(r["preset"].toString());panelActions->setChecked(r["panelPopupActions"].toBool());popupStyle->setCurrentIndex(qMax(0,popupStyle->findData(r["popupStyle"].toString("rounded"))));popupMode->setCurrentIndex(qMax(0,popupMode->findData(r["popupMode"].toString("desktop"))));colorEnabled->setChecked(r["changeColors"].toBool(true));accent=QColor(r["accent"].toString());light->setChecked(r["light"].toBool());
            auto p=r["panel"].toObject();edgeSelect->setCurrentText(p["location"].toString("bottom"));height->setValue(p["height"].toInt(44));floating->setChecked(p["floating"].toBool());
            // Panel IDs are machine-local. Preserve the currently selected local panel.
            panelEnabled->setChecked(r["changePanel"].toBool()&&panelSelect->count()>0);updatePreview();pages->setCurrentIndex(0);
            notice->setText("Profile loaded into the preview. No desktop changes applied.");
        }catch(const std::exception &ex){error(ex.what());}
    });
    auto recovery=new QPushButton("Check for unfinished trials");connect(recovery,&QPushButton::clicked,this,&StudioWindow::recoverPending);v->addWidget(recovery);
    v->addWidget(label("Snapshots are stored locally in a private folder. This app does not download themes, execute profile commands, store passwords, or change your login screen.","subtitle"));
    v->addStretch();return w;
}
void StudioWindow::refreshInventory() {
    if(busy)return;
    if(demoMode) inventory=QJsonObject{{"plasmaAvailable",true},{"colorTool","demo"},{"panels",QJsonArray{QJsonObject{{"id",1},{"location","bottom"},{"height",44},{"floating",true}}}},{"sessions",sessions()}};
    else inventory=inspect();
    panelSelect->clear();
    for(const auto &item:inventory["panels"].toArray()) {auto p=item.toObject();panelSelect->addItem(QString("Panel %1 · %2").arg(p["id"].toInt()).arg(p["location"].toString()),p);}
    if(panelSelect->count()==0) {panelEnabled->setChecked(false);panelEnabled->setEnabled(false);}
    else panelEnabled->setEnabled(true);
    sessionList->clear();
    for(const auto &item:inventory["sessions"].toArray()) {
        auto s=item.toObject();auto li=new QListWidgetItem(s["name"].toString()+"  ·  "+s["type"].toString()+(s["available"].toBool()?"":"  ·  executable missing"),sessionList);
        li->setData(Qt::UserRole,s);
        if(!s["available"].toBool())li->setFlags(li->flags()&~Qt::ItemIsEnabled);
    }
    if(sessionList->count()>0)sessionList->setCurrentRow(0);
    apply->setEnabled(demoMode||(inventory["plasmaAvailable"].toBool()&&inventory["globalInputReady"].toBool()&&!inventory["colorTool"].toString().isEmpty()));
    refreshDesktopTools();
    if(!demoMode&&!apply->isEnabled())notice->setText("Desktop integration unavailable: "+inventory["error"].toString());
    updatePreview();
}
void StudioWindow::choosePreset(const QString &id) {
    if(id=="wallpaper"&&wallpaperPath.isEmpty()){themeSelect->setCurrentIndex(themeSelect->findData(preset));notice->setText("Choose an image in Wallpaper and widgets first.");return;}
    preset=id;
    if(colorEnabled)colorEnabled->setChecked(true);
    themeSelect->setCurrentIndex(themeSelect->findData(id));
    auto palette=omarchyPalette(id);
    if(id=="wallpaper")palette=derivedPalette;
    QStringList ids{"caelestia","ryoku","breeze"};
    for(int i=0;i<presetButtons.size();i++)presetButtons[i]->setChecked(ids[i]==id);
    accent=QColor(id=="caelestia"?"#c4a7ff":id=="ryoku"?"#eea3b2":"#3daee9");
    if (!palette.isEmpty()) accent=QColor(palette["accent"].toString());
    light->setEnabled(palette.isEmpty());
    if (!palette.isEmpty()) light->setChecked(palette["mode"]=="light");
    updatePreview();
}
void StudioWindow::updatePreview() {
    if(!preview)return;
    preview->customPalette=preset=="wallpaper"?derivedPalette:QJsonObject();
    preview->wallpaper=(!wallpaperPath.isEmpty()&&wallpaperEnabled&&wallpaperEnabled->isChecked())?selectedWallpaper:QImage();
    preview->preset=preset;preview->accent=accent;preview->light=light->isChecked();preview->edge=edgeSelect->currentText();preview->floating=floating->isChecked();preview->update();
    if(!colorEnabled->isChecked()) {
        KConfig globals(configFile(),KConfig::SimpleConfig);
        auto read=[&](const char *group,const char *key,const QColor &fallback){auto parts=globals.group(group).readEntry(key,QString()).split(',');if(parts.size()<3)return fallback;QColor c(parts[0].toInt(),parts[1].toInt(),parts[2].toInt());return c.isValid()?c:fallback;};
        QColor bg=read("Colors:Window","BackgroundNormal",palette().color(QPalette::Window));
        QColor fg=read("Colors:Window","ForegroundNormal",palette().color(QPalette::WindowText));
        preview->customPalette={{"background",bg.name()},{"dark_background",bg.darker(110).name()},{"lighter_background",bg.lightnessF()<.5?bg.lighter(135).name():bg.lighter(105).name()},{"foreground",fg.name()}};
        preview->accent=read("General","AccentColor",read("Colors:Selection","BackgroundNormal",palette().color(QPalette::Highlight)));preview->light=bg.lightnessF()>.5;
    }
    accentButton->setText("Accent · "+accent.name());
}
QJsonObject StudioWindow::desired() const {
    auto p=panelSelect->currentData().toJsonObject();
    p["location"]=edgeSelect->currentText();p["height"]=height->value();p["floating"]=floating->isChecked();
    QJsonArray widgets;for(auto choice:widgetChoices)if(choice->isEnabled()&&choice->isChecked())widgets.append(choice->property("plugin").toString());
    return {{"preset",preset},{"accent",accent.name()},{"light",light->isChecked()},{"changePanel",panelEnabled->isChecked()&&panelSelect->count()>0},{"panel",p},{"wallpaperPath",wallpaperPath},{"changeWallpaper",wallpaperEnabled->isChecked()&&!wallpaperPath.isEmpty()},{"desktopId",desktopSelect->currentData().toInt()},{"widgets",widgets},{"changeColors",colorEnabled->isChecked()},{"panelPopupActions",panelActions->isChecked()},{"changePopupLook",true},{"popupStyle",popupStyle->currentData().toString()},{"popupMode",popupMode->currentData().toString()}};
}
void StudioWindow::beginTrial() {
    if(busy)return;
    try {
        QDir oldTrials(stateRoot());
        for(const auto &old:oldTrials.entryList(QDir::Dirs|QDir::NoDotAndDotDot)) {
            if(unfinished(readJson(oldTrials.filePath(old)+"/status.json")["state"].toString()))
                throw std::runtime_error("An unfinished trial must be restored before starting another. Use Recovery and profiles to check it.");
        }
        auto r=desired();validateRequest(r);
        QString id=QUuid::createUuid().toString(QUuid::WithoutBraces);
        transaction=stateRoot()+"/"+id;
        if(!QDir().mkdir(transaction))throw std::runtime_error("Cannot create the trial recovery folder.");
        QFile::setPermissions(transaction,QFile::ReadOwner|QFile::WriteOwner|QFile::ExeOwner);
        r["demo"]=demoMode;
        writeJson(transaction+"/request.json",r);
        busy=true;inputSeen=false;decision.clear();heartbeat=0;
        apply->setEnabled(false);switchButton->setEnabled(false);writeCommand();
        QStringList args{"--worker",id};if(demoMode)args<<"--demo";
        QProcess worker;worker.setProgram(QCoreApplication::applicationFilePath());worker.setArguments(args);
        worker.setStandardOutputFile(transaction+"/worker.log");worker.setStandardErrorFile(transaction+"/worker.log",QIODevice::Append);
        if(!worker.startDetached(&workerPid)) {busy=false;throw std::runtime_error("Could not start the independent rollback process.");}
        notice->setText("Preparing a reversible trial…");
    }catch(const std::exception &ex){busy=false;apply->setEnabled(true);error(ex.what());}
}
void StudioWindow::writeCommand() {
    if(!busy)return;
    writeJson(transaction+"/command.json",{{"heartbeat",double(++heartbeat)},{"input",inputSeen},{"decision",decision}});
}
void StudioWindow::pollTrial() {
    if(!busy)return;
    try {
        writeCommand();auto status=readJson(transaction+"/status.json");QString s=status["state"].toString();
        if((s.isEmpty()||unfinished(s))&&workerPid>0&&kill(pid_t(workerPid),0)<0&&errno==ESRCH) {
            QLockFile lock(transaction+"/worker.lock");lock.setStaleLockTime(0);
            if(lock.tryLock(0)) {
                try {
                    if(QFileInfo::exists(transaction+"/snapshot.json"))recover(transaction,demoMode);
                    else writeJson(transaction+"/status.json",{{"state","failed"},{"message","The rollback process could not start. No settings were applied."}});
                }catch(const std::exception &ex){writeJson(transaction+"/status.json",{{"state","recovery-needed"},{"message",QString::fromUtf8(ex.what())}});}
                status=readJson(transaction+"/status.json");s=status["state"].toString();
            }
        }
        if(s=="pending") {
            if(!confirmation)showConfirmation();
            confirmationText->setText(status["message"].toString());
            if(status["interacted"].toBool())trialExplanation->setText("Timeout removed. Choose Yes or No when you are ready.");
        }
        if(s=="kept"||s=="reverted"||s=="failed"||s=="recovery-needed") {
            busy=false;notice->setText(status["message"].toString());
            if(confirmation){auto d=confirmation;confirmation=nullptr;d->hide();d->deleteLater();}
            apply->setEnabled(true);
            if(s=="recovery-needed")error(status["message"].toString());
            if(closing){QTimer::singleShot(0,this,&QWidget::close);return;}
            refreshInventory();notice->setText(status["message"].toString());
        }
    }catch(const std::exception &ex){notice->setText("Recovery communication error: "+QString::fromUtf8(ex.what()));}
}
void StudioWindow::showConfirmation() {
    confirmation=new QDialog(this);confirmation->setWindowTitle("Keep these changes?");confirmation->setModal(false);confirmation->setMinimumWidth(470);
    auto v=new QVBoxLayout(confirmation);v->setContentsMargins(24,22,24,22);v->setSpacing(16);
    v->addWidget(label("Keep these changes?","section"));
    confirmationText=label("Try your desktop, then choose Yes or No.");v->addWidget(confirmationText);
    trialExplanation=label("No input during the first 15 seconds restores your previous settings. Your first input removes the timeout.","subtitle");v->addWidget(trialExplanation);
    auto row=new QHBoxLayout;row->addStretch();auto no=new QPushButton("No"),yes=new QPushButton("Yes");no->setObjectName("revertButton");yes->setObjectName("keepButton");
    yes->setDefault(false);yes->setAutoDefault(false);no->setDefault(false);no->setAutoDefault(false);
    row->addWidget(no);row->addWidget(yes);v->addLayout(row);
    connect(no,&QPushButton::clicked,this,[this]{decide("no");});connect(yes,&QPushButton::clicked,this,[this]{decide("yes");});
    connect(confirmation,&QDialog::rejected,this,[this]{decide("no");});
    confirmation->show();
}
void StudioWindow::decide(const QString &v) {
    if(!busy)return;
    decision=v;writeCommand();
    if(confirmation)confirmation->setEnabled(false);
}
bool StudioWindow::eventFilter(QObject *,QEvent *e) {
    if(busy&&confirmation&&decision.isEmpty()&&e->spontaneous()) {
        switch(e->type()) {
            case QEvent::KeyPress:case QEvent::MouseButtonPress:case QEvent::MouseMove:case QEvent::Wheel:case QEvent::TouchBegin:case QEvent::TabletPress:
                inputSeen=true;break;
            default:break;
        }
    }
    return false;
}
void StudioWindow::closeEvent(QCloseEvent *e) {
    if(busy){closing=true;decide("no");e->ignore();notice->setText("Restoring before closing…");}
    else e->accept();
}
void StudioWindow::error(const QString &s) {QMessageBox::warning(this,"KamaKiriStudio",s);}
void StudioWindow::recoverPending() {
    if(busy)return;
    try {
        bool blocked=false;
        QDir root(stateRoot());
        for(const auto &id:root.entryList(QDir::Dirs|QDir::NoDotAndDotDot|QDir::NoSymLinks)) {
            if(!QRegularExpression("^[a-f0-9-]{36}$").match(id).hasMatch())continue;
            QString dir=root.filePath(id);auto s=readJson(dir+"/status.json");if(!unfinished(s["state"].toString()))continue;
            QLockFile lock(dir+"/worker.lock");lock.setStaleLockTime(0);
            if(!lock.tryLock(0)){
                notice->setText("Another recovery process is handling an unfinished trial.");blocked=true;
                QTimer::singleShot(4500,this,&StudioWindow::recoverPending);
                continue;
            }
            bool simulated=readJson(dir+"/request.json")["demo"].toBool();
            if(QFileInfo::exists(dir+"/snapshot.json")) {
                recover(dir,simulated);notice->setText("An unfinished trial was restored from its saved snapshot.");
            } else {
                writeJson(dir+"/status.json",{{"state","failed"},{"message","An interrupted trial stopped before changing any settings."}});
                notice->setText("An interrupted trial stopped before changing any settings.");
            }
        }
        apply->setEnabled(!blocked&&(demoMode||(inventory["plasmaAvailable"].toBool()&&inventory["globalInputReady"].toBool()&&!inventory["colorTool"].toString().isEmpty())));
    }catch(const std::exception &ex){apply->setEnabled(false);error(QString::fromUtf8(ex.what()));}
}
void StudioWindow::logoutToSelectedSession() {
    if(busy||demoMode)return;
    auto item=sessionList->currentItem();if(!item)return;
    auto s=item->data(Qt::UserRole).toJsonObject();
    if(!s["available"].toBool())return;
    auto answer=QMessageBox::question(this,"Switch desktop session", "Save your work first.\n\nAfter logging out, select “"+s["name"].toString()+"” in the login screen’s session menu.\n\nOpen KDE’s logout confirmation now?",QMessageBox::Yes|QMessageBox::No,QMessageBox::No);
    if(answer!=QMessageBox::Yes)return;
    auto msg=QDBusMessage::createMethodCall("org.kde.LogoutPrompt","/LogoutPrompt","org.kde.LogoutPrompt","promptLogout");
    auto reply=QDBusConnection::sessionBus().call(msg,QDBus::Block,5000);
    if(reply.type()==QDBusMessage::ErrorMessage)error("KDE could not open the logout screen: "+reply.errorMessage());
}
void StudioWindow::showPreviewPage(const QString &name) {
    QStringList names{"appearance","desktop","dashboard","sessions","recovery","about"};
    int i=names.indexOf(name);if(i>=0)pages->setCurrentIndex(i);if(name=="about")checkLocalUpdate();
}
void StudioWindow::setDemoWallpaper(const QString &path) {
    if(!demoMode)throw std::runtime_error("Demo wallpaper checks require preview mode.");
    selectedWallpaper=loadWallpaper(path);wallpaperPath=QFileInfo(path).canonicalFilePath();derivedPalette=wallpaperPalette(wallpaperPath);
    wallpaperPreview->setPixmap(QPixmap::fromImage(selectedWallpaper).scaled(700,210,Qt::KeepAspectRatio,Qt::SmoothTransformation));
    wallpaperEnabled->setEnabled(true);wallpaperEnabled->setChecked(true);widgetChoices[0]->setChecked(true);choosePreset("wallpaper");
}
void StudioWindow::capture(const QString &path) {grab().save(path);}
void StudioWindow::runUiCheck(const QString &path) {
    QTimer::singleShot(100,this,[this,path]{
        int previousStyle=popupStyle->currentIndex();bool previousColors=colorEnabled->isChecked();
        popupStyle->setCurrentIndex(0);popupStyle->setCurrentIndex(1);
        if(colorEnabled->isChecked()){QCoreApplication::exit(2);return;}
        popupStyle->setCurrentIndex(previousStyle);colorEnabled->setChecked(previousColors);
        capture(path);beginTrial();
    });
    auto check=new QTimer(this);check->setInterval(100);auto elapsed=new QElapsedTimer;elapsed->start();
    connect(check,&QTimer::timeout,this,[this,check,elapsed,path]{
        if(confirmation) {
            confirmation->grab().save(QFileInfo(path).absolutePath()+"/confirmation-preview.png");
            auto button=confirmation->findChild<QPushButton*>("revertButton");if(button)button->click();
        }
        if(!busy&&elapsed->elapsed()>1000){check->stop();delete elapsed;QCoreApplication::exit(0);}
        else if(elapsed->elapsed()>10000){check->stop();delete elapsed;QCoreApplication::exit(2);}
    });check->start();
}

void StudioWindow::refreshManagerLook() {
    if(popupStyle->currentData()!="fluent"){setStyleSheet(referenceManagerStyle);return;}
    bool lightMode=popupMode->currentData()=="light"||(popupMode->currentData()=="desktop"&&palette().color(QPalette::Window).lightnessF()>=.5);
    QString css=referenceManagerStyle;
    const QList<QPair<QString,QString>> replacements{
        {"#15151e",lightMode?"#f3f3f3":"#202020"},{"#efedf7",lightMode?"#1b1b1b":"#f5f5f5"},
        {"#20202c",lightMode?"#ffffff":"#2b2b2b"},{"#353344",lightMode?"#dedede":"#454545"},
        {"#474156",lightMode?"#d4d4d4":"#505050"},{"#a8a3b8",lightMode?"#606060":"#b4b4b4"},
        {"#2a2839",lightMode?"#e9e9e9":"#333333"},{"#242330",lightMode?"#ffffff":"#292929"},
        {"#373247",lightMode?"#dddddd":"#444444"},{"#302740",lightMode?"#e4e4e4":"#3c3c3c"},
        {"#423454",lightMode?"#dfdfdf":"#444444"},{"#51416a",lightMode?"#dddddd":"#4a4a4a"}};
    for(const auto &entry:replacements)css.replace(entry.first,entry.second);
    css.replace("border-radius:12px","border-radius:8px");setStyleSheet(css);
}
