#include "IoContextPool.hpp"

#include <algorithm>

#include "../Utils/Log.hpp"

namespace Flux
{
    IoContextPool::IoContextPool(const std::size_t threadCount)
        : _strand(boost::asio::make_strand(_ioContext))
        , _workGuard(boost::asio::make_work_guard(_ioContext))
        , _threadCount(std::max<std::size_t>(1, threadCount))
        , _status(Status::Started)
    {
        _threads.reserve(_threadCount);
        Log::DebugLog(Log::Format("Initialized io_context pool with workers={}", _threadCount));
    }

    IoContextPool::~IoContextPool()
    {
        _ioContext.stop();   // Hard stop

        if (const auto stopResult = Stop(); !stopResult.has_value())
            Log::ErrorLog(Log::Format("Destructor failed to stop io_context pool: {}",
                                      stopResult.error().message()));
    }

    auto IoContextPool::Run() -> Result
    {
        if (_status != Status::Started)
        {
            Log::WarnLog("Run rejected: io_context pool is not in Started state");
            return std::unexpected(MakeErrorCode(Error::IoContextPoolNotStarted));
        }

        for (std::size_t i = 0; i < _threadCount; ++i)
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
                        Log::ErrorLog(
                            Log::Format("io_context handler threw exception on worker index={}: {}",
                                        workerIndex,
                                        e.what()));
                    }
                    catch (...)
                    {
                        Log::ErrorLog(Log::Format(
                            "io_context handler threw unknown exception on worker index={}",
                            workerIndex));
                    }
                }

                Log::DebugLog(Log::Format("io_context worker stopped: index={}", workerIndex));
            });
        }

        _status = Status::Running;

        Log::InfoLog(Log::Format("io_context pool started with workers={}", _threadCount));

        return {};
    }

    auto IoContextPool::Stop() -> Result
    {
        if (_status != Status::Running)
        {
            Log::WarnLog("Stop rejected: io_context pool is not in Running state");
            return std::unexpected(MakeErrorCode(Error::IoContextPoolNotRunning));
        }

        if (std::ranges::any_of(_threads, [](const std::jthread& thread) -> bool {
                return thread.get_id() == std::this_thread::get_id();
            }))
        {
            Log::ErrorLog("Stop rejected: called from an io_context worker thread");
            return std::unexpected(MakeErrorCode(Error::IoContextPoolStopFromWorkerThread));
        }

        Log::InfoLog(Log::Format("Stopping io_context pool with workers={}", _threads.size()));

        _workGuard.reset();

        for (auto& thread : _threads)
        {
            if (thread.joinable())
                thread.join();
        }

        _status = Status::Stopped;

        Log::InfoLog("io_context pool stopped");

        return {};
    }
}   // namespace Flux
