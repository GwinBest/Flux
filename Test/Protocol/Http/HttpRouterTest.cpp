#include "Protocol/Http/HttpRouter.hpp"

#include "gtest/gtest.h"

#include <boost/beast/http/field.hpp>
#include <boost/beast/http/verb.hpp>
#include <cstddef>
#include <functional>
#include <string>
#include <string_view>

#include "Protocol/Http/HttpField.hpp"
#include "Protocol/Http/HttpRequest.hpp"
#include "Protocol/Http/HttpResponse.hpp"

namespace Flux::Test
{
    static constexpr std::uint32_t HttpVersion11 = 11U;

    namespace
    {
        auto MakeGetRequest(const std::string_view path) -> HttpRequest
        {
            HttpRequest request;
            request.method(boost::beast::http::verb::get);
            request.target(std::string(path));
            request.version(HttpVersion11);
            return request;
        }

        auto MakePostRequest(const std::string_view path) -> HttpRequest
        {
            HttpRequest request;
            request.method(boost::beast::http::verb::post);
            request.target(std::string(path));
            request.version(HttpVersion11);
            return request;
        }

        auto MakePutRequest(const std::string_view path) -> HttpRequest
        {
            HttpRequest request;
            request.method(boost::beast::http::verb::put);
            request.target(std::string(path));
            request.version(HttpVersion11);
            return request;
        }

        auto MakeDeleteRequest(const std::string_view path) -> HttpRequest
        {
            HttpRequest request;
            request.method(boost::beast::http::verb::delete_);
            request.target(std::string(path));
            request.version(HttpVersion11);
            return request;
        }

        auto MakePatchRequest(const std::string_view path) -> HttpRequest
        {
            HttpRequest request;
            request.method(boost::beast::http::verb::patch);
            request.target(std::string(path));
            request.version(HttpVersion11);
            return request;
        }

        auto MakeHeadRequest(const std::string_view path) -> HttpRequest
        {
            HttpRequest request;
            request.method(boost::beast::http::verb::head);
            request.target(std::string(path));
            request.version(HttpVersion11);
            return request;
        }

        auto MakeOptionsRequest(const std::string_view path) -> HttpRequest
        {
            HttpRequest request;
            request.method(boost::beast::http::verb::options);
            request.target(std::string(path));
            request.version(HttpVersion11);
            return request;
        }
    }   // namespace

    TEST(HttpRouteKeyTest, EqualitySameMethodAndPath)
    {
        const HttpRouteKey key1 {.Method = HttpMethod::Get, .Path = "/api/test"};
        const HttpRouteKey key2 {.Method = HttpMethod::Get, .Path = "/api/test"};

        EXPECT_EQ(key1, key2);
    }

    TEST(HttpRouteKeyTest, InequalityDifferentMethod)
    {
        const HttpRouteKey key1 {.Method = HttpMethod::Get, .Path = "/api/test"};
        const HttpRouteKey key2 {.Method = HttpMethod::Post, .Path = "/api/test"};

        EXPECT_NE(key1, key2);
    }

    TEST(HttpRouteKeyTest, InequalityDifferentPath)
    {
        const HttpRouteKey key1 {.Method = HttpMethod::Get, .Path = "/api/test"};
        const HttpRouteKey key2 {.Method = HttpMethod::Get, .Path = "/api/other"};

        EXPECT_NE(key1, key2);
    }

    TEST(HttpRouteKeyTest, EqualityEmptyPath)
    {
        const HttpRouteKey key1 {.Method = HttpMethod::Get, .Path = ""};
        const HttpRouteKey key2 {.Method = HttpMethod::Get, .Path = ""};

        EXPECT_EQ(key1, key2);
    }

    TEST(HttpRouteKeyTest, EqualityRootPath)
    {
        const HttpRouteKey key1 {.Method = HttpMethod::Get, .Path = "/"};
        const HttpRouteKey key2 {.Method = HttpMethod::Get, .Path = "/"};

        EXPECT_EQ(key1, key2);
    }

    TEST(RouteKeyHashTest, SameKeyProducesSameHash)
    {
        constexpr RouteKeyHash hashFn;
        const HttpRouteKey key {.Method = HttpMethod::Get, .Path = "/api/test"};

        EXPECT_EQ(hashFn(key), hashFn(key));
    }

    TEST(RouteKeyHashTest, DifferentMethodProducesDifferentHash)
    {
        constexpr RouteKeyHash hashFn;
        const HttpRouteKey key1 {.Method = HttpMethod::Get, .Path = "/api/test"};
        const HttpRouteKey key2 {.Method = HttpMethod::Post, .Path = "/api/test"};

        EXPECT_NE(hashFn(key1), hashFn(key2));
    }

