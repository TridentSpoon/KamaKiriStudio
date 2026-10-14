// SPDX-License-Identifier: MIT
#pragma once
#include <QJsonObject>
#include <QStringList>
#include <QMap>
namespace Studio {
QStringList trialPluginRequirements(const QJsonObject &request,const QJsonObject &snapshot);
QStringList recoveryPluginRequirements(const QJsonObject &snapshot);
bool btrfsMappingDeviceMatches(const QString &path,const QString &device,const QString &mountInfo);
QMap<QString,QString> plasmaLibraryIdentities(const QString &maps,bool verifyFiles);
void validatePlasmaRuntimeMaps(const QString &plasma,const QString &worker,bool verifyFiles);
QString checkPluginSession(const QStringList &plugins);
bool compilePluginQml(const QString &path,QString *problem);
int probePlasmaPlugin(const QString &plugin);
}
