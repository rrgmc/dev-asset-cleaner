#pragma once

#include <QDialog>
#include <QStringList>

class QListWidget;

// Picks the first-level subfolders of a folder that cleaning must leave alone. Lists the
// subfolders that exist now, and keeps excluded names whose folder is gone, so they still apply
// if it comes back.
class ExcludeDialog : public QDialog
{
    Q_OBJECT

public:
    ExcludeDialog(const QString &path, const QStringList &excludes, QWidget *parent = nullptr);

    // The checked names, sorted without regard to case.
    QStringList excludes() const;

    // A short description of `excludes` for a label, such as "None" or "build, docs".
    static QString summary(const QStringList &excludes);

private:
    QListWidget *m_list;
};
