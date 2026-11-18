#pragma once

#include <QDoubleSpinBox>

// A size in gigabytes (10^9 bytes), where 0 means "off".
class SizeSpinBox : public QDoubleSpinBox
{
    Q_OBJECT

public:
    explicit SizeSpinBox(QWidget *parent = nullptr)
        : QDoubleSpinBox(parent)
    {
        setRange(0, 100000);
        setDecimals(1);
        setSingleStep(1);
        setSuffix(tr(" GB"));
        setSpecialValueText(tr("Off"));
        // Stays at its own width when the form around it grows its fields.
        setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    }

    qint64 bytes() const { return static_cast<qint64>(value() * 1e9); }
    void setBytes(qint64 bytes) { setValue(static_cast<double>(bytes) / 1e9); }
};
