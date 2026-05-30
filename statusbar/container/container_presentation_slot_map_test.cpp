// Copyright 2026 Jeff Koftinoff <jeff.koftinoff@statusbar.com>
// SPDX-License-Identifier: MIT

#include "statusbar/container/container_presentation_slot_map.hpp"

#include "statusbar/test/test.hpp"

#include <cstdint>
#include <optional>

using namespace statusbar;

namespace {

/// Small map: 8 slots, 1 ms each, 8 ms total window. The narrow window
/// makes wraparound easy to exercise.
using SmallMap = container::PresentationSlotMap<uint32_t, 8>;

constexpr int64_t slot_ns = 1'000'000;  // 1 ms

}  // namespace

TEST(presentation_slot_map, fresh_map_has_no_payloads)
{
    SmallMap m{slot_ns};
    EXPECT_EQ(m.live_count(), size_t{0});
    EXPECT_FALSE(m.load(0).has_value());
    EXPECT_FALSE(m.load(5'000'000).has_value());
    EXPECT_FALSE(m.has_payload_at(5'000'000));
}

TEST(presentation_slot_map, store_then_load_returns_payload)
{
    SmallMap m{slot_ns};
    m.store(3'000'000, 42);
    EXPECT_TRUE(m.has_payload_at(3'000'000));
    auto p = m.load(3'000'000);
    EXPECT_TRUE(p.has_value());
    EXPECT_EQ(*p, uint32_t{42});
    EXPECT_EQ(m.live_count(), size_t{1});
}

TEST(presentation_slot_map, load_at_different_time_returns_nullopt)
{
    SmallMap m{slot_ns};
    m.store(3'000'000, 42);
    // Different slot entirely.
    EXPECT_FALSE(m.load(5'000'000).has_value());
}

TEST(presentation_slot_map, load_within_same_slot_returns_payload)
{
    // 1 ms slot width: any time in [3 ms, 4 ms) maps to the same slot
    // and quantizes to the same boundary, so all reads in that range
    // resolve to the stored payload.
    SmallMap m{slot_ns};
    m.store(3'250'000, 42);                     // 3.25 ms
    EXPECT_TRUE(m.has_payload_at(3'000'000));   // start of slot
    EXPECT_TRUE(m.has_payload_at(3'250'000));   // exactly the stored time
    EXPECT_TRUE(m.has_payload_at(3'500'000));   // mid-slot
    EXPECT_TRUE(m.has_payload_at(3'999'999));   // last ns of slot
    EXPECT_FALSE(m.has_payload_at(4'000'000));  // first ns of next slot
}

TEST(presentation_slot_map, store_overwrites_unconditionally)
{
    SmallMap m{slot_ns};
    m.store(2'000'000, 11);
    m.store(2'000'000, 22);  // overwrite at same slot/time
    EXPECT_EQ(*m.load(2'000'000), uint32_t{22});
}

TEST(presentation_slot_map, store_overwrites_stale_slot_at_wrap)
{
    // 8 slots × 1 ms = 8 ms window. Slot index for 2 ms is also the
    // index for 10 ms (2+8). A store at 10 ms overwrites the prior
    // 2 ms entry, and the original 2 ms timestamp becomes stale.
    SmallMap m{slot_ns};
    m.store(2'000'000, 11);
    EXPECT_TRUE(m.has_payload_at(2'000'000));
    m.store(10'000'000, 22);
    EXPECT_FALSE(m.has_payload_at(2'000'000));  // stale, evicted by wrap
    EXPECT_EQ(*m.load(10'000'000), uint32_t{22});
}

TEST(presentation_slot_map, store_if_absent_writes_when_empty)
{
    SmallMap m{slot_ns};
    EXPECT_TRUE(m.store_if_absent(4'000'000, 99));
    EXPECT_EQ(*m.load(4'000'000), uint32_t{99});
}

TEST(presentation_slot_map, store_if_absent_does_not_overwrite_same_time)
{
    // Primary already in the slot; redundant for the same time should
    // not displace it.
    SmallMap m{slot_ns};
    m.store(4'000'000, 11);
    EXPECT_FALSE(m.store_if_absent(4'000'000, 22));
    EXPECT_EQ(*m.load(4'000'000), uint32_t{11});
}

TEST(presentation_slot_map, store_if_absent_writes_when_slot_holds_stale_data)
{
    // Slot index for 1 ms equals slot index for 9 ms (wrap). Earlier
    // store at 1 ms is stale by the time 9 ms arrives, so a
    // store_if_absent at 9 ms should write.
    SmallMap m{slot_ns};
    m.store(1'000'000, 11);
    EXPECT_TRUE(m.store_if_absent(9'000'000, 22));
    EXPECT_EQ(*m.load(9'000'000), uint32_t{22});
    EXPECT_FALSE(m.has_payload_at(1'000'000));  // displaced by wrap
}

