#ifndef KATE_CMAKE_TOOLS_WIDGET_H
#define KATE_CMAKE_TOOLS_WIDGET_H

#include <KTextEditor/MainWindow>
#include <QMap>
#include <ktexteditor/view.h>

#include <QWidget>

#include "ui_cmaketoolswidget.h"

enum class CMakeRunStatus { Failure = 0, Success = 1 };

class CMakeToolsWidget : public QWidget, public Ui::CMakeToolsWidget
{
    Q_OBJECT

public:
    CMakeToolsWidget(KTextEditor::MainWindow *mainwindow, QWidget *parent);

    ~CMakeToolsWidget() override;

    void setSourceToBuildMap(const QMap<QString, QStringList> readedQMap);
    const QMap<QString, QStringList> getSourceToBuildMap();

private Q_SLOTS:
    void guessCMakeListsFolder(KTextEditor::View *v);
    void cmakeToolsSelectBuildFolderButton();
    void cmakeToolsConfigureButton();

private:
    KTextEditor::MainWindow *m_mainWindow;
    void loadWidgetSessionFromSourceToBuildMap(const QString sourcePath);
    void saveWidgetSessionOnSourceToBuildMap(const QString sourcePath, const QString buildPath);
    CMakeRunStatus checkForCMakeCachetxt(const QString buildPathToCheck, const bool errorMessage = true);
    QString getSourceDirFromCMakeCache();
    void searchForBuildDirectoriesInsideSource();
    CMakeRunStatus cmakeToolsCheckifConfigured(const QString sourceCompileCommandsJsonPath, const QString buildCompileCommandsJsonPath);
    CMakeRunStatus cmakeToolsVerifyAndCreateCommandsCompileJson(const QString buildCompileCommandsJsonPath);
    CMakeRunStatus cmakeToolsCreateLinkToCommandsCompileJsonOnSourceFolder(const QString sourceCompileCommandsJsonPath,
                                                                           const QString buildCompileCommandsJsonPath);
    QMap<QString, QStringList> m_sourceToBuildMap;
};

#endif
