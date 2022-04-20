#include "cmaketoolswidget.h"

#include <KTextEditor/Editor>
#include <KTextEditor/MainWindow>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QLineEdit>
#include <QMessageBox>
#include <qstringliteral.h>

CMakeToolsWidget::CMakeToolsWidget(KTextEditor::MainWindow *mainwindow, QWidget *parent)
    : QWidget(parent)
    , m_mainWindow(mainwindow)
{
    setupUi(this);
    m_cmakeProcess.reset(new QProcess);
    m_cmakeProcess->setProcessChannelMode(QProcess::MergedChannels);
    executionLogForCmakeProcess->document()->setDefaultFont(KTextEditor::Editor::instance()->font());
    buildDirectoryPath->lineEdit()->setPlaceholderText(QStringLiteral("Select build folder"));
    connect(m_mainWindow, &KTextEditor::MainWindow::viewChanged, this, &CMakeToolsWidget::guessCMakeListsFolder);
    connect(selectBuildFolderButton, &QToolButton::clicked, this, &CMakeToolsWidget::cmakeToolsSelectBuildFolderButton);
    connect(configureCMakeToolsButton, &QPushButton::clicked, this, &CMakeToolsWidget::cmakeToolsConfigureButtonBeforeProcessFinished);
    connect(m_cmakeProcess.get(), &QProcess::readyReadStandardOutput, this, &CMakeToolsWidget::printCMakeProcessOutputOnPlainTextEdit);
    connect(m_cmakeProcess.get(), &QProcess::finished, this, &CMakeToolsWidget::cmakeToolsConfigureButtonAfterProcessFinished);
    connect(m_cmakeProcess.get(), &QProcess::errorOccurred, this, &CMakeToolsWidget::qProcessError);
}

CMakeToolsWidget::~CMakeToolsWidget() = default;

void CMakeToolsWidget::setSourceToBuildMap(const QMap<QString, QStringList> readedQMap)
{
    m_sourceToBuildMap = readedQMap;
}

const QMap<QString, QStringList> CMakeToolsWidget::getSourceToBuildMap()
{
    return m_sourceToBuildMap;
}

void CMakeToolsWidget::loadWidgetSessionFromSourceToBuildMap(const QString sourcePath)
{
    buildDirectoryPath->clear();

    buildDirectoryPath->insertItems(0, m_sourceToBuildMap[sourcePath]);
}
void CMakeToolsWidget::saveWidgetSessionOnSourceToBuildMap(const QString sourcePath, const QString buildPath)
{
    if (m_sourceToBuildMap[sourcePath].contains(buildPath)) {
        return;
    }
    m_sourceToBuildMap[sourcePath] << buildPath;

    loadWidgetSessionFromSourceToBuildMap(sourcePath);
}

void CMakeToolsWidget::guessCMakeListsFolder(KTextEditor::View *v)
{
    if (v->document()->url().isEmpty()) {
        sourceLabel->setVisible(false);
        sourceDirectoryPath->setVisible(false);
        return;
    }
    sourceLabel->setVisible(true);
    sourceDirectoryPath->setVisible(true);

    QString currentViewDocumentPath = v->document()->url().path();

    currentViewDocumentPath.truncate(currentViewDocumentPath.lastIndexOf(QLatin1Char('/')));

    if (!sourceDirectoryPath->text().isEmpty() && currentViewDocumentPath.contains(sourceDirectoryPath->text())) {
        return;
    }

    QDir iterateUntilLastCMakeLists = QDir(currentViewDocumentPath);

    QDir previousDirectory;

    while (iterateUntilLastCMakeLists.exists(QStringLiteral("CMakeLists.txt"))) {
        previousDirectory = iterateUntilLastCMakeLists;
        if (!iterateUntilLastCMakeLists.cdUp()) {
            break;
        }
    }

    QString guessedPath = previousDirectory.absolutePath();

    sourceDirectoryPath->setText(guessedPath);
    searchForBuildDirectoriesInsideSource();

    if (!m_sourceToBuildMap.contains(guessedPath)) {
        return;
    }

    loadWidgetSessionFromSourceToBuildMap(guessedPath);
    return;
}

