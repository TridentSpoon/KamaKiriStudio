// SPDX-License-Identifier: MIT
#pragma once
#include <QApplication>
#include <QColor>
#include <QImage>
#include <QElapsedTimer>
#include <QJsonArray>
#include <QJsonObject>
#include <QObject>
#include <QTimer>
#include <functional>

namespace Studio {
class InputMonitor;
QJsonObject desktopPalette(const QString &preset,const QString &mode);
QStringList desktopLooks();
QJsonObject comparablePanelWidgetConfig(QJsonObject node,const QMap<QString,QString> &ids={});
QJsonArray orderedPanelWidgets(const QJsonArray &widgets);
QStringList themeWallpapers(const QString &preset);
QJsonArray wallpaperTargets(const QJsonArray &desktops,int selected,const QString &scope,const QJsonArray &included=QJsonArray{});
QList<QImage> spanWallpaper(const QImage &image,const QList<QRect> &screens);
QString associatedWallpaper(const QString &preset);
QJsonObject snapshotDesktopStyling();
QJsonObject snapshotLook(int panelId,bool full=false);
bool panelMatchesState(const QJsonObject &actual,const QJsonObject &expected);
QJsonArray trialPanels(const QJsonObject &request);
int primaryDesktopScreen(const QJsonArray &desktops);
QString monitorDisplayName(const QString &manufacturer,const QString &model,const QString &connector);
QString primaryMonitorArgument(const QJsonArray &outputs,const QString &connector);
QString kdePrimaryConnector(const QJsonArray &outputs);
int kdePrimaryDesktopScreen(const QJsonArray &desktops,const QJsonArray &outputs);
QString applyLookScript(int id,const QString &look,const QString &mode,const QString &owner);
QString restoreLookScript(const QJsonObject &saved,const QString &owner);
void applyLook(const QJsonObject &request,const QString &owner);
void restoreLook(const QJsonObject &saved,const QString &owner);
QJsonArray omarchyPalettes();
QJsonObject omarchyPalette(const QString &id);
QJsonObject desktopInventory();
QStringList safeWidgetTypes();
QImage loadWallpaper(const QString &path);
QJsonObject wallpaperPalette(const QString &path);
QString wallpaperScript(int id,const QString &image);
QString addWidgetsScript(int id,const QJsonArray &types,const QString &owner);
QString panelPopupActionsScript(int id,const QString &owner);
QString removeTrialWidgetsScript(const QString &owner);
QString stateRoot();
QString configFile();
QString jsonString(const QString &value);
QJsonObject readJson(const QString &path);
void writeJson(const QString &path, const QJsonObject &value);
bool unfinished(const QString &state);
QJsonArray panels();
QJsonArray sessions(const QStringList &roots = {});
QString plasmaScript(const QString &script);
QString panelScript(const QJsonObject &panel);
void validateRequest(const QJsonObject &request);
void restoreColors(const QString &backup, const QString &destination);
QString makeScheme(const QJsonObject &request, const QString &id);
void notifyPalette();
QJsonObject inspect();

// Pure decision logic, driven by monotonic elapsed time; no visible countdown.
struct TrialPolicy {
    bool interacted = false;
    bool accepted = false;
    bool rejected = false;
    bool shouldRevert(qint64 elapsedMs, bool ownerAlive) const {
        return !accepted && (rejected || !ownerAlive || (!interacted && elapsedMs >= 15000));
    }
    void input() { interacted = true; }
};

class Worker : public QObject {
    Q_OBJECT
public:
    Worker(const QString &transaction, bool simulate, QObject *parent = nullptr);
    void start();
private:
    QString dir;
    bool demo;
    QJsonObject request, status, snapshot;
    TrialPolicy policy;
    QElapsedTimer trialClock, lifeClock;
    QTimer poll;
    qint64 lastHeartbeat = 0;
    double lastHeartbeatValue = 0;
    bool restoring = false;
    bool armed = false;
    InputMonitor *inputMonitor = nullptr;
    void setState(const QString &state, const QString &message);
    void tick();
    void rollback(const QString &reason);
    void recordInput();
};
void recover(const QString &transaction, bool simulate);
}
