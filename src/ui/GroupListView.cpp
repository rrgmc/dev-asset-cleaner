#include "GroupListView.h"

#include "GroupDelegate.h"
#include "GroupModel.h"

#include <QContextMenuEvent>
#include <QFileInfo>
#include <QKeyEvent>
#include <QMimeData>
#include <QPainter>
#include <QPainterPath>

#include <algorithm>

namespace {
QList<QUuid> folderIds(const QMimeData *mime)
{
    QList<QUuid> ids;
    if (!mime || !mime->hasFormat(GroupListView::folderMimeType()))
        return ids;
    for (const QByteArray &line : mime->data(GroupListView::folderMimeType()).split('\n')) {
        const QUuid id = QUuid::fromString(QString::fromLatin1(line));
        if (!id.isNull())
            ids.append(id);
    }
    return ids;
}

QStringList localFolders(const QMimeData *mime)
{
    QStringList paths;
    if (!mime || !mime->hasUrls())
        return paths;
    for (const QUrl &url : mime->urls()) {
        if (url.isLocalFile() && QFileInfo(url.toLocalFile()).isDir())
            paths.append(url.toLocalFile());
    }
    return paths;
}
} // namespace

GroupListView::GroupListView(GroupModel *model, QWidget *parent)
    : QListView(parent)
    , m_model(model)
{
    setModel(model);
    setItemDelegate(new GroupDelegate(this));
    setSelectionMode(QAbstractItemView::ExtendedSelection);
    setMouseTracking(true);
    setUniformItemSizes(true);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setEditTriggers(QAbstractItemView::EditKeyPressed | QAbstractItemView::DoubleClicked);
    setAcceptDrops(true);
    setDropIndicatorShown(false);

    // A reset (a group added, removed or moved) drops the selection; put it back.
    connect(model, &QAbstractItemModel::modelAboutToBeReset, this, [this] {
        m_keptIds = selectedIds();
        m_keptCurrent = currentId();
        m_keptRow = currentIndex().row();
    });
    connect(model, &QAbstractItemModel::modelReset, this, [this] {
        QItemSelection selection;
        for (const QUuid &id : std::as_const(m_keptIds)) {
            const int row = m_model->rowOf(id);
            if (row >= 0)
                selection.select(m_model->index(row), m_model->index(row));
        }
        int current = m_model->rowOf(m_keptCurrent);
        if (current < 0 && !selection.isEmpty())
            current = selection.indexes().first().row();
        if (current < 0 && m_model->rowCount() > 0)
            current = qBound(0, m_keptRow, m_model->rowCount() - 1);
        if (current >= 0) {
            selectionModel()->setCurrentIndex(m_model->index(current),
                                              QItemSelectionModel::NoUpdate);
            if (selection.isEmpty())
                selection.select(m_model->index(current), m_model->index(current));
            selectionModel()->select(selection, QItemSelectionModel::ClearAndSelect);
        }
        emit selectionChangedIds();
    });
}

QString GroupListView::folderMimeType()
{
    return QStringLiteral("application/x-dev-asset-cleaner-folders");
}

QList<QUuid> GroupListView::selectedIds() const
{
    QList<int> rows;
    for (const QModelIndex &index : selectionModel()->selectedIndexes())
        rows.append(index.row());
    std::sort(rows.begin(), rows.end());
    QList<QUuid> ids;
    for (int row : rows)
        ids.append(m_model->idAt(row));
    if (ids.isEmpty() && currentIndex().isValid())
        ids.append(currentId());
    return ids;
}

QUuid GroupListView::currentId() const
{
    return m_model->idAt(currentIndex().row());
}

bool GroupListView::selectId(const QUuid &id)
{
    const int row = m_model->rowOf(id);
    if (row < 0)
        return false;
    selectionModel()->setCurrentIndex(m_model->index(row), QItemSelectionModel::ClearAndSelect);
    return true;
}

void GroupListView::renameCurrent()
{
    if (currentIndex().isValid())
        edit(currentIndex());
}

namespace {
// The folder commands' keys that act on the group while the group list has focus.
bool isDeleteKey(const QKeyEvent *event)
{
    const Qt::KeyboardModifiers mods = event->modifiers() & ~Qt::KeypadModifier;
    return event->matches(QKeySequence::Delete)
           || (event->key() == Qt::Key_Backspace && mods == Qt::ControlModifier);
}

bool isRenameKey(const QKeyEvent *event)
{
    const Qt::KeyboardModifiers mods = event->modifiers() & ~Qt::KeypadModifier;
    return event->key() == Qt::Key_F2 && mods == Qt::NoModifier;
}
} // namespace

