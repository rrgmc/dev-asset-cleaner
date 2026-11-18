#include "FolderListView.h"

#include "FolderModel.h"
#include "GroupListView.h"

#include <QContextMenuEvent>
#include <QCursor>
#include <QDrag>
#include <QFileInfo>
#include <QKeyEvent>
#include <QMimeData>
#include <QPainter>

FolderListView::FolderListView(FolderModel *model, QWidget *parent)
    : CleanableListView(model, parent)
    , m_model(model)
{
    setSelectionMode(QAbstractItemView::SingleSelection);
    setAcceptDrops(true);
    setDropIndicatorShown(false);
    setDragEnabled(true);

    // A reset (any change to the store) drops the current row; put it back.
    connect(model, &QAbstractItemModel::modelAboutToBeReset, this,
            [this] { m_keptId = currentId(); });
    connect(model, &QAbstractItemModel::modelReset, this, [this] {
        if (!m_keptId.isNull())
            selectId(m_keptId);
    });
}

void FolderListView::startDrag(Qt::DropActions)
{
    // Only the group list takes these drops, and it moves the folder itself: the model's rows
    // must not be removed by a Move drop, as QAbstractItemView would do.
    const QModelIndex index = currentIndex();
    if (!index.isValid())
        return;
    auto *mime = new QMimeData;
    mime->setData(GroupListView::folderMimeType(),
                  m_model->idAt(index.row()).toString(QUuid::WithoutBraces).toLatin1());
    auto *drag = new QDrag(this);
    drag->setMimeData(mime);
    const QRect rect = visualRect(index);
    drag->setPixmap(viewport()->grab(rect));
    drag->setHotSpot(viewport()->mapFromGlobal(QCursor::pos()) - rect.topLeft());
    drag->exec(Qt::MoveAction);
}

QUuid FolderListView::currentId() const
{
    return m_model->idAt(currentIndex().row());
}

bool FolderListView::selectId(const QUuid &id)
{
    const int row = m_model->rowOf(id);
    if (row < 0)
        return false;
    setCurrentIndex(m_model->index(row));
    return true;
}

void FolderListView::focusFirst()
{
    if (!currentIndex().isValid() && m_model->rowCount() > 0)
        setCurrentIndex(m_model->index(0));
    setFocus(Qt::OtherFocusReason);
}

void FolderListView::keyPressEvent(QKeyEvent *event)
{
    const Qt::KeyboardModifiers mods = event->modifiers() & ~Qt::KeypadModifier;
    if (mods == Qt::NoModifier
        && (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter)) {
        if (currentIndex().isValid())
            emit editRequested(currentId());
        return;
    }
    QListView::keyPressEvent(event);
}

void FolderListView::keyboardSearch(const QString &search)
{
    emit typed(search);
}

void FolderListView::buttonClicked(const QModelIndex &index)
{
    emit cleanRequested(m_model->idAt(index.row()));
}

void FolderListView::rowDoubleClicked(const QModelIndex &index)
{
    emit editRequested(m_model->idAt(index.row()));
}

void FolderListView::contextMenuEvent(QContextMenuEvent *event)
{
    const QModelIndex index =
        event->reason() == QContextMenuEvent::Mouse ? indexAt(event->pos()) : currentIndex();
    if (!index.isValid())
        return;
    setCurrentIndex(index);
    const QPoint global = event->reason() == QContextMenuEvent::Mouse
                              ? event->globalPos()
                              : viewport()->mapToGlobal(visualRect(index).bottomLeft());
    emit folderContextMenu(m_model->idAt(index.row()), global);
}

namespace {
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

void FolderListView::dragEnterEvent(QDragEnterEvent *event)
{
    if (!localFolders(event->mimeData()).isEmpty())
        event->acceptProposedAction();
    else
        event->ignore();
}

void FolderListView::dragMoveEvent(QDragMoveEvent *event)
{
    if (!localFolders(event->mimeData()).isEmpty())
        event->acceptProposedAction();
    else
        event->ignore();
}

void FolderListView::dropEvent(QDropEvent *event)
{
    const QStringList paths = localFolders(event->mimeData());
    if (paths.isEmpty()) {
        event->ignore();
        return;
    }
    event->acceptProposedAction();
    emit filesDropped(paths);
}

void FolderListView::paintEvent(QPaintEvent *event)
{
    QListView::paintEvent(event);
    if (m_model->rowCount() > 0)
        return;
    QPainter painter(viewport());
    QColor color = palette().color(QPalette::Text);
    color.setAlphaF(0.45);
    painter.setPen(color);
    const QString text = m_model->groups().isEmpty()
                             ? tr("Add a group with Group > New group")
                             : !m_model->filter().isEmpty()
                                   ? tr("No matches")
                                   : tr("Add a folder with the + button, or drop folders here");
    painter.drawText(viewport()->rect(), Qt::AlignCenter | Qt::TextWordWrap, text);
}

void FolderListView::currentChanged(const QModelIndex &current, const QModelIndex &previous)
{
    QListView::currentChanged(current, previous);
    if (current.isValid())
        selectionModel()->select(current, QItemSelectionModel::ClearAndSelect);
    emit currentFolderChanged(m_model->idAt(current.row()));
}
