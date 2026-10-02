#pragma once

#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/execution.hpp>
#include <boost/asio/prefer.hpp>
#include <boost/asio/query.hpp>
#include <boost/asio/require.hpp>
#include <exception>
#include <memory>
#include <utility>

namespace Flux::Test
{
    // Single-threaded fault injection at the public Asio executor boundary.
    // Copies and property adaptations share a failure consumed by the next submission.
    class FailingExecutor final
    {
    public:
        FailingExecutor(boost::asio::any_io_executor executor,
                        std::shared_ptr<std::exception_ptr> failure)
            : _executor(std::move(executor))
            , _failure(std::move(failure))
        {}

        template<typename Function>
        // NOLINTNEXTLINE(readability-identifier-naming)
        auto execute(Function&& function) const -> void
        {
            if (const auto failure = std::exchange(*_failure, {}))
                std::rethrow_exception(failure);
            _executor.execute(std::forward<Function>(function));
        }

        template<typename Property>
        requires boost::asio::can_query<const boost::asio::any_io_executor&, Property>::value
        // NOLINTNEXTLINE(readability-identifier-naming)
        auto query(const Property& property) const noexcept(
            boost::asio::is_nothrow_query<const boost::asio::any_io_executor&, Property>::value) ->
            typename boost::asio::query_result<const boost::asio::any_io_executor&, Property>::type
        {
            return boost::asio::query(_executor, property);
        }

        template<typename Property>
        requires boost::asio::can_require<const boost::asio::any_io_executor&, Property>::value
        // NOLINTNEXTLINE(readability-identifier-naming)
        auto require(const Property& property) const -> FailingExecutor
        {
            return {boost::asio::require(_executor, property), _failure};
        }

        template<typename Property>
        requires boost::asio::can_prefer<const boost::asio::any_io_executor&, Property>::value
        // NOLINTNEXTLINE(readability-identifier-naming)
        auto prefer(const Property& property) const -> FailingExecutor
        {
            return {boost::asio::prefer(_executor, property), _failure};
        }

        auto operator==(const FailingExecutor&) const noexcept -> bool = default;

    private:
        boost::asio::any_io_executor _executor;
        std::shared_ptr<std::exception_ptr> _failure;
    };
}   // namespace Flux::Test
