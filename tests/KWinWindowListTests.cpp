#include <QtTest>

#include "capture/KWinWindowList.h"
#include "capture/KWinWindowListClient.h"
#include "capture/WindowSnapPolicy.h"

#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#ifdef ESHOT_TEST_HAVE_QJSENGINE
#include <QJSEngine>
#endif

namespace {

QJsonObject windowJson(const QRect &geometry, qint64 pid, const QString &caption = QString())
{
    return QJsonObject{
        {QStringLiteral("geometry"), QJsonArray{geometry.x(), geometry.y(), geometry.width(), geometry.height()}},
        {QStringLiteral("caption"), caption},
        {QStringLiteral("resourceClass"), QStringLiteral("app")},
        {QStringLiteral("pid"), double(pid)},
        {QStringLiteral("managed"), true},
        {QStringLiteral("normalWindow"), true},
        {QStringLiteral("desktops"), QJsonArray{QStringLiteral("desk-1")}},
        {QStringLiteral("activities"), QJsonArray{QStringLiteral("act-1")}},
    };
}

QByteArray snapshotJson(const QJsonArray &windows, const QJsonArray &outputs = {})
{
    return QJsonDocument(QJsonObject{
        {QStringLiteral("version"), 1},
        {QStringLiteral("currentDesktop"), QStringLiteral("desk-1")},
        {QStringLiteral("currentActivity"), QStringLiteral("act-1")},
        {QStringLiteral("outputs"), outputs},
        {QStringLiteral("windows"), windows},
    }).toJson(QJsonDocument::Compact);
}

KWinWindowInfo selectableWindow()
{
    KWinWindowInfo window;
    window.frame = QRect(10, 10, 400, 300);
    window.pid = 100;
    window.normalWindow = true;
    window.desktops = {QStringLiteral("desk-1")};
    window.activities = {QStringLiteral("act-1")};
    return window;
}

KWinWindowSnapshot currentDesktopSnapshot()
{
    KWinWindowSnapshot snapshot;
    snapshot.valid = true;
    snapshot.currentDesktop = QStringLiteral("desk-1");
    snapshot.currentActivity = QStringLiteral("act-1");
    return snapshot;
}

}

class KWinWindowListTests : public QObject
{
    Q_OBJECT

private slots:
    void scriptCallsBackWithServicePathAndToken()
    {
        const QString script = KWinWindowList::script(
            QStringLiteral(":1.42"), QStringLiteral("/io/github/benoks/EShot/KWinWindowList"),
            QStringLiteral("io.github.benoks.EShot.KWinWindowList"),
            QStringLiteral("deliverWindowList"), QStringLiteral("tok\"en"));
        QVERIFY(script.contains(QStringLiteral("var service = \":1.42\";")));
        QVERIFY(script.contains(QStringLiteral("var token = \"tok\\\"en\";")));
        QVERIFY(script.contains(QStringLiteral("callDBus(service, path, iface, method, token, JSON.stringify(result));")));
        // KWin 6 names.
        QVERIFY(script.contains(QStringLiteral("workspace.stackingOrder")));
        QVERIFY(script.contains(QStringLiteral("workspace.screens")));
        QVERIFY(script.contains(QStringLiteral("w.desktops")));
        // KWin 5 names.
        QVERIFY(script.contains(QStringLiteral("workspace.clientList()")));
        QVERIFY(script.contains(QStringLiteral("workspace.numScreens")));
        QVERIFY(script.contains(QStringLiteral("w.desktop > 0")));
        QVERIFY(script.contains(QStringLiteral("frameGeometry")));
    }

    void pluginNameIsPerProcess()
    {
        QCOMPARE(KWinWindowList::pluginName(77), QStringLiteral("eshot-windows-77"));
    }

