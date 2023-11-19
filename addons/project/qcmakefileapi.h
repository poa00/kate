/*
    SPDX-FileCopyrightText: 2023 Alexander Neundorf <neundorf@kde.org>
    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#pragma once

#include <QObject>
#include <QProcess>
#include <QString>

#include <QJsonObject>

#include <set>
#include <vector>


class QCMakeFileApi : public QObject
{
Q_OBJECT
public:
    struct TargetDef
    {
        QString name;
        QString config;
    };

    QCMakeFileApi(const QString& buildDir);
    
    bool runCMake();
    
    const QString& getCMakeExecutable() const;
    QString getCMakeGuiExecutable() const;
    
    void writeQueryFiles();
    
    bool readReplyFiles();

    bool haveKateReplyFiles() const;
    
    const QString& getProjectName() const;
    const QString& getBuildDir() const;
    const QString& getSourceDir() const;
    
    const std::set<QString>& getSourceFiles() const;
    const std::vector<TargetDef>& getTargets() const;


private Q_SLOTS:
    void handleStarted();
    void handleStateChanged(QProcess::ProcessState newState);
    void handleError();
    
private:
    QJsonObject readJsonFile(const QString& filename) const;
    QString findCMakeExecutable(const QString& cmakeCacheFile) const;
    void writeQueryFile(const char* objectKind, int version);

    QString m_cmakeExecutable;
    QString m_cacheFile;
    QString m_buildDir;
    QString m_sourceDir;
    QString m_projectName;
    bool m_cmakeSuccess = true;

    std::set<QString> m_sourceFiles;
    std::vector<TargetDef> m_targets;
};
