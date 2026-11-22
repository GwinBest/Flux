#include "Protocol/Http/HttpServer.hpp"

#include "gtest/gtest.h"

#include <cstddef>
#include <functional>
#include <limits>
#include <memory>
#include <string>
#include <string_view>
#include <type_traits>

#include "Protocol/Http/HttpField.hpp"
#include "Protocol/Http/HttpRequest.hpp"
#include "Protocol/Http/HttpResponse.hpp"
#include "Protocol/Http/HttpRouter.hpp"

namespace Flux::Test
{
    static constexpr std::uint32_t HttpVersion11 = 11U;

    namespace
    {
        auto LoopbackEndpoint() -> Endpoint
        {
            return {boost::asio::ip::address_v4::loopback(), 0};
        }
    }   // namespace

    // Static assertions for HttpServer type properties
    static_assert(!std::is_copy_constructible_v<HttpServer>);
    static_assert(!std::is_copy_assignable_v<HttpServer>);
    static_assert(!std::is_move_constructible_v<HttpServer>);
    static_assert(!std::is_move_assignable_v<HttpServer>);
    static_assert(std::is_destructible_v<HttpServer>);

    TEST(HttpServerTest, ConstructorDefaultOptions)
    {
        const HttpServer server;
        EXPECT_NE(server.Router(), nullptr);
    }

    TEST(HttpServerTest, ConstructorWithCustomEndpoint)
    {
        ServerOptions options;
        options.BindEndpoint = LoopbackEndpoint();
        options.ThreadCount = 1;

        const HttpServer server(options);
        EXPECT_NE(server.Router(), nullptr);
    }

    TEST(HttpServerTest, RouterReturnsValidRouter)
    {
        const HttpServer server;
        const auto router = server.Router();

        ASSERT_NE(router, nullptr);
        EXPECT_NE(router.get(), nullptr);
    }

    TEST(HttpServerTest, RouterReturnsSameInstance)
    {
        const HttpServer server;
        const auto router1 = server.Router();
        const auto router2 = server.Router();

        EXPECT_EQ(router1, router2);
    }

    TEST(HttpServerTest, GetAddsRouteSuccessfully)
    {
        const HttpServer server;
        bool called = false;
        const HttpHandler handler = [&called](const HttpRequest&, HttpResponse&) -> void {
            called = true;
        };

        EXPECT_TRUE(server.Get("/test", handler));
    }

    TEST(HttpServerTest, GetReturnsFalseForDuplicate)
    {
        const HttpServer server;
        const HttpHandler handler = [](const HttpRequest&, HttpResponse&) -> void {};

        EXPECT_TRUE(server.Get("/test", handler));
        EXPECT_FALSE(server.Get("/test", handler));
    }

    TEST(HttpServerTest, PostAddsRouteSuccessfully)
    {
        const HttpServer server;
        bool called = false;
        const HttpHandler handler = [&called](const HttpRequest&, HttpResponse&) -> void {
            called = true;
        };

        EXPECT_TRUE(server.Post("/test", handler));
    }

    TEST(HttpServerTest, PostReturnsFalseForDuplicate)
    {
        const HttpServer server;
        const HttpHandler handler = [](const HttpRequest&, HttpResponse&) -> void {};

        EXPECT_TRUE(server.Post("/test", handler));
        EXPECT_FALSE(server.Post("/test", handler));
    }

    TEST(HttpServerTest, PutAddsRouteSuccessfully)
    {
        const HttpServer server;
        bool called = false;
        const HttpHandler handler = [&called](const HttpRequest&, HttpResponse&) -> void {
            called = true;
        };

        EXPECT_TRUE(server.Put("/test", handler));
    }

    TEST(HttpServerTest, PutReturnsFalseForDuplicate)
    {
        const HttpServer server;
        const HttpHandler handler = [](const HttpRequest&, HttpResponse&) -> void {};

        EXPECT_TRUE(server.Put("/test", handler));
        EXPECT_FALSE(server.Put("/test", handler));
    }

    TEST(HttpServerTest, DeleteAddsRouteSuccessfully)
    {
        const HttpServer server;
        bool called = false;
        const HttpHandler handler = [&called](const HttpRequest&, HttpResponse&) -> void {
            called = true;
        };

        EXPECT_TRUE(server.Delete("/test", handler));
    }

