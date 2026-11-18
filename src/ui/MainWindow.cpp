#include "MainWindow.h"

#include "AppInfo.h"
#include "DetectDialog.h"
#include "SystemFoldersDialog.h"
#include "FolderDialog.h"
#include "FolderListView.h"
#include "FolderModel.h"
#include "Format.h"
#include "GroupListView.h"
#include "GroupModel.h"
#include "Icons.h"
#include "ItemsDialog.h"
#include "LoginItem.h"
#include "SettingsDialog.h"
#include "TrayIcon.h"
#include "TypesDialog.h"
#include "clean/KnownCaches.h"
#include "clean/Monitor.h"
#include "model/Store.h"

#include <QApplication>
#include <QClipboard>
#include <QCloseEvent>
#include <QDesktopServices>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QPushButton>
#include <QSettings>
#include <QSplitter>
#include <QStatusBar>
#include <QTimer>
#include <QToolBar>
#include <QToolButton>
#include <QUrl>
#include <QVBoxLayout>

#ifdef Q_OS_WIN
#include <windows.h>
// windows.h maps this to DocumentPropertiesW, which clashes with QIcon::ThemeIcon.
#undef DocumentProperties
#endif

namespace {
constexpr int MessageTimeoutMs = 5000;
constexpr int ErrorTimeoutMs = 15000;
constexpr int TrayAttempts = 30;
constexpr int TrayRetryMs = 2000;
constexpr int MaxFailuresShown = 10;

QString warningMark()
{
    return QStringLiteral("⚠ ");
}
} // namespace

MainWindow::MainWindow(Store *store, Monitor *monitor, QWidget *parent)
    : QMainWindow(parent)
    , m_store(store)
    , m_monitor(monitor)
    , m_model(new FolderModel(store, monitor, this))
    , m_list(new FolderListView(m_model, this))
    , m_groupModel(new GroupModel(store, monitor, this))
    , m_groups(new GroupListView(m_groupModel, this))
    , m_filter(new QLineEdit(this))
    , m_summary(new QLabel(this))
{
    setWindowTitle(AppInfo::displayName());
    setWindowIcon(Icons::appIcon());
    resize(900, 560);

    m_filter->setPlaceholderText(
        tr("Filter by name or path (%1)")
            .arg(QKeySequence(Qt::CTRL | Qt::Key_F).toString(QKeySequence::NativeText)));
    m_filter->setClearButtonEnabled(true);
    m_filter->installEventFilter(this);
    connect(m_filter, &QLineEdit::textChanged, this, [this](const QString &text) {
        const QUuid id = currentId();
        m_model->setFilter(text);
        m_list->selectId(id);
        updateFolderActions();
    });

    connect(m_list, &FolderListView::cleanRequested, this, &MainWindow::cleanFolder);
    connect(m_list, &FolderListView::editRequested, this, &MainWindow::editFolder);
    connect(m_list, &FolderListView::folderContextMenu, this, &MainWindow::showContextMenu);
    connect(m_list, &FolderListView::currentFolderChanged, this, &MainWindow::updateFolderActions);
    connect(m_list, &FolderListView::filesDropped, this, [this](const QStringList &paths) {
        for (const QString &p : paths)
            addFolderPath(p);
    });
    connect(m_groups, &GroupListView::selectionChangedIds, this, &MainWindow::onGroupsSelected);
    connect(m_groups, &GroupListView::groupContextMenu, this, &MainWindow::showGroupContextMenu);
    connect(m_groups, &GroupListView::deleteRequested, this, &MainWindow::removeGroups);
    connect(m_groups, &GroupListView::foldersDropped, this, &MainWindow::moveFoldersToGroup);
    connect(m_groups, &GroupListView::filesDropped, this,
            [this](const QStringList &paths, const QUuid &groupId) {
                for (const QString &p : paths)
                    addFolderPath(p, {}, {}, groupId);
            });
    connect(m_list, &FolderListView::typed, this, [this](const QString &text) {
        m_filter->setFocus(Qt::ShortcutFocusReason);
        m_filter->setText(m_filter->text() + text);
    });

    connect(m_store, &Store::changed, this, &MainWindow::updateFolderActions);
    connect(m_store, &Store::changed, this, &MainWindow::updateGroupActions);
    connect(m_store, &Store::changed, this, &MainWindow::updateSummary);
    connect(m_store, &Store::saveFailed, this, [this](const QString &error) {
        showMessage(tr("Could not save %1: %2")
                        .arg(QDir::toNativeSeparators(m_store->filePath()), error),
                    true);
    });
    connect(m_monitor, &Monitor::folderChanged, this, &MainWindow::updateFolderActions);
    connect(m_monitor, &Monitor::summaryChanged, this, &MainWindow::updateSummary);
    connect(m_monitor, &Monitor::cleaned, this, &MainWindow::onCleaned);
    connect(m_monitor, &Monitor::cleanRefused, this, [this](const QUuid &id, const QString &reason) {
        const Folder *f = m_store->folder(id);
        QMessageBox::warning(this, tr("Clean"),
                             tr("%1 was not cleaned. %2").arg(f ? f->name : QString(), reason));
    });

    // Groups on the left, the selected groups' folders on the right.
    auto *right = new QWidget(this);
    auto *layout = new QVBoxLayout(right);
#ifdef Q_OS_MACOS
    // The filter sits in the toolbar, as search fields do in Mac windows.
    layout->setContentsMargins(0, 10, 12, 12);
#else
    layout->setContentsMargins(0, 8, 8, 0);
    layout->addWidget(m_filter);
#endif
    layout->setSpacing(6);
    layout->addWidget(m_list, 1);

    auto *left = new QWidget(this);
    auto *leftLayout = new QVBoxLayout(left);
#ifdef Q_OS_MACOS
    leftLayout->setContentsMargins(12, 10, 0, 12);
#else
    leftLayout->setContentsMargins(8, 8, 0, 0);
#endif
    leftLayout->addWidget(m_groups);
    m_groups->setMinimumWidth(120);

    m_splitter = new QSplitter(Qt::Horizontal, this);
    m_splitter->setChildrenCollapsible(false);
    m_splitter->addWidget(left);
    m_splitter->addWidget(right);
    m_splitter->setStretchFactor(1, 1);
    m_splitter->setSizes({200, 700});
    setCentralWidget(m_splitter);
    statusBar()->addPermanentWidget(m_summary);

    createActions();
    createTray();
    if (m_groupModel->rowCount() > 0)
        m_groups->selectId(m_groupModel->idAt(0));
    onGroupsSelected();

    QSettings settings;
    restoreGeometry(settings.value(QStringLiteral("window/geometry")).toByteArray());
    m_splitter->restoreState(settings.value(QStringLiteral("window/splitter")).toByteArray());
}