CMakeRunStatus CMakeToolsWidget::checkForCMakeCachetxt(const QString buildPathToCheck, const bool errorMessage)
{
    QString buildCMakeCachetxtPath = buildPathToCheck + QStringLiteral("/CMakeCache.txt");

    if (!QFileInfo(buildCMakeCachetxtPath).exists()) {
        if (!errorMessage) {
            return CMakeRunStatus::Failure;
        }
        QMessageBox::warning(this, i18n("Warning"), i18n("Folder not selected or the file CMakeCache.txt is not present on the folder selected"));
        return CMakeRunStatus::Failure;
    }

    return CMakeRunStatus::Success;
}

QString CMakeToolsWidget::getSourceDirFromCMakeCache()
{
    if (checkForCMakeCachetxt(buildDirectoryPath->lineEdit()->text()) == CMakeRunStatus::Failure) {
        return QStringLiteral("");
    }

    QFile openCMakeCache(buildDirectoryPath->lineEdit()->text() + QStringLiteral("/CMakeCache.txt"));
    openCMakeCache.open(QIODevice::ReadWrite);
    QTextStream in(&openCMakeCache);
    const QString openedCMakeCacheContents = in.readAll();
    const QString sourcePrefix = QStringLiteral("SOURCE_DIR:STATIC=");

    if (!openedCMakeCacheContents.contains(sourcePrefix)) {
        QMessageBox::warning(this, i18n("Warning"), i18n("CMakeCache.txt does not have the source path"));
        return QStringLiteral("");
    }

    const int startSourceFolderPathIndexOnContents = openedCMakeCacheContents.indexOf(sourcePrefix);
    const int endSourceFolderPathIndexOnContents = openedCMakeCacheContents.indexOf(QStringLiteral("\n"), startSourceFolderPathIndexOnContents);
    QString sourceFolderPath = QStringLiteral("");
    for (int i = startSourceFolderPathIndexOnContents + 18; i != endSourceFolderPathIndexOnContents; i++) {
        sourceFolderPath += openedCMakeCacheContents[i];
    }
    openCMakeCache.close();

    return sourceFolderPath;
}

void CMakeToolsWidget::searchForBuildDirectoriesInsideSource()
{
    QStringListIterator possibleBuildPaths(QDir(sourceDirectoryPath->text()).entryList(QDir::Dirs | QDir::NoDotAndDotDot));

    while (possibleBuildPaths.hasNext()) {
        QString possibleBuildPath = possibleBuildPaths.next();
        possibleBuildPath = sourceDirectoryPath->text() + QStringLiteral("/") + possibleBuildPath;

        if (checkForCMakeCachetxt(possibleBuildPath, false) == CMakeRunStatus::Failure) {
            continue;
        }
        saveWidgetSessionOnSourceToBuildMap(sourceDirectoryPath->text(), possibleBuildPath);
    }
}

void CMakeToolsWidget::cmakeToolsSelectBuildFolderButton()
{
    QString startingDir = QDir::homePath();
    if (sourceDirectoryPath->text() != QStringLiteral("Source")) {
        startingDir = sourceDirectoryPath->text();
    }

    const QString selectedBuildPath = QFileDialog::getExistingDirectory(this, i18n("Get build folder"), startingDir, QFileDialog::ShowDirsOnly);

    if (selectedBuildPath.isEmpty()) {
        return;
    }

    buildDirectoryPath->lineEdit()->setText(selectedBuildPath);
    sourceDirectoryPath->setText(getSourceDirFromCMakeCache());
    searchForBuildDirectoriesInsideSource();
    loadWidgetSessionFromSourceToBuildMap(sourceDirectoryPath->text());
}

void CMakeToolsWidget::printCMakeProcessOutputOnPlainTextEdit()
{
    QString convertedOutput = QString::fromUtf8(m_cmakeProcess->readAllStandardOutput());
    executionLogForCmakeProcess->appendPlainText(convertedOutput);
}