    void parsesWindowsAndOutputs()
    {
        QJsonObject dialog = windowJson(QRect(5, 6, 70, 80), 9, QStringLiteral("Open"));
        dialog.insert(QStringLiteral("dialog"), true);
        dialog.insert(QStringLiteral("normalWindow"), false);
        dialog.insert(QStringLiteral("keepAbove"), true);
        const QByteArray json = snapshotJson(
            {dialog, windowJson(QRect(0, 0, 1920, 1040), 8, QStringLiteral("Editor"))},
            {QJsonObject{{QStringLiteral("name"), QStringLiteral("eDP-1")},
                         {QStringLiteral("geometry"), QJsonArray{0, 0, 1920, 1080}}}});
        const KWinWindowSnapshot snapshot = KWinWindowList::parse(json);
        QVERIFY(snapshot.valid);
        QCOMPARE(snapshot.currentDesktop, QStringLiteral("desk-1"));
        QCOMPARE(snapshot.currentActivity, QStringLiteral("act-1"));
        QCOMPARE(snapshot.outputs.size(), 1);
        QCOMPARE(snapshot.outputs.first().name, QStringLiteral("eDP-1"));
        QCOMPARE(snapshot.outputs.first().geometry, QRect(0, 0, 1920, 1080));
        QCOMPARE(snapshot.windows.size(), 2);
        QCOMPARE(snapshot.windows.at(0).frame, QRect(5, 6, 70, 80));
        QCOMPARE(snapshot.windows.at(0).caption, QStringLiteral("Open"));
        QCOMPARE(snapshot.windows.at(0).pid, qint64(9));
        QVERIFY(snapshot.windows.at(0).dialog);
        QVERIFY(snapshot.windows.at(0).keepAbove);
        QVERIFY(!snapshot.windows.at(0).normalWindow);
        QCOMPARE(snapshot.windows.at(1).desktops, QStringList{QStringLiteral("desk-1")});
    }

    void rejectsMalformedReplies()
    {
        QVERIFY(!KWinWindowList::parse("").valid);
        QVERIFY(!KWinWindowList::parse("not json").valid);
        QVERIFY(!KWinWindowList::parse("[]").valid);
        QVERIFY(!KWinWindowList::parse("{\"outputs\":[]}").valid);
        const KWinWindowSnapshot withError = KWinWindowList::parse(
            "{\"windows\":[],\"error\":\"TypeError\"}");
        QVERIFY(withError.valid);
        QCOMPARE(withError.error, QStringLiteral("TypeError"));
    }

    void filtersWindowsThatCannotBeSelected()
    {
        const KWinWindowSnapshot snapshot = currentDesktopSnapshot();
        QVERIFY(KWinWindowList::isSelectable(selectableWindow(), snapshot, 1));

        KWinWindowInfo own = selectableWindow();
        own.pid = 1;
        QVERIFY(!KWinWindowList::isSelectable(own, snapshot, 1));

        const auto rejectedWith = [&](bool KWinWindowInfo::*member) {
            KWinWindowInfo window = selectableWindow();
            window.*member = true;
            return !KWinWindowList::isSelectable(window, snapshot, 1);
        };
        QVERIFY(rejectedWith(&KWinWindowInfo::minimized));
        QVERIFY(rejectedWith(&KWinWindowInfo::hidden));
        QVERIFY(rejectedWith(&KWinWindowInfo::deleted));
        QVERIFY(rejectedWith(&KWinWindowInfo::desktopWindow));
        QVERIFY(rejectedWith(&KWinWindowInfo::dock));
        QVERIFY(rejectedWith(&KWinWindowInfo::notification));
        QVERIFY(rejectedWith(&KWinWindowInfo::criticalNotification));
        QVERIFY(rejectedWith(&KWinWindowInfo::onScreenDisplay));
        QVERIFY(rejectedWith(&KWinWindowInfo::tooltip));
        QVERIFY(rejectedWith(&KWinWindowInfo::popupMenu));
        QVERIFY(rejectedWith(&KWinWindowInfo::dropdownMenu));
        QVERIFY(rejectedWith(&KWinWindowInfo::menu));
        QVERIFY(rejectedWith(&KWinWindowInfo::splash));
        QVERIFY(rejectedWith(&KWinWindowInfo::appletPopup));
        QVERIFY(rejectedWith(&KWinWindowInfo::popupWindow));
        QVERIFY(rejectedWith(&KWinWindowInfo::comboBox));
        QVERIFY(rejectedWith(&KWinWindowInfo::dndIcon));
        QVERIFY(rejectedWith(&KWinWindowInfo::outline));

        KWinWindowInfo unmanaged = selectableWindow();
        unmanaged.managed = false;
        QVERIFY(!KWinWindowList::isSelectable(unmanaged, snapshot, 1));

        KWinWindowInfo empty = selectableWindow();
        empty.frame = QRect(10, 10, 0, 300);
        QVERIFY(!KWinWindowList::isSelectable(empty, snapshot, 1));

        // Dialogs, utilities, fullscreen and keep-above windows stay.
        KWinWindowInfo dialog = selectableWindow();
        dialog.normalWindow = false;
        dialog.dialog = true;
        dialog.fullScreen = true;
        dialog.keepAbove = true;
        QVERIFY(KWinWindowList::isSelectable(dialog, snapshot, 1));
    }

