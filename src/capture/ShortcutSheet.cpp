#include "ShortcutSheet.h"

#include "core/TranslationManager.h"
#include "ui/OnboardingTips.h"

#include <QFontMetrics>
#include <QList>
#include <QMouseEvent>
#include <QPainter>
#include <QRegularExpression>
#include <QStringList>

namespace {

struct Row {
    QStringList keys;
    QString label;
};

struct Section {
    QString title;
    QList<Row> rows;
};

constexpr int RowHeight = 27;
constexpr int SectionTitleHeight = 26;
constexpr int SectionGap = 14;
constexpr int ColumnGap = 34;
constexpr int KeyLabelGap = 12;
constexpr int Padding = 26;

// Tool names carry their default key, e.g. "Pen (P)"; the sheet shows the
// configured key separately.
QString plainName(const QString &label)
{
    static const QRegularExpression suffix(QStringLiteral("\\s*\\([^)]*\\)\\s*$"));
    return QString(label).remove(suffix);
}

QString shortcut(const char *id, const char *fallback)
{
    return OnboardingTips::overlayShortcutText(QLatin1String(id), QLatin1String(fallback));
}

// Adds a row for a configurable shortcut unless the user disabled it.
void addShortcut(QList<Row> &rows, const char *id, const char *fallback, const QString &label)
{
    const QString key = shortcut(id, fallback);
    if (!key.isEmpty())
        rows.append({{key}, label});
}

QFont sheetFont(int pointSize, QFont::Weight weight)
{
    QFont font;
    font.setPointSize(pointSize);
    font.setWeight(weight);
    return font;
}

int sectionHeight(const Section &section)
{
    return SectionTitleHeight + section.rows.size() * RowHeight;
}

int columnHeight(const QList<Section> &column)
{
    int height = 0;
    for (int i = 0; i < column.size(); ++i)
        height += sectionHeight(column[i]) + (i ? SectionGap : 0);
    return height;
}

int keysWidth(const Row &row)
{
    const QFontMetrics metrics(sheetFont(9, QFont::DemiBold));
    int width = 0;
    for (int i = 0; i < row.keys.size(); ++i)
        width += metrics.horizontalAdvance(row.keys[i]) + 14 + (i ? 16 : 0);
    return width;
}

// Keys are right-aligned in a column as wide as the widest key of the
// column, so short single-letter columns do not waste space.
struct ColumnLayout {
    int keyWidth = 0;
    int labelWidth = 0;
};

ColumnLayout measureColumn(const QList<Section> &column)
{
    ColumnLayout layout;
    const QFontMetrics labels(sheetFont(10, QFont::Normal));
    const QFontMetrics titles(sheetFont(8, QFont::Bold));
    int titleWidth = 0;
    for (const Section &section : column) {
        titleWidth = qMax(titleWidth, titles.horizontalAdvance(section.title.toUpper()));
        for (const Row &row : section.rows) {
            layout.keyWidth = qMax(layout.keyWidth, keysWidth(row));
            layout.labelWidth = qMax(layout.labelWidth, labels.horizontalAdvance(row.label));
        }
    }
    // A few pixels of slack: text measured here can round a pixel wider when
    // drawn, which would otherwise elide the longest label.
    layout.labelWidth = qMax(layout.labelWidth + 6, titleWidth - layout.keyWidth - KeyLabelGap);
    return layout;
}

int drawKeyChip(QPainter &painter, int x, int y, const QString &key)
{
    const QFont font = sheetFont(9, QFont::DemiBold);
    const int width = QFontMetrics(font).horizontalAdvance(key) + 14;
    const QRect chip(x, y + 2, width, RowHeight - 6);
    // Same look as the overlay toolbar buttons.
    painter.setPen(QPen(QColor(0x50, 0x50, 0x50), 1));
    painter.setBrush(QColor(0x3a, 0x3a, 0x3a));
    painter.drawRoundedRect(chip, 6, 6);
    painter.setFont(font);
    painter.setPen(QColor(0xf5, 0xf5, 0xf5));
    painter.drawText(chip, Qt::AlignCenter, key);
    return width;
}

void drawSection(QPainter &painter, const Section &section, int x, int &y, int width,
                 int keyColumnWidth)
{
    painter.setFont(sheetFont(8, QFont::Bold));
    painter.setPen(QColor(0xa8, 0xa8, 0xa8));
    painter.drawText(QRect(x, y, width, SectionTitleHeight - 6),
                     Qt::AlignLeft | Qt::AlignVCenter, section.title.toUpper());
    y += SectionTitleHeight;

    const QFont labelFont = sheetFont(10, QFont::Normal);
    for (const Row &row : section.rows) {
        int keyX = x + qMax(0, keyColumnWidth - keysWidth(row));
        for (int i = 0; i < row.keys.size(); ++i) {
            if (i) {
                painter.setFont(sheetFont(9, QFont::Normal));
                painter.setPen(QColor(0x9a, 0x9a, 0x9a));
                painter.drawText(QRect(keyX, y, 16, RowHeight), Qt::AlignCenter, QStringLiteral("/"));
                keyX += 16;
            }
            keyX += drawKeyChip(painter, keyX, y, row.keys[i]);
        }

        const QRect labelRect(x + keyColumnWidth + KeyLabelGap, y,
                              width - keyColumnWidth - KeyLabelGap, RowHeight);
        painter.setFont(labelFont);
        painter.setPen(QColor(0xd6, 0xd6, 0xd6));
        painter.drawText(labelRect, Qt::AlignLeft | Qt::AlignVCenter,
                         QFontMetrics(labelFont).elidedText(row.label, Qt::ElideRight,
                                                            labelRect.width()));
        y += RowHeight;
    }
}

} // namespace