MainWindow::~MainWindow()
{
    saveGeometryState();
}

void MainWindow::createActions()
{
    QMenu *file = menuBar()->addMenu(tr("&File"));
    QAction *add = file->addAction(tr("&Add folder…"), this, &MainWindow::addFolder);
    add->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_N));
    QAction *detect = file->addAction(tr("&Detect types in a folder…"), this,
                                      [this] { detectTypes(); });
    detect->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_D));
    QAction *system =
        file->addAction(tr("Add &system folders…"), this, &MainWindow::addSystemFolders);
    QMenu *addKnown = file->addMenu(tr("Add a &tool cache"));
    connect(addKnown, &QMenu::aboutToShow, this, [this, addKnown] {
        addKnown->clear();
        fillAddMenu(addKnown);
    });
    QAction *find = file->addAction(tr("&Filter"), this, [this] {
        m_filter->setFocus(Qt::ShortcutFocusReason);
        m_filter->selectAll();
    });
    find->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_F));
    m_addActions << add << detect << system << addKnown->menuAction();
    file->addSeparator();
    QAction *hide = file->addAction(tr("&Hide window"), this, &MainWindow::hideWindow);
    hide->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_W));
    QAction *quit = file->addAction(tr("&Quit"), qApp, &QApplication::quit);
    quit->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_Q));
    quit->setMenuRole(QAction::QuitRole);

    QMenu *folder = menuBar()->addMenu(tr("F&older"));
    m_cleanAction = folder->addAction(tr("&Clean…"), this, [this] { cleanFolder(currentId()); });
    m_cleanAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_K));
    QAction *rescan = folder->addAction(tr("&Rescan"), this, &MainWindow::rescanFolder);
    rescan->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_R));
    QAction *items = folder->addAction(tr("&Show what cleaning deletes…"), this,
                                       &MainWindow::showItems);
    items->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_I));
    folder->addSeparator();
    QAction *open = folder->addAction(tr("&Open folder"), this, [this] { openFolder(currentId()); });
    open->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_O));
    // The filter field keeps its own Copy: a focused line edit claims the shortcut first.
    QAction *copy = folder->addAction(tr("Copy &path"), this, [this] { copyPath(currentId()); });
    copy->setShortcut(QKeySequence(QKeySequence::Copy));
    folder->addSeparator();
    m_moveMenu = folder->addMenu(tr("Move to &group"));
    connect(m_moveMenu, &QMenu::aboutToShow, this, [this] {
        m_moveMenu->clear();
        fillMoveMenu(m_moveMenu, currentId());
    });
    folder->addSeparator();
    QAction *edit = folder->addAction(tr("&Edit…"), this, [this] { editFolder(currentId()); });
    edit->setShortcut(QKeySequence(Qt::Key_F2));
    QAction *remove = folder->addAction(tr("Re&move…"), this, &MainWindow::removeFolder);
    remove->setShortcuts({QKeySequence(QKeySequence::Delete), QKeySequence(Qt::CTRL | Qt::Key_Backspace)});
    QAction *up = folder->addAction(tr("Move &up"), this, [this] {
        const QUuid id = currentId();
        m_store->moveBy(id, -1);
        m_list->selectId(id);
    });
    up->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_Up));
    QAction *down = folder->addAction(tr("Move &down"), this, [this] {
        const QUuid id = currentId();
        m_store->moveBy(id, 1);
        m_list->selectId(id);
    });
    down->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_Down));
    m_folderActions << rescan << items << open << copy << m_moveMenu->menuAction() << edit << remove
                    << up << down;

    QMenu *groupMenu = menuBar()->addMenu(tr("&Group"));
    QAction *newGroup = groupMenu->addAction(tr("&New group"), this, &MainWindow::addGroup);
    newGroup->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_N));
    QAction *renameGroup =
        groupMenu->addAction(tr("&Rename"), this, [this] { m_groups->renameCurrent(); });
    QAction *deleteGroup = groupMenu->addAction(tr("&Delete…"), this, &MainWindow::removeGroups);
    groupMenu->addSeparator();
    m_cleanAllAction = groupMenu->addAction(tr("&Clean the selected groups…"), this,
                                            [this] { cleanAll(false); });
    m_cleanAllAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_K));
    QAction *rescanAll = groupMenu->addAction(tr("Re&scan the selected groups"), this,
                                              [this] { m_monitor->rescanGroups(selectedGroups()); });
    rescanAll->setShortcuts({QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_R), QKeySequence(Qt::Key_F5)});
    groupMenu->addSeparator();
    QAction *groupUp = groupMenu->addAction(tr("Move &up"), this, [this] { moveGroup(-1); });
    groupUp->setShortcut(QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_Up));
    QAction *groupDown = groupMenu->addAction(tr("Move do&wn"), this, [this] { moveGroup(1); });
    groupDown->setShortcut(QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_Down));
    m_groupActions << renameGroup << deleteGroup << rescanAll << groupUp << groupDown;

    QMenu *tools = menuBar()->addMenu(tr("&Tools"));
    m_cleanEverythingAction =
        tools->addAction(tr("Clean &everything…"), this, [this] { cleanAll(true); });
    tools->addAction(tr("Rescan e&verything"), m_monitor, &Monitor::rescanAll);
    tools->addSeparator();
    QAction *types = tools->addAction(tr("Cleaner &types…"), this, &MainWindow::editTypes);
    QAction *settings = tools->addAction(tr("&Settings…"), this, &MainWindow::editSettings);
    settings->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_Comma));
    settings->setMenuRole(QAction::PreferencesRole);

    QMenu *help = menuBar()->addMenu(tr("&Help"));
    QAction *about = help->addAction(tr("&About %1").arg(AppInfo::displayName()), this,
                                     &MainWindow::showAbout);
    about->setMenuRole(QAction::AboutRole);
    QAction *aboutQt = help->addAction(tr("About &Qt"), qApp, &QApplication::aboutQt);
    aboutQt->setMenuRole(QAction::AboutQtRole);

    add->setIcon(Icons::toolIcon(QIcon::ThemeIcon::FolderNew));
    edit->setIcon(Icons::toolIcon(QIcon::ThemeIcon::DocumentProperties));
    remove->setIcon(Icons::toolIcon(QIcon::ThemeIcon::ListRemove));
    rescan->setIcon(Icons::toolIcon(QIcon::ThemeIcon::ViewRefresh));
    m_cleanAction->setIcon(Icons::toolIcon(QIcon::ThemeIcon::EditClear));
    m_cleanAllAction->setIcon(Icons::toolIcon(QIcon::ThemeIcon::EditDelete));
    types->setIcon(Icons::toolIcon(QIcon::ThemeIcon::FormatJustifyLeft));
    settings->setIcon(Icons::toolIcon(QIcon::ThemeIcon::DocumentPageSetup));

    // The toolbar repeats the most used commands, with shorter labels than the menus.
    QToolBar *toolbar = addToolBar(tr("Toolbar"));
    toolbar->setObjectName(QStringLiteral("toolbar"));
    toolbar->setMovable(false);
    toolbar->setFloatable(false);
    toolbar->setToolButtonStyle(Qt::ToolButtonIconOnly);
    toolbar->setIconSize(QSize(16, 16));
    toolbar->setContextMenuPolicy(Qt::PreventContextMenu);
    const auto addButton = [toolbar](QAction *action, const QString &label) {
        // Icon-only buttons take their tooltip from the icon text.
        action->setIconText(label);
        toolbar->addAction(action);
        if (QWidget *button = toolbar->widgetForAction(action))
            button->setFocusPolicy(Qt::NoFocus);
    };
    addButton(add, tr("Add folder"));
    // The Add button also offers the tool caches found on this computer.
    if (auto *addButtonWidget = qobject_cast<QToolButton *>(toolbar->widgetForAction(add))) {
        auto *menu = new QMenu(addButtonWidget);
        connect(menu, &QMenu::aboutToShow, this, [this, menu] {
            menu->clear();
            menu->addAction(tr("Folder…"), this, &MainWindow::addFolder);
            menu->addAction(tr("Detect types in a folder…"), this, [this] { detectTypes(); });
            menu->addAction(tr("System folders…"), this, &MainWindow::addSystemFolders);
            menu->addSeparator();
            fillAddMenu(menu);
            menu->addSeparator();
            menu->addAction(tr("Group"), this, &MainWindow::addGroup);
        });
        addButtonWidget->setMenu(menu);
        addButtonWidget->setPopupMode(QToolButton::MenuButtonPopup);
    }
    addButton(edit, tr("Edit"));
    addButton(remove, tr("Remove"));
    toolbar->addSeparator();
    addButton(rescan, tr("Rescan"));
    addButton(m_cleanAction, tr("Clean"));
    addButton(m_cleanAllAction, tr("Clean the selected groups"));
    toolbar->addSeparator();
    addButton(types, tr("Cleaner types"));
    addButton(settings, tr("Settings"));

    // Tooltips name the command and its shortcut.
    for (QAction *action : toolbar->actions()) {
        if (action->isSeparator())
            continue;
        const auto updateTip = [action] {
            const QString keys = action->shortcut().toString(QKeySequence::NativeText);
            const QString tip = keys.isEmpty() ? action->iconText()
                                               : QStringLiteral("%1 (%2)").arg(action->iconText(), keys);
            if (action->toolTip() != tip)
                action->setToolTip(tip);
        };
        connect(action, &QAction::changed, this, updateTip);
        updateTip();
    }

