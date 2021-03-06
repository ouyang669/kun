

#include <ripple/nodestore/impl/BatchWriter.h>

namespace ripple {
namespace NodeStore {

BatchWriter::BatchWriter (Callback& callback, Scheduler& scheduler)
    : m_callback (callback)
    , m_scheduler (scheduler)
    , mWriteLoad (0)
    , mWritePending (false)
{
    mWriteSet.reserve (batchWritePreallocationSize);
}

BatchWriter::~BatchWriter ()
{
    waitForWriting ();
}

void
BatchWriter::store (std::shared_ptr<NodeObject> const& object)
{
    std::unique_lock<decltype(mWriteMutex)> sl (mWriteMutex);

    while (mWriteSet.size() >= batchWriteLimitSize)
        mWriteCondition.wait (sl);

    mWriteSet.push_back (object);

    if (! mWritePending)
    {
        mWritePending = true;

        m_scheduler.scheduleTask (*this);
    }
}

int
BatchWriter::getWriteLoad ()
{
    std::lock_guard<decltype(mWriteMutex)> sl (mWriteMutex);

    return std::max (mWriteLoad, static_cast<int> (mWriteSet.size ()));
}

void
BatchWriter::performScheduledTask ()
{
    writeBatch ();
}

void
BatchWriter::writeBatch ()
{
    for (;;)
    {
        std::vector< std::shared_ptr<NodeObject> > set;

        set.reserve (batchWritePreallocationSize);

        {
            std::lock_guard<decltype(mWriteMutex)> sl (mWriteMutex);

            mWriteSet.swap (set);
            assert (mWriteSet.empty ());
            mWriteLoad = set.size ();

            if (set.empty ())
            {
                mWritePending = false;
                mWriteCondition.notify_all ();

                return;
            }

        }

        BatchWriteReport report;
        report.writeCount = set.size();
        auto const before = std::chrono::steady_clock::now();

        m_callback.writeBatch (set);

        report.elapsed = std::chrono::duration_cast <std::chrono::milliseconds>
            (std::chrono::steady_clock::now() - before);

        m_scheduler.onBatchWrite (report);
    }
}

void
BatchWriter::waitForWriting ()
{
    std::unique_lock <decltype(mWriteMutex)> sl (mWriteMutex);

    while (mWritePending)
        mWriteCondition.wait (sl);
}

}
}






