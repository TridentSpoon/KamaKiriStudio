// SPDX-License-Identifier: MIT
#pragma once
#include <QJsonObject>
#include <QPainterPath>
namespace Studio {
QPainterPath desktopFramePath(const QRectF &bounds,const QJsonObject &monitor);
void validateDesktopFrame(const QJsonObject &config);
QString desktopFrameHash(const QString &config);
void syncDesktopFrame(bool requireAllScreens=true,const QString &executable=QString());
int runDesktopFrame();
}
