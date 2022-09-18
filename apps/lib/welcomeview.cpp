/*
    SPDX-FileCopyrightText: 2021 Jiří Wolker <woljiri@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "welcomeview.h"

#include <KConfigGroup>
#include <KIconLoader>
#include <KSharedConfig>

#include <QAction>
#include <QClipboard>
#include <QDesktopServices>
#include <QDir>
#include <QGraphicsOpacityEffect>
#include <QGuiApplication>
#include <QMenu>
#include <QResizeEvent>
#include <QStyledItemDelegate>
#include <QVBoxLayout>

#include <klocalizedstring.h>

#include "kateapp.h"
#include "kateviewspace.h"
#include "recentitemsmodel.h"

class RecentsListItemDelegate : public QStyledItemDelegate
{
    Q_OBJECT

public:
    explicit RecentsListItemDelegate(WelcomeView *welcomeView)
        : m_welcomeView(welcomeView)
    {
    }

    WelcomeView *welcomeView() const
    {
        return m_welcomeView;
    }

    bool editorEvent(QEvent *event, QAbstractItemModel *aModel, const QStyleOptionViewItem &styleOptionViewItem, const QModelIndex &index) override
    {
        const RecentItemsModel *model = static_cast<RecentItemsModel *>(aModel);
        const RecentItemsModel::RecentItem *item = model->getItem(index);

        bool willOpenMenu = false;
        QPoint menuPosition;

        if (item != nullptr) {
            if (event->type() == QEvent::ContextMenu) {
                willOpenMenu = true;
                menuPosition = static_cast<QContextMenuEvent *>(event)->globalPos();
            }
            if (event->type() == QEvent::MouseButtonPress) {
                if (static_cast<QMouseEvent *>(event)->button() == Qt::MouseButton::RightButton) {
                    willOpenMenu = true;
                    menuPosition = static_cast<QMouseEvent *>(event)->globalPos();
                }
            }

            if (willOpenMenu) {
                event->accept();

                QMenu menu;

                QAction *copyPathAction = new QAction(i18n("&Copy Location"));
                copyPathAction->setIcon(QIcon::fromTheme(QStringLiteral("edit-copy-path")));
                connect(copyPathAction, &QAction::triggered, this, [item]() {
                    QString path;
                    if (item->url.isLocalFile()) {
                        path = item->url.toLocalFile();
                    } else {
                        path = item->url.toString();
                    }
                    QGuiApplication::clipboard()->setText(path);
                });
                menu.addAction(copyPathAction);

                QAction *showDirectoryAction = new QAction(i18n("&Open Containing Folder"));
                showDirectoryAction->setIcon(QIcon::fromTheme(QStringLiteral("document-open-folder")));
                connect(showDirectoryAction, &QAction::triggered, this, [item]() {
                    if (item->url.isLocalFile()) {
                        QFileInfo fileInfo(item->url.toLocalFile());
                        QDir parentDir = fileInfo.dir();
                        QUrl parentDirUrl = QUrl::fromLocalFile(parentDir.absolutePath());
                        QDesktopServices::openUrl(parentDirUrl);
                    }
                });
                menu.addAction(showDirectoryAction);
                if (!item->url.isLocalFile()) {
                    showDirectoryAction->setEnabled(false);
                }

                QAction *forgetItemAction = new QAction(i18nc("recent items context menu", "&Forget This Item"));
                forgetItemAction->setIcon(QIcon::fromTheme(QStringLiteral("edit-clear-history")));
                connect(forgetItemAction, &QAction::triggered, this, [this, item]() {
                    Q_EMIT welcomeView()->forgetRecentItem(item->url);
                });
                menu.addAction(forgetItemAction);

                menu.exec(menuPosition);

                return true;
            }
        }

        return QStyledItemDelegate::editorEvent(event, aModel, styleOptionViewItem, index);
    }

private:
    WelcomeView *m_welcomeView;
};

WelcomeView::WelcomeView(QWidget *parent)
    : QWidget(parent)
    , m_recentsModel(new RecentItemsModel)
    , m_recentsItemDelegate(new RecentsListItemDelegate(this))
{
    Q_ASSERT(parent);

    setupUi(this);

    setWindowTitle(i18n("Welcome"));

    // ensure proper title and icon for both applications
    welcomeLabel->setText(KateApp::isKate() ? i18n("Welcome to Kate") : i18n("Welcome to KWrite"));
    appIcon->setPixmap(qApp->windowIcon().pixmap(KIconLoader::SizeEnormous, KIconLoader::SizeEnormous));

    connect(openButton, &QPushButton::clicked, this, &WelcomeView::openClicked);
    connect(newButton, &QPushButton::clicked, this, &WelcomeView::newClicked);

    recentsListView->setContextMenuPolicy(Qt::DefaultContextMenu);
    recentsListView->setModel(m_recentsModel);
    recentsListView->setItemDelegate(m_recentsItemDelegate);
    connect(recentsListView, &QListView::activated, this, &WelcomeView::recentsItemActivated);

    connect(m_recentsModel, &RecentItemsModel::layoutChanged, this, &WelcomeView::recentListChanged);

    QVBoxLayout *noRecentsLayout = new QVBoxLayout(recentsListView);
    recentsListView->setLayout(noRecentsLayout);
    m_noRecentsLabel = new QLabel(recentsListView);
    QFont placeholderLabelFont;
    // To match the size of a level 2 Heading/KTitleWidget
    placeholderLabelFont.setPointSize(qRound(placeholderLabelFont.pointSize() * 1.3));
    noRecentsLayout->addWidget(m_noRecentsLabel);
    m_noRecentsLabel->setFont(placeholderLabelFont);
    m_noRecentsLabel->setTextInteractionFlags(Qt::NoTextInteraction);
    m_noRecentsLabel->setWordWrap(true);
    m_noRecentsLabel->setAlignment(Qt::AlignCenter);
    m_noRecentsLabel->setText(i18nc("on welcome screen", "No recent documents"));
    // Match opacity of QML placeholder label component
    auto *effect = new QGraphicsOpacityEffect(m_noRecentsLabel);
    effect->setOpacity(0.5);
    m_noRecentsLabel->setGraphicsEffect(effect);

    connect(forgetAllButton, &QToolButton::clicked, this, &WelcomeView::forgetAllRecents);
}

WelcomeView::~WelcomeView()
{
    delete m_recentsModel;
    delete m_recentsItemDelegate;
}

void WelcomeView::loadRecents()
{
    m_recentsModel->loadEntries(KSharedConfig::openConfig()->group("Recent Files"));
}

int WelcomeView::recentsCount()
{
    return m_recentsModel->rowCount();
}

void WelcomeView::recentsItemActivated(const QModelIndex &index)
{
    const RecentItemsModel::RecentItem *item = m_recentsModel->getItem(index);
    if (item != nullptr) {
        Q_EMIT recentItemClicked(item->url);
    }
}

void WelcomeView::recentListChanged()
{
    if (recentsCount() == 0) {
        m_noRecentsLabel->show();
        forgetAllButton->setEnabled(false);
    } else {
        m_noRecentsLabel->hide();
        forgetAllButton->setEnabled(true);
    }
}

#include "welcomeview.moc"