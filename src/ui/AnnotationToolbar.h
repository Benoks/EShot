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

signals:
    void toolSelected(int toolId);
    void undoRequested();
    void redoRequested();
    void colorChanged(const QColor &color);
    void penWidthChanged(int width);
    void blurIntensityChanged(int intensity);
    void eyedropperRequested();
    void lockToggled(bool locked);

private slots:
    void onToolButtonClicked();
    void onActionButtonClicked();
    void onColorButtonClicked();
    void onWidthSliderChanged(int value);
    void onBlurIntensityChanged(int value);
    void onEyedropperClicked();
    void onLockClicked();

private:
    void setupUI();
    void applyStyles();
    QPushButton* createToolButton(const QString &iconPath, const QString &tooltip,
                                  int toolId, const QString &settingsKey);
    QPushButton* createActionButton(const QString &iconPath, const QString &tooltip,
                                    const QString &action);
    QPushButton* createColorButton(const QColor &color);
    QWidget* createSeparator();

    bool isToolVisible(const QString &key) const;

    QHBoxLayout *m_layout;
    QMap<int, QPushButton*> m_toolButtons;
    QMap<QString, QPushButton*> m_actionButtons;
    QPushButton *m_colorButton;
    int m_currentToolId;
    QColor m_currentColor;
    QStringList m_visibleTools;

    // Yeni: Eyedropper
    QPushButton *m_eyedropperButton;

    // Yeni: Seçim kilidi
    QPushButton *m_lockButton;
    bool m_selectionLocked;

    // Yeni: Bulanıklık şiddeti
    QSlider *m_blurIntensitySlider;
    QLabel *m_blurIntensityLabel;
    QWidget *m_blurIntensityWidget;

    // Geri al/İleri al buton referansları
    QPushButton *m_undoButton;
    QPushButton *m_redoButton;
};

#endif
