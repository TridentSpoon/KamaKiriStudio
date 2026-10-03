// SPDX-License-Identifier: MIT
#pragma once
#include <QWidget>
#include <QListWidget>
#include <QLineEdit>
#include <QLabel>
#include <QProgressBar>
#include <QComboBox>
#include <QTimer>
#include <QElapsedTimer>
#include <KService>
class QPushButton;
class StudioPopup : public QWidget {
public:
    enum Mode { Launcher, Dashboard };
    StudioPopup(Mode mode,bool demo=false,QWidget *parent=nullptr,const QString &style={},const QString &appearance={});
    int visibleApplications() const;
    void setSearch(const QString &query);
protected:
    void paintEvent(QPaintEvent *) override;
    void showEvent(QShowEvent *) override;
    void keyPressEvent(QKeyEvent *) override;
    bool event(QEvent *) override;
private:
    bool demoMode;
    int cornerRadius=24;
    Mode mode;
    QColor background,foreground,accent,surface,muted;
    KService::List applications;
    QListWidget *appList=nullptr;
    QLineEdit *search=nullptr;
    QLabel *clock=nullptr,*date=nullptr,*track=nullptr,*system=nullptr;
    QComboBox *players=nullptr;
    QList<QPushButton*> mediaButtons;
    QProgressBar *cpu=nullptr,*memory=nullptr,*disk=nullptr;
    QLabel *network=nullptr;
    QTimer refresh;
    QElapsedTimer time;
    quint64 lastCpuTotal=0,lastCpuIdle=0,lastNet=0;
    qint64 lastTime=0;
    void filterApplications();
    void launchSelected();
    void updateDashboard();
    void mediaAction(const QString &method);
};
