#include <QtTest>

#include <type_traits>

#include "recording/ScreenRecorder.h"
#include "recording/VideoRecorder.h"

static_assert(std::is_same_v<decltype(&ScreenRecorder::pause), void (ScreenRecorder::*)()>);
static_assert(std::is_same_v<decltype(&ScreenRecorder::resume), void (ScreenRecorder::*)()>);
static_assert(std::is_same_v<decltype(&ScreenRecorder::isPaused), bool (ScreenRecorder::*)() const>);
static_assert(std::is_same_v<decltype(&ScreenRecorder::pausedChanged),
                             void (ScreenRecorder::*)(bool)>);
static_assert(std::is_same_v<decltype(&ScreenRecorder::isFinalizing),
                             bool (ScreenRecorder::*)() const>);
static_assert(std::is_same_v<decltype(&VideoRecorder::isFinalizing),
                             bool (VideoRecorder::*)() const>);

class ScreenRecorderPauseApiTests : public QObject
{
    Q_OBJECT

private slots:
    void exposesPauseResumeStateAndSignal()
    {
        QVERIFY(true);
    }
};

QTEST_APPLESS_MAIN(ScreenRecorderPauseApiTests)
#include "ScreenRecorderPauseApiTests.moc"
