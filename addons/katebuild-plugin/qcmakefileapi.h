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
    //* \a buildDir can be either the cmake build dir or the full path of a CMakeCache.txt
    QCMakeFileApi(const QString &buildDir, bool withSourceFiles = true);

    bool runCMake();

    const QString &getCMakeExecutable() const;
    QString getCMakeGuiExecutable() const;

    bool writeQueryFiles();

    bool readReplyFiles();

    bool haveKateReplyFiles() const;

    //! Returns the command line which will be run. Needed only for asking the user for permission.
    QStringList getCMakeRequestCommandLine() const;

    const std::vector<QString> &getConfigurations() const;
    const QString &getProjectName() const;
    const QString &getBuildDir() const;
    const QString &getSourceDir() const;

    const std::set<QString> &getSourceFiles() const;

    enum class TargetType { Executable = 0, Library = 1, Utility = 2, Unknown = 3 };

    struct Target {
        QString name;
        TargetType type = TargetType::Unknown;
    };

    const std::vector<Target> &getTargets(const QString &config) const;

private Q_SLOTS:
    void handleStarted();
    void handleStateChanged(QProcess::ProcessState newState);
    void handleError();

private:
    QJsonObject readJsonFile(const QString &filename) const;
    QString findCMakeExecutable(const QString &cmakeCacheFile) const;
    QString findCMakeGuiExecutable(const QString &cmakeExecutable) const;
    TargetType typeFromJson(const QString &typeStr) const;
    bool writeQueryFile(const char *objectKind, int version);

    QString m_cmakeExecutable;
    QString m_cmakeGuiExecutable;
    QString m_cacheFile;
    QString m_buildDir;
    QString m_sourceDir;
    QString m_projectName;
    bool m_withSourceFiles;
    bool m_cmakeSuccess = true;

    std::set<QString> m_sourceFiles;
    std::map<QString /*config*/, std::vector<Target>> m_targets;
    const std::vector<Target> m_emptyTargets = {};
    std::vector<QString> m_configs;
};