#ifdef Q_OS_MACOS
    setUnifiedTitleAndToolBarOnMac(true);
    toolbar->setIconSize(QSize(18, 18));
    auto *spacer = new QWidget(toolbar);
    spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    toolbar->addWidget(spacer);
    // Narrow enough to fit a narrow window: the toolbar hides what does not fit.
    m_filter->setMinimumWidth(120);
    m_filter->setMaximumWidth(240);
    toolbar->addWidget(m_filter);
    // Keeps the field's focus ring off the window edge.
    auto *edge = new QWidget(toolbar);
    edge->setFixedWidth(6);
    toolbar->addWidget(edge);
#endif
}

void MainWindow::fillAddMenu(QMenu *menu)
{
    const QList<KnownCache> caches = KnownCaches::find();
    if (caches.isEmpty()) {
        menu->addAction(tr("No tool caches found"))->setEnabled(false);
        return;
    }
    for (const KnownCache &cache : caches) {
        const bool present = m_store->folderWithPath(cache.path) != nullptr;
        QAction *a = menu->addAction(
            tr("%1  (%2)").arg(cache.name, QDir::toNativeSeparators(cache.path)), this,
            [this, cache] { addFolderPath(cache.path, {BuiltInTypes::contentsId()}, cache.name); });
        a->setEnabled(!present);
    }
}

