/*
    SPDX-FileCopyrightText: 2021 Waqar Ahmed <waqar.17a@gmail.com>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/
#include "cmaketoolsplugin.h"

#include <KPluginFactory>
#include <KTextEditor/MainWindow>
#include <KXMLGUIFactory>
#include <ktexteditor/codecompletioninterface.h>

#include <QDebug>
#include <QFileDialog>
#include <QDir>
#include <QMessageBox>
#include <QProcess>
#include <QFile>

K_PLUGIN_FACTORY_WITH_JSON(CMakeToolsPluginFactory, "cmaketoolsplugin.json", registerPlugin<CMakeToolsPlugin>();)

CMakeToolsPlugin::CMakeToolsPlugin(QObject *parent, const QList<QVariant> &)
    : KTextEditor::Plugin(parent)
{
}

CMakeToolsPlugin::~CMakeToolsPlugin() = default;

QObject *CMakeToolsPlugin::createView(KTextEditor::MainWindow *mainWindow)
{
    return new CMakeToolsPluginView(this, mainWindow);
}

CMakeToolsPluginView::CMakeToolsPluginView(CMakeToolsPlugin *plugin, KTextEditor::MainWindow *mainwindow)
    : QObject(plugin)
    , m_mainWindow(mainwindow)
{
    m_toolview.reset(      mainwindow->createToolView(plugin,
                                                   QStringLiteral("kate_private_plugin_katecmaketoolsplugin"),
                                                   KTextEditor::MainWindow::Bottom,
                                                   QIcon::fromTheme(QStringLiteral("folder")),
                                                   i18n("CMake Tools")));
    m_widget = new CMakeToolsWidget(mainwindow, m_toolview.get());

    connect(m_mainWindow, &KTextEditor::MainWindow::viewCreated, this, &CMakeToolsPluginView::onViewCreated);
    /**
     * connect for all already existing views
     */
    const auto views = m_mainWindow->views();
    for (KTextEditor::View *view : views) {
        onViewCreated(view);
    }
}

CMakeToolsPluginView::~CMakeToolsPluginView()
{
    m_mainWindow->guiFactory()->removeClient(this);
}

void CMakeToolsPluginView::onViewCreated(KTextEditor::View *v)
{
    m_widget->checkCMakeListsFolder((v->document()->url()).path());

    if (!CMakeCompletion::isCMakeFile(v->document()->url()))
        return;

    qWarning() << "Registering code completion model for view <<" << v << v->document()->url();


    KTextEditor::CodeCompletionInterface *cci = qobject_cast<KTextEditor::CodeCompletionInterface *>(v);
    if (cci) {
        cci->registerCompletionModel(&m_completion);
    }
}

CMakeToolsWidget::CMakeToolsWidget(KTextEditor::MainWindow *mainwindow, QWidget *parent)
    : QWidget(parent)
    , m_mainWindow(mainwindow)
{
    setupUi(this);
    connect(selectBfolder, &QToolButton::clicked, this, &CMakeToolsWidget::cmakeToolsBuildDir);
    connect(selectSfolder, &QToolButton::clicked, this, &CMakeToolsWidget::cmakeToolsSourceDir);
    connect(configCMakeToolsPlugin, &QPushButton::clicked, this, &CMakeToolsWidget::cmakeToolsGenLink);
}

CMakeToolsWidget::
~CMakeToolsWidget() = default;

void CMakeToolsWidget::checkCMakeListsFolder(QString viewPath){
    QString iterPath = viewPath;
    iterPath.truncate(viewPath.lastIndexOf(QChar(47)));

    if(!sourceDirectoryPath->text().isEmpty() && iterPath.contains(sourceDirectoryPath->text())){
        return;
    }

    QString comparePath = viewPath;
    comparePath.truncate(viewPath.lastIndexOf(QChar(47), 0));

    QString lastPath;

    while(QString::compare(iterPath, comparePath) != 0){
        if(!QFileInfo(iterPath + QStringLiteral("/CMakeLists.txt")).exists()){
            break;
        }
        lastPath = iterPath;
        iterPath.truncate(iterPath.lastIndexOf(QChar(47)));
    }

    sourceDirectoryPath->setText(lastPath);
    return;
}

void CMakeToolsWidget::cmakeToolsBuildDir(){
    const QString prefix = QFileDialog::getExistingDirectory(this, QStringLiteral("Get build folder"),
                                                                   QDir::homePath());
    if (prefix.isEmpty()) {
        return;
    }

    buildDirectoryPath->setText(prefix);
}

