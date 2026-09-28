#pragma once

#include <QTimer>
#include <QWidget>

class QLabel;
class QPushButton;

// Small dismissible callout that points at a control, used for one-time tips.
class HintBubble : public QWidget
{
    Q_OBJECT

public:
    enum class Pointer { Left, Up, Down };

    explicit HintBubble(QWidget *parent = nullptr);

    // Shows text next to anchor (parent coordinates), kept inside bounds.
    void showTip(const QString &text, const QRect &anchor, Pointer pointer, const QRect &bounds);
    void dismiss();

signals:
    void dismissed();

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QLabel *m_label = nullptr;
    QPushButton *m_button = nullptr;
    Pointer m_pointer = Pointer::Left;
    int m_arrowOffset = 0;
    QTimer m_autoHide;
};
