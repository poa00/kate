/*
    SPDX-FileCopyrightText: 2021 Waqar Ahmed <waqar.17a@gmail.com>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/
#ifndef KATE_CMAKE_TOOLS_PLUGIN_H
#define KATE_CMAKE_TOOLS_PLUGIN_H

#include <KConfigGroup>
#include <KSharedConfig>
#include <KTextEditor/Plugin>
#include <KXMLGUIClient>
#include <ktexteditor/sessionconfiginterface.h>

#include <QVariant>
#include <QWidget>

#include "cmakecompletion.h"

/**
 * Plugin
 */

class CMakeToolsWidget;

class CMakeToolsPlugin : public KTextEditor::Plugin
{
    Q_OBJECT

public:
    explicit CMakeToolsPlugin(QObject *parent = nullptr, const QList<QVariant> & = QList<QVariant>());

    ~CMakeToolsPlugin() override;

    QObject *createView(KTextEditor::MainWindow *mainWindow) override;
};

/**
 * Plugin view
 */
class CMakeToolsPluginView : public QObject, public KXMLGUIClient, public KTextEditor::SessionConfigInterface
{
    Q_OBJECT
    Q_INTERFACES(KTextEditor::SessionConfigInterface)

public:
    CMakeToolsPluginView(CMakeToolsPlugin *plugin, KTextEditor::MainWindow *mainwindow);

    ~CMakeToolsPluginView() override;

    void readSessionConfig(const KConfigGroup &config) override;
    void writeSessionConfig(KConfigGroup &config) override;

private Q_SLOTS:
    void onViewCreated(KTextEditor::View *v);
    void handleEsc(QEvent *e);

private:
    KTextEditor::MainWindow *m_mainWindow;
    CMakeCompletion m_completion;
    CMakeToolsWidget *m_widget;
    std::unique_ptr<QWidget> m_toolview;
};

#endif
