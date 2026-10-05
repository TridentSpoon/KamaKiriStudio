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
    for(const auto &value:data["desktops"].toArray()){auto d=value.toObject();if(d["screen"].toInt(-1)<0||!d["active"].toBool(true))continue;desktopSelect->addItem(QString("Screen %1 · desktop %2").arg(d["screen"].toInt()+1).arg(d["id"].toInt()),d["id"].toInt());}
    if(desktopSelect->findData(old)>=0)desktopSelect->setCurrentIndex(desktopSelect->findData(old));
    refreshMonitorControls(data["desktops"].toArray());
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
    QMap<int,QPair<bool,QString>> choices;
    for(int i=0;i<monitorEnabled.size();i++)choices[monitorEnabled[i]->property("screen").toInt()]={monitorEnabled[i]->isChecked(),monitorEdges[i]->currentData().toString()};
    while(auto item=monitorLayout->takeAt(0)){delete item->widget();delete item;}monitorEnabled.clear();monitorEdges.clear();
    for(const auto &entry:desktops){auto desktop=entry.toObject();int screen=desktop["screen"].toInt(-1);if(screen<0||!desktop["active"].toBool(true))continue;
        auto row=new QWidget;auto layout=new QHBoxLayout(row);layout->setContentsMargins(0,0,0,0);
        auto enabled=new QCheckBox(QString("Monitor %1").arg(screen+1));enabled->setProperty("screen",screen);enabled->setProperty("desktopId",desktop["id"].toInt());enabled->setChecked(choices.contains(screen)?choices[screen].first:true);layout->addWidget(enabled);
        auto edge=new QComboBox;for(const auto &pair:QList<QStringList>{{"None","none"},{"Top","top"},{"Right","right"},{"Bottom","bottom"},{"Left","left"}})edge->addItem(pair[0],pair[1]);
        QJsonObject panel;for(const auto &p:inventory["panels"].toArray())if(p.toObject()["screen"].toInt(0)==screen){panel=p.toObject();break;}
        edge->setProperty("panelId",panel.isEmpty()?-1:panel["id"].toInt());edge->setCurrentIndex(qMax(0,edge->findData(choices.contains(screen)?choices[screen].second:panel["location"].toString("none"))));
        bool primary=screen==primaryDesktopScreen(desktops);if(primary){enabled->setText(enabled->text()+" · primary");QString selection=edge->currentData().toString();edge->removeItem(0);edge->setCurrentIndex(edge->findData(selection=="none"?"bottom":selection));}
        edge->setToolTip(primary?"The primary monitor keeps a panel.":"None removes this monitor's panel; No restores it during a trial.");
        edge->setEnabled(enabled->isChecked());connect(enabled,&QCheckBox::toggled,edge,&QWidget::setEnabled);connect(edge,&QComboBox::currentIndexChanged,this,[this,edge,primary]{if(primary&&edge->currentData()!="none")edgeSelect->setCurrentText(edge->currentData().toString());});layout->addWidget(new QLabel("Panel"));layout->addWidget(edge);layout->addStretch();monitorLayout->addWidget(row);monitorEnabled.append(enabled);monitorEdges.append(edge);
    }
}
