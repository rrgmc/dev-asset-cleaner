#pragma once

#include <QListView>
#include <QTimer>
#include <QUuid>

class FolderModel;

// The folder list. Clicking a row's Clean button asks to clean it, Enter or a double click
// edits the folder, and typing goes to the filter. A row can be dragged onto a group.
class FolderListView : public QListView
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
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dragMoveEvent(QDragMoveEvent *event) override;
    void dropEvent(QDropEvent *event) override;
    void paintEvent(QPaintEvent *event) override;
    void startDrag(Qt::DropActions supportedActions) override;
    void currentChanged(const QModelIndex &current, const QModelIndex &previous) override;

private:
    // The row whose enabled Clean button is at `pos`, or an invalid index.
    QModelIndex buttonAt(const QPoint &pos) const;
    // Runs the spinner timer while any row is busy.
    void updateSpinner();

    FolderModel *m_model;
    QPersistentModelIndex m_hoverIndex;
    QUuid m_keptId;  // the current folder across a model reset
    QTimer m_spinnerTimer;  // repaints the busy rows' spinners
};