    void keepsOnlyWindowsOnTheCurrentDesktopAndActivity()
    {
        const KWinWindowSnapshot snapshot = currentDesktopSnapshot();

        KWinWindowInfo otherDesktop = selectableWindow();
        otherDesktop.desktops = {QStringLiteral("desk-2")};
        QVERIFY(!KWinWindowList::isSelectable(otherDesktop, snapshot, 1));
        otherDesktop.onAllDesktops = true;
        QVERIFY(KWinWindowList::isSelectable(otherDesktop, snapshot, 1));

        KWinWindowInfo everyDesktop = selectableWindow();
        everyDesktop.desktops.clear();
        QVERIFY(KWinWindowList::isSelectable(everyDesktop, snapshot, 1));

        KWinWindowInfo otherActivity = selectableWindow();
        otherActivity.activities = {QStringLiteral("act-2")};
        QVERIFY(!KWinWindowList::isSelectable(otherActivity, snapshot, 1));
        otherActivity.activities.append(QStringLiteral("act-1"));
        QVERIFY(KWinWindowList::isSelectable(otherActivity, snapshot, 1));

        KWinWindowInfo everyActivity = selectableWindow();
        everyActivity.activities.clear();
        QVERIFY(KWinWindowList::isSelectable(everyActivity, snapshot, 1));
    }

    void mapsIdentityAtScaleOne()
    {
        const QVector<KWinOutputInfo> outputs = {
            {QStringLiteral("eDP-1"), QRect(0, 0, 1920, 1080)},
            {QStringLiteral("HDMI-A-1"), QRect(1920, 0, 1920, 1080)}};
        const QVector<KWinQtScreen> screens = {
            {QStringLiteral("HDMI-A-1"), QRect(1920, 0, 1920, 1080)},
            {QStringLiteral("eDP-1"), QRect(0, 0, 1920, 1080)}};
        QCOMPARE(KWinWindowList::mapToQt(QRect(2000, 30, 640, 480), outputs, screens),
                 QRect(2000, 30, 640, 480));
        // A window spanning both outputs.
        QCOMPARE(KWinWindowList::mapToQt(QRect(1500, 0, 1000, 500), outputs, screens),
                 QRect(1500, 0, 1000, 500));
    }

    void mapsPerOutputWithXWaylandScaling()
    {
        // KWin logical: a 1.5x 2560x1440 panel (1707x960 logical) left of a
        // 1x 1920x1080 one. XWayland applications that scale themselves see
        // every output scaled by 1.5 (the largest scale).
        const QVector<KWinOutputInfo> outputs = {
            {QStringLiteral("eDP-1"), QRect(0, 0, 1707, 960)},
            {QStringLiteral("DP-2"), QRect(1707, 0, 1920, 1080)}};
        const QVector<KWinQtScreen> screens = {
            {QStringLiteral("eDP-1"), QRect(0, 0, 2560, 1440)},
            {QStringLiteral("DP-2"), QRect(2560, 0, 2880, 1620)}};
        QCOMPARE(KWinWindowList::mapToQt(QRect(1707, 0, 1920, 1080), outputs, screens),
                 QRect(2560, 0, 2880, 1620));
        QCOMPARE(KWinWindowList::mapToQt(QRect(100, 200, 400, 300), outputs, screens),
                 QRect(150, 300, 600, 450));
        // Each corner uses the output it lies on (1607 * 2560 / 1707 = 2410).
        QCOMPARE(KWinWindowList::mapToQt(QRect(1607, 0, 200, 100), outputs, screens),
                 QRect(2410, 0, 300, 150));
    }

