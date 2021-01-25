/* This file is part of the KDE project
   SPDX-FileCopyrightText: 2001 Christoph Cullmann <cullmann@kde.org>
   SPDX-FileCopyrightText: 2001 Joseph Wenninger <jowenn@kde.org>
   SPDX-FileCopyrightText: 2001, 2005 Anders Lund <anders.lund@lund.tdcadsl.dk>

   SPDX-License-Identifier: LGPL-2.0-only
*/
#include "kateviewspace.h"

#include "kateapp.h"
#include "katedebug.h"
#include "katedocmanager.h"
#include "katefileactions.h"
#include "katemainwindow.h"
#include "katesessionmanager.h"
#include "kateupdatedisabler.h"
#include "kateviewmanager.h"
#include <KActionCollection>

#include <KAcceleratorManager>
#include <KConfigGroup>
#include <KLocalizedString>

#include <QApplication>
#include <QClipboard>
#include <QHelpEvent>
#include <QMenu>
#include <QMessageBox>
#include <QStackedWidget>
#include <QToolButton>
#include <QToolTip>
#include <QWhatsThis>

// BEGIN KateViewSpace
KateViewSpace::KateViewSpace(KateViewManager *viewManager, QWidget *parent, const char *name)
    : QWidget(parent)
    , m_viewManager(viewManager)
    , m_isActiveSpace(false)
{
    setObjectName(QString::fromLatin1(name));
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setSpacing(0);
    layout->setContentsMargins(0, 0, 0, 0);

    // BEGIN tab bar
    QHBoxLayout *hLayout = new QHBoxLayout();
    hLayout->setSpacing(0);
    hLayout->setContentsMargins(0, 0, 0, 0);

    // add tab bar
    m_tabBar = new KateTabBar(this);
    connect(m_tabBar, &KateTabBar::currentChanged, this, &KateViewSpace::changeView);
    connect(m_tabBar, &KateTabBar::tabCloseRequested, this, &KateViewSpace::closeTabRequest, Qt::QueuedConnection);
    connect(m_tabBar, &KateTabBar::contextMenuRequest, this, &KateViewSpace::showContextMenu, Qt::QueuedConnection);
    connect(m_tabBar, &KateTabBar::newTabRequested, this, &KateViewSpace::createNewDocument);
    connect(m_tabBar, SIGNAL(activateViewSpaceRequested()), this, SLOT(makeActive()));
    hLayout->addWidget(m_tabBar);

    // add quick open
    m_quickOpen = new QToolButton(this);
    m_quickOpen->setAutoRaise(true);
    KAcceleratorManager::setNoAccel(m_quickOpen);
    m_quickOpen->installEventFilter(this); // on click, active this view space
    hLayout->addWidget(m_quickOpen);

    // forward tab bar quick open action to globa quick open action
    QAction *bridge = new QAction(QIcon::fromTheme(QStringLiteral("tab-duplicate")), i18nc("indicator for more documents", "+%1", 100), this);
    m_quickOpen->setDefaultAction(bridge);
    QAction *quickOpen = m_viewManager->mainWindow()->actionCollection()->action(QStringLiteral("view_quick_open"));
    Q_ASSERT(quickOpen);
    bridge->setToolTip(quickOpen->toolTip());
    bridge->setWhatsThis(i18n("Click here to switch to the Quick Open view."));
    connect(bridge, &QAction::triggered, quickOpen, &QAction::trigger);

    // add vertical split view space
    m_split = new QToolButton(this);
    m_split->setAutoRaise(true);
    m_split->setPopupMode(QToolButton::InstantPopup);
    m_split->setIcon(QIcon::fromTheme(QStringLiteral("view-split-left-right")));
    m_split->addAction(m_viewManager->mainWindow()->actionCollection()->action(QStringLiteral("view_split_vert")));
    m_split->addAction(m_viewManager->mainWindow()->actionCollection()->action(QStringLiteral("view_split_horiz")));
    m_split->addAction(m_viewManager->mainWindow()->actionCollection()->action(QStringLiteral("view_close_current_space")));
    m_split->addAction(m_viewManager->mainWindow()->actionCollection()->action(QStringLiteral("view_close_others")));
    m_split->addAction(m_viewManager->mainWindow()->actionCollection()->action(QStringLiteral("view_hide_others")));
    m_split->setWhatsThis(i18n("Control view space splitting"));
    m_split->installEventFilter(this); // on click, active this view space
    hLayout->addWidget(m_split);

    layout->addLayout(hLayout);
    // END tab bar

    stack = new QStackedWidget(this);
    stack->setFocus();
    stack->setSizePolicy(QSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Expanding));
    layout->addWidget(stack);

    m_group.clear();

    // connect signal to hide/show statusbar
    connect(m_viewManager->mainWindow(), &KateMainWindow::statusBarToggled, this, &KateViewSpace::statusBarToggled);
    connect(m_viewManager->mainWindow(), &KateMainWindow::tabBarToggled, this, &KateViewSpace::tabBarToggled);

    // init the bars...
    statusBarToggled();
    tabBarToggled();
}

