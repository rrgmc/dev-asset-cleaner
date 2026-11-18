#pragma once

#include <QWidget>

// Shows in large coloured text how much the folders use and how much of it can be cleaned:
// for every folder, and for the selected groups when they are not all of them. It sits in the
// toolbar, right of the buttons.
class UsagePanel : public QWidget
{
    Q_OBJECT

public:
    struct Usage {
        qint64 totalBytes = 0;
        qint64 cleanableBytes = 0;
    };

    explicit UsagePanel(QWidget *parent = nullptr);

    // `selection` is shown only when `showSelection` is set.
    void setUsage(const Usage &all, const Usage &selection, bool showSelection);

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override { return sizeHint(); }

protected:
    void paintEvent(QPaintEvent *event) override;
    void changeEvent(QEvent *event) override;

private:
    struct Column {
        QString caption;
        Usage usage;
        QColor color;
    };
    QList<Column> columns() const;
    int columnWidth(const Column &column) const;
    QFont bigFont() const;
    QFont smallFont() const;

    Usage m_all;
    Usage m_selection;
    bool m_showSelection = false;
};
