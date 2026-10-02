#pragma once

#include <boost/asio/dispatch.hpp>
#include <boost/asio/error.hpp>
#include <future>

namespace Flux
{
    template<typename Operation>
    auto DispatchOperation(const boost::asio::any_io_executor& executor, Operation&& operation)
        -> std::future<Result>
    {
        std::packaged_task<Result()> task(std::forward<Operation>(operation));
        auto future = task.get_future();

        boost::asio::dispatch(executor, [task = std::move(task)] mutable -> void { task(); });
        return future;
    }
}   // namespace Flux
