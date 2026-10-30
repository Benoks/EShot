#ifndef ABOUTDIALOG_H
#define ABOUTDIALOG_H

#include <QDialog>

class QLabel;
class QPushButton;
class AboutDialog : public QDialog {
    Q_OBJECT

public:
    explicit AboutDialog(QWidget *parent = nullptr);
    ~AboutDialog();

    void setUpdateInfo(bool available, const QString &version, bool busy, const QString &status);

signals:
    void checkForUpdatesRequested();
    void updateRequested();

private slots:
    void onCheckForUpdates();

private:
    void setupUI();
    void setUpdateStatus(const QString &text, bool highlighted);

    QLabel *m_updateStatusLabel = nullptr;
    QPushButton *m_checkUpdateBtn = nullptr;
    bool m_updateAvailable = false;
};

#endif // ABOUTDIALOG_H
