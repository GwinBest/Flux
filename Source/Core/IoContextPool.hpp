#pragma once

#include <cstddef>
#include <expected>
#include <thread>
#include <vector>

#include "boost/asio/executor_work_guard.hpp"
#include "boost/asio/io_context.hpp"
#include "boost/asio/strand.hpp"

namespace Flux
{
    class IoContextPool final
    {
    public:
        using IoContext = boost::asio::io_context;
        using Strand = boost::asio::strand<boost::asio::io_context::executor_type>;
        using WorkGuard = boost::asio::executor_work_guard<boost::asio::io_context::executor_type>;

    public:
        explicit IoContextPool(const std::size_t threadCount = std::thread::hardware_concurrency());

        IoContextPool(const IoContextPool&) = delete;
        auto operator=(const IoContextPool&) -> IoContextPool& = delete;
        IoContextPool(IoContextPool&&) = delete;
        auto operator=(IoContextPool&&) -> IoContextPool& = delete;

        ~IoContextPool();

        auto Run() -> std::expected<bool, std::string>;
        auto Stop() -> std::expected<bool, std::string>;

        [[nodiscard]] auto GetIoContext() const noexcept -> const IoContext& { return _ioContext; }

        [[nodiscard]] auto GetStrand() const noexcept -> const Strand& { return _strand; }

    private:
        enum class Status : std::uint8_t
        {
            Unknown,
            NotInitialized,
            Started,
            Running,
            Stopped,
        };

    private:
        IoContext _ioContext;
        Strand _strand;
        WorkGuard _workGuard;

        std::vector<std::jthread> _threads;
        std::size_t _threadCount = std::thread::hardware_concurrency();
        std::mutex _mutex;

        std::atomic<Status> _status = Status::NotInitialized;
    };

}   // namespace Flux