    TEST(RouteKeyHashTest, DifferentPathProducesDifferentHash)
    {
        constexpr RouteKeyHash hashFn;
        const HttpRouteKey key1 {.Method = HttpMethod::Get, .Path = "/api/test"};
        const HttpRouteKey key2 {.Method = HttpMethod::Get, .Path = "/api/other"};

        EXPECT_NE(hashFn(key1), hashFn(key2));
    }

    TEST(HttpRouterTest, AddRouteReturnsTrueForNewRoute)
    {
        HttpRouter router;
        const HttpRouteKey key {.Method = HttpMethod::Get, .Path = "/test"};
        const HttpHandler handler = [](const HttpRequest&, HttpResponse&) -> void {};

        EXPECT_TRUE(router.AddRoute(key, handler));
    }

    TEST(HttpRouterTest, AddRouteReturnsFalseForDuplicateRoute)
    {
        HttpRouter router;
        const HttpRouteKey key {.Method = HttpMethod::Get, .Path = "/test"};
        const HttpHandler handler = [](const HttpRequest&, HttpResponse&) -> void {};

        EXPECT_TRUE(router.AddRoute(key, handler));
        EXPECT_FALSE(router.AddRoute(key, handler));
    }

    TEST(HttpRouterTest, MatchRouteReturnsTrueForRegisteredRoute)
    {
        HttpRouter router;
        const HttpRouteKey key {.Method = HttpMethod::Get, .Path = "/test"};
        bool called = false;
        const HttpHandler handler = [&called](const HttpRequest&, HttpResponse&) -> void {
            called = true;
        };

        EXPECT_TRUE(router.AddRoute(key, handler));

        const HttpRequest request = MakeGetRequest("/test");
        HttpResponse response;

        EXPECT_TRUE(router.MatchRoute(request, response));
        EXPECT_TRUE(called);
    }

    TEST(HttpRouterTest, MatchRouteReturnsFalseForUnregisteredRoute)
    {
        const HttpRouter router;

        const HttpRequest request = MakeGetRequest("/nonexistent");
        HttpResponse response;

        EXPECT_FALSE(router.MatchRoute(request, response));
    }

    TEST(HttpRouterTest, MatchRouteEmptyRouterReturnsFalse)
    {
        const HttpRouter router;

        const HttpRequest request = MakeGetRequest("/test");
        HttpResponse response;

        EXPECT_FALSE(router.MatchRoute(request, response));
    }

    TEST(HttpRouterTest, MatchRouteDifferentMethodReturnsFalse)
    {
        HttpRouter router;
        const HttpRouteKey key {.Method = HttpMethod::Get, .Path = "/test"};
        const HttpHandler handler = [](const HttpRequest&, HttpResponse&) -> void {};

        EXPECT_TRUE(router.AddRoute(key, handler));

        const HttpRequest request = MakePostRequest("/test");
        HttpResponse response;

        EXPECT_FALSE(router.MatchRoute(request, response));
    }

    TEST(HttpRouterTest, MatchRouteDifferentPathReturnsFalse)
    {
        HttpRouter router;
        const HttpRouteKey key {.Method = HttpMethod::Get, .Path = "/test"};
        const HttpHandler handler = [](const HttpRequest&, HttpResponse&) -> void {};

        EXPECT_TRUE(router.AddRoute(key, handler));

        const HttpRequest request = MakeGetRequest("/other");
        HttpResponse response;

        EXPECT_FALSE(router.MatchRoute(request, response));
    }

    TEST(HttpRouterTest, MatchRouteEmptyPath)
    {
        HttpRouter router;
        const HttpRouteKey key {.Method = HttpMethod::Get, .Path = ""};
        bool called = false;
        const HttpHandler handler = [&called](const HttpRequest&, HttpResponse&) -> void {
            called = true;
        };

        EXPECT_TRUE(router.AddRoute(key, handler));

        const HttpRequest request = MakeGetRequest("");
        HttpResponse response;

        EXPECT_TRUE(router.MatchRoute(request, response));
        EXPECT_TRUE(called);
    }

    TEST(HttpRouterTest, MatchRouteRootPath)
    {
        HttpRouter router;
        const HttpRouteKey key {.Method = HttpMethod::Get, .Path = "/"};
        bool called = false;
        const HttpHandler handler = [&called](const HttpRequest&, HttpResponse&) -> void {
            called = true;
        };

        EXPECT_TRUE(router.AddRoute(key, handler));

        const HttpRequest request = MakeGetRequest("/");
        HttpResponse response;

        EXPECT_TRUE(router.MatchRoute(request, response));
        EXPECT_TRUE(called);
    }

