/*
    SPDX-FileCopyrightText: 2022 Eric Armbruster <eric1@armbruster-online.de>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "documentlinking.h"

#include "lspclient_debug.h"
#include "lspclientprotocol.h"
#include "lspclientservermanager.h"

#include <KTextEditor/Document>
#include <KTextEditor/MainWindow>
#include <KTextEditor/Range>
#include <KTextEditor/View>

DocumentLinker::DocumentLinker(QSharedPointer<LSPClientServerManager> serverManager, KTextEditor::MainWindow *mainWindow, QObject *parent)
    : QObject(parent)
    , m_serverManager(std::move(serverManager))
    , m_mainWindow(mainWindow)
{
    connect(m_mainWindow, &KTextEditor::MainWindow::viewChanged, this, &DocumentLinker::viewChanged);
}

void DocumentLinker::viewChanged(KTextEditor::View *view)
{
    if (!view) {
        return;
    }

    auto server = m_serverManager->findServer(view);
    if (!server) {
        return;
    }

    const auto &caps = server->capabilities();
    if (!caps.documentLinkProvider.provider) {
        return;
    }

    auto h = [this](const QList<LSPDocumentLink> &reply) {
        if (reply.isEmpty()) {
            qCDebug(LSPCLIENT) << " no document links found";
            return;
        }

        processDocumentLinks(reply);
    };

    server->documentLink(view->document()->url(), this, h);
}

void DocumentLinker::processDocumentLinks(const QList<LSPDocumentLink> &docLinks)
{
    m_docLinks.clear();

    for (const auto &docLink : docLinks) {
        m_docLinks[docLink.range] = docLink;
    }
}

LSPDocumentLink DocumentLinker::getLink(const KTextEditor::Range &range)
{
    return m_docLinks[range];
}

bool DocumentLinker::hasLink(const KTextEditor::Range &range)
{
    return m_docLinks.contains(range);
}