bool KateViewSpace::eventFilter(QObject *obj, QEvent *event)
{
    QToolButton *button = qobject_cast<QToolButton *>(obj);

    // quick open button: show tool tip with shortcut
    if (button == m_quickOpen && event->type() == QEvent::ToolTip) {
        QHelpEvent *e = static_cast<QHelpEvent *>(event);
        QAction *quickOpen = m_viewManager->mainWindow()->actionCollection()->action(QStringLiteral("view_quick_open"));
        Q_ASSERT(quickOpen);
        QToolTip::showText(e->globalPos(), button->toolTip() + QStringLiteral(" (%1)").arg(quickOpen->shortcut().toString()), button);
        return true;
    }

    // quick open button: What's This
    if (button == m_quickOpen && event->type() == QEvent::WhatsThis) {
        QHelpEvent *e = static_cast<QHelpEvent *>(event);
        const int hiddenDocs = hiddenDocuments();
        QString helpText = (hiddenDocs == 0) ? i18n("Click here to switch to the Quick Open view.")
                                             : i18np("Currently, there is one more document open. To see all open documents, switch to the Quick Open view by clicking here.",
                                                     "Currently, there are %1 more documents open. To see all open documents, switch to the Quick Open view by clicking here.",
                                                     hiddenDocs);
        QWhatsThis::showText(e->globalPos(), helpText, m_quickOpen);
        return true;
    }

    // on mouse press on view space bar tool buttons: activate this space
    if (button && !isActiveSpace() && event->type() == QEvent::MouseButtonPress) {
        m_viewManager->setActiveSpace(this);
        if (currentView()) {
            m_viewManager->activateView(currentView()->document());
        }
    }
    return false;
}

void KateViewSpace::statusBarToggled()
{
    KateUpdateDisabler updatesDisabled(m_viewManager->mainWindow());
    for (auto view : qAsConst(m_docToView)) {
        view->setStatusBarEnabled(m_viewManager->mainWindow()->showStatusBar());
    }
}

void KateViewSpace::tabBarToggled()
{
    KateUpdateDisabler updatesDisabled(m_viewManager->mainWindow());
    m_tabBar->setVisible(m_viewManager->mainWindow()->showTabBar());
    m_split->setVisible(m_viewManager->mainWindow()->showTabBar());
    m_quickOpen->setVisible(m_viewManager->mainWindow()->showTabBar());
}

KTextEditor::View *KateViewSpace::createView(KTextEditor::Document *doc)
{
    // should only be called if a view does not yet exist
    Q_ASSERT(!m_docToView.contains(doc));

    /**
     * Create a fresh view
     */
    KTextEditor::View *v = doc->createView(stack, m_viewManager->mainWindow()->wrapper());

    // set status bar to right state
    v->setStatusBarEnabled(m_viewManager->mainWindow()->showStatusBar());

    // restore the config of this view if possible
    if (!m_group.isEmpty()) {
        QString fn = v->document()->url().toString();
        if (!fn.isEmpty()) {
            QString vgroup = QStringLiteral("%1 %2").arg(m_group, fn);

            KateSession::Ptr as = KateApp::self()->sessionManager()->activeSession();
            if (as->config() && as->config()->hasGroup(vgroup)) {
                KConfigGroup cg(as->config(), vgroup);
                v->readSessionConfig(cg);
            }
        }
    }

    // register document, it is shown below through showView() then
    registerDocument(doc);

    // view shall still be not registered
    Q_ASSERT(!m_docToView.contains(doc));

    // insert View into stack
    stack->addWidget(v);
    m_docToView[doc] = v;
    showView(v);

    return v;
}

void KateViewSpace::removeView(KTextEditor::View *v)
{
    // remove view mappings
    Q_ASSERT(m_docToView.contains(v->document()));
    m_docToView.remove(v->document());

    // ...and now: remove from view space
    stack->removeWidget(v);

    // switch to most recently used rather than letting stack choose one
    // (last element could well be v->document() being removed here)
    for (auto it = m_registeredDocuments.rbegin(); it != m_registeredDocuments.rend(); ++it) {
        if (m_docToView.contains(*it)) {
            showView(*it);
            break;
        }
    }
}