    void mapsOutputsWithoutNamesByGeometryOrUnchanged()
    {
        const QVector<KWinOutputInfo> unnamed = {{QString(), QRect(0, 0, 1920, 1080)}};
        const QVector<KWinQtScreen> screens = {{QStringLiteral(":0.0"), QRect(0, 0, 1920, 1080)}};
        QCOMPARE(KWinWindowList::mapToQt(QRect(10, 20, 30, 40), unnamed, screens),
                 QRect(10, 20, 30, 40));
        QCOMPARE(KWinWindowList::mapToQt(QRect(10, 20, 30, 40), {}, screens),
                 QRect(10, 20, 30, 40));
        // A window partly off-screen keeps the scale of its visible output.
        const QVector<KWinOutputInfo> scaled = {{QStringLiteral("A"), QRect(0, 0, 1000, 1000)}};
        const QVector<KWinQtScreen> doubled = {{QStringLiteral("A"), QRect(0, 0, 2000, 2000)}};
        QCOMPARE(KWinWindowList::mapToQt(QRect(-100, 100, 300, 300), scaled, doubled),
                 QRect(-200, 200, 600, 600));
    }

    void buildsOverlayRectsTopmostFirst()
    {
        const QByteArray json = snapshotJson(
            {windowJson(QRect(100, 100, 300, 200), 5, QStringLiteral("top")),
             windowJson(QRect(0, 0, 1920, 1040), 6, QStringLiteral("below")),
             windowJson(QRect(50, 50, 400, 400), 42, QStringLiteral("eshot")),
             windowJson(QRect(1920, 0, 1920, 1040), 7, QStringLiteral("right")),
             windowJson(QRect(10, 10, 10, 10), 8, QStringLiteral("tiny"))},
            {QJsonObject{{QStringLiteral("name"), QStringLiteral("eDP-1")},
                         {QStringLiteral("geometry"), QJsonArray{0, 0, 1920, 1080}}},
             QJsonObject{{QStringLiteral("name"), QStringLiteral("HDMI-A-1")},
                         {QStringLiteral("geometry"), QJsonArray{1920, 0, 1920, 1080}}}});
        const KWinWindowSnapshot snapshot = KWinWindowList::parse(json);
        const QVector<KWinQtScreen> screens = {
            {QStringLiteral("eDP-1"), QRect(0, 0, 1920, 1080)},
            {QStringLiteral("HDMI-A-1"), QRect(1920, 0, 1920, 1080)}};

        const QVector<QRect> rects = KWinWindowList::overlayRects(
            snapshot, 42, screens, QPoint(0, 0), QSize(3840, 1080));
        QCOMPARE(rects, (QVector<QRect>{QRect(100, 100, 300, 200), QRect(0, 0, 1920, 1040),
                                        QRect(1920, 0, 1920, 1040)}));
        const QRect bounds(0, 0, 3840, 1080);
        QCOMPARE(topmostWindowAt(rects, QPoint(150, 150), bounds), QRect(100, 100, 300, 200));
        QCOMPARE(topmostWindowAt(rects, QPoint(600, 600), bounds), QRect(0, 0, 1920, 1040));
        QCOMPARE(topmostWindowAt(rects, QPoint(2000, 10), bounds), QRect(1920, 0, 1920, 1040));
        QVERIFY(topmostWindowAt(rects, QPoint(600, 1060), bounds).isEmpty());

        // Single-output overlay (native Wayland fallback on the right output).
        const QVector<QRect> right = KWinWindowList::overlayRects(
            snapshot, 42, screens, QPoint(1920, 0), QSize(1920, 1080));
        QCOMPARE(right, (QVector<QRect>{QRect(0, 0, 1920, 1040)}));
        QVERIFY(KWinWindowList::overlayRects(KWinWindowSnapshot(), 42, screens,
                                             QPoint(), QSize(100, 100)).isEmpty());
    }

    void windowCaptureOnLinuxKeepsDragSelection()
    {
        QVERIFY(allowsManualSelection(CaptureSelectionMode::Window, true));
        QVERIFY(!allowsManualSelection(CaptureSelectionMode::Window, false));
        QVERIFY(allowsManualSelection(CaptureSelectionMode::FreeRegion, false));
    }