bool GroupListView::event(QEvent *event)
{
    // Take these keys from the window's folder shortcuts (Remove, Edit).
    if (event->type() == QEvent::ShortcutOverride && state() != EditingState) {
        const auto *key = static_cast<QKeyEvent *>(event);
        if (isDeleteKey(key) || isRenameKey(key)) {
            event->accept();
            return true;
        }
    }
    return QListView::event(event);
}

void GroupListView::keyPressEvent(QKeyEvent *event)
{
    if (state() != EditingState && isDeleteKey(event)) {
        if (currentIndex().isValid())
            emit deleteRequested();
        return;
    }
    if (state() != EditingState && isRenameKey(event)) {
        renameCurrent();
        return;
    }
    // Enter renames the group, as it edits a folder in the folder list.
    const Qt::KeyboardModifiers mods = event->modifiers() & ~Qt::KeypadModifier;
    if (mods == Qt::NoModifier && state() != EditingState
        && (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter)) {
        renameCurrent();
        return;
    }
    QListView::keyPressEvent(event);
}

void GroupListView::contextMenuEvent(QContextMenuEvent *event)
{
    const QModelIndex index =
        event->reason() == QContextMenuEvent::Mouse ? indexAt(event->pos()) : currentIndex();
    // A right click on a selected group keeps the selection, so the menu acts on all of them.
    if (index.isValid() && !selectionModel()->isSelected(index))
        selectionModel()->setCurrentIndex(index, QItemSelectionModel::ClearAndSelect);
    else if (index.isValid())
        selectionModel()->setCurrentIndex(index, QItemSelectionModel::NoUpdate);
    const QPoint global = event->reason() == QContextMenuEvent::Mouse || !index.isValid()
                              ? event->globalPos()
                              : viewport()->mapToGlobal(visualRect(index).bottomLeft());
    emit groupContextMenu(m_model->idAt(index.row()), global);
}

void GroupListView::setDropTarget(const QModelIndex &index)
{
    if (index == m_dropTarget)
        return;
    m_dropTarget = index;
    viewport()->update();
}

void GroupListView::dragEnterEvent(QDragEnterEvent *event)
{
    if (!folderIds(event->mimeData()).isEmpty() || !localFolders(event->mimeData()).isEmpty())
        event->acceptProposedAction();
    else
        event->ignore();
}

void GroupListView::dragMoveEvent(QDragMoveEvent *event)
{
    const QModelIndex index = indexAt(event->position().toPoint());
    const bool ok = index.isValid()
                    && (!folderIds(event->mimeData()).isEmpty()
                        || !localFolders(event->mimeData()).isEmpty());
    setDropTarget(ok ? index : QModelIndex());
    if (ok)
        event->acceptProposedAction();
    else
        event->ignore();
}

void GroupListView::dragLeaveEvent(QDragLeaveEvent *event)
{
    setDropTarget({});
    QListView::dragLeaveEvent(event);
}

void GroupListView::dropEvent(QDropEvent *event)
{
    const QModelIndex index = indexAt(event->position().toPoint());
    setDropTarget({});
    const QList<QUuid> ids = folderIds(event->mimeData());
    const QStringList paths = localFolders(event->mimeData());
    if (!index.isValid() || (ids.isEmpty() && paths.isEmpty())) {
        event->ignore();
        return;
    }
    event->acceptProposedAction();
    const QUuid groupId = m_model->idAt(index.row());
    if (!ids.isEmpty())
        emit foldersDropped(ids, groupId);
    else
        emit filesDropped(paths, groupId);
}

void GroupListView::paintEvent(QPaintEvent *event)
{
    QListView::paintEvent(event);
    QPainter painter(viewport());
    if (m_dropTarget.isValid()) {
        painter.setRenderHint(QPainter::Antialiasing);
        QPainterPath path;
        path.addRoundedRect(QRectF(visualRect(m_dropTarget)).adjusted(1.5, 1.5, -1.5, -1.5), 4, 4);
        painter.setPen(QPen(palette().color(QPalette::Active, QPalette::Highlight), 2));
        painter.drawPath(path);
    }
    if (m_model->rowCount() > 0)
        return;
    QColor color = palette().color(QPalette::Text);
    color.setAlphaF(0.45);
    painter.setPen(color);
    painter.drawText(viewport()->rect().adjusted(8, 8, -8, -8), Qt::AlignCenter | Qt::TextWordWrap,
                     tr("No groups"));
}

void GroupListView::selectionChanged(const QItemSelection &selected,
                                     const QItemSelection &deselected)
{
    QListView::selectionChanged(selected, deselected);
    emit selectionChangedIds();
}
