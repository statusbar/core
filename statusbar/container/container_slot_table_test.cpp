// Copyright 2026 Jeff Koftinoff <jeff.koftinoff@statusbar.com>
// SPDX-License-Identifier: MIT

#include "statusbar/container/container_slot_table.hpp"

#include "statusbar/test/test.hpp"

#include <utility>

using statusbar::container::SlotTable;

namespace {

struct TestEntry
{
    int id{};
    auto operator==(TestEntry const& other) const -> bool { return id == other.id; }
};

}  // namespace

TEST(container_slot_table_default, construction)
{
    SlotTable<TestEntry, 4> t;
    EXPECT_EQ(t.size(), 0U);
    EXPECT_TRUE(t.empty());
    EXPECT_EQ(t.capacity(), 4U);
    EXPECT_FALSE(t.is_full());
    EXPECT_EQ(decltype(t)::max_capacity, 4U);
}

TEST(container_slot_table_softcap, construction_sets_capacity)
{
    SlotTable<TestEntry, 8> t{3};
    EXPECT_EQ(t.capacity(), 3U);
    EXPECT_EQ(decltype(t)::max_capacity, 8U);
}

TEST(container_slot_table_softcap, clamps_to_max)
{
    SlotTable<TestEntry, 4> t{100};
    EXPECT_EQ(t.capacity(), 4U);
}

TEST(container_slot_table_add, returns_true_and_grows_size)
{
    SlotTable<TestEntry, 4> t;
    EXPECT_TRUE(t.add(TestEntry{1}));
    EXPECT_EQ(t.size(), 1U);
    EXPECT_FALSE(t.empty());
}

TEST(container_slot_table_add, returns_false_at_soft_cap)
{
    SlotTable<TestEntry, 8> t{2};
    EXPECT_TRUE(t.add(TestEntry{1}));
    EXPECT_TRUE(t.add(TestEntry{2}));
    EXPECT_FALSE(t.add(TestEntry{3}));
    EXPECT_EQ(t.size(), 2U);
    EXPECT_TRUE(t.is_full());
}

TEST(container_slot_table_add, returns_false_at_hard_cap)
{
    SlotTable<TestEntry, 2> t;
    EXPECT_TRUE(t.add(TestEntry{1}));
    EXPECT_TRUE(t.add(TestEntry{2}));
    EXPECT_FALSE(t.add(TestEntry{3}));
    EXPECT_TRUE(t.is_full());
}

TEST(container_slot_table_add, emplace_forwarding_constructs_in_place)
{
    SlotTable<TestEntry, 4> t;
    EXPECT_TRUE(t.add(TestEntry{42}));
    EXPECT_EQ(t[0].id, 42);
}

TEST(container_slot_table_remove, shrinks_size)
{
    SlotTable<TestEntry, 4> t;
    t.add(TestEntry{1});
    t.add(TestEntry{2});
    t.remove(0);
    EXPECT_EQ(t.size(), 1U);
}

TEST(container_slot_table_remove, non_last_swap_with_last)
{
    SlotTable<TestEntry, 4> t;
    t.add(TestEntry{1});
    t.add(TestEntry{2});
    t.add(TestEntry{3});
    // After removing index 0, the former last (id=3) is swapped in.
    t.remove(0);
    EXPECT_EQ(t.size(), 2U);
    EXPECT_EQ(t[0].id, 3);
    EXPECT_EQ(t[1].id, 2);
}

TEST(container_slot_table_remove, last_element_does_not_swap)
{
    SlotTable<TestEntry, 4> t;
    t.add(TestEntry{1});
    t.add(TestEntry{2});
    t.remove(1);
    EXPECT_EQ(t.size(), 1U);
    EXPECT_EQ(t[0].id, 1);
}

TEST(container_slot_table_remove, out_of_range_is_noop)
{
    SlotTable<TestEntry, 4> t;
    t.add(TestEntry{1});
    t.remove(99);
    EXPECT_EQ(t.size(), 1U);
    EXPECT_EQ(t[0].id, 1);
}

