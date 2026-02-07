#include "AboutDialog.h"
#include <QVBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QPixmap>
#include <QApplication>

AboutDialog::AboutDialog(QWidget *parent) : QDialog(parent)
{
    setWindowTitle("Hakkında - EShot");
    setFixedSize(300, 250);
    // Pencere ikonunu kaldır (daha temiz görünüm için) veya ayarla
    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);

    setupUI();
}

AboutDialog::~AboutDialog() {}

void AboutDialog::setupUI()
{
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setSpacing(10);
    layout->setContentsMargins(20, 20, 20, 20);

    // 1. Logo (Küçük ve Smooth)
    QLabel *iconLabel = new QLabel();
    QPixmap icon(":/icons/pen.svg"); // Uygulamanın ana ikonu
    if (!icon.isNull()) {
        // 48x48 boyutuna, yüksek kalitede ölçekle
        iconLabel->setPixmap(icon.scaled(48, 48, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    }
    iconLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(iconLabel);

    // 2. Uygulama Adı
    QLabel *nameLabel = new QLabel("EShot");
    QFont nameFont = nameLabel->font();
    nameFont.setPointSize(18);
    nameFont.setBold(true);
    nameLabel->setFont(nameFont);
    nameLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(nameLabel);

    // 3. Sürüm
    QLabel *verLabel = new QLabel("Sürüm 1.0.0");
    verLabel->setAlignment(Qt::AlignCenter);
    verLabel->setStyleSheet("color: #888; font-size: 11px;");
    layout->addWidget(verLabel);

    // 4. Açıklama / Credit
    QLabel *descLabel = new QLabel(
        "Gelişmiş Windows Ekran Alıntısı Aracı<br>"
        "<a href='https://github.com/Benoks/EShot' style='color: #4285F4; text-decoration: none;'>GitHub Sayfası</a>"
    );
    descLabel->setAlignment(Qt::AlignCenter);
    descLabel->setOpenExternalLinks(true);
    descLabel->setStyleSheet("color: #aaa; margin-top: 5px;");
    layout->addWidget(descLabel);

    layout->addStretch();

    // 5. Kapat Butonu
    QPushButton *closeBtn = new QPushButton("Kapat");
    closeBtn->setCursor(Qt::PointingHandCursor);
    closeBtn->setStyleSheet(R"(
        QPushButton {
            background-color: #333;
            color: white;
            border: 1px solid #555;
            border-radius: 4px;
            padding: 6px 12px;
        }
        QPushButton:hover {
            background-color: #444;
            border-color: #666;
        }
    )");
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::accept);
    
    // Ortala
    QHBoxLayout *btnLayout = new QHBoxLayout();
    btnLayout->addStretch();
    btnLayout->addWidget(closeBtn);
    btnLayout->addStretch();
    layout->addLayout(btnLayout);
}
