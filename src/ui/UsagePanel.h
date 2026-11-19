#pragma once

#include "model/Alert.h"

#include <QColor>
#include <QWidget>

// Shows in large text how much cleaning would free: in every folder, in coral; in the folders
// that count in the warnings when that differs, in the clean warning level's colour; and in the
// selected groups when they are not all of them, in the text colour. It sits in the toolbar,
// right of the buttons.
class UsagePanel : public QWidget
{
    Q_OBJECT

public:
    explicit UsagePanel(QWidget *parent = nullptr);

    // `selectionBytes` is shown only when `showSelection` is set, `warningBytes` only when it
    // differs from `allBytes`. `warningLevel` is the clean warning's level, which colours it.
    void setUsage(qint64 allBytes, qint64 selectionBytes, bool showSelection,
                  qint64 warningBytes, Severity warningLevel);
    // The colour of a column's size, for tests.
    QColor sizeColor(const QString &caption) const;
    // The captions of the columns shown, for tests.
    QStringList captions() const;

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override { return sizeHint(); }

protected:
    void paintEvent(QPaintEvent *event) override;
    void changeEvent(QEvent *event) override;

private:
    struct Column {
        QString caption;
        qint64 cleanableBytes = 0;
        QColor color;  // of the size; invalid for the text colour
    };
    QColor sizeColor(const Column &column) const;
    QList<Column> columns() const;
    int columnWidth(const Column &column) const;
    QFont bigFont() const;
    QFont smallFont() const;

    qint64 m_all = 0;
    qint64 m_selection = 0;
    bool m_showSelection = false;
    qint64 m_warning = 0;
    Severity m_warningLevel = Severity::None;
};