TEST(container_slot_table_clear, empties_and_preserves_soft_cap)
{
    SlotTable<TestEntry, 8> t{3};
    t.add(TestEntry{1});
    t.add(TestEntry{2});
    t.clear();
    EXPECT_EQ(t.size(), 0U);
    EXPECT_TRUE(t.empty());
    EXPECT_EQ(t.capacity(), 3U);
}

TEST(container_slot_table_find, returns_first_match_index)
{
    SlotTable<TestEntry, 4> t;
    t.add(TestEntry{1});
    t.add(TestEntry{2});
    t.add(TestEntry{3});
    auto const idx = t.find_if([](TestEntry const& e) { return e.id == 2; });
    EXPECT_EQ(idx, 1U);
}

TEST(container_slot_table_find, not_found_returns_capacity_with_soft_cap)
{
    SlotTable<TestEntry, 8> t{3};
    t.add(TestEntry{1});
    auto const idx = t.find_if([](TestEntry const& e) { return e.id == 99; });
    EXPECT_EQ(idx, 3U);
}

TEST(container_slot_table_find, not_found_returns_capacity_without_soft_cap)
{
    SlotTable<TestEntry, 4> t;
    t.add(TestEntry{1});
    auto const idx = t.find_if([](TestEntry const& e) { return e.id == 99; });
    EXPECT_EQ(idx, 4U);
}

TEST(container_slot_table_find, empty_table_returns_capacity)
{
    SlotTable<TestEntry, 4> t;
    auto const idx = t.find_if([](TestEntry const&) { return true; });
    EXPECT_EQ(idx, 4U);
}

TEST(container_slot_table_access, operator_const_and_non_const)
{
    SlotTable<TestEntry, 4> t;
    t.add(TestEntry{42});
    t[0].id = 99;
    EXPECT_EQ(std::as_const(t)[0].id, 99);
}

TEST(container_slot_table_access, get_in_range_returns_pointer)
{
    SlotTable<TestEntry, 4> t;
    t.add(TestEntry{42});
    auto* p = t.get(0);
    EXPECT_NE(p, nullptr);
    EXPECT_EQ(p->id, 42);
}

TEST(container_slot_table_access, get_out_of_range_returns_nullptr)
{
    SlotTable<TestEntry, 4> t;
    EXPECT_EQ(t.get(0), nullptr);
    EXPECT_EQ(t.get(99), nullptr);
    t.add(TestEntry{1});
    EXPECT_EQ(t.get(1), nullptr);
}

TEST(container_slot_table_iter, iterates_occupied_slots)
{
    SlotTable<TestEntry, 4> t;
    t.add(TestEntry{1});
    t.add(TestEntry{2});
    int sum = 0;
    for (auto const& e : t) {
        sum += e.id;
    }
    EXPECT_EQ(sum, 3);
}

TEST(container_slot_table_iter, iteration_reflects_remove_reordering)
{
    SlotTable<TestEntry, 4> t;
    t.add(TestEntry{10});
    t.add(TestEntry{20});
    t.add(TestEntry{30});
    t.remove(0);  // id=30 is now at index 0, id=20 at index 1
    int sum = 0;
    for (auto const& e : t) {
        sum += e.id;
    }
    EXPECT_EQ(sum, 50);
}

TEST(container_slot_table_full, is_full_matches_size_equals_capacity)
{
    SlotTable<TestEntry, 2> t;
    EXPECT_FALSE(t.is_full());
    t.add(TestEntry{1});
    EXPECT_FALSE(t.is_full());
    t.add(TestEntry{2});
    EXPECT_TRUE(t.is_full());
    t.remove(0);
    EXPECT_FALSE(t.is_full());
}

TEST_MAIN(statusbar_container, container_slot_table_test)
