// SPDX-License-Identifier: MIT
#include "window.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFrame>
#include <QSettings>
#include <QDesktopServices>
#include <QUrl>
#include <QDir>
#include <QFile>
#include <QCryptographicHash>
#include <QVersionNumber>
#include <QDateTime>
#include <QJsonDocument>
#include <QNetworkRequest>
#include <memory>
#include <QTextStream>
using namespace Studio;
QWidget *StudioWindow::aboutPage() {
    auto root=new QWidget;auto v=new QVBoxLayout(root);v->setContentsMargins(0,0,0,0);v->setSpacing(16);
    auto title=new QLabel("KamaKiriStudio");title->setObjectName("hero");v->addWidget(title);
    auto summary=new QLabel("Version "+QApplication::applicationVersion()+"\nA desktop studio built on KDE Plasma and KWin.");summary->setWordWrap(true);v->addWidget(summary);
    auto details=new QLabel("Kamakiri, Fluent and native Plasma desktop looks, Omarchy palettes and associated wallpapers, native Plasma widgets, KRunner and media controls.\n\nBuilt with Qt "+QString(qVersion())+" and KDE Frameworks. Project code: MIT license. Omarchy palette and Wayland protocol credits ship with the project.\n\nAppearance trials use an independent recovery worker. Yes keeps your changes; No restores them. The first input removes the initial 15-second inactivity timeout.");details->setWordWrap(true);v->addWidget(details);
    auto update=new QFrame;update->setObjectName("card");auto uv=new QVBoxLayout(update);
    auto heading=new QLabel("Updates");heading->setObjectName("section");uv->addWidget(heading);
    updateStatus=new QLabel("Check official GitHub releases or the local project build. Updates are installed only when you choose to do so.");updateStatus->setWordWrap(true);uv->addWidget(updateStatus);
    auto check=new QPushButton("Check local release now");uv->addWidget(check);connect(check,&QPushButton::clicked,this,&StudioWindow::checkLocalUpdate);
    auto online=new QPushButton("Check GitHub releases now");uv->addWidget(online);connect(online,&QPushButton::clicked,this,&StudioWindow::checkOnlineUpdate);
    auto releasePage=new QPushButton("Open official GitHub releases");uv->addWidget(releasePage);connect(releasePage,&QPushButton::clicked,this,[]{QDesktopServices::openUrl(QUrl("https://github.com/TridentSpoon/KamaKiriStudio/releases/latest"));});
    auto monitor=new QCheckBox("Monitor GitHub releases while KamaKiriStudio is open");uv->addWidget(monitor);
    QSettings settings;if(!demoMode)monitor->setChecked(settings.value("updates/monitorGithub",false).toBool());
    updateTimer.setInterval(300000);connect(&updateTimer,&QTimer::timeout,this,&StudioWindow::checkOnlineUpdate);
    connect(monitor,&QCheckBox::toggled,this,[this](bool on){if(!demoMode){QSettings settings;settings.setValue("updates/monitorGithub",on);}if(on){checkOnlineUpdate();updateTimer.start();}else updateTimer.stop();});
    if(monitor->isChecked()){updateTimer.start();QTimer::singleShot(0,this,&StudioWindow::checkOnlineUpdate);}
    auto note=new QLabel("When enabled, checks the official project’s public GitHub release every five minutes. Sends no desktop settings or personal files. It never downloads or installs updates automatically.");note->setWordWrap(true);note->setObjectName("subtitle");uv->addWidget(note);v->addWidget(update);
    auto folder=new QPushButton("Project, documentation and licenses");connect(folder,&QPushButton::clicked,this,[]{QDesktopServices::openUrl(QUrl("https://github.com/TridentSpoon/KamaKiriStudio"));});v->addWidget(folder);v->addStretch();return root;
}
void StudioWindow::checkLocalUpdate() {
    try {
        QString root=QStringLiteral(STUDIO_BUNDLE_DIRECTORY);auto release=readJson(root+"/release.json");
        if(release.isEmpty()){updateStatus->setText("Local release source is unavailable. You can check GitHub releases instead.");return;}
        auto version=release["version"].toString();qsizetype suffix=0;auto candidate=QVersionNumber::fromString(version,&suffix);
        if(candidate.isNull()||suffix!=version.size()||release["app"]!="KamaKiriStudio")throw std::runtime_error("Invalid local release manifest.");
        if(release["status"]=="development"){updateStatus->setText("Development source: "+version+". No verified local release artifact is published for this build. Check GitHub for stable releases.");return;}
        QFile binary(root+"/bin/kamakiri-studio");if(!binary.open(QIODevice::ReadOnly)||binary.size()>32*1024*1024)throw std::runtime_error("Local release binary is unavailable.");
        QCryptographicHash hash(QCryptographicHash::Sha256);if(!hash.addData(&binary)||QString::fromLatin1(hash.result().toHex())!=release["sha256"].toString())throw std::runtime_error("Local release integrity check failed.");
        int comparison=QVersionNumber::compare(candidate,QVersionNumber::fromString(QApplication::applicationVersion()));
        if(comparison>0)setWindowTitle("KamaKiriStudio · Update available");
        updateStatus->setText((comparison>0?"A newer local release is available: ":comparison==0?"You are using the current local release: ":"This app is newer than the local release: ")+version+"\nChecked "+QDateTime::currentDateTime().toString("HH:mm, d MMM yyyy")+". Integrity verified against the local manifest.");
    }catch(const std::exception &e){updateStatus->setText(QString("Update check unavailable: ")+e.what());}
}

