/*
    SPDX-FileCopyrightText: 2022 Eric Armbruster <eric1@armbruster-online.de>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#ifndef LSP_DOCUMENT_LINKING_H
#define LSP_DOCUMENT_LINKING_H

#include <QHash>
#include <QObject>
#include <QPointer>

#include <memory>

namespace KTextEditor
{
class MainWindow;
class View;
class Range;
}

struct LSPDocumentLink;
class LSPClientServerManager;

class DocumentLinker : public QObject
{
    Q_OBJECT

public:
    DocumentLinker(QSharedPointer<LSPClientServerManager> serverManager, KTextEditor::MainWindow *mainWindow, QObject *parent = nullptr);

    void processDocumentLinks(const QList<LSPDocumentLink> &docLinks);

    LSPDocumentLink getLink(const KTextEditor::Range &range);

    bool hasLink(const KTextEditor::Range &range);

private Q_SLOTS:
    void viewChanged(KTextEditor::View *view);

private:
    QHash<KTextEditor::Range, LSPDocumentLink> m_docLinks;

    QSharedPointer<LSPClientServerManager> m_serverManager;

    KTextEditor::MainWindow *m_mainWindow;
};

#endif
