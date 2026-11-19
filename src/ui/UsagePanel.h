#pragma once

#include <QWidget>

// Shows in large coloured text how much cleaning would free: in every folder, and in the
// selected groups when they are not all of them, and in the folders that count in the warnings
// when that differs from every folder. It sits in the toolbar, right of the buttons.
class UsagePanel : public QWidget
{
    Q_OBJECT

public:
    explicit UsagePanel(QWidget *parent = nullptr);

    // `selectionBytes` is shown only when `showSelection` is set, `warningBytes` only when it
    // differs from `allBytes`.
    void setUsage(qint64 allBytes, qint64 selectionBytes, bool showSelection,
                  qint64 warningBytes);
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
    };
    QList<Column> columns() const;
    int columnWidth(const Column &column) const;
    QFont bigFont() const;
    QFont smallFont() const;

    qint64 m_all = 0;
    qint64 m_selection = 0;
    bool m_showSelection = false;
    qint64 m_warning = 0;
};