bool KateViewSpace::showView(KTextEditor::Document *document)
{
    /**
     * nothing can be done if we have now view ready here
     */
    if (!m_docToView.contains(document)) {
        return false;
    }

    /**
     * update mru list order
     */
    const int index = m_registeredDocuments.lastIndexOf(document);
    // move view to end of list
    if (index < 0) {
        return false;
    }
    // move view to end of list
    m_registeredDocuments.removeAt(index);
    m_registeredDocuments.append(document);

    /**
     * show the wanted view
     */
    KTextEditor::View *kv = m_docToView[document];
    stack->setCurrentWidget(kv);
    kv->show();

    /**
     * we need to avoid that below's index changes will mess with current view
     */
    disconnect(m_tabBar, &KateTabBar::currentChanged, this, &KateViewSpace::changeView);

    /**
     * follow current view
     */
    m_tabBar->setCurrentDocument(document);

    // track tab changes again
    connect(m_tabBar, &KateTabBar::currentChanged, this, &KateViewSpace::changeView);
    return true;
}

void KateViewSpace::changeView(int idx)
{
    if (idx == -1) {
        return;
    }

    KTextEditor::Document *doc = m_tabBar->tabDocument(idx);
    Q_ASSERT(doc);

    // make sure we open the view in this view space
    if (!isActiveSpace()) {
        m_viewManager->setActiveSpace(this);
    }

    // tell the view manager to show the view
    m_viewManager->activateView(doc);
}

KTextEditor::View *KateViewSpace::currentView()
{
    // might be 0 if the stack contains no view
    return static_cast<KTextEditor::View *>(stack->currentWidget());
}

bool KateViewSpace::isActiveSpace()
{
    return m_isActiveSpace;
}

void KateViewSpace::setActive(bool active)
{
    m_isActiveSpace = active;
    m_tabBar->setActive(active);
}

void KateViewSpace::makeActive(bool focusCurrentView)
{
    if (!isActiveSpace()) {
        m_viewManager->setActiveSpace(this);
        if (focusCurrentView && currentView()) {
            m_viewManager->activateView(currentView()->document());
        }
    }
    Q_ASSERT(isActiveSpace());
}

void KateViewSpace::registerDocument(KTextEditor::Document *doc)
{
    /**
     * ignore request to register something that is already known
     */
    if (m_registeredDocuments.contains(doc)) {
        return;
    }

    /**
     * remember our new document
     */
    m_registeredDocuments.insert(0, doc);

    /**
     * ensure we cleanup after document is deleted, e.g. we remove the tab bar button
     */
    connect(doc, &QObject::destroyed, this, &KateViewSpace::documentDestroyed);

    /**
     * register document is used in places that don't like view creation
     * therefore we must ensure the currentChanged doesn't trigger that
     */
    disconnect(m_tabBar, &KateTabBar::currentChanged, this, &KateViewSpace::changeView);

    /**
     * create the tab for this document, if necessary
     */
    m_tabBar->setCurrentDocument(doc);

    /**
     * handle later document state changes
     */
    connect(doc, &KTextEditor::Document::documentNameChanged, this, &KateViewSpace::updateDocumentName);
    connect(doc, &KTextEditor::Document::documentUrlChanged, this, &KateViewSpace::updateDocumentUrl);
    connect(doc, &KTextEditor::Document::modifiedChanged, this, &KateViewSpace::updateDocumentState);

    /**
     * allow signals again, now that the tab is there
     */
    connect(m_tabBar, &KateTabBar::currentChanged, this, &KateViewSpace::changeView);
}

void KateViewSpace::documentDestroyed(QObject *doc)
{
    /**
     * WARNING: this pointer is half destroyed
     * only good enough to check pointer value e.g. for hashs
     */
    KTextEditor::Document *invalidDoc = static_cast<KTextEditor::Document *>(doc);
    Q_ASSERT(m_registeredDocuments.contains(invalidDoc));
    m_registeredDocuments.removeAll(invalidDoc);

    /**
     * we shall have no views for this document at this point in time!
     */
    Q_ASSERT(!m_docToView.contains(invalidDoc));

    // disconnect entirely
    disconnect(doc, nullptr, this, nullptr);

    /**
     * remove the tab for this document, if existing
     */
    m_tabBar->removeDocument(invalidDoc);
}

void KateViewSpace::updateDocumentName(KTextEditor::Document *doc)
{
    // update tab button if available, might not be the case for tab limit set!
    const int buttonId = m_tabBar->documentIdx(doc);
    if (buttonId >= 0) {
        m_tabBar->setTabText(buttonId, doc->documentName());
    }
}