CMakeRunStatus CMakeToolsWidget::cmakeToolsCheckIfImproper()
{
    if (buildDirectoryPath->lineEdit()->text().isEmpty() || sourceDirectoryPath->text().isEmpty()) {
        QMessageBox::warning(this,
                             i18n("Source or build folder path empty"),
                             i18n("The source or build folder path was not selected, please select a valid path. The source path is automatically selected "
                                  "when opening a file, the build path can be selected manually"));
        return CMakeRunStatus::Failure;
    }

    if (checkForCMakeCachetxt(buildDirectoryPath->lineEdit()->text()) == CMakeRunStatus::Failure) {
        return CMakeRunStatus::Failure;
    }

    return CMakeRunStatus::Success;
}

void CMakeToolsWidget::cmakeToolsCreateCompileCommandsJson()
{
    m_cmakeProcess->setWorkingDirectory(buildDirectoryPath->lineEdit()->text());
    m_cmakeProcess->start(QStringLiteral("cmake"), QStringList{buildDirectoryPath->lineEdit()->text(), QStringLiteral("-DCMAKE_EXPORT_COMPILE_COMMANDS=ON")});
}

CMakeRunStatus CMakeToolsWidget::cmakeToolsCopyCommandsCompileJsonOnSourceFolder(const QString sourceCompileCommandsJsonpath,
                                                                                 const QString buildCompileCommandsJsonPath,
                                                                                 const bool isChecked)
{
    if (isChecked == false) {
        return CMakeRunStatus::Failure;
    }

    if (QFileInfo(sourceCompileCommandsJsonpath).exists()) {
        QFile(sourceCompileCommandsJsonpath).remove();
    }

    QFile originalCompileCommandsJson(buildCompileCommandsJsonPath);

    if (!originalCompileCommandsJson.copy(sourceCompileCommandsJsonpath)) {
        QMessageBox::warning(this, i18n("Warning"), i18n("Failed to copy in ") + i18n(sourceDirectoryPath->text().toStdString().c_str()));
        return CMakeRunStatus::Failure;
    }

    return CMakeRunStatus::Success;
}

void CMakeToolsWidget::qProcessError()
{
    QMessageBox::warning(this, i18n("Warning"), i18n("Failed to generate the compile_commands.json file"));
    configureCMakeToolsButton->setDisabled(false);
}

void CMakeToolsWidget::cmakeToolsConfigureButtonBeforeProcessFinished()
{
    configureCMakeToolsButton->setDisabled(true);
    m_createCopyCheckBoxState = checkBoxCreateCopyOnSource->isChecked();

    if (cmakeToolsCheckIfImproper() == CMakeRunStatus::Failure) {
        configureCMakeToolsButton->setDisabled(false);
        return;
    }

    cmakeToolsCreateCompileCommandsJson();
}

void CMakeToolsWidget::cmakeToolsConfigureButtonAfterProcessFinished()
{
    const QString sourceCompileCommandsJsonpath = sourceDirectoryPath->text() + QStringLiteral("/compile_commands.json");
    const QString buildCompileCommandsJsonPath = buildDirectoryPath->lineEdit()->text() + QStringLiteral("/compile_commands.json");

    CMakeRunStatus createReturn;

    if (m_cmakeProcess->exitCode() != 0) {
        qProcessError();
        return;
    }

    createReturn = cmakeToolsCopyCommandsCompileJsonOnSourceFolder(sourceCompileCommandsJsonpath, buildCompileCommandsJsonPath, m_createCopyCheckBoxState);

    if (createReturn == CMakeRunStatus::Failure && m_createCopyCheckBoxState == true) {
        configureCMakeToolsButton->setDisabled(false);
        return;
    }
    configureCMakeToolsButton->setDisabled(false);
    saveWidgetSessionOnSourceToBuildMap(sourceDirectoryPath->text(), buildDirectoryPath->lineEdit()->text());
}