    TEST(HttpServerTest, DeleteReturnsFalseForDuplicate)
    {
        const HttpServer server;
        const HttpHandler handler = [](const HttpRequest&, HttpResponse&) -> void {};

        EXPECT_TRUE(server.Delete("/test", handler));
        EXPECT_FALSE(server.Delete("/test", handler));
    }

    TEST(HttpServerTest, PatchAddsRouteSuccessfully)
    {
        const HttpServer server;
        bool called = false;
        const HttpHandler handler = [&called](const HttpRequest&, HttpResponse&) -> void {
            called = true;
        };

        EXPECT_TRUE(server.Patch("/test", handler));
    }

    TEST(HttpServerTest, PatchReturnsFalseForDuplicate)
    {
        const HttpServer server;
        const HttpHandler handler = [](const HttpRequest&, HttpResponse&) -> void {};

        EXPECT_TRUE(server.Patch("/test", handler));
        EXPECT_FALSE(server.Patch("/test", handler));
    }

    TEST(HttpServerTest, HeadAddsRouteSuccessfully)
    {
        const HttpServer server;
        bool called = false;
        const HttpHandler handler = [&called](const HttpRequest&, HttpResponse&) -> void {
            called = true;
        };

        EXPECT_TRUE(server.Head("/test", handler));
    }

    TEST(HttpServerTest, HeadReturnsFalseForDuplicate)
    {
        const HttpServer server;
        const HttpHandler handler = [](const HttpRequest&, HttpResponse&) -> void {};

        EXPECT_TRUE(server.Head("/test", handler));
        EXPECT_FALSE(server.Head("/test", handler));
    }

    TEST(HttpServerTest, OptionsAddsRouteSuccessfully)
    {
        const HttpServer server;
        bool called = false;
        const HttpHandler handler = [&called](const HttpRequest&, HttpResponse&) -> void {
            called = true;
        };

        EXPECT_TRUE(server.Options("/test", handler));
    }

    TEST(HttpServerTest, OptionsReturnsFalseForDuplicate)
    {
        const HttpServer server;
        const HttpHandler handler = [](const HttpRequest&, HttpResponse&) -> void {};

        EXPECT_TRUE(server.Options("/test", handler));
        EXPECT_FALSE(server.Options("/test", handler));
    }

    TEST(HttpServerTest, DifferentMethodsSamePath)
    {
        const HttpServer server;
        const HttpHandler handler = [](const HttpRequest&, HttpResponse&) -> void {};

        // All methods on the same path should succeed
        EXPECT_TRUE(server.Get("/test", handler));
        EXPECT_TRUE(server.Post("/test", handler));
        EXPECT_TRUE(server.Put("/test", handler));
        EXPECT_TRUE(server.Delete("/test", handler));
        EXPECT_TRUE(server.Patch("/test", handler));
        EXPECT_TRUE(server.Head("/test", handler));
        EXPECT_TRUE(server.Options("/test", handler));
    }

    TEST(HttpServerTest, AddRouteDirectly)
    {
        const HttpServer server;
        const HttpRouteKey key {.Method = HttpMethod::Get, .Path = "/direct"};
        const HttpHandler handler = [](const HttpRequest&, HttpResponse&) -> void {};

        EXPECT_TRUE(server.AddRoute(key, handler));
        EXPECT_FALSE(server.AddRoute(key, handler));   // duplicate
    }

    TEST(HttpServerTest, GetWithEmptyPath)
    {
        const HttpServer server;
        const HttpHandler handler = [](const HttpRequest&, HttpResponse&) -> void {};

        EXPECT_TRUE(server.Get("", handler));
    }

    TEST(HttpServerTest, GetWithRootPath)
    {
        const HttpServer server;
        const HttpHandler handler = [](const HttpRequest&, HttpResponse&) -> void {};

        EXPECT_TRUE(server.Get("/", handler));
    }

    TEST(HttpServerTest, GetWithLongPath)
    {
        constexpr std::size_t longPathLength = 1024;
        const HttpServer server;
        const HttpHandler handler = [](const HttpRequest&, HttpResponse&) -> void {};
        const std::string longPath(longPathLength, 'a');

        EXPECT_TRUE(server.Get(longPath, handler));
    }

