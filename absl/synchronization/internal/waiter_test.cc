// Copyright 2023 The Abseil Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "absl/synchronization/internal/waiter.h"

#include <iostream>
#include <ostream>

#include "absl/synchronization/internal/create_thread_identity.h"
#include "absl/synchronization/internal/futex_waiter.h"
#include "absl/synchronization/internal/kernel_timeout.h"
#include "absl/synchronization/internal/pthread_waiter.h"
#include "absl/synchronization/internal/sem_waiter.h"
#include "absl/synchronization/internal/stdcpp_waiter.h"
#include "absl/synchronization/internal/thread_pool.h"
#include "absl/synchronization/internal/win32_waiter.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "gtest/gtest.h"

namespace {

TEST(Waiter, PrintPlatformImplementation) {
  // Allows us to verify that the platform is using the expected implementation.
  std::cout << absl::synchronization_internal::Waiter::kName << std::endl;
}

template <typename T>
class WaiterTest : public ::testing::Test {
 public:
  // Waiter implementations assume that a ThreadIdentity has already been
  // created.
  WaiterTest() {
    absl::synchronization_internal::GetOrCreateCurrentThreadIdentity();
  }
};

TYPED_TEST_SUITE_P(WaiterTest);

constexpr absl::Duration slop = absl::Milliseconds(2);

TYPED_TEST_P(WaiterTest, WaitNoTimeout) {
  absl::synchronization_internal::ThreadPool tp(1);
  TypeParam waiter;
  tp.Schedule([&]() {
    // Include some `Poke()` calls to ensure they don't cause `waiter` to return
    // from `Wait()`.
    waiter.Poke();
    absl::SleepFor(absl::Seconds(1));
    waiter.Poke();
    absl::SleepFor(absl::Seconds(1));
    waiter.Post();
  });
  absl::Time start = absl::Now();
  EXPECT_TRUE(
      waiter.Wait(absl::synchronization_internal::KernelTimeout::Never()));
  absl::Duration waited = absl::Now() - start;
  EXPECT_GE(waited, absl::Seconds(2) - slop);
}

TYPED_TEST_P(WaiterTest, WaitDurationWoken) {
  absl::synchronization_internal::ThreadPool tp(1);
  TypeParam waiter;
  tp.Schedule([&]() {
    // Include some `Poke()` calls to ensure they don't cause `waiter` to return
    // from `Wait()`.
    waiter.Poke();
    absl::SleepFor(absl::Milliseconds(500));
    waiter.Post();
  });
  absl::Time start = absl::Now();
  EXPECT_TRUE(waiter.Wait(
      absl::synchronization_internal::KernelTimeout(absl::Seconds(10))));
  absl::Duration waited = absl::Now() - start;
  EXPECT_GE(waited, absl::Milliseconds(500) - slop);
  EXPECT_LT(waited, absl::Seconds(1));
}

TYPED_TEST_P(WaiterTest, WaitTimeWoken) {
  absl::synchronization_internal::ThreadPool tp(1);
  TypeParam waiter;
  tp.Schedule([&]() {
    // Include some `Poke()` calls to ensure they don't cause `waiter` to return
    // from `Wait()`.
    waiter.Poke();
    absl::SleepFor(absl::Milliseconds(500));
    waiter.Post();
  });
  absl::Time start = absl::Now();
  EXPECT_TRUE(waiter.Wait(absl::synchronization_internal::KernelTimeout(
      start + absl::Seconds(10))));
  absl::Duration waited = absl::Now() - start;
  EXPECT_GE(waited, absl::Milliseconds(500) - slop);
  EXPECT_LT(waited, absl::Seconds(1));
}

TYPED_TEST_P(WaiterTest, WaitDurationReached) {
  TypeParam waiter;
  absl::Time start = absl::Now();
  EXPECT_FALSE(waiter.Wait(
      absl::synchronization_internal::KernelTimeout(absl::Milliseconds(500))));
  absl::Duration waited = absl::Now() - start;
  EXPECT_GE(waited, absl::Milliseconds(500) - slop);
  EXPECT_LT(waited, absl::Seconds(1));
}

TYPED_TEST_P(WaiterTest, WaitTimeReached) {
  TypeParam waiter;
  absl::Time start = absl::Now();
  EXPECT_FALSE(waiter.Wait(absl::synchronization_internal::KernelTimeout(
      start + absl::Milliseconds(500))));
  absl::Duration waited = absl::Now() - start;
  EXPECT_GE(waited, absl::Milliseconds(500) - slop);
  EXPECT_LT(waited, absl::Seconds(1));
}

REGISTER_TYPED_TEST_SUITE_P(WaiterTest,
                            WaitNoTimeout,
                            WaitDurationWoken,
                            WaitTimeWoken,
                            WaitDurationReached,
                            WaitTimeReached);

#ifdef ABSL_INTERNAL_HAVE_FUTEX_WAITER
INSTANTIATE_TYPED_TEST_SUITE_P(Futex, WaiterTest,
                               absl::synchronization_internal::FutexWaiter);
#endif
#ifdef ABSL_INTERNAL_HAVE_PTHREAD_WAITER
INSTANTIATE_TYPED_TEST_SUITE_P(Pthread, WaiterTest,
                               absl::synchronization_internal::PthreadWaiter);
#endif
#ifdef ABSL_INTERNAL_HAVE_SEM_WAITER
INSTANTIATE_TYPED_TEST_SUITE_P(Sem, WaiterTest,
                               absl::synchronization_internal::SemWaiter);
#endif
#ifdef ABSL_INTERNAL_HAVE_WIN32_WAITER
INSTANTIATE_TYPED_TEST_SUITE_P(Win32, WaiterTest,
                               absl::synchronization_internal::Win32Waiter);
#endif
#ifdef ABSL_INTERNAL_HAVE_STDCPP_WAITER
INSTANTIATE_TYPED_TEST_SUITE_P(Stdcpp, WaiterTest,
                               absl::synchronization_internal::StdcppWaiter);
#endif

}  // namespace
