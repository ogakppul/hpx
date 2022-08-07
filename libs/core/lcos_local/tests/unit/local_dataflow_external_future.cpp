//  Copyright (c) 2020 ETH Zurich
//  Copyright (c) 2015-2022 Hartmut Kaiser
//  Copyright (c) 2013 Thomas Heller
//
//  SPDX-License-Identifier: BSL-1.0
//  Distributed under the Boost Software License, Version 1.0. (See accompanying
//  file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <hpx/errors/try_catch_exception_ptr.hpp>
#include <hpx/local/chrono.hpp>
#include <hpx/local/execution.hpp>
#include <hpx/local/functional.hpp>
#include <hpx/local/future.hpp>
#include <hpx/local/init.hpp>
#include <hpx/local/tuple.hpp>
#include <hpx/modules/memory.hpp>
#include <hpx/modules/testing.hpp>
#include <hpx/program_options.hpp>

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <functional>
#include <iostream>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

std::atomic<bool> done{false};

///////////////////////////////////////////////////////////////////////////////
struct external_future_executor
{
    // This is not actually called by dataflow, but it is used for the return
    // type calculation of it. dataflow_finalize has to set the same type to
    // the future state.
    template <typename F, typename... Ts>
    friend decltype(auto) tag_invoke(hpx::parallel::execution::async_execute_t,
        external_future_executor const&, F&& f, Ts&&... ts)
    {
        if constexpr (std::is_void_v<hpx::util::invoke_result_t<F, Ts...>>)
        {
            // The completion of f is signalled out-of-band.
            hpx::invoke(std::forward<F>(f), std::forward<Ts>(ts)...);
            return hpx::async(
                []() { hpx::util::yield_while([]() { return !done; }); });
        }
        else
        {
            // The completion of f is signalled out-of-band.
            auto&& r = hpx::invoke(std::forward<F>(f), std::forward<Ts>(ts)...);
            return hpx::async([r = std::move(r)]() {
                hpx::util::yield_while([]() { return !done; });
                return r;
            });
        }
    }

    template <typename Frame, typename F, typename Futures>
    void dataflow_finalize(Frame&& frame, F&& f, Futures&& futures)
    {
        static constexpr bool is_void =
            std::remove_pointer_t<std::decay_t<Frame>>::is_void::value;

        hpx::detail::try_catch_exception_ptr(
            [&]() {
                if constexpr (is_void)
                {
                    hpx::util::invoke_fused(
                        std::forward<F>(f), std::forward<Futures>(futures));

                    // Signal completion from another thread/task.
                    hpx::intrusive_ptr<
                        std::remove_pointer_t<std::decay_t<Frame>>>
                        frame_p(frame);
                    hpx::apply([frame_p = std::move(frame_p)]() {
                        hpx::util::yield_while([]() { return !done; });
                        frame_p->set_data(hpx::util::unused_type{});
                    });
                }
                else
                {
                    auto&& r = hpx::util::invoke_fused(
                        std::forward<F>(f), std::forward<Futures>(futures));

                    // Signal completion from another thread/task.
                    hpx::intrusive_ptr<
                        std::remove_pointer_t<std::decay_t<Frame>>>
                        frame_p(frame);
                    hpx::apply(
                        [frame_p = std::move(frame_p), r = std::move(r)]() {
                            hpx::util::yield_while([]() { return !done; });
                            frame_p->set_data(std::move(r));
                        });
                }
            },
            [&](std::exception_ptr ep) {
                frame->set_exception(std::move(ep));
            });
    }
};

struct additional_argument
{
};

struct external_future_additional_argument_executor
{
    // This is not actually called by dataflow, but it is used for the return
    // type calculation of it. dataflow_finalize has to set the same type to the
    // future state.
    template <typename F, typename... Ts>
    friend decltype(auto) tag_invoke(hpx::parallel::execution::async_execute_t,
        external_future_additional_argument_executor const&, F&& f, Ts&&... ts)
    {
        if constexpr (std::is_void_v<hpx::util::invoke_result_t<F,
                          additional_argument, Ts...>>)
        {
            // The completion of f is signalled out-of-band.
            hpx::invoke(std::forward<F>(f), additional_argument{},
                std::forward<Ts>(ts)...);
            return hpx::async(
                []() { hpx::util::yield_while([]() { return !done; }); });
        }
        else
        {
            // The completion of f is signalled out-of-band.
            auto&& r = hpx::invoke(std::forward<F>(f), additional_argument{},
                std::forward<Ts>(ts)...);
            return hpx::async([r = std::move(r)]() {
                hpx::util::yield_while([]() { return !done; });
                return r;
            });
        }
    }

