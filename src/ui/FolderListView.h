#pragma once

#include "CleanableListView.h"
#include "ReorderDrop.h"

#include <QUuid>

class FolderModel;

// The folder list. Clicking a row's Clean button asks to clean it, a double click shows what
// cleaning it deletes, Enter edits the folder, and typing goes to the filter. A row can be dragged onto a group, or, in
// manual order, to another place among its group's folders.
class FolderListView : public CleanableListView
{
    Q_OBJECT

public:
    explicit FolderListView(FolderModel *model, QWidget *parent = nullptr);

    FolderModel *folderModel() const { return m_model; }

    QUuid currentId() const;
    bool selectId(const QUuid &id);
    void focusFirst();

signals:
    void cleanRequested(const QUuid &id);
    void editRequested(const QUuid &id);
    void detailsRequested(const QUuid &id);
    void typed(const QString &text);
    void currentFolderChanged(const QUuid &id);
    void folderContextMenu(const QUuid &id, const QPoint &globalPos);
    void filesDropped(const QStringList &paths);
    // A folder dragged within the list, to just before or after another folder.
    void folderMoveRequested(const QUuid &id, const QUuid &targetId, bool after);

protected:
    void keyPressEvent(QKeyEvent *event) override;
    void keyboardSearch(const QString &search) override;
    void buttonClicked(const QModelIndex &index) override;
    void rowDoubleClicked(const QModelIndex &index) override;
    void contextMenuEvent(QContextMenuEvent *event) override;
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dragMoveEvent(QDragMoveEvent *event) override;
    void dragLeaveEvent(QDragLeaveEvent *event) override;
    void dropEvent(QDropEvent *event) override;
    void paintEvent(QPaintEvent *event) override;
    void startDrag(Qt::DropActions supportedActions) override;
    void currentChanged(const QModelIndex &current, const QModelIndex &previous) override;

private:
    // The folder this list is dragging, when it may be moved within the list: manual order only.
    QUuid reorderedFolder(const QDropEvent *event) const;
    // Where that folder lands: next to a folder of its own group, or nowhere.
    ReorderDrop::Target reorderTarget(const QDropEvent *event) const;
    void setReorderTarget(const ReorderDrop::Target &target);

    FolderModel *m_model;
    ReorderDrop::Target m_reorderTarget;
    QUuid m_keptId;  // the current folder across a model reset
};