TEST(presentation_slot_map, store_after_store_if_absent_overwrites)
{
    // Redundant arrived first (slot now holds the redundant payload at
    // T). Primary arrives next for the same T — primary is the
    // canonical source and overwrites unconditionally.
    SmallMap m{slot_ns};
    EXPECT_TRUE(m.store_if_absent(2'000'000, 11));
    m.store(2'000'000, 22);
    EXPECT_EQ(*m.load(2'000'000), uint32_t{22});
}

TEST(presentation_slot_map, clear_invalidates_slot)
{
    SmallMap m{slot_ns};
    m.store(3'000'000, 42);
    EXPECT_TRUE(m.has_payload_at(3'000'000));
    m.clear(3'000'000);
    EXPECT_FALSE(m.has_payload_at(3'000'000));
    EXPECT_FALSE(m.load(3'000'000).has_value());
    // Clearing an already-empty slot is a no-op.
    m.clear(3'000'000);
    EXPECT_FALSE(m.has_payload_at(3'000'000));
}

TEST(presentation_slot_map, reset_wipes_all_slots)
{
    SmallMap m{slot_ns};
    m.store(1'000'000, 1);
    m.store(2'000'000, 2);
    m.store(3'000'000, 3);
    EXPECT_EQ(m.live_count(), size_t{3});
    m.reset();
    EXPECT_EQ(m.live_count(), size_t{0});
    EXPECT_FALSE(m.load(1'000'000).has_value());
    EXPECT_FALSE(m.load(2'000'000).has_value());
    EXPECT_FALSE(m.load(3'000'000).has_value());
}

TEST(presentation_slot_map, multiple_independent_slots)
{
    SmallMap m{slot_ns};
    m.store(1'000'000, 1);
    m.store(2'000'000, 2);
    m.store(7'000'000, 7);
    EXPECT_EQ(*m.load(1'000'000), uint32_t{1});
    EXPECT_EQ(*m.load(2'000'000), uint32_t{2});
    EXPECT_EQ(*m.load(7'000'000), uint32_t{7});
    EXPECT_EQ(m.live_count(), size_t{3});
}

TEST(presentation_slot_map, capacity_and_window_constants_match_template)
{
    SmallMap m{slot_ns};
    static_assert(SmallMap::capacity == 8);
    EXPECT_EQ(m.slot_width_ns(), slot_ns);
    EXPECT_EQ(m.window_ns(), slot_ns * 8);
}

TEST(presentation_slot_map, redundancy_pattern_primary_first)
{
    // Walks through the canonical primary/redundant flow: primary fills
    // a slot, redundant for the same time is rejected, redundant for a
    // gap (no primary) fills it.
    using container::PresentationSlotMap;
    PresentationSlotMap<uint32_t, 16> m{slot_ns};

    // T=1ms primary lands first.
    m.store(1'000'000, 0xAA);
    // Redundant copy of the same primary arrives later (different wall
    // time, same wire timestamp) — primary already wins.
    EXPECT_FALSE(m.store_if_absent(1'000'000, 0xBB));
    EXPECT_EQ(*m.load(1'000'000), uint32_t{0xAA});

    // T=2ms primary is lost on the wire; only the redundant arrives.
    EXPECT_TRUE(m.store_if_absent(2'000'000, 0xCC));
    EXPECT_EQ(*m.load(2'000'000), uint32_t{0xCC});

    // T=3ms loses both copies — slot stays empty, consumer treats as loss.
    EXPECT_FALSE(m.has_payload_at(3'000'000));
}

TEST(presentation_slot_map, redundancy_pattern_redundant_first)
{
    // Redundant arrives ahead of primary (rare reordering). Slot
    // initially carries the redundant payload; primary arrives second
    // and overwrites unconditionally because store() (not
    // store_if_absent) is the canonical write.
    using container::PresentationSlotMap;
    PresentationSlotMap<uint32_t, 16> m{slot_ns};

    EXPECT_TRUE(m.store_if_absent(5'000'000, 0xCC));
    EXPECT_EQ(*m.load(5'000'000), uint32_t{0xCC});
    m.store(5'000'000, 0xAA);  // primary catches up
    EXPECT_EQ(*m.load(5'000'000), uint32_t{0xAA});
}

TEST_MAIN(statusbar_container, container_presentation_slot_map_test)