ShortcutSheetLayer::ShortcutSheetLayer(QWidget *parent)
    : QWidget(parent)
{
    setAttribute(Qt::WA_TranslucentBackground);
    setFocusPolicy(Qt::NoFocus);
    hide();
}

void ShortcutSheetLayer::open(const QRect &monitorRect)
{
    m_monitorRect = monitorRect;
    if (parentWidget())
        setGeometry(parentWidget()->rect());
    show();
    raise();
    update();
}

void ShortcutSheetLayer::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.fillRect(rect(), QColor(0, 0, 0, 110));
    drawShortcutSheet(painter, m_monitorRect.isValid() ? m_monitorRect : rect());
}

void ShortcutSheetLayer::mousePressEvent(QMouseEvent *event)
{
    event->accept();
    hide();
    emit closed();
}

void drawShortcutSheet(QPainter &painter, const QRect &monitorRect)
{
    using TM = TranslationManager;

    Section selection{TM::tr("sheetSectionSelection"), {
        {{TM::tr("keyDrag")}, TM::tr("sheetSelectArea")},
        {{TM::tr("keyDoubleClick")}, TM::tr("sheetSelectScreen")},
    }};
    const QString copyKey = shortcut("actionCopy", "Ctrl+C");
    selection.rows.append({copyKey.isEmpty() ? QStringList{QStringLiteral("Enter")}
                                             : QStringList{QStringLiteral("Enter"), copyKey},
                           TM::captureHintCopy()});
    addShortcut(selection.rows, "actionSave", "Ctrl+S", TM::captureHintSave());
    addShortcut(selection.rows, "actionPin", "Ctrl+P", plainName(TM::actionPin()));
    addShortcut(selection.rows, "actionLock", "K", plainName(TM::actionLock()));
    selection.rows.append({{QStringLiteral("Esc")}, TM::tr("sheetCancel")});

    Section more{TM::tr("sheetSectionMore"), {}};
    addShortcut(more.rows, "actionOcr", "Ctrl+O", plainName(TM::actionOcr()));
    addShortcut(more.rows, "actionUpload", "Ctrl+U", TM::uploadToService());
    addShortcut(more.rows, "actionGoogleLens", "Ctrl+L", TM::visualSearchAction());
    addShortcut(more.rows, "actionGif", "Ctrl+G", TM::recordingStartTitle());
    addShortcut(more.rows, "actionVideo", "Ctrl+Shift+V", TM::videoRecordingTitle());

    Section tools{TM::tr("sheetSectionTools"), {}};
    addShortcut(tools.rows, "toolPen", "P", plainName(TM::toolPen()));
    addShortcut(tools.rows, "toolArrow", "A", plainName(TM::toolArrow()));
    addShortcut(tools.rows, "toolLine", "L", plainName(TM::toolLine()));
    addShortcut(tools.rows, "toolRectangle", "R", plainName(TM::toolRect()));
    addShortcut(tools.rows, "toolCircle", "C", plainName(TM::toolCircle()));
    addShortcut(tools.rows, "toolSemiRect", "D", plainName(TM::toolSemiRect()));
    addShortcut(tools.rows, "toolHighlighter", "H", plainName(TM::toolHighlighter()));
    addShortcut(tools.rows, "toolText", "T", plainName(TM::toolText()));
    addShortcut(tools.rows, "toolBlur", "B", plainName(TM::toolBlur()));
    addShortcut(tools.rows, "toolPixelate", "M", plainName(TM::toolPixelate()));
    addShortcut(tools.rows, "toolCounter", "N", plainName(TM::toolCounter()));
    addShortcut(tools.rows, "toolEraser", "X", plainName(TM::toolEraser()));
    addShortcut(tools.rows, "actionEyedropper", "I", plainName(TM::toolEyedropper()));

    Section editing{TM::tr("sheetSectionEditing"), {}};
    addShortcut(editing.rows, "actionUndo", "Ctrl+Z", plainName(TM::toolUndo()));
    addShortcut(editing.rows, "actionRedo", "Ctrl+Shift+Z", plainName(TM::toolRedo()));
    editing.rows.append({{QStringLiteral("Del")}, TM::tr("sheetDeleteSelected")});
    editing.rows.append({{TM::tr("keyCtrlClick")}, TM::tr("sheetMoveObject")});
    editing.rows.append({{TM::tr("keyShiftDrag")}, TM::tr("sheetStraight")});
    editing.rows.append({{TM::tr("keyDoubleClick"), QStringLiteral("F2")}, TM::tr("sheetEditText")});

    const Section typing{TM::tr("sheetSectionText"), {
        {{QStringLiteral("Enter")}, TM::tr("sheetConfirm")},
        {{QStringLiteral("Shift+Enter")}, TM::tr("sheetNewLine")},
        {{QStringLiteral("Ctrl+B")}, TM::tr("textBold")},
        {{QStringLiteral("Ctrl +"), QStringLiteral("Ctrl −")}, TM::tr("sheetTextSize")},
    }};

    const QList<QList<Section>> columns = {{selection, more}, {tools}, {editing, typing}};

    const QString title = TM::tr("sheetTitle");
    const QString closeHint = TM::tr("sheetClose");
    const QString footer = TM::tr("sheetQuickSettings");

    QList<ColumnLayout> layouts;
    int contentWidth = ColumnGap * (columns.size() - 1);
    int contentHeight = 0;
    for (const auto &column : columns) {
        layouts.append(measureColumn(column));
        contentWidth += layouts.last().keyWidth + KeyLabelGap + layouts.last().labelWidth;
        contentHeight = qMax(contentHeight, columnHeight(column));
    }
    const int headerHeight = 50;
    const int footerHeight = 42;
    QSize cardSize(Padding * 2 + contentWidth,
                   Padding + headerHeight + contentHeight + footerHeight + Padding / 2);
    const QRect area = monitorRect.adjusted(24, 24, -24, -24);
    cardSize = cardSize.boundedTo(area.size());
    QRect card(QPoint(0, 0), cardSize);
    card.moveCenter(area.center());

    painter.save();
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setClipRect(card.adjusted(-2, -2, 2, 2));

    // Overlay panel palette: #2d2d2d body, #404040 border, soft drop shadow.
    painter.setPen(Qt::NoPen);
    for (int i = 1; i <= 6; ++i) {
        painter.setBrush(QColor(0, 0, 0, 22));
        painter.drawRoundedRect(card.adjusted(-i, -i + 4, i, i + 4), 10 + i, 10 + i);
    }
    painter.setPen(QPen(QColor(0x40, 0x40, 0x40), 1));
    painter.setBrush(QColor(0x2d, 0x2d, 0x2d));
    painter.drawRoundedRect(card, 10, 10);

    // Header: title on the left, how to close on the right.
    const int left = card.left() + Padding;
    const int right = card.right() - Padding;
    const QRect headerRect(left, card.top() + Padding - 6, right - left, 30);
    painter.setFont(sheetFont(15, QFont::DemiBold));
    painter.setPen(QColor(0xf5, 0xf5, 0xf5));
    painter.drawText(headerRect, Qt::AlignLeft | Qt::AlignVCenter, title);
    painter.setFont(sheetFont(9, QFont::Normal));
    painter.setPen(QColor(0x9a, 0x9a, 0x9a));
    painter.drawText(headerRect, Qt::AlignRight | Qt::AlignVCenter, closeHint);
    painter.setPen(QPen(QColor(0x40, 0x40, 0x40), 1));
    const int dividerY = headerRect.bottom() + 12;
    painter.drawLine(left, dividerY, right, dividerY);

    // On a small screen the card is narrower than measured; shrink the
    // label columns evenly and let long labels elide.
    const int overflow = qMax(0, Padding * 2 + contentWidth - card.width());
    const int shrink = (overflow + int(columns.size()) - 1) / int(columns.size());
    const int top = dividerY + 14;
    int x = left;
    for (int c = 0; c < columns.size(); ++c) {
        const int width = layouts[c].keyWidth + KeyLabelGap
            + qMax(40, layouts[c].labelWidth - shrink);
        int y = top;
        for (int s = 0; s < columns[c].size(); ++s) {
            if (s)
                y += SectionGap;
            drawSection(painter, columns[c][s], x, y, width, layouts[c].keyWidth);
        }
        x += width + ColumnGap;
    }

    // Footer: point at the Quick Settings tab, the easiest thing to miss.
    const QRect footerRect(left, card.bottom() - footerHeight + 2, right - left, footerHeight - 14);
    painter.setPen(QPen(QColor(0x50, 0x50, 0x50), 1));
    painter.setBrush(QColor(0x3a, 0x3a, 0x3a));
    painter.drawRoundedRect(footerRect, 8, 8);
    painter.setFont(sheetFont(10, QFont::Normal));
    painter.setPen(QColor(0xe6, 0xe6, 0xe6));
    painter.drawText(footerRect.adjusted(14, 0, -14, 0), Qt::AlignLeft | Qt::AlignVCenter,
                     QFontMetrics(painter.font()).elidedText(footer, Qt::ElideRight,
                                                             footerRect.width() - 28));
    painter.restore();
}