    void clientIgnoresRepliesWithoutAPendingRequest()
    {
        KWinWindowListClient client;
        QSignalSpy received(&client, &KWinWindowListClient::windowListReceived);
        client.deliverWindowList(QStringLiteral("stale"), QString::fromUtf8(snapshotJson({})));
        QCOMPARE(received.count(), 0);
        QVERIFY(!client.isPending());
    }

#ifdef ESHOT_TEST_HAVE_QJSENGINE
    void scriptRunsAgainstKWin6Api()
    {
        QJSEngine engine;
        engine.evaluate(QStringLiteral(
            "var captured = null;\n"
            "function callDBus(service, path, iface, method, token, json) {\n"
            "    captured = { service: service, path: path, iface: iface, method: method, token: token, json: json };\n"
            "}\n"
            "function geo(x, y, w, h) { return { x: x, y: y, width: w, height: h }; }\n"
            "var desk1 = { id: 'd1' }, desk2 = { id: 'd2' };\n"
            "var workspace = {\n"
            "    screens: [ { name: 'eDP-1', geometry: geo(0, 0, 1920, 1080) } ],\n"
            "    currentDesktop: desk1,\n"
            "    currentActivity: 'a1',\n"
            "    stackingOrder: [\n"
            "        { caption: 'bottom', pid: 3, frameGeometry: geo(0, 0, 800, 600), normalWindow: true, desktops: [desk1], activities: ['a1'], managed: true },\n"
            "        { caption: 'panel', pid: 2, frameGeometry: geo(0, 1040, 1920, 40), dock: true, desktops: [], activities: [], onAllDesktops: true },\n"
            "        { caption: 'top', pid: 4, frameGeometry: geo(10.4, 20.6, 300, 200), dialog: true, desktops: [desk2], activities: [] }\n"
            "    ]\n"
            "};\n"));
        const QJSValue result = engine.evaluate(KWinWindowList::script(
            QStringLiteral(":1.5"), QStringLiteral("/p"), QStringLiteral("i.f"),
            QStringLiteral("m"), QStringLiteral("t1")));
        QVERIFY2(!result.isError(), qPrintable(result.toString()));
        const QJSValue captured = engine.globalObject().property(QStringLiteral("captured"));
        QCOMPARE(captured.property(QStringLiteral("service")).toString(), QStringLiteral(":1.5"));
        QCOMPARE(captured.property(QStringLiteral("token")).toString(), QStringLiteral("t1"));
        const KWinWindowSnapshot snapshot = KWinWindowList::parse(
            captured.property(QStringLiteral("json")).toString().toUtf8());
        QVERIFY(snapshot.valid);
        QVERIFY2(snapshot.error.isEmpty(), qPrintable(snapshot.error));
        QCOMPARE(snapshot.currentDesktop, QStringLiteral("d1"));
        QCOMPARE(snapshot.currentActivity, QStringLiteral("a1"));
        QCOMPARE(snapshot.outputs.size(), 1);
        QCOMPARE(snapshot.windows.size(), 3);
        QCOMPARE(snapshot.windows.at(0).caption, QStringLiteral("top"));
        QCOMPARE(snapshot.windows.at(0).frame, QRect(10, 21, 300, 200));
        QCOMPARE(snapshot.windows.at(0).desktops, QStringList{QStringLiteral("d2")});
        QVERIFY(snapshot.windows.at(0).dialog);
        QVERIFY(snapshot.windows.at(1).dock);
        QCOMPARE(snapshot.windows.at(2).caption, QStringLiteral("bottom"));
        QVERIFY(!KWinWindowList::isSelectable(snapshot.windows.at(0), snapshot, 1));
        QVERIFY(!KWinWindowList::isSelectable(snapshot.windows.at(1), snapshot, 1));
        QVERIFY(KWinWindowList::isSelectable(snapshot.windows.at(2), snapshot, 1));
    }

