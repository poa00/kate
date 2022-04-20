/*
    SPDX-FileCopyrightText: 2021 Waqar Ahmed <waqar.17a@gmail.com>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "cmaketoolsplugin.h"
#include "cmaketoolswidget.h"

#include <KPluginFactory>
#include <KTextEditor/MainWindow>
#include <KXMLGUIFactory>
#include <ktexteditor/codecompletioninterface.h>

#include <QDebug>

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
    m_toolview.reset(mainwindow->createToolView(plugin,
                                                QStringLiteral("kate_private_plugin_katecmaketoolsplugin"),
                                                KTextEditor::MainWindow::Bottom,
                                                QIcon::fromTheme(QStringLiteral("cmake")),
                                                i18n("CMake")));

    KConfigGroup config(KSharedConfig::openConfig(), "cmake-tools");
    m_widget = new CMakeToolsWidget(mainwindow, m_toolview.get());

    connect(m_mainWindow, &KTextEditor::MainWindow::viewCreated, this, &CMakeToolsPluginView::onViewCreated);
    connect(m_mainWindow, &KTextEditor::MainWindow::unhandledShortcutOverride, this, &CMakeToolsPluginView::handleEsc);

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

void CMakeToolsPluginView::readSessionConfig(const KConfigGroup &cg)
{
    QMap<QString, QStringList> tempSourceToBuildMap;
    QStringList tempKeyList = cg.keyList();

    for (int i = 0; i < tempKeyList.size(); i++) {
        tempSourceToBuildMap[tempKeyList[i]] = cg.readEntry(tempKeyList[i], QStringList());
    }
    m_widget->setSourceToBuildMap(tempSourceToBuildMap);
}

void CMakeToolsPluginView::writeSessionConfig(KConfigGroup &cg)
{
    const QMap<QString, QStringList> tempQMap = m_widget->getSourceToBuildMap();

    for (auto iter = tempQMap.constBegin(); iter != tempQMap.constEnd(); ++iter) {
        cg.writeEntry(iter.key(), iter.value());
    }
    cg.sync();
}

void CMakeToolsPluginView::onViewCreated(KTextEditor::View *v)
{
    if (!CMakeCompletion::isCMakeFile(v->document()->url()))
        return;

    qWarning() << "Registering code completion model for view <<" << v << v->document()->url();

    KTextEditor::CodeCompletionInterface *cci = qobject_cast<KTextEditor::CodeCompletionInterface *>(v);
    if (cci) {
        cci->registerCompletionModel(&m_completion);
    }
}

void CMakeToolsPluginView::handleEsc(QEvent *e)
{
    if (!m_mainWindow) {
        return;
    }

    QKeyEvent *k = static_cast<QKeyEvent *>(e);

    if (k->key() == Qt::Key_Escape && k->modifiers() == Qt::NoModifier) {
        if (m_toolview->isVisible()) {
            m_mainWindow->hideToolView(m_toolview.get());
        }
    }
}

#include "cmaketoolsplugin.moc"
