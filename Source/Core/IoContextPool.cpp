#include "IoContextPool.hpp"

#include <algorithm>
#include <iostream>

namespace Flux
{
    IoContextPool::IoContextPool(const std::size_t threadCount)
        : _strand(boost::asio::make_strand(_ioContext))
        , _workGuard(boost::asio::make_work_guard(_ioContext))
        , _threadCount(std::max<std::size_t>(1, threadCount))
        , _status(Status::Started)
    {
        _threads.reserve(_threadCount);
    }

    IoContextPool::~IoContextPool()
    {
        _ioContext.stop();   // Using for hard stop
        Stop();
    }

    auto IoContextPool::Run() -> void
    {
        if (_status != Status::Started)
            return;

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
                        std::cerr << e.what() << '\n';
                    }
                }
            });
        }

        _status = Status::Running;
    }

    auto IoContextPool::Stop() -> void
    {
        if (_status != Status::Running)
            return;

        _workGuard.reset();

        for (auto& thread : _threads)
        {
            if (thread.joinable())
                thread.join();
        }
        _threads.clear();

        _status = Status::Stopped;
    }
}   // namespace Flux
