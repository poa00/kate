#ifndef KATE_CMAKE_TOOLS_WIDGET_H
#define KATE_CMAKE_TOOLS_WIDGET_H

#include <KTextEditor/MainWindow>
#include <QMap>
#include <ktexteditor/view.h>

#include <QProcess>
#include <QWidget>
#include <memory>

#include "ui_cmaketoolswidget.h"

enum class CMakeRunStatus { Failure = 0, Success = 1 };

class CMakeToolsWidget : public QWidget, public Ui::CMakeToolsWidget
{
    Q_OBJECT

public:
    CMakeToolsWidget(KTextEditor::MainWindow *mainwindow, QWidget *parent);

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
    bool hasDirCmakeCache(const QString &buildPathToCheck, bool errorMessage = true);
    QString getSourceDirFromCMakeCache();
    void searchForBuildDirectories();
    bool isCmakeToolsValid();
    void qProcessError(QProcess::ProcessError error);
    CMakeRunStatus copyCompileCommandsToSource(const QString &sourceCompileCommandsPath, const QString &buildCompileCommandsPath);

    KTextEditor::MainWindow *m_mainWindow;
    QMap<QString, QStringList> m_sourceToBuildMap;
    std::unique_ptr<QProcess> m_cmakeProcess;
    bool m_createCopyCheckBoxState;
};

#endif
