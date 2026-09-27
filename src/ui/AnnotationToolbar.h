#ifndef ANNOTATIONTOOLBAR_H
#define ANNOTATIONTOOLBAR_H

#include <QWidget>
#include <QHBoxLayout>
#include <QPushButton>
#include <QMap>
#include <QColor>
#include <QStringList>
#include <QSlider>
#include <QLabel>
#include <QFontComboBox>
#include <QSpinBox>

class AnnotationToolbar : public QWidget {
    Q_OBJECT

public:
    explicit AnnotationToolbar(QWidget *parent = nullptr);
    ~AnnotationToolbar();

    void refreshTools();
    void refreshToolTips();
    void selectTool(int toolId);
    void setUndoEnabled(bool enabled);
    void setRedoEnabled(bool enabled);
    void setBlurIntensity(int intensity);
    void setColor(const QColor &color);
    void setSelectionLocked(bool locked);
    bool hasVisibleTools() const;

signals:
    void toolSelected(int toolId);
    void undoRequested();
    void redoRequested();
    void colorChanged(const QColor &color);
    void modalDialogClosed();
    void penWidthChanged(int width);
    void blurIntensityChanged(int intensity);
    void textFontFamilyChanged(const QString &family);
    void textFontSizeChanged(int size);
    void eyedropperRequested();
    void lockToggled(bool locked);
    void ocrRequested();
    void uploadRequested();
    void googleLensRequested();
    void gifRequested();
    void videoRequested();

private slots:
    void onToolButtonClicked();
    void onActionButtonClicked();
    void onColorButtonClicked();
    void onBlurIntensityChanged(int value);
    void onEyedropperClicked();
    void onLockClicked();

private:
    void setupUI();
    void applyStyles();
    void updateDynamicOptionVisibility();
    QPushButton* createToolButton(const QString &iconPath, const QString &tooltip,
                                  int toolId, const QString &settingsKey);
    QPushButton* createActionButton(const QString &iconPath, const QString &tooltip,
                                    const QString &action);
    QPushButton* createColorButton(const QColor &color);
    QWidget* createSeparator();

    bool isToolVisible(const QString &key) const;
    bool isControlVisible(const QString &key) const;

    QHBoxLayout *m_layout;
    QMap<int, QPushButton*> m_toolButtons;
    QMap<QString, QPushButton*> m_actionButtons;
    QMap<QString, QWidget*> m_optionalControls;
    QPushButton *m_colorButton;
    QSlider *m_widthSlider;
    int m_currentToolId;
    QColor m_currentColor;
    QStringList m_visibleTools;
    QStringList m_visibleControls;

    // New: Eyedropper
    QPushButton *m_eyedropperButton;

    // New: Selection lock
    QPushButton *m_lockButton;
    bool m_selectionLocked;

    // New: Blur strength
    QSlider *m_blurIntensitySlider;
    QLabel *m_blurIntensityLabel;
    QWidget *m_blurIntensityWidget;

    // Text options
    QWidget *m_textOptionsWidget;
    QFontComboBox *m_textFontCombo;
    QSpinBox *m_textSizeSpin;

    // Undo/Redo button references
    QPushButton *m_undoButton;
    QPushButton *m_redoButton;

    // OCR and upload
    QPushButton *m_ocrButton;
    QPushButton *m_uploadButton;
    QPushButton *m_lensButton;
    QPushButton *m_gifButton;
    QPushButton *m_videoButton;
};

#endif
