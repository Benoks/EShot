#ifndef UPLOADDIALOG_H
#define UPLOADDIALOG_H

#include <QDialog>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QString>
#include <QPixmap>
#include <QComboBox>

class ImageUploader;
class QGroupBox;

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

private:
    void translateUi();
    void setBusy(bool busy);
    void rebuildAuthSection();
    void rebuildUploader();
    void updateAuthHelp();

    QPixmap m_pixmap;
    ImageUploader *m_uploader = nullptr;
    bool m_autoUploadTried = false;

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
};

#endif
