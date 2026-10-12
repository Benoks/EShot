#include "GifEncodeWorker.h"
#include "GifEncoder.h"

GifEncodeWorker::GifEncodeWorker(GifEncoder *encoder, int maxQueuedFrames)
    : m_encoder(encoder),
      m_maxQueuedFrames(qMax(1, maxQueuedFrames)),
      m_thread([this]() { run(); })
{
}

GifEncodeWorker::~GifEncodeWorker()
{
    stopThread(true);
}

bool GifEncodeWorker::addFrame(const QImage &image, int delayCs)
{
    std::unique_lock<std::mutex> lock(m_mutex);
    m_space.wait(lock, [this]() {
        return m_failed || m_closing || static_cast<int>(m_queue.size()) < m_maxQueuedFrames;
    });
    if (m_failed || m_closing)
        return false;
    m_queue.emplace_back(image, delayCs);
    lock.unlock();
    m_wake.notify_one();
    return true;
}

bool GifEncodeWorker::finish()
{
    stopThread(false);
    std::lock_guard<std::mutex> lock(m_mutex);
    return !m_failed;
}

QString GifEncodeWorker::error() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_error;
}

void GifEncodeWorker::stopThread(bool dropQueued)
{
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_closing = true;
        if (dropQueued)
            m_queue.clear();
    }
    m_wake.notify_all();
    m_space.notify_all();
    if (m_thread.joinable())
        m_thread.join();
}

void GifEncodeWorker::run()
{
    for (;;) {
        std::pair<QImage, int> frame;
        {
            std::unique_lock<std::mutex> lock(m_mutex);
            m_wake.wait(lock, [this]() { return m_closing || !m_queue.empty(); });
            if (m_queue.empty())
                return;  // closing and drained
            frame = std::move(m_queue.front());
            m_queue.pop_front();
        }
        m_space.notify_one();
        const bool ok = m_encoder && m_encoder->addFrame(frame.first, frame.second);
        if (!ok) {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_failed = true;
            m_error = m_encoder ? m_encoder->errorString() : QStringLiteral("no GIF encoder");
            m_queue.clear();
            m_space.notify_all();
            return;
        }
    }
}
