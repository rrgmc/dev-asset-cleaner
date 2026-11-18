#pragma once

#include "CleanableListView.h"

#include <QUuid>

class FolderModel;

// The folder list. Clicking a row's Clean button asks to clean it, Enter or a double click
// edits the folder, and typing goes to the filter. A row can be dragged onto a group.
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
    void typed(const QString &text);
    void currentFolderChanged(const QUuid &id);
    void folderContextMenu(const QUuid &id, const QPoint &globalPos);
    void filesDropped(const QStringList &paths);

protected:
    void keyPressEvent(QKeyEvent *event) override;
    void keyboardSearch(const QString &search) override;
    void buttonClicked(const QModelIndex &index) override;
    void rowDoubleClicked(const QModelIndex &index) override;
    void contextMenuEvent(QContextMenuEvent *event) override;
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dragMoveEvent(QDragMoveEvent *event) override;
    void dropEvent(QDropEvent *event) override;
    void paintEvent(QPaintEvent *event) override;
    void startDrag(Qt::DropActions supportedActions) override;
    void currentChanged(const QModelIndex &current, const QModelIndex &previous) override;

private:
    FolderModel *m_model;
    QUuid m_keptId;  // the current folder across a model reset
};
