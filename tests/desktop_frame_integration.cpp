// SPDX-License-Identifier: MIT
// Run only inside the private nested KWin session created by desktop_frame_integration.py.
#include "desktop_frame.h"
#include "core.h"
#include <QApplication>
#include <QScreen>
#include <QWidget>
#include <QWindow>
#include <QSettings>
#include <QJsonDocument>
#include <QProcess>
#include <QTest>
#include <LayerShellQt/window.h>
#include <QTextStream>
#include <QSaveFile>
#include <signal.h>
int main(int argc,char **argv){
    QApplication app(argc,argv);app.setApplicationName("KamaKiriStudio");app.setOrganizationName("KamaKiriStudio");app.setQuitOnLastWindowClosed(false);
    if(argc!=2||qEnvironmentVariable("KAMAKIRI_PRIVATE_FRAME_TEST")!="1")return 2;
    auto screens=app.screens();if(screens.size()!=2)return 3;
    QProcess frame;frame.start(QString::fromLocal8Bit(argv[1]),{"--desktop-frame"});if(!frame.waitForStarted())return 4;
    auto stop=[&]{const auto original=frame.processId(),active=Studio::readJson(Studio::stateRoot()+"/frame-status.json")["pid"].toInteger();if(frame.state()!=QProcess::NotRunning){frame.terminate();if(!frame.waitForFinished(3000)){frame.kill();frame.waitForFinished();}}if(active>0&&active!=original)kill(active,SIGTERM);};
    auto ready=[&](const QString &text,int surfaces){QElapsedTimer t;t.start();while(t.elapsed()<7000){app.processEvents();auto status=Studio::readJson(Studio::stateRoot()+"/frame-status.json");if((status["pid"].toInteger()==frame.processId()||(frame.state()==QProcess::NotRunning&&status["pid"].toInteger()>0))&&status["hash"]==Studio::desktopFrameHash(text)&&status["ready"].toBool()&&status["surfaces"].toInt()==surfaces)return true;QTest::qWait(30);}QTextStream(stderr)<<"Status: "<<QJsonDocument(Studio::readJson(Studio::stateRoot()+"/frame-status.json")).toJson()<<" Process: "<<frame.state()<<"\n"<<frame.readAllStandardError().right(12000);return false;};
    if(!ready("",0)){stop();return 5;}
    if(!qEnvironmentVariable("KAMAKIRI_TEST_UPDATE_BINARY").isEmpty()){
        QFile source(qEnvironmentVariable("KAMAKIRI_TEST_UPDATE_BINARY"));QSaveFile target(QString::fromLocal8Bit(argv[1]));if(!source.open(QIODevice::ReadOnly)||!target.open(QIODevice::WriteOnly)){stop();return 21;}target.write(source.readAll());target.setPermissions(QFile::ReadOwner|QFile::WriteOwner|QFile::ExeOwner);if(!target.commit()){stop();return 22;}
    }
    QWidget rail;rail.setWindowFlags(Qt::FramelessWindowHint|Qt::Tool|Qt::WindowDoesNotAcceptFocus);rail.winId();rail.windowHandle()->setScreen(screens.first());
    auto layer=LayerShellQt::Window::get(rail.windowHandle());using L=LayerShellQt::Window;
    layer->setScreen(screens.first());layer->setAnchors(L::Anchors(L::AnchorLeft)|L::AnchorTop|L::AnchorBottom);layer->setLayer(L::LayerTop);layer->setKeyboardInteractivity(L::KeyboardInteractivityNone);layer->setExclusiveZone(42);layer->setDesiredSize(QSize(42,0));rail.resize(42,800);rail.show();QTest::qWait(300);
    QJsonArray outputs;for(int i=0;i<screens.size();++i)outputs.append(QJsonObject{{"name",screens[i]->name()},{"edge",i==0?"left":"none"},{"panelSize",i==0?42:0}});
    QJsonObject config{{"version",1},{"color","#14231c"},{"screens",outputs}};const auto text=QString::fromUtf8(QJsonDocument(config).toJson(QJsonDocument::Compact));
    QSettings settings;settings.setValue("frame/config",text);settings.sync();
    try{Studio::syncDesktopFrame(true,QString::fromLocal8Bit(argv[1]));}catch(const std::exception &e){QTextStream(stderr)<<e.what();stop();return 6;}
    if(!ready(text,10)){stop();return 6;}
    QTest::qWait(200);if(rail.height()!=screens.first()->size().height()){QTextStream(stderr)<<"Rail height "<<rail.height();stop();return 20;}
    QWidget client;client.setWindowFlags(Qt::FramelessWindowHint);client.winId();client.windowHandle()->setScreen(screens.first());client.showMaximized();QTest::qWait(500);
    const QSize expected(screens.first()->size().width()-42-64,screens.first()->size().height()-64);
    if(client.frameGeometry().size()!=expected){QTextStream(stderr)<<"Maximised size "<<client.frameGeometry().width()<<"x"<<client.frameGeometry().height()<<" expected "<<expected.width()<<"x"<<expected.height()<<"\n";stop();return 7;}
    client.showFullScreen();QTest::qWait(300);if(client.size()!=screens.first()->size()){stop();return 8;}
    if(!client.isActiveWindow()){QTextStream(stderr)<<"Frame stole focus\n";stop();return 9;}
    const auto secondName=screens[1]->name();
    if(QProcess::execute("kscreen-doctor",{"output."+secondName+".disable"})!=0){stop();return 15;}
    QElapsedTimer hotplug;hotplug.start();while(hotplug.elapsed()<4000){app.processEvents();if(Studio::readJson(Studio::stateRoot()+"/frame-status.json")["surfaces"].toInt()==5)break;QTest::qWait(30);}
    if(Studio::readJson(Studio::stateRoot()+"/frame-status.json")["surfaces"].toInt()!=5){stop();return 16;}
    try{Studio::syncDesktopFrame(false,QString::fromLocal8Bit(argv[1]));}catch(const std::exception &e){QTextStream(stderr)<<e.what();stop();return 18;}
    if(QProcess::execute("kscreen-doctor",{"output."+secondName+".enable"})!=0||!ready(text,10)){stop();return 17;}
    // Simulate No / restoration, then a process restart with the saved configuration.
    settings.remove("frame/config");settings.sync();try{Studio::syncDesktopFrame(false,QString::fromLocal8Bit(argv[1]));}catch(...){stop();return 19;}if(!ready("",0)){stop();return 10;}
    client.showNormal();client.showMaximized();QTest::qWait(300);
    if(client.frameGeometry().size()!=QSize(screens.first()->size().width()-42,screens.first()->size().height())){stop();return 11;}
    settings.setValue("frame/config",text);settings.sync();if(!ready(text,10)){stop();return 12;}stop();frame.start(QString::fromLocal8Bit(argv[1]),{"--desktop-frame"});if(!frame.waitForStarted()||!ready(text,10)){stop();return 13;}
    settings.remove("frame/config");settings.sync();if(!ready("",0)){stop();return 14;}stop();
    QTextStream(stdout)<<"PASS: two outputs; rail-aware maximised area; fullscreen size and focus; output unplug/replug; disable restores area; saved-frame restart.\n";return 0;
}