void MainWindow::createTray()
{
    if (!TrayIcon::isAvailable()) {
        // At sign-in the app can start before the taskbar; try again for a minute.
        if (++m_trayAttempts < TrayAttempts)
            QTimer::singleShot(TrayRetryMs, this, &MainWindow::createTray);
        return;
    }
    m_trayMenu = new QMenu(this);
    m_tray = new TrayIcon(Icons::trayIcon(), AppInfo::displayName(), m_trayMenu, this);
    connect(m_tray, &TrayIcon::triggered, this, &MainWindow::toggleVisible);
    updateTray();
    // Started hidden at sign-in: the app lives only in the tray. Checked once main() has had
    // the chance to show the window.
    QTimer::singleShot(0, this, [this] {
        if (!isVisible())
            m_tray->setDockIconVisible(false);
    });
}

void MainWindow::updateSummary()
{
    QStringList free;
    for (const Volume &v : m_monitor->volumes()) {
        free.append(tr("%1%2 free on %3")
                        .arg(v.low ? warningMark() : QString(), Format::bytes(v.freeBytes), v.name));
    }
    const QList<QUuid> groups = selectedGroups();
    const qint64 groupCleanable = m_monitor->groupCleanableBytes(groups);
    QString text;
    if (m_store->groups().size() > 1 && !groups.isEmpty()) {
        text = tr("Selected: %1, %2 to clean   ·   All: %3, %4 to clean")
                   .arg(Format::bytes(m_monitor->groupTotalBytes(groups)),
                        Format::bytes(groupCleanable), Format::bytes(m_monitor->totalBytes()),
                        Format::bytes(m_monitor->cleanableBytes()));
    } else {
        text = tr("Folders: %1, %2 to clean")
                   .arg(Format::bytes(m_monitor->totalBytes()),
                        Format::bytes(m_monitor->cleanableBytes()));
    }
    if (!free.isEmpty())
        text += QStringLiteral("   ·   ") + free.join(QStringLiteral(", "));
    m_summary->setText(text);
    m_cleanAllAction->setEnabled(groupCleanable > 0);
    m_cleanEverythingAction->setEnabled(m_monitor->cleanableBytes() > 0);

    // The tray menu and icon follow, at most once per event loop turn.
    if (!m_trayUpdatePending) {
        m_trayUpdatePending = true;
        QTimer::singleShot(0, this, [this] {
            m_trayUpdatePending = false;
            updateTray();
        });
    }
}

void MainWindow::updateTray()
{
    if (!m_tray)
        return;
    const Warnings warnings = m_monitor->warnings();

    // The menu: each group's folders, each disk, then the commands. With one group the folders
    // are listed directly; with several, each group is a submenu.
    m_trayMenu->clear();
    const bool submenus = m_store->groups().size() > 1;
    QHash<QUuid, QMenu *> groupMenus;
    for (const Group &g : m_store->groups()) {
        if (!submenus)
            break;
        const bool large = std::any_of(warnings.largeFolders.cbegin(), warnings.largeFolders.cend(),
                                       [this, &g](const QUuid &id) {
                                           const Folder *f = m_store->folder(id);
                                           return f && f->groupId == g.id;
                                       });
        QMenu *menu = m_trayMenu->addMenu(
            QStringLiteral("%1%2: %3").arg(
                large ? warningMark() : QString(), g.name,
                tr("%1, %2 to clean")
                    .arg(Format::bytes(m_monitor->groupTotalBytes({g.id})),
                         Format::bytes(m_monitor->groupCleanableBytes({g.id})))));
        groupMenus.insert(g.id, menu);
    }
    QList<QUuid> allGroups;
    for (const Group &g : m_store->groups())
        allGroups.append(g.id);
    for (const Folder &f : m_store->foldersIn(allGroups)) {
        const ScanResult r = m_monitor->result(f.id);
        const bool large = warnings.largeFolders.contains(f.id);
        QString detail;
        switch (m_monitor->state(f.id)) {
        case Monitor::State::Scanning:
            detail = tr("scanning…");
            break;
        case Monitor::State::Cleaning:
            detail = tr("cleaning…");
            break;
        case Monitor::State::Idle:
            if (!r.error.isEmpty())
                detail = r.error;
            else if (!r.valid)
                detail = tr("not scanned");
            else
                detail = tr("%1, %2 to clean")
                             .arg(Format::bytes(r.totalBytes), Format::bytes(r.cleanableBytes));
            break;
        }
        const QUuid id = f.id;
        QMenu *menu = submenus ? groupMenus.value(f.groupId) : m_trayMenu;
        menu->addAction(
            QStringLiteral("%1%2: %3").arg(large ? warningMark() : QString(), f.name, detail), this,
            [this, id] {
                showAndFocus();
                selectFolder(id);
            });
    }
    for (QMenu *menu : std::as_const(groupMenus)) {
        if (menu->isEmpty())
            menu->addAction(tr("No folders"))->setEnabled(false);
    }
    if (!m_store->folders().isEmpty() || submenus)
        m_trayMenu->addSeparator();
    QStringList tipLines{AppInfo::displayName()};
    tipLines.append(tr("Folders: %1, %2 to clean")
                        .arg(Format::bytes(m_monitor->totalBytes()),
                             Format::bytes(m_monitor->cleanableBytes())));
    for (const Volume &v : m_monitor->volumes()) {
        const QString line = tr("%1%2 free of %3 on %4")
                                 .arg(v.low ? warningMark() : QString(), Format::bytes(v.freeBytes),
                                      Format::bytes(v.totalBytes), v.name);
        m_trayMenu->addAction(line)->setEnabled(false);
        tipLines.append(line);
    }
    if (warnings.totalTooLarge)
        tipLines.append(warningMark() + tr("The folders hold more than %1.")
                                            .arg(Format::bytes(m_store->settings().totalWarnBytes)));
    for (const QUuid &id : warnings.largeFolders) {
        if (const Folder *f = m_store->folder(id))
            tipLines.append(warningMark() + tr("%1 holds more than %2.")
                                                .arg(f->name, Format::bytes(f->warnBytes)));
    }
    m_trayMenu->addSeparator();
    QAction *trayCleanAll = m_trayMenu->addAction(tr("Clean everything…"), this, [this] {
        showAndFocus();
        cleanAll(true);
    });
    trayCleanAll->setEnabled(m_monitor->cleanableBytes() > 0);
    m_trayMenu->addAction(tr("Rescan everything"), m_monitor, &Monitor::rescanAll);
    m_trayMenu->addSeparator();
    m_trayMenu->addAction(tr("Show %1").arg(AppInfo::displayName()), this, &MainWindow::showAndFocus);
    m_trayMenu->addAction(tr("Settings…"), this, [this] {
        showAndFocus();
        editSettings();
    });
    m_trayMenu->addSeparator();
    m_trayMenu->addAction(tr("Quit"), qApp, &QApplication::quit);

    m_tray->setIcon(Icons::trayIcon(warnings.any()));
    m_tray->setToolTip(tipLines.join(QLatin1Char('\n')));

    // Notify once when a new warning appears, while the window is hidden.
    QString warningText;
    if (!warnings.lowVolumes.isEmpty())
        warningText = tr("Free space is low on %1.").arg(warnings.lowVolumes.first().name);
    else if (warnings.totalTooLarge || !warnings.largeFolders.isEmpty())
        warningText = tr("Monitored folders are over their size limit.");
    if (warningText != m_lastWarning) {
        m_lastWarning = warningText;
        if (!warningText.isEmpty() && !isVisible())
            m_tray->showWarning(AppInfo::displayName(), warningText);
    }
}

