#pragma once

#include <QListView>
#include <QPersistentModelIndex>
#include <QTimer>

// A list of rows painted by FolderDelegate, over any model that answers FolderModel's roles.
// It handles the rows' Clean buttons and turns the spinners of busy rows. Subclasses decide
// what a click on a button and a double click on a row do.
class CleanableListView : public QListView
{
    Q_OBJECT

public:
    explicit CleanableListView(QAbstractItemModel *model, QWidget *parent = nullptr);

protected:
    // A click on the enabled Clean button of `index`.
    virtual void buttonClicked(const QModelIndex &index) = 0;
    // A double click on `index` outside its button.
    virtual void rowDoubleClicked(const QModelIndex &index) { Q_UNUSED(index) }

    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;

private:
    // The row whose enabled Clean button is at `pos`, or an invalid index.
    QModelIndex buttonAt(const QPoint &pos) const;
    // Runs the spinner timer while any row is busy.
    void updateSpinner();

    QPersistentModelIndex m_hoverIndex;
    QTimer m_spinnerTimer;  // repaints the busy rows' spinners
};
