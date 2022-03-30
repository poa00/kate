#ifndef KATE_CMAKE_TOOLS_WIDGET_H
#define KATE_CMAKE_TOOLS_WIDGET_H

#include <KTextEditor/MainWindow>
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

private Q_SLOTS:
    void checkCMakeListsFolder(KTextEditor::View *v);
    void cmakeToolsBuildDir();
    void cmakeToolsGenLink();

private:
    KTextEditor::MainWindow *m_mainWindow;
    CMakeRunStatus cmakeToolsCheckifConfigured(const QString sourceCompile_Commands_json_path, const QString buildCompile_Commands_json_path);
    CMakeRunStatus cmakeToolsVerifyAndCreateCommands_Compilejson(const QString buildCompile_Commands_json_path);
    CMakeRunStatus cmakeToolsCreateLink(const QString sourceCompile_Commands_json_path, const QString buildCompile_Commands_json_path);
};

#endif