QUuid MainWindow::currentId() const
{
    const QUuid id = m_list->currentId();
    return m_store->folder(id) ? id : QUuid();
}

QList<QUuid> MainWindow::selectedGroups() const
{
    QList<QUuid> ids;
    for (const QUuid &id : m_groups->selectedIds()) {
        if (m_store->group(id))
            ids.append(id);
    }
    return ids;
}

QUuid MainWindow::currentGroup() const
{
    const QUuid id = m_groups->currentId();
    if (m_store->group(id))
        return id;
    const QList<QUuid> selected = selectedGroups();
    return selected.isEmpty() ? QUuid() : selected.first();
}

void MainWindow::onGroupsSelected()
{
    const QUuid id = currentId();
    m_model->setGroups(selectedGroups());
    if (!id.isNull())
        m_list->selectId(id);
    updateFolderActions();
    updateGroupActions();
    updateSummary();
}

void MainWindow::updateGroupActions()
{
    const bool hasGroup = !currentGroup().isNull();
    for (QAction *action : std::as_const(m_groupActions))
        action->setEnabled(hasGroup);
    for (QAction *action : std::as_const(m_addActions))
        action->setEnabled(hasGroup);
}

void MainWindow::selectFolder(const QUuid &id)
{
    const Folder *f = m_store->folder(id);
    if (!f)
        return;
    if (!selectedGroups().contains(f->groupId))
        m_groups->selectId(f->groupId);
    m_filter->clear();
    m_list->selectId(id);
}

void MainWindow::updateFolderActions()
{
    const QUuid id = currentId();
    const bool hasFolder = !id.isNull();
    for (QAction *action : std::as_const(m_folderActions))
        action->setEnabled(hasFolder);
    const ScanResult r = m_monitor->result(id);
    m_cleanAction->setEnabled(hasFolder && m_monitor->state(id) == Monitor::State::Idle
                              && r.valid && r.cleanableBytes > 0);
}

void MainWindow::showAbout()
{
    const QString version = QApplication::applicationVersion();
    QMessageBox box(this);
    box.setWindowTitle(tr("About %1").arg(AppInfo::displayName()));
    box.setIconPixmap(Icons::appIcon().pixmap(64, 64));
    box.setTextFormat(Qt::RichText);
    box.setText(QStringLiteral("<h3>%1 %2</h3>")
                    .arg(AppInfo::displayName().toHtmlEscaped(), version.toHtmlEscaped()));
    box.setInformativeText(
        tr("Watches the space folders use, and cleans build outputs and caches.")
        + QStringLiteral("<br><br>")
        + QStringLiteral("Rangel Reale &lt;<a href=\"mailto:%1\">%1</a>&gt;<br>"
                         "<a href=\"%2\">%2</a>")
              .arg(QStringLiteral("realerangel@gmail.com"), AppInfo::homepage()));
    box.setStandardButtons(QMessageBox::Ok);
    box.exec();
}

void MainWindow::showAndFocus()
{
    if (m_tray)
        m_tray->setDockIconVisible(true);
    if (isMinimized())
        showNormal();
    else
        show();
    raise();
    activateWindow();
    m_list->focusFirst();
}

void MainWindow::toggleVisible()
{
#ifdef Q_OS_WIN
    // Qt's activation state can lag behind when another application took the foreground.
    const bool inFront = GetForegroundWindow() == reinterpret_cast<HWND>(winId());
#else
    const bool inFront = isActiveWindow();
#endif
    if (isVisible() && !isMinimized() && inFront)
        hideWindow();
    else
        showAndFocus();
}

