#pragma once

#include <QDialog>

class ControlCenterDialog final : public QDialog {
    Q_OBJECT

public:
    enum class Action { None, Capture, Settings, About, Quit };

    explicit ControlCenterDialog(QWidget *parent = nullptr);
    Action selectedAction() const;

private:
    void choose(Action action);

    Action m_selectedAction = Action::None;
};
