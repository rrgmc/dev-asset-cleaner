#pragma once

#include "clean/Scanner.h"

#include <QDialog>
#include <QFutureWatcher>

#include <atomic>
#include <memory>

class QLabel;
class QLineEdit;
class QPushButton;
class QTreeWidget;
class Store;

// Searches one folder for the cleaner types that apply to it, such as Rust and JavaScript in a
// folder of projects, and lets the user pick which ones the folder should use. Runs the search
// in the background; it can be canceled.
class DetectDialog : public QDialog
{
    Q_OBJECT

public:
    // With a path, the search starts as soon as the dialog shows.
    DetectDialog(Store *store, const QString &path = {}, QWidget *parent = nullptr);
    ~DetectDialog() override;

    QString path() const;
    // The types left checked, in the order of Store::types().
    QStringList typeIds() const;

protected:
    void showEvent(QShowEvent *event) override;

private:
    void browse();
    void start();
    void cancel();
    void finished();
    void updateButtons();

    Store *m_store;
    QLineEdit *m_path;
    QPushButton *m_search;
    QLabel *m_status;
    QTreeWidget *m_results;
    QPushButton *m_ok;
    QFutureWatcher<DetectResult> m_watcher;
    std::shared_ptr<std::atomic_bool> m_cancel;
    bool m_autoStart;
};