    TEST(HttpServerTest, GetWithQueryParameters)
    {
        const HttpServer server;
        const HttpHandler handler = [](const HttpRequest&, HttpResponse&) -> void {};

        EXPECT_TRUE(server.Get("/search?q=test&page=1", handler));
    }

    TEST(HttpServerTest, GetWithSpecialCharacters)
    {
        const HttpServer server;
        const HttpHandler handler = [](const HttpRequest&, HttpResponse&) -> void {};

        EXPECT_TRUE(server.Get("/api/v1/resource/123", handler));
    }

    TEST(HttpServerTest, RouterIntegrationGet)
    {
        const HttpServer server;
        bool called = false;
        const HttpHandler handler = [&called](const HttpRequest&, HttpResponse&) -> void {
            called = true;
        };

        (void)server.Get("/hello", handler);

        const auto router = server.Router();
        HttpRequest request;
        request.method(boost::beast::http::verb::get);
        request.target("/hello");
        request.version(HttpVersion11);
        HttpResponse response;

        EXPECT_TRUE(router->MatchRoute(request, response));
        EXPECT_TRUE(called);
    }

    TEST(HttpServerTest, RouterIntegrationPost)
    {
        const HttpServer server;
        bool called = false;
        const HttpHandler handler = [&called](const HttpRequest&, HttpResponse&) -> void {
            called = true;
        };

        (void)server.Post("/submit", handler);

        const auto router = server.Router();
        HttpRequest request;
        request.method(boost::beast::http::verb::post);
        request.target("/submit");
        request.version(HttpVersion11);
        HttpResponse response;

        EXPECT_TRUE(router->MatchRoute(request, response));
        EXPECT_TRUE(called);
    }

    TEST(HttpServerTest, RouterIntegrationPut)
    {
        const HttpServer server;
        bool called = false;
        const HttpHandler handler = [&called](const HttpRequest&, HttpResponse&) -> void {
            called = true;
        };

        (void)server.Put("/update", handler);

        const auto router = server.Router();
        HttpRequest request;
        request.method(boost::beast::http::verb::put);
        request.target("/update");
        request.version(HttpVersion11);
        HttpResponse response;

        EXPECT_TRUE(router->MatchRoute(request, response));
        EXPECT_TRUE(called);
    }

    TEST(HttpServerTest, RouterIntegrationDelete)
    {
        const HttpServer server;
        bool called = false;
        const HttpHandler handler = [&called](const HttpRequest&, HttpResponse&) -> void {
            called = true;
        };

        (void)server.Delete("/remove", handler);

        const auto router = server.Router();
        HttpRequest request;
        request.method(boost::beast::http::verb::delete_);
        request.target("/remove");
        request.version(HttpVersion11);
        HttpResponse response;

        EXPECT_TRUE(router->MatchRoute(request, response));
        EXPECT_TRUE(called);
    }

    TEST(HttpServerTest, RouterIntegrationPatch)
    {
        const HttpServer server;
        bool called = false;
        const HttpHandler handler = [&called](const HttpRequest&, HttpResponse&) -> void {
            called = true;
        };

        (void)server.Patch("/modify", handler);

        const auto router = server.Router();
        HttpRequest request;
        request.method(boost::beast::http::verb::patch);
        request.target("/modify");
        request.version(HttpVersion11);
        HttpResponse response;

        EXPECT_TRUE(router->MatchRoute(request, response));
        EXPECT_TRUE(called);
    }

    TEST(HttpServerTest, RouterIntegrationHead)
    {
        const HttpServer server;
        bool called = false;
        const HttpHandler handler = [&called](const HttpRequest&, HttpResponse&) -> void {
            called = true;
        };

        (void)server.Head("/check", handler);

        const auto router = server.Router();
        HttpRequest request;
        request.method(boost::beast::http::verb::head);
        request.target("/check");
        request.version(HttpVersion11);
        HttpResponse response;

        EXPECT_TRUE(router->MatchRoute(request, response));
        EXPECT_TRUE(called);
    }

