// Copyright 2026 Jeff Koftinoff <jeff.koftinoff@statusbar.com>
// SPDX-License-Identifier: MIT

#include "statusbar/mem/mem_pmr_unique.hpp"
#include "statusbar/test/test.hpp"

#include <cstddef>
#include <memory_resource>
#include <utility>

namespace {

using statusbar::mem::pmr_make_unique;
using statusbar::mem::PmrDeleter;
using statusbar::mem::PmrUniquePtr;

// A memory_resource that records every allocate/deallocate so tests can assert
// the storage came from (and returned to) the chosen resource, with the right
// sized-deallocation arguments.
class TrackingResource : public std::pmr::memory_resource
{
  public:
    size_t allocations{0};
    size_t deallocations{0};
    size_t last_alloc_bytes{0};
    size_t last_alloc_align{0};
    size_t last_dealloc_bytes{0};
    size_t last_dealloc_align{0};

  private:
    auto do_allocate(size_t bytes, size_t align) -> void* override
    {
        ++allocations;
        last_alloc_bytes = bytes;
        last_alloc_align = align;
        return std::pmr::new_delete_resource()->allocate(bytes, align);
    }

    void do_deallocate(void* ptr, size_t bytes, size_t align) override
    {
        ++deallocations;
        last_dealloc_bytes = bytes;
        last_dealloc_align = align;
        std::pmr::new_delete_resource()->deallocate(ptr, bytes, align);
    }

    [[nodiscard]] auto do_is_equal(std::pmr::memory_resource const& other) const noexcept -> bool override
    {
        return this == &other;
    }
};

// Counts live instances to prove the destructor runs on reset/scope-exit.
struct Tracked
{
    static inline int live = 0;
    int value{0};

    explicit Tracked(int v)
        : value{v}
    {
        ++live;
    }
    ~Tracked() { --live; }

    Tracked(Tracked const&) = delete;
    auto operator=(Tracked const&) -> Tracked& = delete;
};

}  // namespace

TEST(statusbar_mem_pmr_unique, allocates_and_constructs)
{
    TrackingResource res;
    auto p = pmr_make_unique<int>(&res, 42);

    EXPECT_TRUE(static_cast<bool>(p));
    EXPECT_EQ(*p, 42);
    EXPECT_EQ(res.allocations, 1U);
    EXPECT_EQ(res.deallocations, 0U);
    EXPECT_EQ(res.last_alloc_bytes, sizeof(int));
    EXPECT_EQ(res.last_alloc_align, alignof(int));
    // Deleter remembers the resource.
    EXPECT_EQ(p.get_deleter().resource(), &res);
}

TEST(statusbar_mem_pmr_unique, frees_to_same_resource_with_sized_dealloc)
{
    TrackingResource res;
    {
        auto p = pmr_make_unique<double>(&res, 3.5);
        EXPECT_EQ(*p, 3.5);
    }
    EXPECT_EQ(res.allocations, 1U);
    EXPECT_EQ(res.deallocations, 1U);
    EXPECT_EQ(res.last_dealloc_bytes, sizeof(double));
    EXPECT_EQ(res.last_dealloc_align, alignof(double));
}

TEST(statusbar_mem_pmr_unique, runs_destructor)
{
    TrackingResource res;
    EXPECT_EQ(Tracked::live, 0);
    {
        auto p = pmr_make_unique<Tracked>(&res, 7);
        EXPECT_EQ(p->value, 7);
        EXPECT_EQ(Tracked::live, 1);
    }
    EXPECT_EQ(Tracked::live, 0);
    EXPECT_EQ(res.deallocations, 1U);
}

TEST(statusbar_mem_pmr_unique, move_transfers_single_ownership)
{
    TrackingResource res;
    EXPECT_EQ(Tracked::live, 0);
    {
        auto a = pmr_make_unique<Tracked>(&res, 1);
        PmrUniquePtr<Tracked> b = std::move(a);
        EXPECT_FALSE(static_cast<bool>(a));
        EXPECT_TRUE(static_cast<bool>(b));
        EXPECT_EQ(b->value, 1);
        EXPECT_EQ(Tracked::live, 1);
        // moved-to deleter still points at the original resource
        EXPECT_EQ(b.get_deleter().resource(), &res);
    }
    // Exactly one allocation and one deallocation despite the move.
    EXPECT_EQ(res.allocations, 1U);
    EXPECT_EQ(res.deallocations, 1U);
    EXPECT_EQ(Tracked::live, 0);
}

TEST(statusbar_mem_pmr_unique, reset_releases_early)
{
    TrackingResource res;
    auto p = pmr_make_unique<Tracked>(&res, 9);
    EXPECT_EQ(Tracked::live, 1);
    p.reset();
    EXPECT_EQ(Tracked::live, 0);
    EXPECT_EQ(res.deallocations, 1U);
    EXPECT_FALSE(static_cast<bool>(p));
}

TEST_MAIN(statusbar_mem, mem_pmr_unique_test)
