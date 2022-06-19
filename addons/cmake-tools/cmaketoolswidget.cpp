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

#include <KLocalizedString>
#include <memory>

CMakeToolsWidget::CMakeToolsWidget(KTextEditor::MainWindow *mainwindow, QWidget *parent)
    : QWidget(parent)
    , m_mainWindow(mainwindow)
{
    setupUi(this);
    m_cmakeProcess = std::make_unique<QProcess>(new QProcess);
    m_cmakeProcess->setProcessChannelMode(QProcess::MergedChannels);
    cmakeExecutionLog->document()->setDefaultFont(KTextEditor::Editor::instance()->font());
    buildDirPath->lineEdit()->setPlaceholderText(QStringLiteral("Select build folder"));

    connect(m_mainWindow, &KTextEditor::MainWindow::viewChanged, this, &CMakeToolsWidget::guessCMakeListsFolder);
    connect(selectBuildFolderBtn, &QToolButton::clicked, this, &CMakeToolsWidget::selectBuildFolder);
    connect(configureCMakeToolsBtn, &QPushButton::clicked, this, &CMakeToolsWidget::onConfigureBtnClicked);
    connect(m_cmakeProcess.get(), &QProcess::readyReadStandardOutput, this, &CMakeToolsWidget::appendCMakeProcessOutput);
    connect(m_cmakeProcess.get(), &QProcess::finished, this, &CMakeToolsWidget::cmakeFinished);
    connect(m_cmakeProcess.get(), &QProcess::errorOccurred, this, &CMakeToolsWidget::qProcessError);
}

CMakeToolsWidget::~CMakeToolsWidget() = default;

void CMakeToolsWidget::setSourceToBuildMap(const QMap<QString, QStringList> &readedQMap)
{
    m_sourceToBuildMap = readedQMap;
}

QMap<QString, QStringList> CMakeToolsWidget::getSourceToBuildMap()
{
    return m_sourceToBuildMap;
}

void CMakeToolsWidget::loadWidgetSession(const QString &sourcePath)
{
    buildDirPath->clear();

    buildDirPath->insertItems(0, m_sourceToBuildMap[sourcePath]);
}
void CMakeToolsWidget::saveWidgetSession(const QString &sourcePath, const QString &buildPath)
{
    if (m_sourceToBuildMap[sourcePath].contains(buildPath)) {
        return;
    }
    m_sourceToBuildMap[sourcePath] << buildPath;

    loadWidgetSession(sourcePath);
}

