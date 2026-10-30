#include "KWinWindowList.h"

#include "core/LinuxKWinScript.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPointF>


namespace KWinWindowList {

namespace {

QRect rectFromJson(const QJsonValue &value)
{
    const QJsonArray array = value.toArray();
    if (array.size() != 4)
        return {};
    return QRect(qRound(array.at(0).toDouble()), qRound(array.at(1).toDouble()),
                 qRound(array.at(2).toDouble()), qRound(array.at(3).toDouble()));
}

QStringList stringsFromJson(const QJsonValue &value)
{
    QStringList strings;
    const QJsonArray array = value.toArray();
    for (const QJsonValue &item : array) {
        const QString text = item.isString() ? item.toString()
                                             : QString::number(item.toDouble());
        if (!text.isEmpty())
            strings.append(text);
    }
    return strings;
}

struct OutputScreenPair {
    QRect output;
    QRect screen;
};

QVector<OutputScreenPair> pairOutputsWithScreens(const QVector<KWinOutputInfo> &outputs,
                                                 const QVector<KWinQtScreen> &screens)
{
    QVector<OutputScreenPair> pairs;
    QVector<bool> screenUsed(screens.size(), false);
    QVector<bool> outputPaired(outputs.size(), false);
    for (int i = 0; i < outputs.size(); ++i) {
        const KWinOutputInfo &output = outputs.at(i);
        if (output.name.isEmpty() || output.geometry.isEmpty())
            continue;
        for (int j = 0; j < screens.size(); ++j) {
            if (!screenUsed.at(j) && screens.at(j).name == output.name
                && !screens.at(j).geometry.isEmpty()) {
                pairs.append({output.geometry, screens.at(j).geometry});
                screenUsed[j] = true;
                outputPaired[i] = true;
                break;
            }
        }
    }
    // KWin 5 reports outputs without names; identical geometry is the
    // unscaled case and maps one to one.
    for (int i = 0; i < outputs.size(); ++i) {
        if (outputPaired.at(i) || outputs.at(i).geometry.isEmpty())
            continue;
        for (int j = 0; j < screens.size(); ++j) {
            if (!screenUsed.at(j) && screens.at(j).geometry == outputs.at(i).geometry) {
                pairs.append({outputs.at(i).geometry, screens.at(j).geometry});
                screenUsed[j] = true;
                break;
            }
        }
    }
    return pairs;
}

QPointF mapPoint(const QPointF &point, const OutputScreenPair &pair)
{
    const qreal sx = pair.screen.width() / qreal(pair.output.width());
    const qreal sy = pair.screen.height() / qreal(pair.output.height());
    return QPointF(pair.screen.x() + (point.x() - pair.output.x()) * sx,
                   pair.screen.y() + (point.y() - pair.output.y()) * sy);
}

const OutputScreenPair *pairContaining(const QVector<OutputScreenPair> &pairs, const QPoint &point)
{
    for (const OutputScreenPair &pair : pairs) {
        if (pair.output.contains(point))
            return &pair;
    }
    return nullptr;
}

}

QString script(const QString &service, const QString &path, const QString &interface,
               const QString &method, const QString &token)
{
    using LinuxKWinScript::jsString;
    return QStringLiteral(
        "(function () {\n"
        "    var service = %1;\n"
        "    var path = %2;\n"
        "    var iface = %3;\n"
        "    var method = %4;\n"
        "    var token = %5;\n"
        "    function rect(g) {\n"
        "        return g ? [Math.round(g.x), Math.round(g.y), Math.round(g.width), Math.round(g.height)]\n"
        "                 : [0, 0, 0, 0];\n"
        "    }\n"
        "    function flag(w, name) { return w[name] === true; }\n"
        "    var result = { version: 1, outputs: [], windows: [], currentDesktop: \"\", currentActivity: \"\" };\n"
        "    try {\n"
        "        var screens = workspace.screens;\n"
        "        if (screens && screens.length !== undefined) {\n"
        "            for (var s = 0; s < screens.length; ++s)\n"
        "                result.outputs.push({ name: String(screens[s].name || \"\"), geometry: rect(screens[s].geometry) });\n"
        "        } else if (typeof workspace.numScreens === \"number\") {\n"
        "            for (var n = 0; n < workspace.numScreens; ++n)\n"
        "                result.outputs.push({ name: \"\", geometry: rect(workspace.clientArea(KWin.ScreenArea, n, workspace.currentDesktop)) });\n"
        "        }\n"
        "        var desktop = workspace.currentDesktop;\n"
        "        result.currentDesktop = desktop && typeof desktop === \"object\" ? String(desktop.id) : String(desktop || \"\");\n"
        "        result.currentActivity = String(workspace.currentActivity || \"\");\n"
        "        var list = workspace.stackingOrder;\n"
        "        if (!list || list.length === undefined) {\n"
        "            list = typeof workspace.windowList === \"function\" ? workspace.windowList() : workspace.clientList();\n"
        "            list = Array.prototype.slice.call(list);\n"
        "            list.sort(function (a, b) { return (a.stackingOrder || 0) - (b.stackingOrder || 0); });\n"
        "        }\n"
        "        for (var i = list.length - 1; i >= 0; --i) {\n"
        "            var w = list[i];\n"
        "            if (!w)\n"
        "                continue;\n"
        "            var desktops = [];\n"
        "            if (w.desktops && w.desktops.length !== undefined) {\n"
        "                for (var d = 0; d < w.desktops.length; ++d)\n"
        "                    desktops.push(String(w.desktops[d].id));\n"
        "            } else if (typeof w.desktop === \"number\" && w.desktop > 0) {\n"
        "                desktops.push(String(w.desktop));\n"
        "            }\n"
        "            var activities = [];\n"
        "            if (w.activities && w.activities.length !== undefined) {\n"
        "                for (var a = 0; a < w.activities.length; ++a)\n"
        "                    activities.push(String(w.activities[a]));\n"
        "            }\n"
        "            result.windows.push({\n"
        "                geometry: rect(w.frameGeometry),\n"
        "                caption: String(w.caption || \"\"),\n"
        "                resourceClass: String(w.resourceClass || \"\"),\n"
        "                pid: Number(w.pid || 0),\n"
        "                managed: w.managed !== false,\n"
        "                deleted: flag(w, \"deleted\"),\n"
        "                minimized: flag(w, \"minimized\"),\n"
        "                hidden: flag(w, \"hidden\"),\n"
        "                onAllDesktops: flag(w, \"onAllDesktops\"),\n"
        "                desktops: desktops,\n"
        "                activities: activities,\n"
        "                normalWindow: flag(w, \"normalWindow\"),\n"
        "                dialog: flag(w, \"dialog\"),\n"
        "                utility: flag(w, \"utility\"),\n"
        "                desktopWindow: flag(w, \"desktopWindow\"),\n"
        "                dock: flag(w, \"dock\"),\n"
        "                toolbar: flag(w, \"toolbar\"),\n"
        "                menu: flag(w, \"menu\"),\n"
        "                splash: flag(w, \"splash\"),\n"
        "                dropdownMenu: flag(w, \"dropdownMenu\"),\n"
        "                popupMenu: flag(w, \"popupMenu\"),\n"
        "                tooltip: flag(w, \"tooltip\"),\n"
        "                notification: flag(w, \"notification\"),\n"
        "                criticalNotification: flag(w, \"criticalNotification\"),\n"
        "                onScreenDisplay: flag(w, \"onScreenDisplay\"),\n"
        "                appletPopup: flag(w, \"appletPopup\"),\n"
        "                comboBox: flag(w, \"comboBox\"),\n"
        "                dndIcon: flag(w, \"dndIcon\"),\n"
        "                popupWindow: flag(w, \"popupWindow\"),\n"
        "                outline: flag(w, \"outline\"),\n"
        "                fullScreen: flag(w, \"fullScreen\"),\n"
        "                keepAbove: flag(w, \"keepAbove\")\n"
        "            });\n"
        "        }\n"
        "    } catch (e) {\n"
        "        result.error = String(e);\n"
        "    }\n"
        "    callDBus(service, path, iface, method, token, JSON.stringify(result));\n"
        "})();\n")
        .arg(jsString(service), jsString(path), jsString(interface), jsString(method),
             jsString(token));
}

QString pluginName(qint64 pid)
{
    return QStringLiteral("eshot-windows-%1").arg(pid);
}

KWinWindowSnapshot parse(const QByteArray &json)
{
    KWinWindowSnapshot snapshot;
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(json, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject())
        return snapshot;
    const QJsonObject root = document.object();
    if (!root.value(QStringLiteral("windows")).isArray())
        return snapshot;

    snapshot.valid = true;
    snapshot.error = root.value(QStringLiteral("error")).toString();
    snapshot.currentDesktop = root.value(QStringLiteral("currentDesktop")).toString();
    snapshot.currentActivity = root.value(QStringLiteral("currentActivity")).toString();
    const QJsonArray outputs = root.value(QStringLiteral("outputs")).toArray();
    for (const QJsonValue &value : outputs) {
        const QJsonObject object = value.toObject();
        snapshot.outputs.append({object.value(QStringLiteral("name")).toString(),
                                 rectFromJson(object.value(QStringLiteral("geometry")))});
    }
    const QJsonArray windows = root.value(QStringLiteral("windows")).toArray();
    for (const QJsonValue &value : windows) {
        const QJsonObject o = value.toObject();
        auto flag = [&o](const char *name, bool fallback = false) {
            const QJsonValue v = o.value(QLatin1String(name));
            return v.isBool() ? v.toBool() : fallback;
        };
        KWinWindowInfo window;
        window.frame = rectFromJson(o.value(QStringLiteral("geometry")));
        window.caption = o.value(QStringLiteral("caption")).toString();
        window.resourceClass = o.value(QStringLiteral("resourceClass")).toString();
        window.pid = static_cast<qint64>(o.value(QStringLiteral("pid")).toDouble());
        window.managed = flag("managed", true);
        window.deleted = flag("deleted");
        window.minimized = flag("minimized");
        window.hidden = flag("hidden");
        window.onAllDesktops = flag("onAllDesktops");
        window.desktops = stringsFromJson(o.value(QStringLiteral("desktops")));
        window.activities = stringsFromJson(o.value(QStringLiteral("activities")));
        window.normalWindow = flag("normalWindow");
        window.dialog = flag("dialog");
        window.utility = flag("utility");
        window.desktopWindow = flag("desktopWindow");
        window.dock = flag("dock");
        window.toolbar = flag("toolbar");
        window.menu = flag("menu");
        window.splash = flag("splash");
        window.dropdownMenu = flag("dropdownMenu");
        window.popupMenu = flag("popupMenu");
        window.tooltip = flag("tooltip");
        window.notification = flag("notification");
        window.criticalNotification = flag("criticalNotification");
        window.onScreenDisplay = flag("onScreenDisplay");
        window.appletPopup = flag("appletPopup");
        window.comboBox = flag("comboBox");
        window.dndIcon = flag("dndIcon");
        window.popupWindow = flag("popupWindow");
        window.outline = flag("outline");
        window.fullScreen = flag("fullScreen");
        window.keepAbove = flag("keepAbove");
        snapshot.windows.append(window);
    }
    return snapshot;
}

bool isSelectable(const KWinWindowInfo &window, const KWinWindowSnapshot &snapshot,
                  qint64 ownPid)
{
    if (ownPid > 0 && window.pid == ownPid)
        return false;
    if (!window.managed || window.deleted || window.minimized || window.hidden)
        return false;
    if (window.desktopWindow || window.dock || window.menu || window.splash
        || window.dropdownMenu || window.popupMenu || window.tooltip
        || window.notification || window.criticalNotification || window.onScreenDisplay
        || window.appletPopup || window.comboBox || window.dndIcon || window.popupWindow
        || window.outline) {
        return false;
    }
    if (window.frame.width() <= 0 || window.frame.height() <= 0)
        return false;
    if (!window.onAllDesktops && !window.desktops.isEmpty()
        && !snapshot.currentDesktop.isEmpty()
        && !window.desktops.contains(snapshot.currentDesktop)) {
        return false;
    }
    if (!window.activities.isEmpty() && !snapshot.currentActivity.isEmpty()
        && !window.activities.contains(snapshot.currentActivity)) {
        return false;
    }
    return true;
}

QRect mapToQt(const QRect &compositorRect, const QVector<KWinOutputInfo> &outputs,
              const QVector<KWinQtScreen> &screens)
{
    if (compositorRect.isEmpty())
        return {};
    const QVector<OutputScreenPair> pairs = pairOutputsWithScreens(outputs, screens);
    if (pairs.isEmpty())
        return compositorRect;

    // A corner outside every output (window partly off-screen) uses the
    // output that shows most of the window.
    const OutputScreenPair *fallback = nullptr;
    qint64 bestArea = 0;
    for (const OutputScreenPair &pair : pairs) {
        const QRect overlap = pair.output.intersected(compositorRect);
        const qint64 area = qint64(overlap.width()) * overlap.height();
        if (area > bestArea) {
            bestArea = area;
            fallback = &pair;
        }
    }
    if (!fallback)
        return compositorRect;

    const QPoint topLeft = compositorRect.topLeft();
    const QPoint bottomRightInside = compositorRect.bottomRight();
    const OutputScreenPair *topLeftPair = pairContaining(pairs, topLeft);
    const OutputScreenPair *bottomRightPair = pairContaining(pairs, bottomRightInside);
    const QPointF mappedTopLeft = mapPoint(QPointF(topLeft),
                                           topLeftPair ? *topLeftPair : *fallback);
    const QPointF mappedBottomRight = mapPoint(
        QPointF(compositorRect.x() + compositorRect.width(),
                compositorRect.y() + compositorRect.height()),
        bottomRightPair ? *bottomRightPair : *fallback);
    const int left = qRound(mappedTopLeft.x());
    const int top = qRound(mappedTopLeft.y());
    const int right = qRound(mappedBottomRight.x());
    const int bottom = qRound(mappedBottomRight.y());
    if (right <= left || bottom <= top)
        return {};
    return QRect(left, top, right - left, bottom - top);
}

QVector<QRect> overlayRects(const KWinWindowSnapshot &snapshot, qint64 ownPid,
                            const QVector<KWinQtScreen> &screens,
                            const QPoint &overlayOrigin, const QSize &overlaySize)
{
    QVector<QRect> rects;
    if (!snapshot.valid || overlaySize.isEmpty())
        return rects;
    const QRect bounds(QPoint(0, 0), overlaySize);
    for (const KWinWindowInfo &window : snapshot.windows) {
        if (!isSelectable(window, snapshot, ownPid))
            continue;
        const QRect local = mapToQt(window.frame, snapshot.outputs, screens)
                                .translated(-overlayOrigin)
                                .intersected(bounds);
        if (local.width() >= 16 && local.height() >= 16)
            rects.append(local);
    }
    return rects;
}

}
