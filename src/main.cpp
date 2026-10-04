// SPDX-License-Identifier: MIT
#include "window.h"
#include "popup.h"
#include "input_monitor.h"
#include <QApplication>
#include <QCommandLineParser>
#include <QLockFile>
#include <QFileInfo>
#include <QDir>
#include <QRegularExpression>
#include <QJsonDocument>
#include <QTextStream>
#include <QMessageBox>
#include <KIdleTime>
#include <unistd.h>
#include <sys/stat.h>

static QString transactionPath(const QString &id) {
    if(!QRegularExpression("^[a-f0-9]{8}-[a-f0-9]{4}-[a-f0-9]{4}-[a-f0-9]{4}-[a-f0-9]{12}$").match(id).hasMatch())throw std::runtime_error("Invalid trial ID.");
    QString path=Studio::stateRoot()+"/"+id;
    struct stat st;
    if(lstat(QFile::encodeName(path).constData(),&st)!=0||!S_ISDIR(st.st_mode)||st.st_uid!=getuid()||(st.st_mode&0077))throw std::runtime_error("Trial folder is not private or owned by you.");
    return path;
}
int main(int argc,char **argv) {
    QApplication app(argc,argv);
    app.setApplicationName("KamaKiriStudio");app.setOrganizationName("KamaKiriStudio");app.setApplicationVersion("0.4.1");
    app.setQuitOnLastWindowClosed(false);
    QCommandLineParser p;p.setApplicationDescription("KDE-first appearance trials and installed-session switching.");p.addHelpOption();p.addVersionOption();
    p.addOptions({{"popup-style","Popup style: rounded or fluent.","style"},{"popup-mode","Popup mode: desktop, light or dark.","mode"},{"launcher","Open the rounded application popup."},{"dashboard","Open the rounded dashboard popup."},{"check-updates","Report the official GitHub release check without opening a window."},{"capture-page","Select a page for capture.","name"},{"demo-wallpaper","Set an image for an isolated preview check.","path"},{"demo","Preview without changing desktop settings."},{"worker","Independent trial watchdog (internal).","id"},{"recover","Restore an unfinished trial.","id"},{"recover-all","Restore abandoned trials at login without opening a window."},{"inspect","Print read-only desktop integration details."},{"ui-check","Run an isolated GUI confirmation check and capture an image.","path"},{"capture","Save a screenshot of the app, then exit.","path"},{"probe-input","Test global input reporting without changing settings."}});
    p.process(app);
    try {
        if(getuid()==0)throw std::runtime_error("Run KamaKiriStudio as your desktop user, not root.");
        if(p.isSet("inspect")){QTextStream(stdout)<<QJsonDocument(Studio::inspect()).toJson();return 0;}
        if(p.isSet("recover-all")) {
            QDir root(Studio::stateRoot());
            bool failed=false;
            for(const auto &id:root.entryList(QDir::Dirs|QDir::NoDotAndDotDot|QDir::NoSymLinks)) {
                try {
                    QString dir=transactionPath(id);
                    if(!Studio::unfinished(Studio::readJson(dir+"/status.json")["state"].toString()))continue;
                    QLockFile lock(dir+"/worker.lock");lock.setStaleLockTime(0);if(!lock.tryLock(0))continue;
                    if(!QFileInfo::exists(dir+"/snapshot.json")) {
                        Studio::writeJson(dir+"/status.json",{{"state","failed"},{"message","An interrupted trial stopped before any settings were changed."}});
                    } else Studio::recover(dir,Studio::readJson(dir+"/request.json")["demo"].toBool());
                }catch(const std::exception &ex){failed=true;QTextStream(stderr)<<ex.what()<<"\n";}
            }
            return failed?1:0;
        }
        if(p.isSet("probe-input")) {
            Studio::InputMonitor monitor;
            if(!monitor.prepare()){QTextStream(stderr)<<"Global input protocol is unavailable.\n";return 1;}
            QObject::connect(&monitor,&Studio::InputMonitor::activity,&app,[&app]{QTextStream(stdout)<<"GLOBAL_INPUT_DETECTED\n";app.quit();});
            if(!monitor.arm())return 1;
            QTextStream(stdout)<<"GLOBAL_INPUT_PROTOCOL_READY (ignores idle inhibitors)\n";
            QTimer::singleShot(12000,&app,&QCoreApplication::quit);return app.exec();
        }
        if(p.isSet("worker")||p.isSet("recover")) {
            QString id=p.value(p.isSet("worker")?"worker":"recover");QString dir=transactionPath(id);
            QLockFile lock(dir+"/worker.lock");lock.setStaleLockTime(0);if(!lock.tryLock(0))throw std::runtime_error("A watchdog already owns this trial.");
            bool demo=p.isSet("demo");
            if(Studio::readJson(dir+"/request.json")["demo"].toBool()!=demo)throw std::runtime_error("Trial mode mismatch.");
            if(p.isSet("recover")){Studio::recover(dir,demo);return 0;}
            Studio::Worker worker(dir,demo);QTimer::singleShot(0,&worker,&Studio::Worker::start);return app.exec();
        }
        if(p.isSet("launcher")||p.isSet("dashboard")) {
            if(p.isSet("popup-style")&&!QStringList{"rounded","fluent"}.contains(p.value("popup-style")))throw std::runtime_error("Unknown popup style.");
            if(p.isSet("popup-mode")&&!QStringList{"desktop","light","dark"}.contains(p.value("popup-mode")))throw std::runtime_error("Unknown popup mode.");
            StudioPopup popup(p.isSet("launcher")?StudioPopup::Launcher:StudioPopup::Dashboard,p.isSet("demo"),nullptr,p.value("popup-style"),p.value("popup-mode"));
            popup.show();QObject::connect(&app,&QApplication::lastWindowClosed,&app,&QCoreApplication::quit);
            if(p.isSet("capture")){auto path=p.value("capture");QTimer::singleShot(350,&popup,[&popup,&app,path]{popup.grab().save(path);app.quit();});}
            return app.exec();
        }
        QLockFile single(Studio::stateRoot()+"/manager.lock");single.setStaleLockTime(0);
        if(!single.tryLock(0)){QMessageBox::information(nullptr,"KamaKiriStudio","The manager is already open.");return 1;}
        StudioWindow window(p.isSet("demo"));
        if(p.isSet("demo-wallpaper"))window.setDemoWallpaper(p.value("demo-wallpaper"));
        if(p.isSet("capture-page"))window.showPreviewPage(p.value("capture-page"));
        if(p.isSet("check-updates")){window.reportUpdateCheck();return app.exec();}
        window.show();
        QObject::connect(&app,&QApplication::lastWindowClosed,&app,&QCoreApplication::quit);
        if(p.isSet("ui-check")) {
            if(!p.isSet("demo"))throw std::runtime_error("UI checks require --demo.");
            window.runUiCheck(p.value("ui-check"));
        } else if(p.isSet("capture")) {QString path=p.value("capture");QTimer::singleShot(300,&window,[&window,&app,path]{window.capture(path);app.quit();});}
        return app.exec();
    }catch(const std::exception &ex){QTextStream(stderr)<<ex.what()<<"\n";return 1;}
}