void KateViewSpace::updateDocumentUrl(KTextEditor::Document *doc)
{
    // update tab button if available, might not be the case for tab limit set!
    const int buttonId = m_tabBar->documentIdx(doc);
    if (buttonId >= 0) {
        m_tabBar->setTabToolTip(buttonId, doc->url().toDisplayString());
    }
}

void KateViewSpace::updateDocumentState(KTextEditor::Document *doc)
{
    QIcon icon;
    if (doc->isModified()) {
        icon = QIcon::fromTheme(QStringLiteral("document-save"));
    }

    // update tab button if available, might not be the case for tab limit set!
    const int buttonId = m_tabBar->documentIdx(doc);
    if (buttonId >= 0) {
        m_tabBar->setTabIcon(buttonId, icon);
    }
}

void KateViewSpace::closeTabRequest(int idx)
{
    auto *doc = m_tabBar->tabDocument(idx);
    Q_ASSERT(doc);
    m_viewManager->slotDocumentClose(doc);
}

void KateViewSpace::createNewDocument()
{
    // make sure we open the view in this view space
    if (!isActiveSpace()) {
        m_viewManager->setActiveSpace(this);
    }

    // create document
    KTextEditor::Document *doc = KateApp::self()->documentManager()->createDoc();

    // tell the view manager to show the document
    m_viewManager->activateView(doc);
}

void KateViewSpace::focusPrevTab()
{
    const int id = m_tabBar->prevTab();
    if (id >= 0) {
        changeView(id);
    }
}

void KateViewSpace::focusNextTab()
{
    const int id = m_tabBar->nextTab();
    if (id >= 0) {
        changeView(id);
    }
}

int KateViewSpace::hiddenDocuments() const
{
    const int hiddenDocs = KateApp::self()->documents().count() - m_tabBar->count();
    Q_ASSERT(hiddenDocs >= 0);
    return hiddenDocs;
}

void KateViewSpace::showContextMenu(int idx, const QPoint &globalPos)
{
    // right now, show no context menu on empty tab bar space
    if (idx < 0) {
        return;
    }

    auto *doc = m_tabBar->tabDocument(idx);
    Q_ASSERT(doc);

    auto addActionFromCollection = [this](QMenu *menu, const char *action_name) {
        QAction *action = m_viewManager->mainWindow()->action(action_name);
        return menu->addAction(action->icon(), action->text());
    };

    QMenu menu(this);
    QAction *aCloseTab = menu.addAction(QIcon::fromTheme(QStringLiteral("tab-close")), i18n("&Close Document"));
    QAction *aCloseOthers = menu.addAction(QIcon::fromTheme(QStringLiteral("tab-close-other")), i18n("Close Other &Documents"));
    menu.addSeparator();
    QAction *aCopyPath = addActionFromCollection(&menu, "file_copy_filepath");
    QAction *aOpenFolder = addActionFromCollection(&menu, "file_open_containing_folder");
    QAction *aFileProperties = addActionFromCollection(&menu, "file_properties");
    menu.addSeparator();
    QAction *aRenameFile = addActionFromCollection(&menu, "file_rename");
    QAction *aDeleteFile = addActionFromCollection(&menu, "file_delete");
    menu.addSeparator();
    QMenu *mCompareWithActive = new QMenu(i18n("Compare with active document"), &menu);
    mCompareWithActive->setIcon(QIcon::fromTheme(QStringLiteral("kompare")));
    menu.addMenu(mCompareWithActive);

    if (KateApp::self()->documentManager()->documentList().count() < 2) {
        aCloseOthers->setEnabled(false);
    }

    if (doc->url().isEmpty()) {
        aCopyPath->setEnabled(false);
        aOpenFolder->setEnabled(false);
        aRenameFile->setEnabled(false);
        aDeleteFile->setEnabled(false);
        aFileProperties->setEnabled(false);
        mCompareWithActive->setEnabled(false);
    }

    auto activeDocument = KTextEditor::Editor::instance()->application()->activeMainWindow()->activeView()->document(); // used for mCompareWithActive which is used with another tab which is not active
    // both documents must have urls and must not be the same to have the compare feature enabled
    if (activeDocument->url().isEmpty() || activeDocument == doc) {
        mCompareWithActive->setEnabled(false);
    }

    if (mCompareWithActive->isEnabled()) {
        for (auto &&diffTool : KateFileActions::supportedDiffTools()) {
            QAction *compareAction = mCompareWithActive->addAction(diffTool);
            compareAction->setData(diffTool);
        }
    }

    QAction *choice = menu.exec(globalPos);

    if (!choice) {
        return;
    }

    if (choice == aCloseTab) {
        closeTabRequest(idx);
    } else if (choice == aCloseOthers) {
        KateApp::self()->documentManager()->closeOtherDocuments(doc);
    } else if (choice == aCopyPath) {
        KateFileActions::copyFilePathToClipboard(doc);
    } else if (choice == aOpenFolder) {
        KateFileActions::openContainingFolder(doc);
    } else if (choice == aFileProperties) {
        KateFileActions::openFilePropertiesDialog(doc);
    } else if (choice == aRenameFile) {
        KateFileActions::renameDocumentFile(this, doc);
    } else if (choice == aDeleteFile) {
        KateFileActions::deleteDocumentFile(this, doc);
    } else if (choice->parent() == mCompareWithActive) {
        QString actionData = choice->data().toString(); // name of the executable of the diff program
        if (!KateFileActions::compareWithExternalProgram(activeDocument, doc, actionData)) {
            QMessageBox::information(this, i18n("Could not start program"), i18n("The selected program could not be started. Maybe it is not installed."), QMessageBox::StandardButton::Ok);
        }
    }
}