void MainWindow::hideWindow()
{
    saveGeometryState();
    if (m_tray) {
        hide();
        m_tray->setDockIconVisible(false);
    } else {
        showMinimized();
    }
}

void MainWindow::saveGeometryState()
{
    QSettings settings;
    settings.setValue(QStringLiteral("window/geometry"), saveGeometry());
    settings.setValue(QStringLiteral("window/splitter"), m_splitter->saveState());
}

void MainWindow::showMessage(const QString &text, bool error)
{
    statusBar()->showMessage(text, error ? ErrorTimeoutMs : MessageTimeoutMs);
    if (error && m_tray && !isVisible())
        m_tray->showWarning(AppInfo::displayName(), text);
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    if (m_tray && m_store->settings().closeToTray) {
        hideWindow();
        event->ignore();
        return;
    }
    saveGeometryState();
    event->accept();
    QApplication::quit();
}

void MainWindow::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Escape && event->modifiers() == Qt::NoModifier) {
        if (!m_filter->text().isEmpty())
            m_filter->clear();
        else
            hideWindow();
        return;
    }
    QMainWindow::keyPressEvent(event);
}

bool MainWindow::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_filter && event->type() == QEvent::KeyPress) {
        auto *key = static_cast<QKeyEvent *>(event);
        switch (key->key()) {
        case Qt::Key_Down:
        case Qt::Key_Return:
        case Qt::Key_Enter:
            m_list->focusFirst();
            return true;
        case Qt::Key_Escape:
            if (!m_filter->text().isEmpty())
                m_filter->clear();
            else
                hideWindow();
            return true;
        default:
            break;
        }
    }
    return QMainWindow::eventFilter(watched, event);
}

void MainWindow::addFolder()
{
    const QUuid groupId = currentGroup();
    if (groupId.isNull()) {
        showMessage(tr("Add a group first."));
        return;
    }
    const QString dir = QFileDialog::getExistingDirectory(this, tr("Add folder"));
    if (dir.isEmpty())
        return;
    // New folders start with "Folder contents"; the dialog picks the type.
    Folder f;
    f.path = QDir::cleanPath(QDir::fromNativeSeparators(dir));
    f.name = QFileInfo(f.path).fileName();
    f.groupId = groupId;
    if (const Folder *existing = m_store->folderWithPath(f.path)) {
        selectFolder(existing->id);
        showMessage(tr("%1 is already in the list.").arg(existing->name));
        return;
    }
    FolderDialog dialog(m_store, f, this);
    if (dialog.exec() != QDialog::Accepted)
        return;
    const Folder chosen = dialog.folder();
    if (m_store->folderWithPath(chosen.path)) {
        addFolderPath(chosen.path);  // selects it
        return;
    }
    Folder added = chosen;
    added.groupId = groupId;
    const QUuid id = m_store->addFolder(added);
    selectFolder(id);
    m_list->setFocus();
}

void MainWindow::detectTypes(const QUuid &id)
{
    const Folder *existing = m_store->folder(id);
    DetectDialog dialog(m_store, existing ? existing->path : QString(), this);
    if (dialog.exec() != QDialog::Accepted)
        return;
    const QString path = dialog.path();
    const QStringList types = dialog.typeIds();
    // The dialog's folder may be another one already in the list.
    if (const Folder *f = m_store->folderWithPath(path)) {
        Folder updated = *f;
        updated.typeIds = types;
        m_store->updateFolder(updated);
        m_monitor->rescan(updated.id);
        selectFolder(updated.id);
        showMessage(tr("%1 now uses the detected types.").arg(updated.name));
        return;
    }
    addFolderPath(path, types);
}

void MainWindow::addSystemFolders()
{
    SystemFoldersDialog dialog(m_store, SystemFolders::find(), this);
    if (dialog.exec() != QDialog::Accepted)
        return;
    for (const SystemFolder &f : dialog.chosen())
        addFolderPath(f.path, {BuiltInTypes::contentsId()}, f.name);
}

void MainWindow::addFolderPath(const QString &path, const QStringList &typeIds, const QString &name,
                               const QUuid &groupId)
{
    const QString clean = QDir::cleanPath(QDir::fromNativeSeparators(path));
    if (const Folder *existing = m_store->folderWithPath(clean)) {
        const Group *g = m_store->group(existing->groupId);
        selectFolder(existing->id);
        showMessage(tr("%1 is already in the group %2.").arg(existing->name, g ? g->name : QString()));
        return;
    }
    Folder f;
    f.groupId = groupId.isNull() ? currentGroup() : groupId;
    if (!m_store->group(f.groupId)) {
        showMessage(tr("Add a group first."));
        return;
    }
    f.path = clean;
    f.name = name.isEmpty() ? QFileInfo(clean).fileName() : name;
    if (f.name.isEmpty())
        f.name = QDir::toNativeSeparators(clean);
    f.typeIds = typeIds.isEmpty() ? QStringList{BuiltInTypes::contentsId()} : typeIds;
    const QUuid id = m_store->addFolder(f);
    selectFolder(id);
    m_list->setFocus();
}

void MainWindow::editFolder(const QUuid &id)
{
    const Folder *f = m_store->folder(id);
    if (!f)
        return;
    FolderDialog dialog(m_store, *f, this);
    if (dialog.exec() != QDialog::Accepted)
        return;
    const Folder before = *m_store->folder(id);
    const Folder after = dialog.folder();
    m_store->updateFolder(after);
    if (before.path != after.path || before.typeIds != after.typeIds)
        m_monitor->rescan(id);
}

