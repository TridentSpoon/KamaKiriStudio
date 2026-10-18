// SPDX-License-Identifier: MIT
#include "wallpaper_gallery.h"
#include "core.h"
#include <QPainter>
#include <QPainterPath>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QFileInfo>
#include <cmath>
#include <QIcon>
WallpaperGallery::WallpaperGallery(QWidget *parent):QWidget(parent){setMouseTracking(true);setFocusPolicy(Qt::StrongFocus);setAccessibleName("Theme wallpaper honeycomb");}
void WallpaperGallery::setWallpapers(const QStringList &p){iconMode=false;labels.clear();paths=p;images.clear();for(const auto &path:paths){try{images.append(Studio::loadWallpaper(path));}catch(...){images.append(QImage());}}updateHeight();update();}
void WallpaperGallery::setIcons(const QStringList &values,const QStringList &names){iconMode=true;paths=values;labels=names;images.clear();for(const auto &value:values)images.append(value=="kamakiri-windows"?QImage():QIcon::fromTheme(value,QIcon(value)).pixmap(48,48).toImage());setAccessibleName("Start button icon honeycomb");updateHeight();update();}
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
    for(int i=0;i<hexes.size();i++){QPainterPath clip;clip.addPolygon(hexes[i]);clip.closeSubpath();auto bounds=hexes[i].boundingRect();p.save();p.setClipPath(clip);p.fillRect(bounds,palette().color(QPalette::Base));if(iconMode){
            const auto center=bounds.center()-QPointF(0,10);
            if(paths[i]=="kamakiri-windows"){p.setPen(Qt::NoPen);p.setBrush(palette().color(QPalette::Highlight));for(int y=0;y<2;++y)for(int x=0;x<2;++x)p.drawRect(QRectF(center.x()-19+x*21,center.y()-19+y*21,18,18));}
            else if(!images[i].isNull())p.drawImage(QRectF(center-QPointF(24,24),QSizeF(48,48)),images[i]);
            else {p.save();p.translate(center);p.setPen(QPen(palette().color(QPalette::WindowText),2));p.setBrush(Qt::NoBrush);
                if(paths[i]=="view-grid"){for(int row=0;row<3;++row)for(int col=0;col<3;++col)p.drawRect(QRectF(-18+col*13,-18+row*13,8,8));}
                else if(paths[i]=="application-menu"){for(int row=-1;row<=1;++row)p.drawLine(QPointF(-18,row*12),QPointF(18,row*12));}
                else if(paths[i]=="computer"){p.drawRoundedRect(QRectF(-21,-18,42,28),2,2);p.drawLine(0,10,0,20);p.drawLine(-12,20,12,20);}
                else if(paths[i]=="system-run"){QPolygonF triangle;triangle<<QPointF(-12,-20)<<QPointF(20,0)<<QPointF(-12,20);p.drawPolygon(triangle);}
                else if(paths[i]=="go-home"){QPolygonF house;house<<QPointF(-20,-1)<<QPointF(0,-20)<<QPointF(20,-1)<<QPointF(16,-1)<<QPointF(16,20)<<QPointF(-16,20)<<QPointF(-16,-1);p.drawPolygon(house);p.drawRect(QRectF(-5,5,10,15));}
                p.restore();
            }
            p.setPen(palette().color(QPalette::WindowText));p.drawText(QRectF(bounds.left()+20,bounds.center().y()+20,bounds.width()-40,24),Qt::AlignCenter,fontMetrics().elidedText(labels.value(i),Qt::ElideRight,int(bounds.width()-40)));
        }else if(!images[i].isNull()){auto scaled=images[i].scaled(bounds.size().toSize(),Qt::KeepAspectRatioByExpanding,Qt::SmoothTransformation);p.drawImage(bounds.center()-QPointF(scaled.width()/2.,scaled.height()/2.),scaled);}p.restore();p.setBrush(Qt::NoBrush);p.setPen(QPen(palette().color(paths[i]==selected?QPalette::Highlight:QPalette::Mid),paths[i]==selected?4:2));p.drawPath(clip);}
}
void WallpaperGallery::mousePressEvent(QMouseEvent *e){if(e->button()!=Qt::LeftButton)return;auto hexes=cells();for(int i=0;i<hexes.size();i++)if(hexes[i].containsPoint(e->position(),Qt::OddEvenFill)){selected=paths[i];update();if(onSelected)onSelected(selected);break;}}
void WallpaperGallery::mouseMoveEvent(QMouseEvent *e){auto hexes=cells();QString tip;for(int i=0;i<hexes.size();i++)if(hexes[i].containsPoint(e->position(),Qt::OddEvenFill)){tip=iconMode?labels.value(i):QFileInfo(paths[i]).completeBaseName();if(tip.contains("__"))tip=tip.section("__",1);break;}setToolTip(tip);}
void WallpaperGallery::keyPressEvent(QKeyEvent *e){int index=qMax(0,paths.indexOf(selected));if(e->key()==Qt::Key_Right||e->key()==Qt::Key_Down)index=qMin(paths.size()-1,index+1);else if(e->key()==Qt::Key_Left||e->key()==Qt::Key_Up)index=qMax(0,index-1);else if(e->key()!=Qt::Key_Return&&e->key()!=Qt::Key_Space){QWidget::keyPressEvent(e);return;}if(index>=0&&index<paths.size()){selected=paths[index];update();if(onSelected)onSelected(selected);}}
