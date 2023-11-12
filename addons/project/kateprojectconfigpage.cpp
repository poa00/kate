/* This file is part of the KDE project

   SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "kateprojectconfigpage.h"
#include "kateprojectplugin.h"

#include <KLocalizedString>
#include <KUrlRequester>
#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QVBoxLayout>

KateProjectConfigPage::KateProjectConfigPage(QWidget *parent, KateProjectPlugin *plugin)
    : KTextEditor::ConfigPage(parent)
    , m_plugin(plugin)
{
    auto layout = new QFormLayout(this);
    layout->setFieldGrowthPolicy(QFormLayout::FieldsStayAtSizeHint);
    layout->setFormAlignment(Qt::AlignHCenter | Qt::AlignTop);
    layout->setContentsMargins(0, style()->pixelMetric(QStyle::PM_LayoutTopMargin), 0, 0);

    QGroupBox *group = new QGroupBox(QString(), this);
    group->setFlat(true);
    group->setContentsMargins(0, style()->pixelMetric(QStyle::PM_LayoutTopMargin), 0, 0);
    group->setWhatsThis(
        i18n("Project plugin is able to autoload repository working copies when "
             "there is no .kateproject file defined yet."));

    auto autoloadLayout = new QVBoxLayout(group);
    autoloadLayout->setContentsMargins({});

    m_cbAutoGit = new QCheckBox(i18n("&Git"), this);
    autoloadLayout->addWidget(m_cbAutoGit);

    m_cbAutoSubversion = new QCheckBox(i18n("&Subversion"), this);
    autoloadLayout->addWidget(m_cbAutoSubversion);
    m_cbAutoMercurial = new QCheckBox(i18n("&Mercurial"), this);
    autoloadLayout->addWidget(m_cbAutoMercurial);
    m_cbAutoFossil = new QCheckBox(i18n("&Fossil"), this);
    autoloadLayout->addWidget(m_cbAutoFossil);

    layout->addRow(i18nc("@label", "Autoload Repositories:"), group);

    m_cbSessionRestoreOpenProjects = new QCheckBox(i18n("Restore Open Projects"), this);
    layout->addRow(i18nc("@label", "Session Behavior:"), m_cbSessionRestoreOpenProjects);

    m_cbIndexEnabled = new QCheckBox(i18n("Enable indexing"), this);
    m_cbIndexEnabled->setWhatsThis(i18n("Project ctags index settings"));
    layout->addRow(i18nc("@label", "Project Index:"), m_cbIndexEnabled);

    m_indexPath = new KUrlRequester(this);
    m_indexPath->setMode(KFile::Directory | KFile::ExistingOnly | KFile::LocalOnly);
    m_indexPath->setToolTip(i18n("The system temporary directory is used if not specified, which may overflow for very large repositories"));
    m_indexPath->setMaximumWidth(300);
    layout->addRow(i18n("Directory for index files:"), m_indexPath);

    group = new QGroupBox(this);
    group->setFlat(true);
    group->setContentsMargins(0, style()->pixelMetric(QStyle::PM_LayoutTopMargin), 0, 0);
    auto vbox = new QVBoxLayout(group);
    vbox->setContentsMargins({});
    group->setWhatsThis(i18n("Project plugin is able to perform some operations across multiple projects"));
    m_cbMultiProjectCompletion = new QCheckBox(i18n("Cross-Project Completion"), this);
    vbox->addWidget(m_cbMultiProjectCompletion);
    m_cbMultiProjectGoto = new QCheckBox(i18n("Cross-Project Goto Symbol"), this);
    vbox->addWidget(m_cbMultiProjectGoto);
    layout->addRow(i18nc("@label", "Cross-Project Functionality:"), group);

    /** Git specific **/
    group = new QGroupBox(this);
    group->setFlat(true);

    auto grid = new QGridLayout(group);
    grid->setContentsMargins({});
    auto label = new QLabel(i18n("Single click action in the git status view"), this);
    m_cmbSingleClick = new QComboBox(this);
    m_cmbSingleClick->addItem(i18n("No Action"));
    m_cmbSingleClick->addItem(i18n("Show Diff"));
    m_cmbSingleClick->addItem(i18n("Open file"));
    m_cmbSingleClick->addItem(i18n("Stage / Unstage"));
    label->setBuddy(m_cmbSingleClick);
    grid->addWidget(label, 0, 0);
    grid->addWidget(m_cmbSingleClick, 0, 1);

    label = new QLabel(i18n("Double click action in the git status view"), this);
    m_cmbDoubleClick = new QComboBox(this);
    m_cmbDoubleClick->addItem(i18n("No Action"));
    m_cmbDoubleClick->addItem(i18n("Show Diff"));
    m_cmbDoubleClick->addItem(i18n("Open file"));
    m_cmbDoubleClick->addItem(i18n("Stage / Unstage"));
    label->setBuddy(m_cmbDoubleClick);
    grid->addWidget(label, 1, 0);
    grid->addWidget(m_cmbDoubleClick, 1, 1);

    layout->addRow(i18nc("@label", "Git:"), group);

    connect(m_cbAutoGit, &QCheckBox::stateChanged, this, &KateProjectConfigPage::slotMyChanged);
    connect(m_cbAutoSubversion, &QCheckBox::stateChanged, this, &KateProjectConfigPage::slotMyChanged);
    connect(m_cbAutoMercurial, &QCheckBox::stateChanged, this, &KateProjectConfigPage::slotMyChanged);
    connect(m_cbAutoFossil, &QCheckBox::stateChanged, this, &KateProjectConfigPage::slotMyChanged);
    connect(m_cbSessionRestoreOpenProjects, &QCheckBox::stateChanged, this, &KateProjectConfigPage::slotMyChanged);
    connect(m_cbIndexEnabled, &QCheckBox::stateChanged, this, &KateProjectConfigPage::slotMyChanged);
    connect(m_indexPath, &KUrlRequester::textChanged, this, &KateProjectConfigPage::slotMyChanged);
    connect(m_indexPath, &KUrlRequester::urlSelected, this, &KateProjectConfigPage::slotMyChanged);
    connect(m_cbMultiProjectCompletion, &QCheckBox::stateChanged, this, &KateProjectConfigPage::slotMyChanged);
    connect(m_cbMultiProjectGoto, &QCheckBox::stateChanged, this, &KateProjectConfigPage::slotMyChanged);

    connect(m_cmbSingleClick, QOverload<int>::of(&QComboBox::activated), this, &KateProjectConfigPage::slotMyChanged);
    connect(m_cmbDoubleClick, QOverload<int>::of(&QComboBox::activated), this, &KateProjectConfigPage::slotMyChanged);

    reset();
}