    TEST(HttpRouterTest, MatchRouteSetsResponse)
    {
        HttpRouter router;
        const HttpRouteKey key {.Method = HttpMethod::Get, .Path = "/hello"};
        const HttpHandler handler = [](const HttpRequest&, HttpResponse& resp) -> void {
            resp.Ok().Set(HttpField::ContentType, "text/plain").Body("Hello, World!");
        };

        EXPECT_TRUE(router.AddRoute(key, handler));

        const HttpRequest request = MakeGetRequest("/hello");
        HttpResponse response;

        (void)router.MatchRoute(request, response);

        EXPECT_EQ(response.StatusCode(), HttpStatus::Ok);
        EXPECT_EQ(response.Body(), "Hello, World!");
    }

    TEST(HttpRouterTest, MatchRoutePassesRequestToHandler)
    {
        HttpRouter router;
        const HttpRouteKey key {.Method = HttpMethod::Post, .Path = "/submit"};
        std::string capturedTarget;
        HttpMethod capturedMethod {HttpMethod::Unknown};
        const HttpHandler handler = [&capturedTarget, &capturedMethod](const HttpRequest& req,
                                                                       HttpResponse&) -> void {
            capturedTarget = std::string(req.Path());
            capturedMethod = req.Method();
        };

        EXPECT_TRUE(router.AddRoute(key, handler));

        const HttpRequest request = MakePostRequest("/submit");
        HttpResponse response;

        (void)router.MatchRoute(request, response);

        EXPECT_EQ(capturedTarget, "/submit");
        EXPECT_EQ(capturedMethod, HttpMethod::Post);
    }

    TEST(HttpRouterTest, AddMultipleRoutesDifferentMethods)
    {
        HttpRouter router;
        bool getCalled = false;
        bool postCalled = false;

        EXPECT_TRUE(router.AddRoute(
            HttpRouteKey {.Method = HttpMethod::Get, .Path = "/test"},
            [&getCalled](const HttpRequest&, HttpResponse&) -> void { getCalled = true; }));
        EXPECT_TRUE(router.AddRoute(
            HttpRouteKey {.Method = HttpMethod::Post, .Path = "/test"},
            [&postCalled](const HttpRequest&, HttpResponse&) -> void { postCalled = true; }));

        {
            const HttpRequest request = MakeGetRequest("/test");
            HttpResponse response;
            (void)router.MatchRoute(request, response);
            EXPECT_TRUE(getCalled);
            EXPECT_FALSE(postCalled);
        }

        {
            const HttpRequest request = MakePostRequest("/test");
            HttpResponse response;
            (void)router.MatchRoute(request, response);
            EXPECT_TRUE(getCalled);
            EXPECT_TRUE(postCalled);
        }
    }

    TEST(HttpRouterTest, AddMultipleRoutesDifferentPaths)
    {
        HttpRouter router;
        bool path1Called = false;
        bool path2Called = false;

        EXPECT_TRUE(router.AddRoute(
            HttpRouteKey {.Method = HttpMethod::Get, .Path = "/path1"},
            [&path1Called](const HttpRequest&, HttpResponse&) -> void { path1Called = true; }));
        EXPECT_TRUE(router.AddRoute(
            HttpRouteKey {.Method = HttpMethod::Get, .Path = "/path2"},
            [&path2Called](const HttpRequest&, HttpResponse&) -> void { path2Called = true; }));

        {
            const HttpRequest request = MakeGetRequest("/path1");
            HttpResponse response;
            (void)router.MatchRoute(request, response);
            EXPECT_TRUE(path1Called);
            EXPECT_FALSE(path2Called);
        }

        {
            const HttpRequest request = MakeGetRequest("/path2");
            HttpResponse response;
            (void)router.MatchRoute(request, response);
            EXPECT_TRUE(path1Called);
            EXPECT_TRUE(path2Called);
        }
    }