void KateViewSpace::saveConfig(KConfigBase *config, int myIndex, const QString &viewConfGrp)
{
    //   qCDebug(LOG_KATE)<<"KateViewSpace::saveConfig("<<myIndex<<", "<<viewConfGrp<<") - currentView: "<<currentView()<<")";
    QString groupname = QString(viewConfGrp + QStringLiteral("-ViewSpace %1")).arg(myIndex);

    // aggregate all views in view space (LRU ordered)
    QVector<KTextEditor::View *> views;
    QStringList lruList;
    for (KTextEditor::Document *doc : documentList()) {
        lruList << doc->url().toString();
        if (m_docToView.contains(doc)) {
            views.append(m_docToView[doc]);
        }
    }

    KConfigGroup group(config, groupname);
    group.writeEntry("Documents", lruList);
    group.writeEntry("Count", views.count());

    if (currentView()) {
        group.writeEntry("Active View", currentView()->document()->url().toString());
    }

    // Save file list, including cursor position in this instance.
    int idx = 0;
    for (QVector<KTextEditor::View *>::iterator it = views.begin(); it != views.end(); ++it) {
        if (!(*it)->document()->url().isEmpty()) {
            group.writeEntry(QStringLiteral("View %1").arg(idx), (*it)->document()->url().toString());

            // view config, group: "ViewSpace <n> url"
            QString vgroup = QStringLiteral("%1 %2").arg(groupname, (*it)->document()->url().toString());
            KConfigGroup viewGroup(config, vgroup);
            (*it)->writeSessionConfig(viewGroup);
        }

        ++idx;
    }
}

void KateViewSpace::restoreConfig(KateViewManager *viewMan, const KConfigBase *config, const QString &groupname)
{
    KConfigGroup group(config, groupname);

    // workaround for the weird bug where the tabbar sometimes becomes invisible after opening a session via the session chooser dialog or the --start cmd option
    // TODO: Debug the actual reason for the bug. See https://invent.kde.org/utilities/kate/-/merge_requests/189
    m_tabBar->hide();
    m_tabBar->show();

    // set back bar status to configured variant
    tabBarToggled();

    // restore Document lru list so that all tabs from the last session reappear
    const QStringList lruList = group.readEntry("Documents", QStringList());
    for (int i = 0; i < lruList.size(); ++i) {
        // ignore non-existing documents
        if (auto doc = KateApp::self()->documentManager()->findDocument(QUrl(lruList[i]))) {
            registerDocument(doc);
        }
    }

    // restore active view properties
    const QString fn = group.readEntry("Active View");
    if (!fn.isEmpty()) {
        KTextEditor::Document *doc = KateApp::self()->documentManager()->findDocument(QUrl(fn));

        if (doc) {
            // view config, group: "ViewSpace <n> url"
            QString vgroup = QStringLiteral("%1 %2").arg(groupname, fn);
            KConfigGroup configGroup(config, vgroup);

            auto view = viewMan->createView(doc, this);
            if (view) {
                view->readSessionConfig(configGroup);
                m_tabBar->setCurrentDocument(doc);
            }
        }
    }

    // avoid empty view space
    if (m_docToView.isEmpty()) {
        viewMan->createView(KateApp::self()->documentManager()->documentList().first(), this);
    }

    m_group = groupname; // used for restroing view configs later
}
// END KateViewSpace
