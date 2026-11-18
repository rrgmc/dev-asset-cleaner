#pragma once

#include "clean/Scanner.h"

#include <QDialog>
#include <QFutureWatcher>

#include <atomic>
#include <memory>

class QLabel;
class QLineEdit;
class QPushButton;
class SpinnerWidget;
class QTreeWidget;
class Store;

// Adds a Matching folder, or changes the types of one: searches the folder for the cleaner
// types that apply to it, such as Rust and JavaScript in a folder of projects, and lets the
// user pick which ones it should use. Runs the search in the background; it can be canceled.
// A Whole folder already in the list cannot be turned into a Matching folder here.
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
    // The first-level subfolders left out of the search, and of cleaning once added. They start
    // as the excluded folders of the folder in the list with this path, if there is one.
    QStringList excludes() const;
    void setExcludes(const QStringList &excludes);

protected:
    void showEvent(QShowEvent *event) override;

private:
    void browse();
    void chooseExcludes();
    void start();
    void cancel();
    void finished();
    void updateButtons();
    // The path is a Whole folder in the list, whose kind cannot change.
    bool isWholeFolder() const;

    Store *m_store;
    QLineEdit *m_path;
    QPushButton *m_search;
    QPushButton *m_exclude;
    QLabel *m_excludeLabel;
    QStringList m_excludes;
    QString m_excludesPath;  // the folder m_excludes belong to
    QLabel *m_status;
    SpinnerWidget *m_spinner;
    QTreeWidget *m_results;
    QPushButton *m_ok;
    QFutureWatcher<DetectResult> m_watcher;
    std::shared_ptr<std::atomic_bool> m_cancel;
    bool m_autoStart;
};
