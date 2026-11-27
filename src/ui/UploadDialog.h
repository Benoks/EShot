#ifndef UPLOADDIALOG_H
#define UPLOADDIALOG_H

#include <QDialog>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QString>
#include <QPixmap>
#include <QComboBox>
#include "core/UploadHistory.h"

class ImageUploader;
class QGroupBox;
class QListWidget;
class QStackedWidget;

class UploadDialog : public QDialog {
    Q_OBJECT

public:
    explicit UploadDialog(QWidget *parent = nullptr);
    ~UploadDialog();

    void setImage(const QPixmap &pixmap);

protected:
    void showEvent(QShowEvent *event) override;

private slots:
    void onProviderChanged(int index);
    void onUploadClicked();
    void onUploading();
    void onSucceeded(const QString &url, const QString &deleteUrl);
    void onFailed(const QString &reason);
    void onOpenLink();
    void onCopyLink();
    void onSaveAuth();
    void onToggleHistory();
    void onClearHistory();

private:
    void translateUi();
    void setBusy(bool busy);
    void rebuildAuthSection();
    void rebuildUploader();
    void updateAuthHelp();
    void refreshHistory();
    void updateHistoryButtons();
    const UploadHistory::Entry *currentHistoryEntry() const;

    QPixmap m_pixmap;
    ImageUploader *m_uploader = nullptr;
    bool m_autoUploadTried = false;
    UploadHistory m_history;

    QComboBox *m_providerCombo;
    QGroupBox *m_authBox;
    QLineEdit *m_authEdit;
    QLabel *m_authHelpLabel;
    QPushButton *m_saveAuthBtn;
    QPushButton *m_uploadBtn;
    QLabel *m_statusLabel;
    QLineEdit *m_linkEdit;
    QLineEdit *m_deleteEdit;
    QPushButton *m_copyLinkBtn;
    QPushButton *m_openLinkBtn;
    QPushButton *m_closeBtn;

    QStackedWidget *m_pages;
    QListWidget *m_historyList;
    QPushButton *m_historyBtn;
    QPushButton *m_historyCopyBtn;
    QPushButton *m_historyOpenBtn;
    QPushButton *m_historyCopyDeleteBtn;
    QPushButton *m_historyRemoveBtn;
    QPushButton *m_clearHistoryBtn;
    QLabel *m_historyStatusLabel;
};

#endif
