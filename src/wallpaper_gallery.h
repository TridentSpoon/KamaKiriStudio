// SPDX-License-Identifier: MIT
#pragma once
#include <QWidget>
#include <QImage>
#include <functional>
class WallpaperGallery : public QWidget {
public:
    explicit WallpaperGallery(QWidget *parent=nullptr);
    void setWallpapers(const QStringList &paths);
    void setSelected(const QString &path);
    std::function<void(const QString &)> onSelected;
protected:
    void paintEvent(QPaintEvent *) override;
    void mousePressEvent(QMouseEvent *) override;
    void mouseMoveEvent(QMouseEvent *) override;
    void keyPressEvent(QKeyEvent *) override;
    void resizeEvent(QResizeEvent *) override;
private:
    QStringList paths;
    QList<QImage> images;
    QString selected;
    QList<QPolygonF> cells() const;
    void updateHeight();
};
