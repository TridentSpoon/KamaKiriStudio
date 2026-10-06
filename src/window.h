// SPDX-License-Identifier: MIT
#pragma once
#include "core.h"
#include <QMainWindow>
#include <QDialog>
#include <QPushButton>
#include <QLabel>
#include <QComboBox>
#include <QCheckBox>
#include <QSpinBox>
#include <QListWidget>
#include <QStackedWidget>
#include <QProcess>
#include <QPainter>
#include <QNetworkAccessManager>
#include <QNetworkReply>

class WallpaperGallery;
class QVBoxLayout;
class DesktopPreview : public QWidget {
public:
    QString look, preset = "caelestia", edge = "bottom";
    QImage wallpaper;
    QJsonObject customPalette;
    QColor accent = QColor("#c4a7ff");
    bool light = false, floating = true;
    explicit DesktopPreview(QWidget *parent=nullptr):QWidget(parent) {setMinimumHeight(220);}
protected:
    void paintEvent(QPaintEvent *) override;
};
class StudioWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit StudioWindow(bool demo, QWidget *parent=nullptr);
    void capture(const QString &path);
    void showPreviewPage(const QString &name);
    void setDemoLook(const QString &look);
    void setDemoWallpaper(const QString &path);
    void reportUpdateCheck();
    void runUiCheck(const QString &path);
protected:
    bool eventFilter(QObject *watched,QEvent *event) override;
    void closeEvent(QCloseEvent *event) override;
private:
    bool demoMode, busy=false, inputSeen=false, closing=false;
    QString preset="caelestia", transaction, decision, wallpaperPath;
    QImage selectedWallpaper;
    QJsonObject derivedPalette;
    QComboBox *desktopSelect;
    QComboBox *wallpaperScope=nullptr, *wallpaperTheme=nullptr;
    QComboBox *appearanceScope=nullptr, *appearanceDisplay=nullptr;
    WallpaperGallery *wallpaperGallery=nullptr, *appearanceGallery=nullptr;
    QVBoxLayout *monitorLayout=nullptr;
    QList<QCheckBox*> monitorEnabled;
    QList<QComboBox*> monitorEdges;
    bool wallpaperOnly=false;
    bool monitorChanging=false;
    bool primaryMonitorReady=false, primaryQueryRunning=false;
    QString primaryMonitorName;
    QLabel *monitorStatus=nullptr;
    QJsonArray currentMonitorDesktops;
    void queryKdeOutputs(std::function<void(QJsonArray,QString)> callback);
    void refreshPrimaryMonitor(const QString &expected={});
    void identifyMonitors();
    void setPrimaryMonitor(const QString &connector);
    void refreshMonitorControls(const QJsonArray &desktops);
    void selectWallpaper(const QString &path,bool matchColors);
    void refreshWallpaperGallery();
    QCheckBox *wallpaperEnabled=nullptr, *wallpaperColors=nullptr;
    QLabel *wallpaperPreview=nullptr;
    QPushButton *desktopApply;
    QList<QCheckBox*> widgetChoices;
    QColor accent=QColor("#c4a7ff");
    qint64 heartbeat=0;
    qint64 workerPid=0;
    QJsonObject inventory;
    QList<QPushButton*> presetButtons;
    DesktopPreview *preview;
    QPushButton *apply, *accentButton;
    QLabel *notice, *confirmationText, *trialExplanation;
    QPushButton *restoreBlocked=nullptr;
    QLabel *recoveryStatus=nullptr;
    QComboBox *lookSelect;
    QCheckBox *themeWallpaperEnabled;
    void chooseLook();
    void syncThemeWallpaper();
    QComboBox *themeSelect, *panelSelect, *edgeSelect, *popupStyle, *popupMode;
    QCheckBox *colorEnabled=nullptr, *panelActions;
    QCheckBox *panelEnabled, *floating, *light;
    QSpinBox *height;
    QDialog *confirmation=nullptr;
    QTimer timer;
    QStackedWidget *pages;
    QLabel *updateStatus;
    QTimer updateTimer;
    QNetworkAccessManager *updateNetwork=nullptr;
    QNetworkReply *updateReply=nullptr;
    QProcess *updateProcess=nullptr;
    void updateNow();
    void checkOnlineUpdate();
    QWidget *aboutPage();
    void checkLocalUpdate();
    QWidget *appearancePage();
    QWidget *desktopPage();
    void refreshDesktopTools();
    QWidget *recoveryPage();
    void refreshInventory();
    void choosePreset(const QString &id);
    void updatePreview();
    QJsonObject desired() const;
    void beginTrial();
    void writeCommand();
    void pollTrial();
    void decide(const QString &value);
    void showConfirmation();
    void recoverPending();
    void error(const QString &message);
};
