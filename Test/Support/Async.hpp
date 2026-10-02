#pragma once

#include <boost/asio/executor_work_guard.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/post.hpp>
#include <chrono>
#include <future>
#include <stdexcept>
#include <type_traits>
#include <utility>

namespace Flux::Test
{
    // Drive real handlers with a deadline; no sleeps or access to private state.
    template<typename Predicate>
    auto PumpUntil(boost::asio::io_context& context, const Predicate& predicate) -> bool
    {
        using namespace std::chrono_literals;
        const auto work = boost::asio::make_work_guard(context);
        const auto deadline = std::chrono::steady_clock::now() + 3s;
        while (!predicate())
        {
            if (std::chrono::steady_clock::now() >= deadline)
                return false;
            if (context.stopped())
                context.restart();
            context.run_one_for(10ms);
        }
        return true;
    }

    template<typename Value>
    auto AwaitFuture(boost::asio::io_context& context, std::future<Value> future) -> Value
    {
        if (!PumpUntil(context, [&future] -> bool {
                return future.wait_for(std::chrono::seconds(0)) == std::future_status::ready;
            }))
            throw std::runtime_error("Asynchronous operation exceeded the test deadline");
        return future.get();
    }

    template<typename Executor, typename Operation>
    auto OnExecutor(boost::asio::io_context& context,
                    const Executor& executor,
                    Operation&& operation)
    {
        using Value = std::invoke_result_t<Operation>;
        std::packaged_task<Value()> task(std::forward<Operation>(operation));
        auto future = task.get_future();
        boost::asio::post(executor, [task = std::move(task)] mutable -> void { task(); });
        return AwaitFuture(context, std::move(future));
    }
}   // namespace Flux::Test