void MainWindow::removeFolder()
{
    const Folder *f = m_store->folder(currentId());
    if (!f)
        return;
    const QUuid id = f->id;
    const auto answer = QMessageBox::question(
        this, tr("Remove folder"),
        tr("Stop monitoring \"%1\"? Nothing in the folder is deleted.").arg(f->name));
    if (answer != QMessageBox::Yes)
        return;
    const int row = m_model->rowOf(id);
    m_store->removeFolder(id);
    if (m_model->rowCount() > 0)
        m_list->setCurrentIndex(m_model->index(qMin(row, m_model->rowCount() - 1)));
}

void MainWindow::rescanFolder()
{
    const QUuid id = currentId();
    if (!id.isNull())
        m_monitor->rescan(id);
}

void MainWindow::cleanFolder(const QUuid &id)
{
    const Folder *f = m_store->folder(id);
    if (!f || m_monitor->state(id) != Monitor::State::Idle)
        return;
    const ScanResult r = m_monitor->result(id);
    if (m_confirmCleaning) {
        const QList<CleanerType> types = m_store->typesFor(*f);
        const bool contents = std::any_of(types.cbegin(), types.cend(), [](const CleanerType &t) {
            return t.mode == CleanerType::Mode::Contents;
        });
        const QString what = contents
                                 ? tr("everything inside %1").arg(QDir::toNativeSeparators(f->path))
                                 : tr("%n folder(s) that its types match", nullptr,
                                      static_cast<int>(r.items.size()));
        QMessageBox box(QMessageBox::Warning, tr("Clean %1").arg(f->name),
                        tr("Delete %1, about %2? This cannot be undone.")
                            .arg(what, Format::bytes(r.cleanableBytes)),
                        QMessageBox::Cancel, this);
        QPushButton *details = box.addButton(tr("Show items…"), QMessageBox::HelpRole);
        QPushButton *ok = box.addButton(tr("Delete"), QMessageBox::DestructiveRole);
        box.setDefaultButton(QMessageBox::Cancel);
        // The Show items button would close the box; reopen it afterwards.
        for (;;) {
            box.exec();
            if (box.clickedButton() != details)
                break;
            ItemsDialog(f->name, f->path, r, this).exec();
        }
        if (box.clickedButton() != ok)
            return;
    }
    m_monitor->clean(id);
}

void MainWindow::cleanAll(bool everything)
{
    const QList<QUuid> groups = selectedGroups();
    const qint64 cleanable =
        everything ? m_monitor->cleanableBytes() : m_monitor->groupCleanableBytes(groups);
    if (cleanable <= 0)
        return;
    if (m_confirmCleaning) {
        QStringList lines;
        for (const Folder &f : everything ? m_store->folders() : m_store->foldersIn(groups)) {
            const ScanResult r = m_monitor->result(f.id);
            if (r.cleanableBytes > 0)
                lines.append(QStringLiteral("• %1: %2").arg(f.name, Format::bytes(r.cleanableBytes)));
        }
        const QString question =
            everything
                ? tr("Delete what every folder's types match, about %1? This cannot be undone.")
                : tr("Delete what the types of the selected groups' folders match, about %1? "
                     "This cannot be undone.");
        QMessageBox box(QMessageBox::Warning, everything ? tr("Clean everything") : tr("Clean groups"),
                        question.arg(Format::bytes(cleanable)), QMessageBox::Cancel, this);
        box.setInformativeText(lines.join(QLatin1Char('\n')));
        QPushButton *ok = box.addButton(tr("Delete"), QMessageBox::DestructiveRole);
        box.setDefaultButton(QMessageBox::Cancel);
        box.exec();
        if (box.clickedButton() != ok)
            return;
    }
    if (everything)
        m_monitor->cleanAll();
    else
        m_monitor->cleanGroups(groups);
}

void MainWindow::addGroup()
{
    // A name not taken yet: "New group", "New group 2", ...
    QString name = tr("New group");
    for (int n = 2;; ++n) {
        const bool taken = std::any_of(m_store->groups().cbegin(), m_store->groups().cend(),
                                       [&name](const Group &g) { return g.name == name; });
        if (!taken)
            break;
        name = tr("New group %1").arg(n);
    }
    const QUuid id = m_store->addGroup(name);
    m_groups->selectId(id);
    m_groups->setFocus();
    m_groups->renameCurrent();
}

void MainWindow::removeGroups()
{
    const QList<QUuid> ids = selectedGroups();
    if (ids.isEmpty())
        return;
    const int folders = static_cast<int>(m_store->foldersIn(ids).size());
    QString question;
    if (ids.size() == 1) {
        question = tr("Delete the group \"%1\" and stop monitoring its %n folder(s)? Nothing in the "
                      "folders is deleted.",
                      nullptr, folders)
                       .arg(m_store->group(ids.first())->name);
    } else {
        question = tr("Delete %1 groups and stop monitoring their %n folder(s)? Nothing in the "
                      "folders is deleted.",
                      nullptr, folders)
                       .arg(ids.size());
    }
    if (QMessageBox::question(this, tr("Delete group"), question) != QMessageBox::Yes)
        return;
    for (const QUuid &id : ids)
        m_store->removeGroup(id);
}

void MainWindow::moveGroup(int delta)
{
    const QUuid id = currentGroup();
    if (id.isNull())
        return;
    m_store->moveGroupBy(id, delta);
    m_groups->selectId(id);
}

void MainWindow::moveFoldersToGroup(const QList<QUuid> &folderIds, const QUuid &groupId)
{
    const Group *g = m_store->group(groupId);
    if (!g)
        return;
    const QString name = g->name;
    int moved = 0;
    for (const QUuid &id : folderIds) {
        const Folder *f = m_store->folder(id);
        if (!f || f->groupId == groupId)
            continue;
        m_store->moveToGroup(id, groupId);
        ++moved;
    }
    if (moved > 0)
        showMessage(tr("Moved %n folder(s) to %1.", nullptr, moved).arg(name));
}

