#include "IoContextPool.hpp"

#include <algorithm>
#include <cstdio>
#include <exception>

#include "Utils/Log.hpp"

namespace Flux
{
    IoContextPool::IoContextPool(const std::size_t threadCount)
        : _strand(boost::asio::make_strand(_ioContext))
        , _workGuard(boost::asio::make_work_guard(_ioContext))
        , _threadCount(std::max<std::size_t>(1, threadCount))
        , _status(Status::Started)
    {
        _threads.reserve(_threadCount);
        DebugLog("Initialized io_context pool with workers={}", _threadCount);
    }

    IoContextPool::~IoContextPool()
    {
        if (_status == Status::Stopped)
            return;

        try
        {
            _ioContext.stop();   // Hard stop

            if (const auto stopResult = Stop(); !stopResult.has_value())
                ErrorLog("Destructor failed to stop io_context pool: {}",
                         stopResult.error().message());
        }
        catch (...)
        {
            // Workers may still use this object if stopping or joining fails.
            // Terminate explicitly rather than destroy resources beneath them.
            std::fputs("Failed to shut down io_context pool during destruction\n", stderr);
            std::terminate();
        }
    }

    auto IoContextPool::Run() -> Result
    {
        const std::scoped_lock lock(_mutex);

        if (_status != Status::Started)
        {
            WarnLog("Run rejected: io_context pool is not in Started state");
            return std::unexpected(MakeErrorCode(Error::IoContextPoolNotStarted));
        }

        for (std::size_t i = 0; i < _threadCount; ++i)
        {
            try
            {
                _threads.emplace_back([this, workerIndex = i] -> void {
                    while (!_ioContext.stopped())
                    {
                        try
                        {
                            _ioContext.run();
                        }
                        catch (const std::exception& e)
                        {
                            ErrorLog("io_context handler threw exception on worker index={}: {}",
                                     workerIndex,
                                     e.what());
                        }
                        catch (...)
                        {
                            ErrorLog(
                                "io_context handler threw unknown exception on worker index={}",
                                workerIndex);
                        }
                    }

                    DebugLog("io_context worker stopped: index={}", workerIndex);
                });
            }
            catch (std::exception& e)
            {
                ErrorLog("Cannot create io_context thread: {}", e.what());

                _workGuard.reset();
                _ioContext.stop();

                _threads.clear();

                _status = Status::NotInitialized;

                return std::unexpected(MakeErrorCode(Error::IoContextPoolNotStarted));
            }
        }

        _status = Status::Running;

        InfoLog("io_context pool started with workers={}", _threadCount);

        return {};
    }

    auto IoContextPool::Stop() -> Result
    {
        const std::scoped_lock lock(_mutex);

        if (_status != Status::Running)
        {
            WarnLog("Stop rejected: io_context pool is not in Running state");
            return std::unexpected(MakeErrorCode(Error::IoContextPoolNotRunning));
        }

        if (std::ranges::any_of(_threads, [](const std::jthread& thread) -> bool {
                return thread.get_id() == std::this_thread::get_id();
            }))
        {
            ErrorLog("Stop rejected: called from an io_context worker thread");
            return std::unexpected(MakeErrorCode(Error::IoContextPoolStopFromWorkerThread));
        }

        InfoLog("Stopping io_context pool with workers={}", _threads.size());

        _workGuard.reset();

        for (auto& thread : _threads)
        {
            if (thread.joinable())
                thread.join();
        }

        _status = Status::Stopped;

        InfoLog("io_context pool stopped");

        return {};
    }
}   // namespace Flux