void StudioWindow::checkOnlineUpdate() {
    if(updateReply)return;
    if(demoMode){updateStatus->setText("Preview mode: online checks are disabled. The installed app checks the official GitHub repository.");return;}
    if(!updateNetwork)updateNetwork=new QNetworkAccessManager(this);
    QNetworkRequest request(QUrl("https://api.github.com/repos/TridentSpoon/KamaKiriStudio/releases/latest"));
    request.setRawHeader("Accept","application/vnd.github+json");request.setRawHeader("User-Agent",("KamaKiriStudio/"+QApplication::applicationVersion()).toUtf8());
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,QNetworkRequest::ManualRedirectPolicy);request.setTransferTimeout(10000);
    updateStatus->setText("Checking official GitHub releases…");updateReply=updateNetwork->get(request);
    auto reply=updateReply;auto payload=std::make_shared<QByteArray>();
    connect(reply,&QIODevice::readyRead,this,[reply,payload]{payload->append(reply->readAll());if(payload->size()>128*1024)reply->abort();});
    connect(reply,&QNetworkReply::finished,this,[this,reply,payload]{
        payload->append(reply->readAll());int status=reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if(status==404)updateStatus->setText("No public release is published yet.");
        else if(reply->error()!=QNetworkReply::NoError||status!=200||payload->size()>128*1024)updateStatus->setText("Could not check GitHub releases. Your installed version is "+QApplication::applicationVersion()+". Try again later.");
        else {
            auto document=QJsonDocument::fromJson(*payload);auto release=document.object();QString tag=release["tag_name"].toString();QString version=tag.startsWith('v')?tag.mid(1):tag;qsizetype suffix=0;
            auto candidate=QVersionNumber::fromString(version,&suffix);
            if(!document.isObject()||candidate.isNull()||suffix!=version.size()||release["draft"].toBool()||release["prerelease"].toBool())updateStatus->setText("GitHub returned an unsupported release format.");
            else {int comparison=QVersionNumber::compare(candidate,QVersionNumber::fromString(QApplication::applicationVersion()));
                updateStatus->setText((comparison>0?"Update available: ":comparison==0?"You are up to date: ":"You are running a newer build than the latest release: ")+version+"\nChecked "+QDateTime::currentDateTime().toString("HH:mm, d MMM yyyy")+". Open official GitHub releases to review it.");
                if(comparison>0)setWindowTitle("KamaKiriStudio · Update available");
            }
        }
        updateReply=nullptr;reply->deleteLater();
    });
}

void StudioWindow::reportUpdateCheck() {
    checkOnlineUpdate();
    auto finish=[this]{QTimer::singleShot(0,this,[this]{QTextStream(stdout)<<updateStatus->text()<<"\n";QCoreApplication::quit();});};
    if(updateReply)connect(updateReply,&QNetworkReply::finished,this,finish);else finish();
}