void MainWindow::fillMoveMenu(QMenu *menu, const QUuid &folderId)
{
    const Folder *f = m_store->folder(folderId);
    if (!f)
        return;
    for (const Group &g : m_store->groups()) {
        const QUuid groupId = g.id;
        QAction *a = menu->addAction(g.name, this, [this, folderId, groupId] {
            moveFoldersToGroup({folderId}, groupId);
        });
        a->setCheckable(true);
        a->setChecked(groupId == f->groupId);
        a->setEnabled(groupId != f->groupId);
    }
}

void MainWindow::showGroupContextMenu(const QUuid &id, const QPoint &globalPos)
{
    QMenu menu(this);
    const bool hasGroup = m_store->group(id) != nullptr;
    const QList<QUuid> groups = selectedGroups();
    if (hasGroup) {
        menu.addAction(tr("&Clean…"), this, [this] { cleanAll(false); })
            ->setEnabled(m_monitor->groupCleanableBytes(groups) > 0);
        menu.addAction(tr("Re&scan"), this, [this, groups] { m_monitor->rescanGroups(groups); });
        menu.addSeparator();
        menu.addAction(tr("&Rename"), this, [this] { m_groups->renameCurrent(); })
            ->setEnabled(groups.size() == 1);
        menu.addAction(tr("&Delete…"), this, &MainWindow::removeGroups);
        menu.addSeparator();
    }
    menu.addAction(tr("&New group"), this, &MainWindow::addGroup);
    menu.exec(globalPos);
}

void MainWindow::onCleaned(const QUuid &id, const CleanResult &result)
{
    const Folder *f = m_store->folder(id);
    const QString name = f ? f->name : QString();
    showMessage(tr("Freed %1 in %2.").arg(Format::bytes(result.freedBytes), name));
    if (result.failures.isEmpty())
        return;
    QStringList shown = result.failures.mid(0, MaxFailuresShown);
    if (result.failures.size() > MaxFailuresShown)
        shown.append(tr("…and %n more.", nullptr,
                        static_cast<int>(result.failures.size() - MaxFailuresShown)));
    QMessageBox box(QMessageBox::Warning, tr("Clean %1").arg(name),
                    tr("%n item(s) could not be deleted.", nullptr,
                       static_cast<int>(result.failures.size())),
                    QMessageBox::Ok, isVisible() ? this : nullptr);
    box.setDetailedText(shown.join(QLatin1Char('\n')));
    box.exec();
}

void MainWindow::showItems()
{
    const Folder *f = m_store->folder(currentId());
    if (!f)
        return;
    ItemsDialog(f->name, f->path, m_monitor->result(f->id), this).exec();
}

void MainWindow::openFolder(const QUuid &id)
{
    if (const Folder *f = m_store->folder(id))
        QDesktopServices::openUrl(QUrl::fromLocalFile(f->path));
}

void MainWindow::copyPath(const QUuid &id)
{
    const Folder *f = m_store->folder(id);
    if (!f)
        return;
    const QString path = QDir::toNativeSeparators(f->path);
    QApplication::clipboard()->setText(path);
    showMessage(tr("Copied %1").arg(path));
}

void MainWindow::editTypes()
{
    TypesDialog dialog(m_store->customTypes(), this);
    if (dialog.exec() != QDialog::Accepted)
        return;
    m_store->setCustomTypes(dialog.customTypes());
    // Rules may have changed for any folder.
    m_monitor->rescanAll();
}

void MainWindow::editSettings()
{
    SettingsDialog dialog(m_store->settings(), m_tray != nullptr, this);
    if (dialog.exec() != QDialog::Accepted)
        return;
    m_store->setSettings(dialog.settings());

    if (!LoginItem::isSupported() || dialog.startAtSignIn() == LoginItem::isEnabled())
        return;
    QString error;
    if (!LoginItem::setEnabled(dialog.startAtSignIn(), &error)) {
        QMessageBox::warning(this, tr("Settings"),
                             tr("Could not change whether the app starts at sign-in: %1").arg(error));
    } else if (LoginItem::needsApproval()) {
        const auto answer = QMessageBox::information(
            this, tr("Settings"),
            tr("macOS starts the app at sign-in only after you allow it in System Settings > "
               "General > Login Items. Open System Settings now?"),
            QMessageBox::Open | QMessageBox::Cancel, QMessageBox::Open);
        if (answer == QMessageBox::Open)
            LoginItem::openSystemSettings();
    }
}

void MainWindow::showContextMenu(const QUuid &id, const QPoint &globalPos)
{
    const Folder *f = m_store->folder(id);
    if (!f)
        return;
    const ScanResult r = m_monitor->result(id);
    const bool idle = m_monitor->state(id) == Monitor::State::Idle;
    QMenu menu(this);
    menu.addAction(tr("&Clean…"), this, [this, id] { cleanFolder(id); })
        ->setEnabled(idle && r.valid && r.cleanableBytes > 0);
    menu.addAction(tr("&Rescan"), this, [this, id] { m_monitor->rescan(id); })->setEnabled(idle);
    menu.addAction(tr("&Show what cleaning deletes…"), this, &MainWindow::showItems);
    menu.addAction(tr("&Detect types…"), this, [this, id] { detectTypes(id); });
    menu.addSeparator();
    menu.addAction(tr("&Open folder"), this, [this, id] { openFolder(id); });
    menu.addAction(tr("Copy &path"), this, [this, id] { copyPath(id); });
    menu.addSeparator();
    fillMoveMenu(menu.addMenu(tr("Move to &group")), id);
    menu.addSeparator();
    menu.addAction(tr("&Edit…"), this, [this, id] { editFolder(id); });
    menu.addAction(tr("Re&move…"), this, &MainWindow::removeFolder);
    menu.exec(globalPos);
}
