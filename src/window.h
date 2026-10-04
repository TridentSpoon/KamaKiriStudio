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

class DesktopPreview : public QWidget {
public:
    QString preset = "caelestia", edge = "bottom";
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
    QComboBox *desktopSelect, *playerSelect;
    QCheckBox *wallpaperEnabled=nullptr, *wallpaperColors=nullptr;
    QLabel *wallpaperPreview, *clockLabel, *dateLabel, *systemLabel, *trackLabel;
    QPushButton *desktopApply;
    QList<QCheckBox*> widgetChoices;
    QList<QPushButton*> mediaButtons;
    QTimer dashboardTimer;
    QColor accent=QColor("#c4a7ff");
    qint64 heartbeat=0;
    qint64 workerPid=0;
    QJsonObject inventory;
    QList<QPushButton*> presetButtons;
    DesktopPreview *preview;
    QPushButton *apply, *accentButton, *switchButton;
    QLabel *notice, *confirmationText, *trialExplanation;
    QComboBox *themeSelect, *panelSelect, *edgeSelect, *popupStyle, *popupMode;
    QCheckBox *colorEnabled=nullptr, *panelActions;
    QCheckBox *panelEnabled, *floating, *light;
    QSpinBox *height;
    QListWidget *sessionList;
    QDialog *confirmation=nullptr;
    QTimer timer;
    QStackedWidget *pages;
    QLabel *updateStatus;
    QTimer updateTimer;
    QNetworkAccessManager *updateNetwork=nullptr;
    QNetworkReply *updateReply=nullptr;
    void checkOnlineUpdate();
    QWidget *aboutPage();
    void checkLocalUpdate();
    QWidget *appearancePage();
    QWidget *desktopPage();
    QWidget *dashboardPage();
    void refreshDesktopTools();
    void refreshDashboard();
    void mediaAction(const QString &method);
    QWidget *sessionsPage();
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
    void logoutToSelectedSession();
    void error(const QString &message);
};
