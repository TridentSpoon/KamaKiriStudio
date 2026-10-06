// SPDX-License-Identifier: MIT
#include "window.h"
#include <QScreen>
#include <QGuiApplication>
#include "wallpaper_gallery.h"
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QFormLayout>
#include <QFrame>
#include <QSignalBlocker>
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
#include <QQuickWidget>
#include <QQuickItem>
#include <KConfig>
#include <KConfigGroup>
#include "popup.h"
#include <signal.h>
#include <errno.h>

using namespace Studio;
static QLabel *label(const QString &text,const QString &name=QString()) {
    auto l=new QLabel(text); l->setWordWrap(true);
    auto font=l->font();if(name=="hero"){font.setPointSizeF(font.pointSizeF()*2);font.setBold(true);}else if(name=="section"){font.setPointSizeF(font.pointSizeF()*1.25);font.setBold(true);}l->setFont(font); if(!name.isEmpty()) l->setObjectName(name); return l;
}
static QFrame *card() {auto f=new QFrame;f->setObjectName("card");f->setFrameShape(QFrame::StyledPanel);return f;}

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
    auto central=new QWidget;setCentralWidget(central);
    auto layout=new QHBoxLayout(central);layout->setContentsMargins(22,22,22,22);layout->setSpacing(25);
    auto side=new QVBoxLayout;side->setSpacing(10);
    side->addWidget(label("KAMAKIRI\nSTUDIO","section"));
    side->addWidget(label("A desktop that feels like you.","subtitle"));side->addSpacing(25);
    pages=new QStackedWidget;
    pages->addWidget(appearancePage());pages->addWidget(recoveryPage());pages->addWidget(aboutPage());
    auto navigation=new QButtonGroup(this);navigation->setExclusive(true);
    QStringList names{"Appearance","Recovery and profiles","About and updates"};
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
    auto displayRefresh=new QTimer(this);displayRefresh->setSingleShot(true);displayRefresh->setInterval(750);
    connect(displayRefresh,&QTimer::timeout,this,[this]{if(!busy&&!monitorChanging)refreshInventory();});
    auto watchScreen=[displayRefresh](QScreen *screen){QObject::connect(screen,&QScreen::geometryChanged,displayRefresh,[displayRefresh]{displayRefresh->start();});};
    for(auto screen:QGuiApplication::screens())watchScreen(screen);
    connect(qApp,&QGuiApplication::screenAdded,this,[displayRefresh,watchScreen](QScreen *screen){watchScreen(screen);displayRefresh->start();});
    connect(qApp,&QGuiApplication::screenRemoved,this,[displayRefresh]{displayRefresh->start();});
    connect(qApp,&QGuiApplication::primaryScreenChanged,this,[displayRefresh]{displayRefresh->start();});
    refreshInventory();
    QTimer::singleShot(0,this,[this]{syncThemeWallpaper();});
    updatePreview();
    QTimer::singleShot(0,this,&StudioWindow::recoverPending);
}
QWidget *StudioWindow::appearancePage() {
    auto scroll=new QScrollArea;scroll->setWidgetResizable(true);scroll->setFrameShape(QFrame::NoFrame);auto root=new QWidget;scroll->setWidget(root);auto v=new QVBoxLayout(root);v->setContentsMargins(0,0,8,0);v->setSpacing(12);
    v->addWidget(label("Your desktop, reimagined.","hero"));
    v->addWidget(label("Choose your desktop layout and menus, then its colors and wallpaper.","subtitle"));
    lookSelect=new QComboBox;lookSelect->addItem("Keep current layout · customize colors","");lookSelect->addItem("Default Plasma · native KDE desktop","plasma");lookSelect->addItem("Kamakiri style · rounded menus and left rail","caelestia");lookSelect->addItem("Fluent 11 · Windows 11 layout","fluent11");lookSelect->addItem("Fluent 10 · Windows 10 layout","fluent10");
    QSettings savedLook;lookSelect->setCurrentIndex(qMax(0,lookSelect->findData(savedLook.value("desktop/look",""))));
    v->addWidget(label("Desktop look","section"));v->addWidget(lookSelect);
    colorEnabled=new QCheckBox("Include KDE color changes in this trial");colorEnabled->setChecked(true);
    themeSelect=new QComboBox;
    themeSelect->addItem("Lavender","caelestia");
    themeSelect->addItem("Graphite and rose","ryoku");
    themeSelect->addItem("Windows · default blue","windows");
    themeSelect->addItem("Breeze · KDE classic","breeze");
    themeSelect->addItem("From your wallpaper · choose an image first","wallpaper");
    for (const auto &value:omarchyPalettes()) {
        auto p=value.toObject(); themeSelect->addItem(p["name"].toString(),p["id"].toString());
    }
    v->addWidget(label("Theme and wallpaper","section")); v->addWidget(themeSelect);
    themeWallpaperEnabled=new QCheckBox("Use the associated theme wallpaper");themeWallpaperEnabled->setChecked(true);connect(themeWallpaperEnabled,&QCheckBox::toggled,this,[this]{syncThemeWallpaper();});
    connect(themeSelect,&QComboBox::currentIndexChanged,this,[this]{choosePreset(themeSelect->currentData().toString());});
    preview=new DesktopPreview;v->addWidget(preview);
    v->addWidget(label("Monitors","section"));
    v->addWidget(label("Switch monitors on to include their wallpaper and panel. KDE colors and window styling apply across the session.","subtitle"));
    auto displayActions=new QHBoxLayout;
    auto identify=new QPushButton("Identify monitors");identify->setObjectName("identifyMonitors");displayActions->addWidget(identify);
    connect(identify,&QPushButton::clicked,this,&StudioWindow::identifyMonitors);
    auto refreshDisplays=new QPushButton("Refresh displays");displayActions->addWidget(refreshDisplays);
    connect(refreshDisplays,&QPushButton::clicked,this,[this]{if(!busy&&!monitorChanging)refreshInventory();});
    displayActions->addStretch();v->addLayout(displayActions);
    auto monitorRoot=new QWidget;monitorLayout=new QVBoxLayout(monitorRoot);monitorLayout->setContentsMargins(0,0,0,0);v->addWidget(monitorRoot);
    v->addWidget(label("Wallpaper placement","section"));appearanceScope=new QComboBox;appearanceScope->setObjectName("appearanceWallpaperScope");v->addWidget(appearanceScope);
    appearanceDisplay=new QComboBox;appearanceDisplay->setObjectName("appearanceWallpaperDisplay");v->addWidget(appearanceDisplay);
    v->addWidget(label("Theme wallpapers","section"));appearanceGallery=new WallpaperGallery;v->addWidget(appearanceGallery);
    appearanceGallery->onSelected=[this](const QString &path){if(!busy)selectWallpaper(path,false);};
    auto attribution=label("As seen in and inspired by Omarchy","subtitle");attribution->setObjectName("themeAttribution");attribution->setAlignment(Qt::AlignRight);v->addWidget(attribution);
    auto wallpaperApply=new QPushButton("Apply wallpaper");wallpaperApply->setObjectName("applyWallpaper");v->addWidget(wallpaperApply);
    connect(wallpaperApply,&QPushButton::clicked,this,[this]{if(busy)return;if(wallpaperPath.isEmpty()){error("Choose a wallpaper first.");return;}wallpaperOnly=true;beginTrial();wallpaperOnly=false;});
    auto advanced=new QPushButton("Advanced settings");advanced->setCheckable(true);v->addWidget(advanced);
    auto options=card();auto form=new QFormLayout(options);form->setContentsMargins(16,12,16,12);form->setVerticalSpacing(10);form->addRow(colorEnabled);form->addRow(themeWallpaperEnabled);
    accentButton=new QPushButton("Accent · #c4a7ff");
    connect(accentButton,&QPushButton::clicked,this,[this]{auto c=QColorDialog::getColor(accent,this,"Choose an accent");if(c.isValid()){accent=c;colorEnabled->setChecked(true);updatePreview();}});
    light=new QCheckBox("Light surfaces");connect(light,&QCheckBox::toggled,this,&StudioWindow::updatePreview);
    auto paletteRow=new QHBoxLayout;paletteRow->addWidget(accentButton);paletteRow->addWidget(light);paletteRow->addStretch();form->addRow("Palette",paletteRow);
    panelEnabled=new QCheckBox("Apply panel settings");panelEnabled->setChecked(true);
    connect(panelEnabled,&QCheckBox::toggled,this,[this](bool enabled){panelSelect->setEnabled(enabled);edgeSelect->setEnabled(enabled);height->setEnabled(enabled);floating->setEnabled(enabled);});
    panelSelect=new QComboBox;auto panelRow=new QHBoxLayout;panelRow->addWidget(panelEnabled);panelRow->addWidget(panelSelect,1);form->addRow("Plasma panel",panelRow);
    edgeSelect=new QComboBox;edgeSelect->addItems({"bottom","top","left","right"});
    height=new QSpinBox;height->setRange(24,96);height->setValue(44);height->setSuffix(" px");
    floating=new QCheckBox("Floating");floating->setChecked(true);
    auto geometry=new QHBoxLayout;geometry->addWidget(edgeSelect);geometry->addWidget(height);geometry->addWidget(floating);geometry->addStretch();form->addRow("Layout",geometry);
    connect(edgeSelect,&QComboBox::currentTextChanged,this,&StudioWindow::updatePreview);
    connect(edgeSelect,&QComboBox::currentTextChanged,this,[this](const QString &edge){int id=panelSelect->currentData().toJsonObject()["id"].toInt(-1);for(auto choice:monitorEdges)if(id>=0&&choice->property("panelId").toInt()==id)choice->setCurrentIndex(choice->findData(edge));});
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
    connect(popupStyle,&QComboBox::currentIndexChanged,this,[this]{colorEnabled->setChecked(false);});connect(popupMode,&QComboBox::currentIndexChanged,this,[this]{colorEnabled->setChecked(false);});
    connect(lookSelect,&QComboBox::currentIndexChanged,this,[this]{chooseLook();});
    connect(popupMode,&QComboBox::currentIndexChanged,this,[this]{if(!lookSelect->currentData().toString().isEmpty()){colorEnabled->setChecked(true);updatePreview();}});
    auto popupRow=new QHBoxLayout;popupRow->addWidget(popupStyle);popupRow->addWidget(popupMode);form->addRow("Menu style and mode",popupRow);
    auto previews=new QHBoxLayout;
    for(const auto &entry:QList<QStringList>{{"Preview launcher","launcher"},{"Preview dashboard","dashboard"}}){auto button=new QPushButton(entry[0]);previews->addWidget(button);connect(button,&QPushButton::clicked,this,[this,kind=entry[1]]{if(kind=="launcher"&&lookSelect->currentData()=="plasma"){QMessageBox::information(this,"Default Plasma","This look uses KDE’s native Application Launcher, Breeze styling and a bottom panel. The custom Start preview is available for Kamakiri and Fluent looks.");return;}if(kind=="launcher"&&!lookSelect->currentData().toString().isEmpty()){auto q=new QQuickWidget; q->setAttribute(Qt::WA_DeleteOnClose);q->setResizeMode(QQuickWidget::SizeRootObjectToView);q->setSource(QUrl("qrc:/start/StartMenu.qml"));if(auto root=q->rootObject()){root->setProperty("look",lookSelect->currentData());root->setProperty("appearance",popupMode->currentData());root->setProperty("preview",true);}q->resize(lookSelect->currentData()=="fluent10"?760:640,650);q->setWindowTitle("Start menu preview");q->show();}else {auto popup=new StudioPopup(kind=="launcher"?StudioPopup::Launcher:StudioPopup::Dashboard,demoMode,this,popupStyle->currentData().toString(),popupMode->currentData().toString());popup->setAttribute(Qt::WA_DeleteOnClose);popup->show();}});}
    form->addRow("Try popup look",previews);
    connect(presetLayout,&QComboBox::currentIndexChanged,this,[this](int i){if(i==0)return;panelEnabled->setChecked(true);edgeSelect->setCurrentText(i==1?"left":"bottom");height->setValue(i==1?40:48);floating->setChecked(true);panelActions->setChecked(lookSelect->currentData()=="caelestia");colorEnabled->setChecked(false);updatePreview();});
    connect(colorEnabled,&QCheckBox::toggled,this,&StudioWindow::updatePreview);
    form->addRow(desktopPage());
    v->addWidget(options);options->hide();connect(advanced,&QPushButton::toggled,options,&QWidget::setVisible);
    notice=label("Select a style, then try it on your desktop.","subtitle");notice->setMinimumHeight(34);v->addWidget(notice);
    restoreBlocked=new QPushButton("Repair and restore previous settings");restoreBlocked->hide();v->addWidget(restoreBlocked);connect(restoreBlocked,&QPushButton::clicked,this,[this]{pages->setCurrentIndex(1);recoverPending();});
    auto bottom=new QHBoxLayout;
    bottom->addWidget(label("Widgets, shortcuts, and authentication stay yours.","subtitle"),1);
    apply=new QPushButton(demoMode?"Try confirmation flow":"Apply & try");apply->setObjectName("primary");connect(apply,&QPushButton::clicked,this,&StudioWindow::beginTrial);bottom->addWidget(apply);v->addLayout(bottom);
    choosePreset("omarchy-osaka-jade");
    if(!lookSelect->currentData().toString().isEmpty())chooseLook();
    return scroll;
}
QWidget *StudioWindow::recoveryPage() {
    auto w=new QWidget;auto v=new QVBoxLayout(w);v->setContentsMargins(0,0,0,0);v->setSpacing(16);
    v->addWidget(label("Experiment with a way back.","hero"));
    auto explanation=card();auto e=new QVBoxLayout(explanation);
    e->addWidget(label("Keep these changes?","section"));
    e->addWidget(label("A 15-second countdown restores your previous settings if there is no keyboard, pointer, or touch input. Once input is detected, the timeout is removed and only your Yes or No decides.","subtitle"));
    e->addWidget(label("You can test other windows while the confirmation stays open. Closing or crashing the manager restores an undecided trial.","subtitle"));v->addWidget(explanation);
    auto profiles=card();auto pl=new QVBoxLayout(profiles);pl->addWidget(label("Portable appearance profiles","section"));
    pl->addWidget(label("Save your choices as data. Importing a profile only updates the preview; use Apply & try to test it.","subtitle"));
    auto buttons=new QHBoxLayout;auto save=new QPushButton("Export profile…"),load=new QPushButton("Import profile…");buttons->addWidget(save);buttons->addWidget(load);buttons->addStretch();pl->addLayout(buttons);v->addWidget(profiles);
    connect(save,&QPushButton::clicked,this,[this]{try {auto path=QFileDialog::getSaveFileName(this,"Export appearance",QString(),"JSON profile (*.json)");if(!path.isEmpty()){auto data=desired();data["format"]="kamakiri-studio-v1";writeJson(path,data);notice->setText("Profile exported.");}}catch(const std::exception &ex){error(ex.what());}});
    connect(load,&QPushButton::clicked,this,[this]{
        if(busy){error("Finish the current trial before importing a profile.");return;}
        auto path=QFileDialog::getOpenFileName(this,"Import appearance",QString(),"JSON profile (*.json)");if(path.isEmpty())return;
        try {auto r=readJson(path);if(r["format"]!="kamakiri-studio-v1")throw std::runtime_error("This is not a KamaKiriStudio profile.");validateRequest(r);
            wallpaperScope->setCurrentIndex(qMax(0,wallpaperScope->findData(r["wallpaperScope"].toString("single"))));
            wallpaperPath=r["wallpaperPath"].toString();selectedWallpaper=wallpaperPath.isEmpty()?QImage():loadWallpaper(wallpaperPath);derivedPalette=wallpaperPath.isEmpty()?QJsonObject():wallpaperPalette(wallpaperPath);wallpaperEnabled->setEnabled(!wallpaperPath.isEmpty());wallpaperEnabled->setChecked(r["changeWallpaper"].toBool());
            if(!wallpaperPath.isEmpty())wallpaperPreview->setPixmap(QPixmap::fromImage(loadWallpaper(wallpaperPath)).scaled(700,210,Qt::KeepAspectRatio,Qt::SmoothTransformation));
            for(auto choice:widgetChoices)choice->setChecked(choice->isEnabled()&&r["widgets"].toArray().contains(choice->property("plugin").toString()));
            lookSelect->setCurrentIndex(qMax(0,lookSelect->findData(r["desktopLook"].toString())));choosePreset(r["preset"].toString());panelActions->setChecked(r["panelPopupActions"].toBool());popupStyle->setCurrentIndex(qMax(0,popupStyle->findData(r["popupStyle"].toString("rounded"))));popupMode->setCurrentIndex(qMax(0,popupMode->findData(r["popupMode"].toString("desktop"))));colorEnabled->setChecked(r["changeColors"].toBool(true));accent=QColor(r["accent"].toString());light->setChecked(r["light"].toBool());
            auto p=r["panel"].toObject();edgeSelect->setCurrentText(p["location"].toString("bottom"));height->setValue(p["height"].toInt(44));floating->setChecked(p["floating"].toBool());
            // Panel IDs are machine-local. Preserve the currently selected local panel.
            panelEnabled->setChecked(r["changePanel"].toBool()&&panelSelect->count()>0);updatePreview();pages->setCurrentIndex(0);
            notice->setText("Profile loaded into the preview. No desktop changes applied.");
        }catch(const std::exception &ex){error(ex.what());}
    });
    recoveryStatus=label("Restore a trial that was interrupted or could not finish. Your saved backup is kept if restoration fails.","subtitle");v->addWidget(recoveryStatus);
    auto recovery=new QPushButton("Repair and restore previous settings");connect(recovery,&QPushButton::clicked,this,&StudioWindow::recoverPending);v->addWidget(recovery);
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
    apply->setEnabled(demoMode||(inventory["plasmaAvailable"].toBool()&&inventory["globalInputReady"].toBool()&&!inventory["colorTool"].toString().isEmpty()));
    refreshDesktopTools();
    if(!demoMode&&!apply->isEnabled())notice->setText("Desktop integration unavailable: "+inventory["error"].toString());
    updatePreview();
}
void StudioWindow::choosePreset(const QString &id) {
    if(id=="wallpaper"&&wallpaperPath.isEmpty()){themeSelect->setCurrentIndex(themeSelect->findData(preset));notice->setText("Choose a local image in Advanced settings first.");return;}
    if(auto credit=findChild<QLabel*>("themeAttribution"))credit->setVisible(id.startsWith("omarchy-"));
    bool changed=preset!=id;
    preset=id;
    if(changed&&!associatedWallpaper(id).isEmpty()){QSignalBlocker block(themeWallpaperEnabled);themeWallpaperEnabled->setChecked(true);}
    if(colorEnabled)colorEnabled->setChecked(true);
    {QSignalBlocker block(themeSelect);themeSelect->setCurrentIndex(themeSelect->findData(id));}
    auto palette=desktopPalette(id,"desktop");
    if(id=="wallpaper")palette=derivedPalette;
    QStringList ids{"caelestia","ryoku","breeze"};
    for(int i=0;i<presetButtons.size();i++)presetButtons[i]->setChecked(ids[i]==id);
    accent=QColor(id=="caelestia"?"#c4a7ff":id=="ryoku"?"#eea3b2":"#3daee9");
    if (!palette.isEmpty()) accent=QColor(palette["accent"].toString());
    light->setEnabled(palette.isEmpty());
    if (!palette.isEmpty()) light->setChecked(palette["mode"]=="light");
    syncThemeWallpaper();
    refreshWallpaperGallery();
    updatePreview();
}
void StudioWindow::updatePreview() {
    if(!preview)return;
    preview->look=lookSelect->currentData().toString();
    preview->customPalette=preset=="wallpaper"?derivedPalette:desktopPalette(preset,preview->look.isEmpty()?"desktop":popupMode->currentData().toString());
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
    QJsonArray wallpaperDisplays,panelChoices,removedPanels,createdPanels;
    for(int i=0;i<monitorEnabled.size();i++)if(monitorEnabled[i]->isChecked()){
        wallpaperDisplays.append(monitorEnabled[i]->property("desktopId").toInt());int id=monitorEdges[i]->property("panelId").toInt();QString edge=monitorEdges[i]->currentData().toString();
        if(edge=="none"){for(const auto &v:inventory["panels"].toArray())if(v.toObject()["screen"].toInt(0)==monitorEnabled[i]->property("screen").toInt())removedPanels.append(v.toObject()["id"]);continue;}
        if(id<0){createdPanels.append(QJsonObject{{"screen",monitorEnabled[i]->property("screen").toInt()},{"location",edge},{"height",height->value()},{"floating",floating->isChecked()}});continue;}
        for(const auto &v:inventory["panels"].toArray()){auto panel=v.toObject();if(panel["id"].toInt()==id){panel["location"]=edge;panel["height"]=height->value();panel["floating"]=floating->isChecked();panelChoices.append(panel);}}
    }
    if(!monitorEnabled.isEmpty()&&wallpaperDisplays.isEmpty())throw std::runtime_error("Switch on at least one monitor before applying.");
    auto result=QJsonObject{{"wallpaperScope",wallpaperScope?wallpaperScope->currentData().toString():"single"},{"desktopLook",lookSelect->currentData().toString()},{"preset",preset},{"accent",accent.name()},{"light",light->isChecked()},{"changePanel",panelEnabled->isChecked()&&panelSelect->count()>0},{"panel",p},{"wallpaperPath",wallpaperPath},{"changeWallpaper",wallpaperEnabled->isChecked()&&!wallpaperPath.isEmpty()},{"desktopId",desktopSelect->currentData().toInt()},{"widgets",widgets},{"changeColors",colorEnabled->isChecked()},{"panelPopupActions",panelActions->isChecked()&&lookSelect->currentData()=="caelestia"},{"changePopupLook",true},{"popupStyle",popupStyle->currentData().toString()},{"popupMode",popupMode->currentData().toString()}};
    if(!monitorEnabled.isEmpty()){
        if(!wallpaperDisplays.contains(result["desktopId"]))result["widgets"]=QJsonArray{};
        result["wallpaperDesktopIds"]=wallpaperDisplays;result["panels"]=panelChoices;result["removePanelIds"]=removedPanels;result["createPanels"]=createdPanels;
        result["changePanel"]=panelEnabled->isChecked()&&(!panelChoices.isEmpty()||!removedPanels.isEmpty()||!createdPanels.isEmpty());
        if(!panelChoices.isEmpty())result["panel"]=panelChoices.first();
        if(!result["changePanel"].toBool()){result["desktopLook"]="";result["panelPopupActions"]=false;}
    }
    if(wallpaperOnly){result["changeWallpaper"]=true;result["changeColors"]=false;result["changePanel"]=false;result["panels"]=QJsonArray{};result["removePanelIds"]=QJsonArray{};result["createPanels"]=QJsonArray{};result["desktopLook"]="";result["changePopupLook"]=false;result["panelPopupActions"]=false;result["widgets"]=QJsonArray{};}
    return result;
}
void StudioWindow::beginTrial() {
    if(monitorChanging){error("Wait for the primary-monitor change to finish before starting an appearance trial.");return;}
    if(updateProcess){error("Wait for the app update to finish before starting an appearance trial.");return;}
    if(busy)return;
    try {
        QDir oldTrials(stateRoot());
        for(const auto &old:oldTrials.entryList(QDir::Dirs|QDir::NoDotAndDotDot)) {
            if(unfinished(readJson(oldTrials.filePath(old)+"/status.json")["state"].toString()))
                throw std::runtime_error("An unfinished trial must be restored before starting another. Click Repair and restore previous settings to retry. Your backup has been preserved.");
        }
        auto r=desired();validateRequest(r);
        QString id=QUuid::createUuid().toString(QUuid::WithoutBraces);
        transaction=stateRoot()+"/"+id;
        if(!QDir().mkdir(transaction))throw std::runtime_error("Cannot create the trial recovery folder.");
        QFile::setPermissions(transaction,QFile::ReadOwner|QFile::WriteOwner|QFile::ExeOwner);
        r["demo"]=demoMode;
        writeJson(transaction+"/request.json",r);
        busy=true;inputSeen=false;decision.clear();heartbeat=0;
        apply->setEnabled(false);writeCommand();
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
            confirmationText->setText(decision.isEmpty()?status["message"].toString():decision=="yes"?"Saving your choice…":"Restoring your previous settings…");
            if(decision.isEmpty()&&status["interacted"].toBool())trialExplanation->setText("Timeout removed. Choose Yes or No when you are ready.");
            else if(decision.isEmpty())trialExplanation->setText(QString("Restoring in %1 seconds unless you interact. Your first input removes the timeout.").arg(status["remainingSeconds"].toInt(15)));
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
    if(confirmation){
        for(auto button:confirmation->findChildren<QPushButton*>())button->setEnabled(false);
        confirmationText->setText(v=="yes"?"Saving your choice…":"Restoring your previous settings…");
        trialExplanation->setText("Waiting for the recovery worker to finish.");
    }
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
    if(updateProcess){pages->setCurrentIndex(2);updateStatus->setText("An update is in progress. Wait for it to finish before closing.");e->ignore();return;}
    if(busy){closing=true;decide("no");e->ignore();notice->setText("Restoring before closing…");}
    else {e->accept();QCoreApplication::quit();}
}
void StudioWindow::error(const QString &s) {QMessageBox::warning(this,"KamaKiriStudio",s);}
void StudioWindow::recoverPending() {
    if(busy)return;
    try {
        bool blocked=false, restoredAny=false;
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
                recover(dir,simulated);restoredAny=true;notice->setText("An unfinished trial was restored from its saved snapshot.");
            } else {
                writeJson(dir+"/status.json",{{"state","failed"},{"message","An interrupted trial stopped before changing any settings."}});
                notice->setText("An interrupted trial stopped before changing any settings.");
            }
        }
        if(recoveryStatus)recoveryStatus->setText(blocked||restoredAny?notice->text():"No unfinished trials. Your desktop is ready for a new trial.");
        if(restoreBlocked)restoreBlocked->setVisible(blocked);
        apply->setEnabled(!blocked&&(demoMode||(inventory["plasmaAvailable"].toBool()&&inventory["globalInputReady"].toBool()&&!inventory["colorTool"].toString().isEmpty())));
        desktopApply->setEnabled(apply->isEnabled()&&desktopSelect->count()>0);
        apply->setToolTip(apply->isEnabled()?QString():notice->text());desktopApply->setToolTip(apply->toolTip());
    }catch(const std::exception &ex){
        const QString message="Apply is blocked: an unfinished trial could not be restored. "+QString::fromUtf8(ex.what())+" Click Repair and restore previous settings to retry. Your backup has been preserved.";
        apply->setEnabled(false);desktopApply->setEnabled(false);notice->setText(message);if(recoveryStatus)recoveryStatus->setText(message);if(restoreBlocked)restoreBlocked->show();apply->setToolTip(message);desktopApply->setToolTip(message);error(message);
    }
}
void StudioWindow::showPreviewPage(const QString &name) {
    QStringList names{"appearance","recovery","about"};
    int i=names.indexOf(name=="desktop"?"appearance":name);if(i>=0)pages->setCurrentIndex(i);if(name=="about")checkLocalUpdate();
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
        if(!styleSheet().isEmpty()||palette()!=QApplication::palette()||colorEnabled->isChecked()){QCoreApplication::exit(2);return;}
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

void StudioWindow::chooseLook() {
    auto look=lookSelect->currentData().toString();panelActions->setEnabled(look=="caelestia");if(look.isEmpty()){panelActions->setChecked(false);return;}
    // Prefer an existing panel at the requested edge instead of creating stacked taskbars.
    QString targetEdge=look=="caelestia"?"left":"bottom";
    for(int i=0;i<panelSelect->count();i++)if(panelSelect->itemData(i).toJsonObject()["location"]==targetEdge){panelSelect->setCurrentIndex(i);break;}
    if(look=="plasma"){choosePreset("breeze");light->setChecked(true);popupMode->setCurrentIndex(popupMode->findData("desktop"));}
    panelEnabled->setChecked(true);edgeSelect->setCurrentText(look=="caelestia"?"left":"bottom");height->setValue(look=="caelestia"?48:(look=="fluent11"||look=="plasma")?48:40);floating->setChecked(look=="caelestia"||look=="plasma");
    panelActions->setEnabled(look=="caelestia");panelActions->setChecked(look=="caelestia");popupStyle->setCurrentIndex(popupStyle->findData(look=="caelestia"?"rounded":"fluent"));colorEnabled->setChecked(true);updatePreview();
}
void StudioWindow::syncThemeWallpaper() {
    if(!wallpaperPreview||!themeWallpaperEnabled->isChecked())return;
    auto path=associatedWallpaper(preset);if(path.isEmpty())return;
    try {selectedWallpaper=loadWallpaper(path);wallpaperPath=path;wallpaperEnabled->setEnabled(true);wallpaperEnabled->setChecked(true);wallpaperPreview->setPixmap(QPixmap::fromImage(selectedWallpaper).scaled(700,180,Qt::KeepAspectRatio,Qt::SmoothTransformation));updatePreview();}catch(const std::exception &e){notice->setText(QString::fromUtf8(e.what()));}
}
void StudioWindow::setDemoLook(const QString &look) {
    if(!demoMode||!desktopLooks().contains(look))throw std::runtime_error("Invalid preview desktop look.");
    lookSelect->setCurrentIndex(lookSelect->findData(look));
}
