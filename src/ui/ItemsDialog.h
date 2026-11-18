#pragma once

#include "clean/Scanner.h"

#include <QDialog>

// Lists what cleaning a folder would delete, from its last scan, largest first.
class ItemsDialog : public QDialog
{
    Q_OBJECT

public:
    ItemsDialog(const QString &folderName, const QString &folderPath, const ScanResult &result,
                QWidget *parent = nullptr);
};
