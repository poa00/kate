/*
    SPDX-FileCopyrightText: 2023 Alexander Neundorf <neundorf@kde.org>
    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "qcmakefileapi.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTextStream>
#include <QProcess>
#include <QRegularExpression>

#include <algorithm>

QCMakeFileApi::QCMakeFileApi(const QString& cmakeCacheFile)
: QObject()
, m_cmakeExecutable()
, m_cacheFile()
, m_buildDir()
{
    qDebug() << "--------------- cachefile: "<< cmakeCacheFile;
    QFileInfo fi(cmakeCacheFile);
    if (fi.isDir())
    {
        qDebug() << "isDir" << "filepath: " << fi.absoluteFilePath() << " path " << fi.absolutePath();
        m_buildDir = fi.absoluteFilePath();
        m_cacheFile = m_buildDir + QStringLiteral("/CMakeCache.txt");
        
    }
    else
    {
        m_buildDir = fi.absolutePath();
        m_cacheFile = fi.absoluteFilePath();
    }
    
    qDebug() << "builddir: " << m_buildDir << " cachefile: " << m_cacheFile;

    m_cmakeExecutable = findCMakeExecutable(m_cacheFile);
}


const QString& QCMakeFileApi::getCMakeExecutable() const
{
    return m_cmakeExecutable;
}


QString QCMakeFileApi::getCMakeGuiExecutable() const
{
    QString cmakeGui;
    QFileInfo fi(m_cmakeExecutable);
//    qDebug() << "++++++++++ cmake: " << m_cmakeExecutable << " abs: " << fi.
    if (fi.isAbsolute() && fi.isFile() && fi.isExecutable())
    {
        cmakeGui = fi.absolutePath() + QStringLiteral("/cmake-gui") + (m_cmakeExecutable.endsWith(QStringLiteral(".exe")) ? QStringLiteral(".exe") : QString());
    }
    return cmakeGui;
}


QString QCMakeFileApi::findCMakeExecutable(const QString& cmakeCacheFile) const
{
    QFile cacheFile(cmakeCacheFile);
    if (cacheFile.open(QIODevice::ReadOnly)) {
        QRegularExpression re(QStringLiteral("CMAKE_COMMAND:.+=(.+)$"));
        
        QTextStream in(&cacheFile);
        while (!in.atEnd()) {
            QString line = in.readLine();
            QRegularExpressionMatch match = re.match(line);
            if (match.hasMatch()) {
                return match.captured(1);
            }
        }
    }

    return QString(QStringLiteral("cmake"));
}


bool QCMakeFileApi::runCMake()
{
    m_cmakeSuccess = true;
    QProcess cmakeProc;
    cmakeProc.setProgram(m_cmakeExecutable);
    cmakeProc.setWorkingDirectory(m_buildDir);
    cmakeProc.setArguments({QStringLiteral("-DCMAKE_EXPORT_COMPILE_COMMANDS=ON"), QStringLiteral(".")});
    connect(&cmakeProc, &QProcess::started, this, &QCMakeFileApi::handleStarted);
    connect(&cmakeProc, &QProcess::stateChanged, this, &QCMakeFileApi::handleStateChanged);
    connect(&cmakeProc, &QProcess::errorOccurred, this, &QCMakeFileApi::handleError);
    cmakeProc.start();
    cmakeProc.waitForFinished();
    return m_cmakeSuccess;
    
}


void QCMakeFileApi::handleStarted()
{
    printf("Started !\n");
}


void QCMakeFileApi::handleStateChanged(QProcess::ProcessState newState)
{
    printf("State changed: %d !\n", (int)newState);
}


void QCMakeFileApi::handleError()
{
    printf("error !\n");
    m_cmakeSuccess = false;
}


void QCMakeFileApi::writeQueryFiles()
{
    writeQueryFile("codemodel", 2);
    writeQueryFile("cmakeFiles", 1);
}


void QCMakeFileApi::writeQueryFile(const char* objectKind, int version)
{
    QDir buildDir(m_buildDir);
    QString queryFileDir = QStringLiteral("%1/.cmake/api/v1/query/client-kate/")
                                 .arg(m_buildDir);
    buildDir.mkpath(queryFileDir);
    
    QString queryFilePath = QStringLiteral("%1/%2-v%3")
                               .arg(queryFileDir).arg(QLatin1String(objectKind)).arg(version);
    QFile file(queryFilePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return;
    }
}


QJsonObject QCMakeFileApi::readJsonFile(const QString& filename) const
{
    const QDir replyDir(QStringLiteral("%1/.cmake/api/v1/reply/").arg(m_buildDir));
    
    QString indexFile = replyDir.absoluteFilePath(filename);
    fprintf(stderr, "reply index files: %s\n", indexFile.toUtf8().data());
    
    QFile file(indexFile);
    bool success = file.open(QIODevice::ReadOnly);
    QByteArray fileContents = file.readAll();
    QJsonDocument jsonDoc = QJsonDocument::fromJson(fileContents);
    QJsonObject docObj = jsonDoc.object();

    return docObj;
}


bool QCMakeFileApi::haveKateReplyFiles() const
{
    const QDir replyDir(QStringLiteral("%1/.cmake/api/v1/reply/").arg(m_buildDir));
    QStringList indexFiles = replyDir.entryList({QStringLiteral("index-*.json")}, QDir::Files);
    if (indexFiles.isEmpty())
    {
        return false;
    }
    
    QString indexFile = replyDir.absoluteFilePath(indexFiles.at(0));
    
    QJsonObject docObj = readJsonFile(indexFile);

    QJsonObject replyObj = docObj.value(QStringLiteral("reply")).toObject();
    
    return (replyObj.contains(QStringLiteral("client-kate")) && (replyObj.value(QStringLiteral("client-kate")).isObject()));
}


bool QCMakeFileApi::readReplyFiles()
{
    const QDir replyDir(QStringLiteral("%1/.cmake/api/v1/reply/").arg(m_buildDir));
    QStringList indexFiles = replyDir.entryList({QStringLiteral("index-*.json")}, QDir::Files);
    if (indexFiles.isEmpty())
    {
        return false;
    }
    
    m_sourceDir.clear();
    m_sourceFiles.clear();
    m_targets.clear();
    
    QString indexFile = replyDir.absoluteFilePath(indexFiles.at(0));
    fprintf(stderr, "reply index files: %s\n", indexFile.toUtf8().data());
    
    QJsonObject docObj = readJsonFile(indexFile);
    qWarning() << "docObj: " << docObj;

    QJsonObject cmakeObj = docObj.value(QStringLiteral("cmake")).toObject();
    qWarning() << "cmake: " << cmakeObj;
    
    QJsonObject replyObj = docObj.value(QStringLiteral("reply")).toObject();
    qWarning() << "reply: " << replyObj;
    
    QJsonObject kateObj = replyObj.value(QStringLiteral("client-kate")).toObject();
    qWarning() << "kate: " << kateObj;

    QJsonObject cmakeFilesObj = kateObj.value(QStringLiteral("cmakeFiles-v1")).toObject();
    qWarning() << "cmakefiles: " << cmakeFilesObj;
    QString cmakeFilesFile = cmakeFilesObj.value(QStringLiteral("jsonFile")).toString();
    
    QJsonObject codeModelObj = kateObj.value(QStringLiteral("codemodel-v2")).toObject();
    qWarning() << "code model: " << codeModelObj;
    QString codemodelFile = codeModelObj.value(QStringLiteral("jsonFile")).toString();
    
    qWarning() << "cmakeFiles: " << cmakeFilesFile << " codemodelfile: " << codemodelFile;


    QJsonObject cmakeFilesDoc = readJsonFile(cmakeFilesFile);

    QJsonObject pathsObj = cmakeFilesDoc.value(QStringLiteral("paths")).toObject();
    m_sourceDir = pathsObj.value(QStringLiteral("source")).toString();
    qWarning() << "srcdir: " << m_sourceDir;
    
    QJsonArray cmakeFiles = cmakeFilesDoc.value(QStringLiteral("inputs")).toArray();
    qWarning() << cmakeFiles.count() << " cmake files";
    for(int i=0; i<cmakeFiles.count(); i++)
    {
        QJsonObject fileObj = cmakeFiles.at(i).toObject();
        bool isCMake = fileObj.value(QStringLiteral("isCMake")).toBool();
        bool isGenerated = fileObj.value(QStringLiteral("isGenerated")).toBool();
        QString filename = fileObj.value(QStringLiteral("path")).toString();
        if ((isCMake == false) && (isGenerated == false))
        {
            qWarning() << "#" << i << ": " << filename << " cmake: " << isCMake << " gen: " << isGenerated;
            m_sourceFiles.insert(filename);
        }
    }

    QJsonObject codeModelDoc = readJsonFile(codemodelFile);
    QJsonArray configs = codeModelDoc.value(QStringLiteral("configurations")).toArray();
    qWarning() << "configs: " << configs;

    for(int i=0; i<configs.count(); i++)
    {
        QJsonObject configObj = configs.at(i).toObject();
        QString configName = configObj.value(QStringLiteral("name")).toString();
        qWarning() << "config: " << configName;
        QJsonArray projects = configObj.value(QStringLiteral("projects")).toArray();
        for(int projIdx=0; projIdx<projects.count(); projIdx++)
        {
            QJsonObject projectObj = projects.at(projIdx).toObject();
            QString projectName = projectObj.value(QStringLiteral("name")).toString();
            if (!projectName.isEmpty())
            {
                // this will usually be the first one. Should we check that this
                // one contains directoryIndex 0 ?
                qDebug() << "------------ projectname: " << projectName;
                m_projectName = projectName;
                break;
            }
        }
        QJsonArray targets = configObj.value(QStringLiteral("targets")).toArray();
        m_targets.push_back({QStringLiteral("clean"), configName});
        for(int tgtIdx=0; tgtIdx<targets.count(); tgtIdx++)
        {
            QJsonObject targetObj = targets.at(tgtIdx).toObject();
            QString targetName = targetObj.value(QStringLiteral("name")).toString();

            m_targets.push_back({targetName, configName});
            
            QString targetJsonFile = targetObj.value(QStringLiteral("jsonFile")).toString();
            
            qWarning() << "config: " << configName << " target: " << targetName << " json: " << targetJsonFile;
            

            QJsonObject targetDoc = readJsonFile(targetJsonFile);
            QJsonArray sources = targetDoc.value(QStringLiteral("sources")).toArray();
            for(int srcIdx=0; srcIdx<sources.count(); srcIdx++)
            {
                QJsonObject sourceObj = sources.at(srcIdx).toObject();
                QString filePath = sourceObj.value(QStringLiteral("path")).toString();
                qWarning() << "source: " << filePath;
                m_sourceFiles.insert(filePath);
            }
        }
    }
    
    return true;

}


const QString& QCMakeFileApi::getProjectName() const
{
    return m_projectName;
}


const std::set<QString>& QCMakeFileApi::getSourceFiles() const
{
    return m_sourceFiles;
}


const std::vector<QCMakeFileApi::TargetDef>& QCMakeFileApi::getTargets() const
{
    return m_targets;
}


const QString& QCMakeFileApi::getSourceDir() const
{
    return m_sourceDir;
}


const QString& QCMakeFileApi::getBuildDir() const
{
    return m_buildDir;
}