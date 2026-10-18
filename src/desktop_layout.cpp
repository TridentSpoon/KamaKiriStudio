// SPDX-License-Identifier: MIT
#include "desktop_layout.h"
#include <QQuickItem>
#include <QUrl>
#include <stdexcept>
QQuickWidget *desktopLayoutPreview(const QString &wallpaper,const QJsonObject &palette) {
    auto view=new QQuickWidget;
    view->setResizeMode(QQuickWidget::SizeRootObjectToView);
    view->setSource(QUrl("qrc:/desktop/DesktopLayout.qml"));
    auto root=view->rootObject();
    if(!root){delete view;throw std::runtime_error("The Kamakiri layout preview could not be loaded.");}
    root->setProperty("wallpaper",QUrl::fromLocalFile(wallpaper));
    for(const auto &pair:QList<QStringList>{{"surface","background"},{"foreground","foreground"},{"accent","accent"}})
        if(palette.contains(pair[1]))root->setProperty(pair[0].toUtf8().constData(),palette[pair[1]].toString());
    view->setWindowTitle("Kamakiri rail and frame — layout preview");
    view->resize(1100,700);
    view->setMinimumSize(640,440);
    view->setAttribute(Qt::WA_DeleteOnClose);
    return view;
}
