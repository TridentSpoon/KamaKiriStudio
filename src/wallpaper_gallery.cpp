// SPDX-License-Identifier: MIT
#include "wallpaper_gallery.h"
#include "core.h"
#include <QPainter>
#include <QPainterPath>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QFileInfo>
#include <cmath>
WallpaperGallery::WallpaperGallery(QWidget *parent):QWidget(parent){setMouseTracking(true);setFocusPolicy(Qt::StrongFocus);setAccessibleName("Theme wallpaper honeycomb");}
void WallpaperGallery::setWallpapers(const QStringList &p){paths=p;images.clear();for(const auto &path:paths){try{images.append(Studio::loadWallpaper(path));}catch(...){images.append(QImage());}}updateHeight();update();}
void WallpaperGallery::setSelected(const QString &p){selected=p;update();}
QList<QPolygonF> WallpaperGallery::cells() const {
    const double radius=62,step=radius*std::sqrt(3.);int columns=qMax(1,int((width()-28-radius*.5)/(radius*1.5)));QList<QPolygonF> result;
    for(int i=0;i<paths.size();i++){int col=i%columns,row=i/columns;QPointF center(14+radius+col*radius*1.5,14+step*.5+row*step+(col%2)*step*.5);QPolygonF hex;
        for(int corner=0;corner<6;corner++){double a=corner*3.141592653589793/3;hex<<center+QPointF(radius*std::cos(a),radius*std::sin(a));}result.append(hex);}
    return result;
}
void WallpaperGallery::updateHeight(){double bottom=100;for(const auto &hex:cells())bottom=qMax(bottom,hex.boundingRect().bottom()+20);setMinimumHeight(int(bottom));setMaximumHeight(int(bottom));}
void WallpaperGallery::resizeEvent(QResizeEvent *e){QWidget::resizeEvent(e);updateHeight();}
void WallpaperGallery::paintEvent(QPaintEvent *){
    QPainter p(this);p.setRenderHint(QPainter::Antialiasing);auto hexes=cells();
    if(paths.isEmpty()){p.setPen(palette().color(QPalette::WindowText));p.drawText(rect(),Qt::AlignCenter,"No bundled wallpapers for this theme. Choose a local image below.");return;}
    for(int i=0;i<hexes.size();i++){QPainterPath clip;clip.addPolygon(hexes[i]);clip.closeSubpath();auto bounds=hexes[i].boundingRect();p.save();p.setClipPath(clip);p.fillRect(bounds,palette().color(QPalette::Base));if(!images[i].isNull()){auto scaled=images[i].scaled(bounds.size().toSize(),Qt::KeepAspectRatioByExpanding,Qt::SmoothTransformation);p.drawImage(bounds.center()-QPointF(scaled.width()/2.,scaled.height()/2.),scaled);}p.restore();p.setBrush(Qt::NoBrush);p.setPen(QPen(palette().color(paths[i]==selected?QPalette::Highlight:QPalette::Mid),paths[i]==selected?4:2));p.drawPath(clip);}
}
void WallpaperGallery::mousePressEvent(QMouseEvent *e){if(e->button()!=Qt::LeftButton)return;auto hexes=cells();for(int i=0;i<hexes.size();i++)if(hexes[i].containsPoint(e->position(),Qt::OddEvenFill)){selected=paths[i];update();if(onSelected)onSelected(selected);break;}}
void WallpaperGallery::mouseMoveEvent(QMouseEvent *e){auto hexes=cells();QString tip;for(int i=0;i<hexes.size();i++)if(hexes[i].containsPoint(e->position(),Qt::OddEvenFill)){tip=QFileInfo(paths[i]).completeBaseName();if(tip.contains("__"))tip=tip.section("__",1);break;}setToolTip(tip);}
void WallpaperGallery::keyPressEvent(QKeyEvent *e){int index=qMax(0,paths.indexOf(selected));if(e->key()==Qt::Key_Right||e->key()==Qt::Key_Down)index=qMin(paths.size()-1,index+1);else if(e->key()==Qt::Key_Left||e->key()==Qt::Key_Up)index=qMax(0,index-1);else if(e->key()!=Qt::Key_Return&&e->key()!=Qt::Key_Space){QWidget::keyPressEvent(e);return;}if(index>=0&&index<paths.size()){selected=paths[index];update();if(onSelected)onSelected(selected);}}