    template <typename Frame, typename F, typename Futures>
    void dataflow_finalize(Frame&& frame, F&& f, Futures&& futures)
    {
        static constexpr bool is_void =
            std::remove_pointer_t<std::decay_t<Frame>>::is_void::value;

        hpx::detail::try_catch_exception_ptr(
            [&]() {
                additional_argument a{};
                if constexpr (is_void)
                {
                    hpx::util::invoke_fused(std::forward<F>(f),
                        hpx::tuple_cat(
                            hpx::tie(a), std::forward<Futures>(futures)));

                    // Signal completion from another thread/task.
                    hpx::intrusive_ptr<
                        std::remove_pointer_t<std::decay_t<Frame>>>
                        frame_p(frame);
                    hpx::apply([frame_p = std::move(frame_p)]() {
                        hpx::util::yield_while([]() { return !done; });
                        frame_p->set_data(hpx::util::unused_type{});
                    });
                }
                else
                {
                    auto&& r = hpx::util::invoke_fused(std::forward<F>(f),
                        hpx::tuple_cat(
                            hpx::tie(a), std::forward<Futures>(futures)));

                    // Signal completion from another thread/task.
                    hpx::intrusive_ptr<
                        std::remove_pointer_t<std::decay_t<Frame>>>
                        frame_p(frame);
                    hpx::apply(
                        [frame_p = std::move(frame_p), r = std::move(r)]() {
                            hpx::util::yield_while([]() { return !done; });
                            frame_p->set_data(std::move(r));
                        });
                }
            },
            [&](std::exception_ptr ep) {
                frame->set_exception(std::move(ep));
            });
    }
};

namespace hpx { namespace parallel { namespace execution {
    template <>
    struct is_two_way_executor<external_future_executor> : std::true_type
    {
    };

    template <>
    struct is_two_way_executor<external_future_additional_argument_executor>
      : std::true_type
    {
    };
}}}    // namespace hpx::parallel::execution

int hpx_main()
{
    // We time the spawn and the wait. The wait should take significantly
    // longer than the spawn, and the wait should be long.
    {
        external_future_executor exec;
        hpx::chrono::high_resolution_timer t;
        hpx::future<void> f = hpx::dataflow(exec, []() {
            // This represents an asynchronous operation which has an
            // out-of-band mechanism for signaling completion.
            hpx::apply([]() {
                hpx::this_thread::sleep_for(std::chrono::milliseconds(500));
                done = true;
            });
        });
        double spawn_time = t.elapsed();
        t.restart();
        f.get();
        double wait_time = t.elapsed();
        HPX_TEST_LT(spawn_time, wait_time);
        HPX_TEST_LT(0.3, wait_time);
    }

    {
        done = false;

        external_future_executor exec;
        hpx::chrono::high_resolution_timer t;
        hpx::future<int> f = hpx::dataflow(exec, []() {
            // This represents an asynchronous operation which has an
            // out-of-band mechanism for signaling completion.
            hpx::apply([]() {
                hpx::this_thread::sleep_for(std::chrono::milliseconds(500));
                done = true;
            });

            return 42;
        });
        double spawn_time = t.elapsed();
        t.restart();
        int r = f.get();
        HPX_TEST_EQ(r, 42);
        double wait_time = t.elapsed();
        HPX_TEST_LT(spawn_time, wait_time);
        HPX_TEST_LT(0.3, wait_time);
    }

    {
        done = false;

        external_future_additional_argument_executor exec;
        hpx::chrono::high_resolution_timer t;
        hpx::future<void> f = hpx::dataflow(exec, [](additional_argument) {
            // This represents an asynchronous operation which has an
            // out-of-band mechanism for signaling completion.
            hpx::apply([]() {
                hpx::this_thread::sleep_for(std::chrono::milliseconds(500));
                done = true;
            });
        });
        double spawn_time = t.elapsed();
        t.restart();
        f.get();
        double wait_time = t.elapsed();
        HPX_TEST_LT(spawn_time, wait_time);
        HPX_TEST_LT(0.3, wait_time);
    }

    {
        done = false;

        external_future_additional_argument_executor exec;
        hpx::chrono::high_resolution_timer t;
        hpx::future<int> f = hpx::dataflow(exec, [](additional_argument) {
            // This represents an asynchronous operation which has an
            // out-of-band mechanism for signaling completion.
            hpx::apply([]() {
                hpx::this_thread::sleep_for(std::chrono::milliseconds(500));
                done = true;
            });

            return 42;
        });
        double spawn_time = t.elapsed();
        t.restart();
        int r = f.get();
        HPX_TEST_EQ(r, 42);
        double wait_time = t.elapsed();
        HPX_TEST_LT(spawn_time, wait_time);
        HPX_TEST_LT(0.3, wait_time);
    }

    return hpx::local::finalize();
}

int main(int argc, char* argv[])
{
    HPX_TEST_EQ_MSG(hpx::local::init(hpx_main, argc, argv), 0,
        "HPX main exited with non-zero status");

    return hpx::util::report_errors();
}