void CMakeToolsWidget::guessCMakeListsFolder(KTextEditor::View *v)
{
    bool docIsEmpty = v->document()->url().isEmpty();
    sourceLabel->setVisible(docIsEmpty);
    sourceDirPath->setVisible(docIsEmpty);

    if (docIsEmpty) {
        return;
    }

    QString currentViewDocumentPath = v->document()->url().path();
    currentViewDocumentPath.truncate(currentViewDocumentPath.lastIndexOf(QLatin1Char('/')));

    if (!sourceDirPath->text().isEmpty() && currentViewDocumentPath.contains(sourceDirPath->text())) {
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

    QString guessedPath = previousDirectory.canonicalPath();

    sourceDirPath->setText(guessedPath);
    searchForBuildDirectories();
}

bool CMakeToolsWidget::hasDirCmakeCache(const QString &buildPathToCheck, bool errorMessage)
{
    QString buildCMakeCachetxtPath = buildPathToCheck + QStringLiteral("/CMakeCache.txt");

    if (!QFileInfo::exists(buildCMakeCachetxtPath)) {
        if (!errorMessage) {
            return false;
        }
        QMessageBox::warning(this, i18n("Warning"), i18n("Folder not selected or the file CMakeCache.txt is not present on the folder selected"));
        return false;
    }

    return true;
}

QString CMakeToolsWidget::getSourceDirFromCMakeCache()
{
    if (!hasDirCmakeCache(buildDirPath->lineEdit()->text())) {
        return QStringLiteral("");
    }

    QFile openCMakeCache(buildDirPath->lineEdit()->text() + QStringLiteral("/CMakeCache.txt"));
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

void CMakeToolsWidget::searchForBuildDirectories()
{
    QStringList possibleBuildPaths = QDir(sourceDirPath->text()).entryList(QDir::Dirs | QDir::NoDotAndDotDot);

    for (const QString &possibleBuildPath : possibleBuildPaths) {
        QString path = sourceDirPath->text() + QStringLiteral("/") + possibleBuildPath;

        if (hasDirCmakeCache(path, false)) {
            saveWidgetSession(sourceDirPath->text(), path);
        }
    }
}

void CMakeToolsWidget::selectBuildFolder()
{
    QString startingDir = QDir::homePath();
    if (sourceDirPath->text() != QStringLiteral("Source")) {
        startingDir = sourceDirPath->text();
    }

    const QString selectedBuildPath = QFileDialog::getExistingDirectory(this, i18n("Get build folder"), startingDir, QFileDialog::ShowDirsOnly);

    if (selectedBuildPath.isEmpty()) {
        return;
    }

    buildDirPath->lineEdit()->setText(selectedBuildPath);
    sourceDirPath->setText(getSourceDirFromCMakeCache());
    searchForBuildDirectories();
    loadWidgetSession(sourceDirPath->text());
}

void CMakeToolsWidget::appendCMakeProcessOutput()
{
    QString convertedOutput = QString::fromUtf8(m_cmakeProcess->readAllStandardOutput());
    cmakeExecutionLog->appendPlainText(convertedOutput);
}

bool CMakeToolsWidget::isCmakeToolsValid()
{
    if (sourceDirPath->text().isEmpty()) {
        QMessageBox::warning(this, i18n("Source folder path empty"), i18n("Please select a valid path for the source folder by opening a non-empty document."));
        return false;
    }

    if (buildDirPath->lineEdit()->text().isEmpty()) {
        QMessageBox::warning(this, i18n("Build folder path empty"), i18n("Please select a valid path for the build folder."));
        return false;
    }

    return hasDirCmakeCache(buildDirPath->lineEdit()->text());
}

CMakeRunStatus CMakeToolsWidget::copyCompileCommandsToSource(const QString &sourceCompileCommandsPath, const QString &buildCompileCommandsPath)
{
    if (QFileInfo::exists(sourceCompileCommandsPath)) {
        QFile(sourceCompileCommandsPath).remove();
    }

    QFile oldCompileCommands(buildCompileCommandsPath);

    if (!oldCompileCommands.copy(sourceCompileCommandsPath)) {
        QMessageBox::warning(this, i18n("Warning"), i18n("Failed to copy in ") + i18n(sourceDirPath->text().toStdString().c_str()));
        return CMakeRunStatus::Failure;
    }

    return CMakeRunStatus::Success;
}

void CMakeToolsWidget::qProcessError(QProcess::ProcessError error)
{
    Q_UNUSED(error);

    QMessageBox::warning(this, i18n("Warning"), i18n("Failed to generate the compile_commands.json file"));
    configureCMakeToolsBtn->setDisabled(false);
}

void CMakeToolsWidget::onConfigureBtnClicked()
{
    configureCMakeToolsBtn->setDisabled(true);
    m_createCopyCheckBoxState = checkBoxCreateCopyOnSource->isChecked();

    if (!isCmakeToolsValid()) {
        configureCMakeToolsBtn->setDisabled(false);
        return;
    }

    m_cmakeProcess->setWorkingDirectory(buildDirPath->lineEdit()->text());
    m_cmakeProcess->start(QStringLiteral("cmake"), QStringList{buildDirPath->lineEdit()->text(), QStringLiteral("-DCMAKE_EXPORT_COMPILE_COMMANDS=ON")});
}

void CMakeToolsWidget::cmakeFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    Q_UNUSED(exitCode)

    const QString sourceCompileCommandsPath = sourceDirPath->text() + QStringLiteral("/compile_commands.json");
    const QString buildCompileCommandsPath = buildDirPath->lineEdit()->text() + QStringLiteral("/compile_commands.json");

    if (exitStatus == QProcess::CrashExit) {
        qProcessError(m_cmakeProcess->error());
        return;
    }

    if (m_createCopyCheckBoxState) {
        CMakeRunStatus copyStatus = copyCompileCommandsToSource(sourceCompileCommandsPath, buildCompileCommandsPath);

        if (copyStatus == CMakeRunStatus::Failure) {
            configureCMakeToolsBtn->setEnabled(true);
            return;
        }
    }

    configureCMakeToolsBtn->setEnabled(true);
    saveWidgetSession(sourceDirPath->text(), buildDirPath->lineEdit()->text());
}
