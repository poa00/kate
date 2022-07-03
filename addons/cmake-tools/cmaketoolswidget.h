#ifndef KATE_CMAKE_TOOLS_WIDGET_H
#define KATE_CMAKE_TOOLS_WIDGET_H

#include <KTextEditor/MainWindow>
#include <ktexteditor/view.h>

#include <QMap>
#include <QProcess>
#include <QWidget>

#include <memory>

#include "cmaketoolsplugin.h"
#include "ui_cmaketoolswidget.h"

class CMakeToolsWidget : public QWidget, public Ui::CMakeToolsWidget
{
    Q_OBJECT

public:
    CMakeToolsWidget(KTextEditor::MainWindow *mainwindow, CMakeToolsPlugin *plugin, QWidget *parent);

    ~CMakeToolsWidget() override;

    void setSourceToBuildMap(const QMap<QString, QStringList> &readedQMap);
    QMap<QString, QStringList> getSourceToBuildMap();

private Q_SLOTS:
    void guessCMakeListsFolder(KTextEditor::View *v);
    void selectBuildFolder();
    void onConfigureBtnClicked();
    void cmakeFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void appendCMakeProcessOutput();

private:
    void loadWidgetSession(const QString &sourcePath);
    void saveWidgetSession(const QString &sourcePath, const QString &buildPath);
    bool hasCmakeCacheDir(const QString &buildPathToCheck);
    QString getSourceDirFromCMakeCache();
    void searchForBuildDirectories();
    bool isCmakeToolsValid();
    void qProcessError(QProcess::ProcessError error);
    bool copyCompileCommandsToSource(const QString &sourceCompileCommandsPath, const QString &buildCompileCommandsPath);

    KTextEditor::MainWindow *m_mainWindow;
    CMakeToolsPlugin *m_plugin;
    QMap<QString, QStringList> m_sourceToBuildMap;
    QProcess m_cmakeProcess;
    bool m_createCopyCheckBoxState;
};

#endif