    void scriptRunsAgainstKWin5Api()
    {
        QJSEngine engine;
        engine.evaluate(QStringLiteral(
            "var captured = null;\n"
            "function callDBus(service, path, iface, method, token, json) { captured = json; }\n"
            "function geo(x, y, w, h) { return { x: x, y: y, width: w, height: h }; }\n"
            "var KWin = { ScreenArea: 7 };\n"
            "var workspace = {\n"
            "    numScreens: 2,\n"
            "    currentDesktop: 2,\n"
            "    currentActivity: 'a1',\n"
            "    clientArea: function (area, screen, desktop) { return screen === 0 ? geo(0, 0, 1920, 1080) : geo(1920, 0, 1280, 1024); },\n"
            "    clientList: function () { return [\n"
            "        { caption: 'upper', pid: 5, stackingOrder: 9, frameGeometry: geo(5, 5, 100, 100), normalWindow: true, desktop: -1, onAllDesktops: true, activities: [] },\n"
            "        { caption: 'lower', pid: 6, stackingOrder: 1, frameGeometry: geo(0, 0, 500, 500), normalWindow: true, desktop: 2, activities: ['a1'] }\n"
            "    ]; }\n"
            "};\n"));
        const QJSValue result = engine.evaluate(KWinWindowList::script(
            QStringLiteral(":1.5"), QStringLiteral("/p"), QStringLiteral("i.f"),
            QStringLiteral("m"), QStringLiteral("t1")));
        QVERIFY2(!result.isError(), qPrintable(result.toString()));
        const KWinWindowSnapshot snapshot = KWinWindowList::parse(
            engine.globalObject().property(QStringLiteral("captured")).toString().toUtf8());
        QVERIFY(snapshot.valid);
        QVERIFY2(snapshot.error.isEmpty(), qPrintable(snapshot.error));
        QCOMPARE(snapshot.currentDesktop, QStringLiteral("2"));
        QCOMPARE(snapshot.outputs.size(), 2);
        QCOMPARE(snapshot.outputs.at(1).geometry, QRect(1920, 0, 1280, 1024));
        QVERIFY(snapshot.outputs.at(1).name.isEmpty());
        QCOMPARE(snapshot.windows.size(), 2);
        QCOMPARE(snapshot.windows.at(0).caption, QStringLiteral("upper"));
        QVERIFY(snapshot.windows.at(0).desktops.isEmpty());
        QCOMPARE(snapshot.windows.at(1).desktops, QStringList{QStringLiteral("2")});
        QVERIFY(KWinWindowList::isSelectable(snapshot.windows.at(0), snapshot, 1));
        QVERIFY(KWinWindowList::isSelectable(snapshot.windows.at(1), snapshot, 1));
    }
#endif

    // Opt-in end-to-end check against the running KWin:
    // ESHOT_TEST_LIVE_KWIN=1 ctest -R kwin_window_list
    void liveKWinAnswersWithinTheTimeout()
    {
        if (qEnvironmentVariableIntValue("ESHOT_TEST_LIVE_KWIN") != 1)
            QSKIP("set ESHOT_TEST_LIVE_KWIN=1 to query the running KWin");
        QDBusConnection bus = QDBusConnection::sessionBus();
        if (!bus.isConnected() || !bus.interface()
            || !bus.interface()->isServiceRegistered(QStringLiteral("org.kde.KWin")).value()) {
            QSKIP("KWin is not on the session bus");
        }
        KWinWindowListClient client;
        QSignalSpy received(&client, &KWinWindowListClient::windowListReceived);
        QSignalSpy unavailable(&client, &KWinWindowListClient::windowListUnavailable);
        QElapsedTimer timer;
        timer.start();
        QVERIFY(client.request(1000));
        QTRY_VERIFY_WITH_TIMEOUT(!received.isEmpty() || !unavailable.isEmpty(), 3000);
        QCOMPARE(unavailable.count(), 0);
        const auto snapshot = received.first().first().value<KWinWindowSnapshot>();
        qInfo() << "live KWin reply after" << timer.elapsed() << "ms:"
                << snapshot.windows.size() << "windows," << snapshot.outputs.size() << "outputs";
        QVERIFY(snapshot.valid);
        QVERIFY(!snapshot.outputs.isEmpty());
        QVERIFY(!client.isPending());
    }

    // A screenshot can block the GUI thread past the timeout; the reply that
    // is already queued must still be used.
    void liveKWinReplySurvivesABlockedGuiThread()
    {
        if (qEnvironmentVariableIntValue("ESHOT_TEST_LIVE_KWIN") != 1)
            QSKIP("set ESHOT_TEST_LIVE_KWIN=1 to query the running KWin");
        QDBusConnection bus = QDBusConnection::sessionBus();
        if (!bus.isConnected() || !bus.interface()
            || !bus.interface()->isServiceRegistered(QStringLiteral("org.kde.KWin")).value()) {
            QSKIP("KWin is not on the session bus");
        }
        KWinWindowListClient client;
        QSignalSpy received(&client, &KWinWindowListClient::windowListReceived);
        QSignalSpy unavailable(&client, &KWinWindowListClient::windowListUnavailable);
        QVERIFY(client.request(300));
        QThread::msleep(600);
        QTRY_VERIFY_WITH_TIMEOUT(!received.isEmpty() || !unavailable.isEmpty(), 3000);
        QCOMPARE(unavailable.count(), 0);
        QCOMPARE(received.count(), 1);
    }
};

QTEST_GUILESS_MAIN(KWinWindowListTests)
#include "KWinWindowListTests.moc"
