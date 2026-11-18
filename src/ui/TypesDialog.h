#pragma once

#include "model/CleanerType.h"

#include <QDialog>

class QLabel;
class QLineEdit;
class QListWidget;
class QPushButton;
class QTableWidget;

// Lists the cleaner types and edits the custom ones. Built-in types are read-only, but can be
// duplicated as a starting point.
class TypesDialog : public QDialog
{
    Q_OBJECT

public:
    TypesDialog(const QList<CleanerType> &customTypes, QWidget *parent = nullptr);

    QList<CleanerType> customTypes() const;

private:
    void rebuildList(const QString &selectId);
    void currentRowChanged(int row);
    void loadEditor(const CleanerType &type);
    void storeEditor();
    CleanerType *currentCustom();
    void addType(CleanerType type);
    void removeType();
    void addRule();
    void removeRule();
    void updateEnabled();

    QList<CleanerType> m_custom;
    QString m_currentId;
    bool m_loading = false;

    QListWidget *m_list;
    QPushButton *m_duplicate;
    QPushButton *m_remove;
    QLineEdit *m_name;
    QLineEdit *m_description;
    QTableWidget *m_rules;
    QPushButton *m_addRule;
    QPushButton *m_removeRule;
    QLabel *m_readOnly;
};