    TEST(HttpServerTest, RouterIntegrationOptions)
    {
        const HttpServer server;
        bool called = false;
        const HttpHandler handler = [&called](const HttpRequest&, HttpResponse&) -> void {
            called = true;
        };

        (void)server.Options("/config", handler);

        const auto router = server.Router();
        HttpRequest request;
        request.method(boost::beast::http::verb::options);
        request.target("/config");
        request.version(HttpVersion11);
        HttpResponse response;

        EXPECT_TRUE(router->MatchRoute(request, response));
        EXPECT_TRUE(called);
    }

    TEST(HttpServerTest, RouterIntegrationNotFound)
    {
        const HttpServer server;
        (void)server.Get("/exists", [](const HttpRequest&, HttpResponse&) -> void {});

        const auto router = server.Router();
        HttpRequest request;
        request.method(boost::beast::http::verb::get);
        request.target("/does-not-exist");
        request.version(HttpVersion11);
        HttpResponse response;

        EXPECT_FALSE(router->MatchRoute(request, response));
    }

    TEST(HttpServerTest, NullHandlerInGet)
    {
        const HttpServer server;
        const HttpHandler handler;   // empty/null handler

        EXPECT_TRUE(server.Get("/null", handler));
    }

    TEST(HttpServerTest, NullHandlerInPost)
    {
        const HttpServer server;
        const HttpHandler handler;   // empty/null handler

        EXPECT_TRUE(server.Post("/null", handler));
    }

    TEST(HttpServerTest, MultipleRoutesDifferentPaths)
    {
        const HttpServer server;
        bool path1Called = false;
        bool path2Called = false;
        bool path3Called = false;

        (void)server.Get("/path1", [&path1Called](const HttpRequest&, HttpResponse&) -> void {
            path1Called = true;
        });
        (void)server.Get("/path2", [&path2Called](const HttpRequest&, HttpResponse&) -> void {
            path2Called = true;
        });
        (void)server.Post("/path3", [&path3Called](const HttpRequest&, HttpResponse&) -> void {
            path3Called = true;
        });

        const auto router = server.Router();

        {
            HttpRequest request;
            request.method(boost::beast::http::verb::get);
            request.target("/path1");
            request.version(HttpVersion11);
            HttpResponse response;
            (void)router->MatchRoute(request, response);
            EXPECT_TRUE(path1Called);
            EXPECT_FALSE(path2Called);
            EXPECT_FALSE(path3Called);
        }

        {
            HttpRequest request;
            request.method(boost::beast::http::verb::get);
            request.target("/path2");
            request.version(HttpVersion11);
            HttpResponse response;
            (void)router->MatchRoute(request, response);
            EXPECT_TRUE(path1Called);
            EXPECT_TRUE(path2Called);
            EXPECT_FALSE(path3Called);
        }

        {
            HttpRequest request;
            request.method(boost::beast::http::verb::post);
            request.target("/path3");
            request.version(HttpVersion11);
            HttpResponse response;
            (void)router->MatchRoute(request, response);
            EXPECT_TRUE(path1Called);
            EXPECT_TRUE(path2Called);
            EXPECT_TRUE(path3Called);
        }
    }

    TEST(HttpServerTest, ConstructorWithThreadCountZero)
    {
        ServerOptions options;
        options.BindEndpoint = LoopbackEndpoint();
        options.ThreadCount = 0;

        const HttpServer server(options);
        EXPECT_NE(server.Router(), nullptr);
        EXPECT_GT(server.GetThreadCount(), 0U);   // Should normalize to at least 1
    }

    TEST(HttpServerTest, ConstructorWithThreadCountOne)
    {
        ServerOptions options;
        options.BindEndpoint = LoopbackEndpoint();
        options.ThreadCount = 1;

        const HttpServer server(options);
        EXPECT_NE(server.Router(), nullptr);
        EXPECT_EQ(server.GetThreadCount(), 1U);
    }

    TEST(HttpServerTest, ConstructorWithMaxConnections)
    {
        constexpr auto maxConnections = 100;
        ServerOptions options;
        options.BindEndpoint = LoopbackEndpoint();
        options.ThreadCount = 1;
        options.MaxConnections = maxConnections;

        const HttpServer server(options);
        EXPECT_NE(server.Router(), nullptr);
        EXPECT_LE(server.GetMaxConnections(), 100U);
    }