QString KateProjectConfigPage::name() const
{
    return i18n("Projects");
}

QString KateProjectConfigPage::fullName() const
{
    return i18nc("Groupbox title", "Projects Properties");
}

QIcon KateProjectConfigPage::icon() const
{
    return QIcon::fromTheme(QLatin1String("project-open"), QIcon::fromTheme(QLatin1String("view-list-tree")));
}

void KateProjectConfigPage::apply()
{
    if (!m_changed) {
        return;
    }

    m_changed = false;

    m_plugin->setAutoRepository(m_cbAutoGit->checkState() == Qt::Checked,
                                m_cbAutoSubversion->checkState() == Qt::Checked,
                                m_cbAutoMercurial->checkState() == Qt::Checked,
                                m_cbAutoFossil->checkState() == Qt::Checked);
    m_plugin->setIndex(m_cbIndexEnabled->checkState() == Qt::Checked, m_indexPath->url());
    m_plugin->setMultiProject(m_cbMultiProjectCompletion->checkState() == Qt::Checked, m_cbMultiProjectGoto->checkState() == Qt::Checked);

    m_plugin->setSingleClickAction((ClickAction)m_cmbSingleClick->currentIndex());
    m_plugin->setDoubleClickAction((ClickAction)m_cmbDoubleClick->currentIndex());

    m_plugin->setRestoreProjectsForSession(m_cbSessionRestoreOpenProjects->isChecked());
}

void KateProjectConfigPage::reset()
{
    m_cbAutoGit->setCheckState(m_plugin->autoGit() ? Qt::Checked : Qt::Unchecked);
    m_cbAutoSubversion->setCheckState(m_plugin->autoSubversion() ? Qt::Checked : Qt::Unchecked);
    m_cbAutoMercurial->setCheckState(m_plugin->autoMercurial() ? Qt::Checked : Qt::Unchecked);
    m_cbAutoFossil->setCheckState(m_plugin->autoFossil() ? Qt::Checked : Qt::Unchecked);
    m_cbIndexEnabled->setCheckState(m_plugin->getIndexEnabled() ? Qt::Checked : Qt::Unchecked);
    m_indexPath->setUrl(m_plugin->getIndexDirectory());
    m_cbMultiProjectCompletion->setCheckState(m_plugin->multiProjectCompletion() ? Qt::Checked : Qt::Unchecked);
    m_cbMultiProjectGoto->setCheckState(m_plugin->multiProjectGoto() ? Qt::Checked : Qt::Unchecked);

    m_cmbSingleClick->setCurrentIndex((int)m_plugin->singleClickAcion());
    m_cmbDoubleClick->setCurrentIndex((int)m_plugin->doubleClickAcion());

    m_cbSessionRestoreOpenProjects->setCheckState(m_plugin->restoreProjectsForSession() ? Qt::Checked : Qt::Unchecked);

    m_changed = false;
}

void KateProjectConfigPage::defaults()
{
    reset();
}

void KateProjectConfigPage::slotMyChanged()
{
    m_changed = true;
    Q_EMIT changed();
}

#include "moc_kateprojectconfigpage.cpp"
