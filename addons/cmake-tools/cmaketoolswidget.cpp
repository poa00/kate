#include "cmaketoolswidget.h"

#include <KTextEditor/MainWindow>
#include <QDebug>
#include <QFileDialog>
#include <QDir>
#include <QMessageBox>
#include <QProcess>
#include <QFile>

CMakeToolsWidget::CMakeToolsWidget(KTextEditor::MainWindow *mainwindow, QWidget *parent)
    : QWidget(parent)
    , m_mainWindow(mainwindow)
{
    setupUi(this);
    connect(m_mainWindow, &KTextEditor::MainWindow::viewChanged, this, &CMakeToolsWidget::checkCMakeListsFolder);
    connect(selectBfolder, &QToolButton::clicked, this, &CMakeToolsWidget::cmakeToolsBuildDir);
    connect(selectSfolder, &QToolButton::clicked, this, &CMakeToolsWidget::cmakeToolsSourceDir);
    connect(configCMakeToolsPlugin, &QPushButton::clicked, this, &CMakeToolsWidget::cmakeToolsGenLink);
}

CMakeToolsWidget::
~CMakeToolsWidget() = default;

void CMakeToolsWidget::checkCMakeListsFolder(KTextEditor::View *v) {

    auto getDocumentPath = [&v]() {
        return v->document()->url().path();
    };

    QString iterPath = getDocumentPath();
    iterPath.truncate(iterPath.lastIndexOf(QLatin1Char('/')));

    if(!sourceDirectoryPath->text().isEmpty() && iterPath.contains(sourceDirectoryPath->text())){
        return;
    }

    QString comparePath = getDocumentPath();
    comparePath.truncate(comparePath.lastIndexOf(QLatin1Char('/'), 0));

    QString lastPath;

    while(iterPath != comparePath) {
        if(!QFileInfo(iterPath + QStringLiteral("/CMakeLists.txt")).exists()){
            break;
        }
        lastPath = iterPath;
        iterPath.truncate(iterPath.lastIndexOf(QLatin1Char('/')));
    }

    sourceDirectoryPath->setText(lastPath);
    return;
}

void CMakeToolsWidget::cmakeToolsBuildDir() {
    const QString prefix = QFileDialog::getExistingDirectory(this, i18n("Get build folder"),
                                                                   QDir::homePath());
    if (prefix.isEmpty()) {
        return;
    }

    buildDirectoryPath->setText(prefix);
}

void CMakeToolsWidget::cmakeToolsSourceDir() {
    const QString prefix = QFileDialog::getExistingDirectory(this, i18n("Get source folder"),
                                                                   QDir::homePath());
    if (prefix.isEmpty()) {
        return;
    }

    sourceDirectoryPath->setText(prefix);
}

CMakeRunStatus CMakeToolsWidget::cmakeToolsCheckifConfigured(QString sourceCompileCommandsJsonpath,
                                                  QString buildCompileCommandsJsonpath) {
    if(QFileInfo(buildCompileCommandsJsonpath).exists() &&
                        QString::compare(QFileInfo(sourceCompileCommandsJsonpath).symLinkTarget(),
                        buildCompileCommandsJsonpath) == 0) {

        QMessageBox::information(this, i18n("Already configured"),
                                       i18n("The project is already configured"));
        return CMakeRunStatus::Failure;
    }
    return CMakeRunStatus::Success;
}

CMakeRunStatus CMakeToolsWidget::cmakeToolsVerifyAndCreateCommands_Compilejson(QString buildCompileCommandsJsonpath) {
    QProcess cmakeProcess;
    int cmakeProcessReturn;

    if(QFileInfo(buildCompileCommandsJsonpath).exists()){
        return CMakeRunStatus::Success;
    }

    QString buildCMakeCachetxtPath = buildDirectoryPath->text() + QStringLiteral("/CMakeCache.txt");

    if(!QFileInfo(buildCMakeCachetxtPath).exists()) {
        QMessageBox::warning(this, i18n("Warning"),
                                   i18n("File CMakeCache.txt not present on folder ") +
                                   i18n(buildDirectoryPath->text().toStdString().c_str()));
        return CMakeRunStatus::Failure;
    }

    if(QMessageBox::Yes != QMessageBox::question(this, i18n("Generate file?"),
                                                       i18n("Do you wish to generate the file compile_commands.json?"))) {
        return CMakeRunStatus::Failure;
    }

    cmakeProcess.setWorkingDirectory(buildDirectoryPath->text());
    cmakeProcessReturn = cmakeProcess.execute(QStringLiteral("cmake"), QStringList{buildDirectoryPath->text(),
                                                                       QStringLiteral("-DCMAKE_EXPORT_COMPILE_COMMANDS=ON")});

    if(!cmakeProcess.waitForFinished(-1) && cmakeProcessReturn != 0) {
        QMessageBox::warning(this, i18n("Warning"),
                                   i18n("Failed to generate the compile_commands.json file ") +
                                   i18n((QStringLiteral("\nCMake return code: %1").arg(cmakeProcessReturn)).toStdString().c_str()));
        return CMakeRunStatus::Failure;
    }

    return CMakeRunStatus::Success;
}

CMakeRunStatus CMakeToolsWidget::cmakeToolsCreateLink(QString sourceCompileCommandsJsonpath,
                                                      QString buildCompileCommandsJsonpath) {
    if(QFileInfo(sourceCompileCommandsJsonpath).exists()) {
        QFile(sourceCompileCommandsJsonpath).remove();
    }

    QFile orig(buildCompileCommandsJsonpath);

    if(!orig.link(sourceCompileCommandsJsonpath)) {
        QMessageBox::warning(this, i18n("Warning"),
                                   i18n("Failed to create link in ") +
                                   i18n(sourceDirectoryPath->text().toStdString().c_str()));
        return CMakeRunStatus::Failure;
    }

    return CMakeRunStatus::Success;
}

void CMakeToolsWidget::cmakeToolsGenLink() {
    const QString sourceCompileCommandsJsonpath = sourceDirectoryPath->text() + QStringLiteral("/compile_commands.json");
    const QString buildCompileCommandsJsonpath = buildDirectoryPath->text() + QStringLiteral("/compile_commands.json");

    CMakeRunStatus createReturn;

    createReturn = cmakeToolsCheckifConfigured(sourceCompileCommandsJsonpath, buildCompileCommandsJsonpath);

    if(createReturn == CMakeRunStatus::Failure) {
        return;
    }

    createReturn = cmakeToolsVerifyAndCreateCommands_Compilejson(buildCompileCommandsJsonpath);

    if(createReturn == CMakeRunStatus::Failure) {
        return;
    }

    createReturn = cmakeToolsCreateLink(sourceCompileCommandsJsonpath, buildCompileCommandsJsonpath);

    if(createReturn == CMakeRunStatus::Failure) {
        return;
    }

    QMessageBox::information(this, i18n("Success"),
                                   i18n("The plugin was configured successfully in ") +
                                   i18n(sourceDirectoryPath->text().toStdString().c_str()));
}