    TEST(HttpServerTest, ConstructorWithMaxConnectionsZero)
    {
        ServerOptions options;
        options.BindEndpoint = LoopbackEndpoint();
        options.ThreadCount = 1;
        options.MaxConnections = 0;

        const HttpServer server(options);
        EXPECT_EQ(server.GetMaxConnections(), 0U);
    }

    TEST(HttpServerTest, GetOptionsReturnsConfiguredOptions)
    {
        constexpr auto maxConnections = 50;
        ServerOptions options;
        options.BindEndpoint = LoopbackEndpoint();
        options.ThreadCount = 2;
        options.MaxConnections = maxConnections;

        const HttpServer server(options);
        const auto& returnedOptions = server.GetOptions();

        EXPECT_EQ(returnedOptions.BindEndpoint, options.BindEndpoint);
        EXPECT_EQ(returnedOptions.MaxConnections, options.MaxConnections);
    }

    TEST(HttpServerTest, BoundaryValuesThreadCount)
    {
        // ThreadCount 0 causes vector::reserve(0) crash in IoContextPool,
        // and max value is impractical — test only valid boundary values.
        for (const auto threadCount : {std::size_t {1}, std::size_t {2}})
        {
            SCOPED_TRACE(threadCount);
            ServerOptions options;
            options.BindEndpoint = LoopbackEndpoint();
            options.ThreadCount = threadCount;

            const HttpServer server(options);
            EXPECT_NE(server.Router(), nullptr);
            EXPECT_EQ(server.GetThreadCount(), threadCount);
        }
    }

    TEST(HttpServerTest, BoundaryValuesMaxConnections)
    {
        constexpr auto maximum = std::numeric_limits<std::size_t>::max();
        for (const auto maxConn : {std::size_t {0}, std::size_t {1}, maximum})
        {
            SCOPED_TRACE(maxConn);
            ServerOptions options;
            options.BindEndpoint = LoopbackEndpoint();
            options.ThreadCount = 1;
            options.MaxConnections = maxConn;

            const HttpServer server(options);
            EXPECT_EQ(server.GetOptions().MaxConnections, maxConn);
            EXPECT_LE(server.GetMaxConnections(), maxConn);
        }
    }

    TEST(HttpServerTest, ResponseSetByHandler)
    {
        const HttpServer server;
        (void)server.Get("/response", [](const HttpRequest&, HttpResponse& resp) -> void {
            resp.Status(HttpStatus::Created)
                .Set(HttpField::ContentType, "application/json")
                .Body("Created resource");
        });

        const auto router = server.Router();
        HttpRequest request;
        request.method(boost::beast::http::verb::get);
        request.target("/response");
        request.version(HttpVersion11);
        HttpResponse response;

        (void)router->MatchRoute(request, response);

        EXPECT_EQ(response.StatusCode(), HttpStatus::Created);
        EXPECT_EQ(response.Body(), "Created resource");
        EXPECT_EQ(response.Header(HttpField::ContentType), "application/json");
    }

    TEST(HttpServerTest, RequestDataPassedToHandler)
    {
        const HttpServer server;
        std::string capturedTarget;
        HttpMethod capturedMethod {HttpMethod::Unknown};
        std::string capturedHeader;

        (void)server.Post(
            "/data",
            [&capturedTarget, &capturedMethod, &capturedHeader](const HttpRequest& req,
                                                                HttpResponse&) -> void {
                capturedTarget = std::string(req.Path());
                capturedMethod = req.Method();
                if (req.HasHeader(HttpField::ContentType))
                {
                    capturedHeader = std::string(
                        req[HttpField::   // NOLINT(*-pro-bounds-avoid-unchecked-container-access)
                            ContentType]);
                }
            });

        const auto router = server.Router();
        HttpRequest request;
        request.method(boost::beast::http::verb::post);
        request.target("/data");
        request.version(HttpVersion11);
        request.set(ToString(HttpField::ContentType), "application/json");
        request.body() = R"({"key": "value"})";
        request.prepare_payload();
        HttpResponse response;

        (void)router->MatchRoute(request, response);

        EXPECT_EQ(capturedTarget, "/data");
        EXPECT_EQ(capturedMethod, HttpMethod::Post);
        EXPECT_EQ(capturedHeader, "application/json");
    }

}   // namespace Flux::Test
