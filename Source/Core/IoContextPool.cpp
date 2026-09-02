#include "IoContextPool.hpp"

#include <algorithm>

#include <spdlog/spdlog.h>

namespace Flux
{
    IoContextPool::IoContextPool(const std::size_t threadCount)
        : _strand(boost::asio::make_strand(_ioContext))
        , _workGuard(boost::asio::make_work_guard(_ioContext))
        , _threadCount(std::max<std::size_t>(1, threadCount))
        , _status(Status::Started)
    {
        _threads.reserve(_threadCount);
        spdlog::debug("IoContextPool initialized with {} worker thread(s)", _threadCount);
    }

    IoContextPool::~IoContextPool()
    {
        _ioContext.stop();

        if (_status == Status::Running)
        {
            if (const auto stopResult = Stop(); !stopResult.has_value())
                spdlog::error("Failed to stop IoContextPool from destructor: {}",
                              stopResult.error());
        }
    }

    auto IoContextPool::Run() -> std::expected<bool, std::string>
    {
        const std::scoped_lock lock(_mutex);

        if (_status != Status::Started)
        {
            spdlog::warn("IoContextPool run rejected: status is not Started");
            return std::unexpected(
                "IoContextPool::Run(): IO context pool status is not Status::Started");
        }

        spdlog::info("Starting IoContextPool with {} worker thread(s)", _threadCount);

        for (std::size_t i = 0; i < _threadCount; ++i)
        {
            _threads.emplace_back([this] -> void {
                while (!_ioContext.stopped())
                {
                    try
                    {
                        _ioContext.run();
                    }
                    catch (const std::exception& e)
                    {
                        spdlog::error("IoContextPool handler threw exception: {}", e.what());
                    }
                }
            });
        }

        _status = Status::Running;
        spdlog::info("IoContextPool started");

        return true;
    }

    auto IoContextPool::Stop() -> std::expected<bool, std::string>
    {
        const std::scoped_lock lock(_mutex);

        if (_status != Status::Running)
        {
            spdlog::warn("IoContextPool stop rejected: status is not Running");
            return std::unexpected(
                "IoContextPool::Stop():IO context pool status is not Status::Running");
        }

        if (std::ranges::any_of(_threads, [](const std::jthread& thread) -> bool {
                return thread.get_id() == std::this_thread::get_id();
            }))
        {
            spdlog::error("IoContextPool stop rejected from an IO worker thread");
            return std::unexpected(
                "IoContextPool::Stop(): cannot be called from an IO worker thread");
        }

        spdlog::info("Stopping IoContextPool");
        _workGuard.reset();

        for (auto& thread : _threads)
        {
            if (thread.joinable())
                thread.join();
        }

        _status = Status::Stopped;
        spdlog::info("IoContextPool stopped");

        return true;
    }
}   // namespace Flux
