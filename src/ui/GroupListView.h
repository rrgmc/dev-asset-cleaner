#pragma once

#include <QListView>
#include <QUuid>

class GroupModel;

// The group list beside the folder list. Several groups can be selected; the folder list shows
// all their folders. Folders dragged from the folder list onto a group move to it, and folders
// dropped from Explorer or Finder are added to the group under them.
class GroupListView : public QListView
{
    Q_OBJECT

public:
    explicit GroupListView(GroupModel *model, QWidget *parent = nullptr);

    GroupModel *groupModel() const { return m_model; }

    // The selected groups in list order; the current group when nothing is selected.
    QList<QUuid> selectedIds() const;
    QUuid currentId() const;
    bool selectId(const QUuid &id);  // selects only this group
    void renameCurrent();

    // The type of the drag the folder list starts: folder ids, one per line.
    static QString folderMimeType();

signals:
    void selectionChangedIds();
    void foldersDropped(const QList<QUuid> &folderIds, const QUuid &groupId);
    void filesDropped(const QStringList &paths, const QUuid &groupId);
    void groupContextMenu(const QUuid &id, const QPoint &globalPos);
    void deleteRequested();  // Delete pressed in the list

protected:
    bool event(QEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dragMoveEvent(QDragMoveEvent *event) override;
    void dragLeaveEvent(QDragLeaveEvent *event) override;
    void dropEvent(QDropEvent *event) override;
    void paintEvent(QPaintEvent *event) override;
    void selectionChanged(const QItemSelection &selected,
                          const QItemSelection &deselected) override;

private:
    void setDropTarget(const QModelIndex &index);

    GroupModel *m_model;
    QPersistentModelIndex m_dropTarget;
    // Kept across a model reset, which drops the selection.
    QList<QUuid> m_keptIds;
    QUuid m_keptCurrent;
    int m_keptRow = -1;
};