    TEST(HttpRouterTest, AllHttpMethodsSupported)
    {
        HttpRouter router;
        bool getCalled = false;
        bool postCalled = false;
        bool putCalled = false;
        bool deleteCalled = false;
        bool patchCalled = false;
        bool headCalled = false;
        bool optionsCalled = false;

        EXPECT_TRUE(router.AddRoute(
            HttpRouteKey {.Method = HttpMethod::Get, .Path = "/test"},
            [&getCalled](const HttpRequest&, HttpResponse&) -> void { getCalled = true; }));
        EXPECT_TRUE(router.AddRoute(
            HttpRouteKey {.Method = HttpMethod::Post, .Path = "/test"},
            [&postCalled](const HttpRequest&, HttpResponse&) -> void { postCalled = true; }));
        EXPECT_TRUE(router.AddRoute(
            HttpRouteKey {.Method = HttpMethod::Put, .Path = "/test"},
            [&putCalled](const HttpRequest&, HttpResponse&) -> void { putCalled = true; }));
        EXPECT_TRUE(router.AddRoute(
            HttpRouteKey {.Method = HttpMethod::Delete, .Path = "/test"},
            [&deleteCalled](const HttpRequest&, HttpResponse&) -> void { deleteCalled = true; }));
        EXPECT_TRUE(router.AddRoute(
            HttpRouteKey {.Method = HttpMethod::Patch, .Path = "/test"},
            [&patchCalled](const HttpRequest&, HttpResponse&) -> void { patchCalled = true; }));
        EXPECT_TRUE(router.AddRoute(
            HttpRouteKey {.Method = HttpMethod::Head, .Path = "/test"},
            [&headCalled](const HttpRequest&, HttpResponse&) -> void { headCalled = true; }));
        EXPECT_TRUE(router.AddRoute(
            HttpRouteKey {.Method = HttpMethod::Options, .Path = "/test"},
            [&optionsCalled](const HttpRequest&, HttpResponse&) -> void { optionsCalled = true; }));

        HttpResponse resp;
        EXPECT_TRUE(router.MatchRoute(MakeGetRequest("/test"), resp));
        EXPECT_TRUE(getCalled);

        resp = HttpResponse();
        EXPECT_TRUE(router.MatchRoute(MakePostRequest("/test"), resp));
        EXPECT_TRUE(postCalled);

        resp = HttpResponse();
        EXPECT_TRUE(router.MatchRoute(MakePutRequest("/test"), resp));
        EXPECT_TRUE(putCalled);

        resp = HttpResponse();
        EXPECT_TRUE(router.MatchRoute(MakeDeleteRequest("/test"), resp));
        EXPECT_TRUE(deleteCalled);

        resp = HttpResponse();
        EXPECT_TRUE(router.MatchRoute(MakePatchRequest("/test"), resp));
        EXPECT_TRUE(patchCalled);

        resp = HttpResponse();
        EXPECT_TRUE(router.MatchRoute(MakeHeadRequest("/test"), resp));
        EXPECT_TRUE(headCalled);

        resp = HttpResponse();
        EXPECT_TRUE(router.MatchRoute(MakeOptionsRequest("/test"), resp));
        EXPECT_TRUE(optionsCalled);
    }

    TEST(HttpRouterTest, LongPath)
    {
        constexpr std::size_t longPathLength = 1024;
        HttpRouter router;
        bool called = false;
        const std::string longPath(longPathLength, 'a');
        const HttpHandler handler = [&called](const HttpRequest&, HttpResponse&) -> void {
            called = true;
        };

        EXPECT_TRUE(
            router.AddRoute(HttpRouteKey {.Method = HttpMethod::Get, .Path = longPath}, handler));

        const HttpRequest request = MakeGetRequest(longPath);
        HttpResponse response;

        EXPECT_TRUE(router.MatchRoute(request, response));
        EXPECT_TRUE(called);
    }

    TEST(HttpRouterTest, PathWithQueryParameters)
    {
        HttpRouter router;
        bool called = false;
        const HttpHandler handler = [&called](const HttpRequest&, HttpResponse&) -> void {
            called = true;
        };

        EXPECT_TRUE(router.AddRoute(
            HttpRouteKey {.Method = HttpMethod::Get, .Path = "/search?q=test&page=1"},
            handler));

        const HttpRequest request = MakeGetRequest("/search?q=test&page=1");
        HttpResponse response;

        EXPECT_TRUE(router.MatchRoute(request, response));
        EXPECT_TRUE(called);
    }

    TEST(HttpRouterTest, PathWithSpecialCharacters)
    {
        HttpRouter router;
        bool called = false;
        const HttpHandler handler = [&called](const HttpRequest&, HttpResponse&) -> void {
            called = true;
        };

        EXPECT_TRUE(router.AddRoute(
            HttpRouteKey {.Method = HttpMethod::Get, .Path = "/api/v1/resource/123"},
            handler));

        const HttpRequest request = MakeGetRequest("/api/v1/resource/123");
        HttpResponse response;

        EXPECT_TRUE(router.MatchRoute(request, response));
        EXPECT_TRUE(called);
    }

    TEST(HttpRouterTest, NullHandlerDoesNotCrash)
    {
        HttpRouter router;
        const HttpRouteKey key {.Method = HttpMethod::Get, .Path = "/null"};
        const HttpHandler handler;   // empty/null handler

        EXPECT_TRUE(router.AddRoute(key, handler));

        const HttpRequest request = MakeGetRequest("/null");

        // MatchRoute should return true (route found), but calling null handler should throw
        bool threw = false;
        try
        {
            HttpResponse response;
            (void)router.MatchRoute(request, response);
        }
        catch (const std::bad_function_call&)
        {
            threw = true;
        }
        EXPECT_TRUE(threw);
    }

}   // namespace Flux::Test