void CMakeToolsWidget::cmakeToolsSourceDir(){
    const QString prefix = QFileDialog::getExistingDirectory(this, QStringLiteral("Get source folder"),
                                                                   QDir::homePath());
    if (prefix.isEmpty()) {
        return;
    }

    sourceDirectoryPath->setText(prefix);
}

int CMakeToolsWidget::cmakeToolsCheckifConfigured(QString sourceCompile_Commands_json_path,
                                                  QString buildCompile_Commands_json_path){
    if(QString::compare(QFileInfo(sourceCompile_Commands_json_path).symLinkTarget(),
                        buildCompile_Commands_json_path) == 0){
        QMessageBox::information(this, QStringLiteral("Already configured"),
                                       QStringLiteral("The plugin is already configured"));
        return 1;
    }
    return 0;
}

int CMakeToolsWidget::cmakeToolsVerifyAndCreateCommands_Compilejson(QString buildCompile_Commands_json_path){
    QProcess cmakeProcess;
    int cmakeProcessReturn;

    if(QFileInfo(buildCompile_Commands_json_path).exists()){
        return 0;
    }

    QString buildCMakeCachetxtPath = buildDirectoryPath->text() + QStringLiteral("/CMakeCache.txt");

    if(!QFileInfo(buildCMakeCachetxtPath).exists()){
        QMessageBox::warning(this, QStringLiteral("Warning"),
                                   QStringLiteral("File CMakeCache.txt not present on folder ") + buildDirectoryPath->text());
        return 1;
    }

    if(QMessageBox::Yes != QMessageBox::question(this, QStringLiteral("Generate file?"),
                                                       QStringLiteral("Do you wish to generate the file compile_commands.json?"))){
        return 1;
    }

    cmakeProcess.setWorkingDirectory(buildDirectoryPath->text());
    cmakeProcessReturn = cmakeProcess.execute(QStringLiteral("cmake"), QStringList{buildDirectoryPath->text(),
                                                                       QStringLiteral("-DCMAKE_EXPORT_COMPILE_COMMANDS=ON")});

    if(!cmakeProcess.waitForFinished(-1) && cmakeProcessReturn != 0){
        QMessageBox::warning(this, QStringLiteral("Warning"),
                                   QStringLiteral("Failed to generate the compile_commands.json file ") +
                                   QStringLiteral("\nCMake return code: %1").arg(cmakeProcessReturn));
        return 1;
    }

    return 0;
}

int CMakeToolsWidget::cmakeToolsCreateLink(QString sourceCompile_Commands_json_path,
                                           QString buildCompile_Commands_json_path, int createReturn){
    if(createReturn == 1){
        return 1;
    }

    QFile orig(buildCompile_Commands_json_path);

    if(QFileInfo(sourceCompile_Commands_json_path).exists()){
        QFile(sourceCompile_Commands_json_path).remove();
    }

    if(!orig.link(sourceCompile_Commands_json_path)){
        QMessageBox::warning(this, QStringLiteral("Warning"),
                                   QStringLiteral("Failed to create link in ") + sourceDirectoryPath->text());
        return 1;
    }

    return 0;
}

void CMakeToolsWidget::cmakeToolsGenLink(){
    QMessageBox msg;

    const QString sourceCompile_Commands_json_path = sourceDirectoryPath->text() + QStringLiteral("/compile_commands.json");
    const QString buildCompile_Commands_json_path = buildDirectoryPath->text() + QStringLiteral("/compile_commands.json");

    int createReturn;

    createReturn = CMakeToolsWidget::cmakeToolsCheckifConfigured
                    (sourceCompile_Commands_json_path, buildCompile_Commands_json_path);

    if(createReturn == 1){
        return;
    }

    createReturn = CMakeToolsWidget::cmakeToolsVerifyAndCreateCommands_Compilejson(buildCompile_Commands_json_path);
    createReturn = CMakeToolsWidget::cmakeToolsCreateLink
                                   (sourceCompile_Commands_json_path, buildCompile_Commands_json_path, createReturn);

    if(createReturn == 1){
        return;
    }

    QMessageBox::information(this, QStringLiteral("Success"),
                                   QStringLiteral("The plugin was configured successfully in ") +
                                   sourceDirectoryPath->text());
}

#include "cmaketoolsplugin.moc"
