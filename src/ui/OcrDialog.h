#ifndef OCRDIALOG_H
#define OCRDIALOG_H

#include <QDialog>
#include <QTextEdit>
#include <QLabel>
#include <QPushButton>
#include <QComboBox>
#include <QString>
#include <QPixmap>

class OcrEngine;

class OcrDialog : public QDialog {
    Q_OBJECT

public:
    explicit OcrDialog(const QPixmap &pixmap, QWidget *parent = nullptr);
    ~OcrDialog();

    void setLanguageTag(const QString &tag);

private slots:
    void onTextReady(const QString &text);
    void onOcrFailed(const QString &reason);
    void onCopyClicked();
    void onTranslateClicked();
    void onRetryClicked();
    void onLanguageChanged(int index);

private:
    void setBusy(bool busy);
    void translateUi();
    void runOcr();
    void populateLanguages();
    bool isLanguageInstalled(const QString &tag) const;
    int firstInstalledLanguageIndex() const;

    QPixmap m_pixmap;
    OcrEngine *m_engine;
    QString m_languageTag = "auto";
    QString m_preferredLanguageTag = "en";

    QComboBox *m_langCombo;
    QLabel *m_statusLabel;
    QTextEdit *m_textEdit;
    QPushButton *m_copyBtn;
    QPushButton *m_translateBtn;
    QPushButton *m_retryBtn;
    QPushButton *m_closeBtn;
};

#endif
