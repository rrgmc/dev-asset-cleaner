#pragma once

#include <QLocale>
#include <QString>

namespace Format {

// "1.4 GB", in decimal units.
inline QString bytes(qint64 value)
{
    return QLocale().formattedDataSize(value, 1, QLocale::DataSizeSIFormat);
}

} // namespace Format
