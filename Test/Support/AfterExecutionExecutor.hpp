#pragma once

#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/execution.hpp>
#include <boost/asio/prefer.hpp>
#include <boost/asio/query.hpp>
#include <boost/asio/require.hpp>
#include <functional>
#include <memory>
#include <utility>

namespace Flux::Test
{
    // Run a one-shot observer after the next handler, on the wrapped executor.
    // This lets tests observe completion boundaries without depending on TCP buffering.
    class AfterExecutionExecutor final
    {
    public:
        AfterExecutionExecutor(boost::asio::any_io_executor executor,
                               std::shared_ptr<std::function<void()>> afterNext)
            : _executor(std::move(executor))
            , _afterNext(std::move(afterNext))
        {}

        template<typename Function>
        // NOLINTNEXTLINE(readability-identifier-naming)
        auto execute(Function&& function) const -> void
        {
            _executor.execute([afterNext = _afterNext,
                               function = std::forward<Function>(function)] mutable -> void {
                const auto observer = std::exchange(*afterNext, {});
                function();
                if (observer)
                    observer();
            });
        }

        template<typename Property>
        requires boost::asio::can_query<const boost::asio::any_io_executor&, Property>::value
        // NOLINTNEXTLINE(readability-identifier-naming)
        [[nodiscard]] auto query(const Property& property) const noexcept(
            boost::asio::is_nothrow_query<const boost::asio::any_io_executor&, Property>::value)
            -> boost::asio::query_result<const boost::asio::any_io_executor&, Property>::type
        {
            return boost::asio::query(_executor, property);
        }

        template<typename Property>
        requires boost::asio::can_require<const boost::asio::any_io_executor&, Property>::value
        // NOLINTNEXTLINE(readability-identifier-naming)
        [[nodiscard]] auto require(const Property& property) const -> AfterExecutionExecutor
        {
            return {boost::asio::require(_executor, property), _afterNext};
        }

        template<typename Property>
        requires boost::asio::can_prefer<const boost::asio::any_io_executor&, Property>::value
        // NOLINTNEXTLINE(readability-identifier-naming)
        [[nodiscard]] auto prefer(const Property& property) const -> AfterExecutionExecutor
        {
            return {boost::asio::prefer(_executor, property), _afterNext};
        }

        auto operator==(const AfterExecutionExecutor&) const noexcept -> bool = default;

    private:
        boost::asio::any_io_executor _executor;
        std::shared_ptr<std::function<void()>> _afterNext;
    };
}   // namespace Flux::Test
