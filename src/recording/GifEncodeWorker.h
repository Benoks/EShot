#pragma once

#include <QImage>
#include <QString>

#include <condition_variable>
#include <deque>
#include <mutex>
#include <thread>
#include <utility>

class GifEncoder;

// Runs GifEncoder::addFrame() on a worker thread so quantizing and writing
// frames does not stall the GUI thread that captures them. The encoder must
// be open; between construction and finish() only the worker touches it.
class GifEncodeWorker {
public:
    // At most `maxQueuedFrames` wait for the encoder; addFrame() blocks
    // beyond that, which bounds the memory a slow encoder can pile up.
    explicit GifEncodeWorker(GifEncoder *encoder, int maxQueuedFrames = 32);
    // Drops queued frames and waits for the frame being encoded.
    ~GifEncodeWorker();

    GifEncodeWorker(const GifEncodeWorker &) = delete;
    GifEncodeWorker &operator=(const GifEncodeWorker &) = delete;

    // Queues a frame; false once an earlier frame failed (see error()).
    bool addFrame(const QImage &image, int delayCs);
    // Encodes the queued frames and stops the thread; true when every frame
    // was written. The encoder belongs to the caller again afterwards.
    bool finish();
    QString error() const;

private:
    void run();
    void stopThread(bool dropQueued);

    GifEncoder *m_encoder = nullptr;
    const int m_maxQueuedFrames;
    mutable std::mutex m_mutex;
    std::condition_variable m_wake;
    std::condition_variable m_space;
    std::deque<std::pair<QImage, int>> m_queue;
    bool m_closing = false;
    bool m_failed = false;
    QString m_error;
    // Last member: starts once everything above is initialized.
    std::thread m_thread;
};
