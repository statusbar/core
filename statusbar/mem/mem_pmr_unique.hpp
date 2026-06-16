// Copyright 2026 Jeff Koftinoff <jeff.koftinoff@statusbar.com>
// SPDX-License-Identifier: MIT
//
// PmrDeleter / PmrUniquePtr / pmr_make_unique — a make_unique analog that
// allocates the object from a caller-supplied std::pmr::memory_resource.
//
// Motivation: std::make_unique always uses global operator new. For code paths
// where the client should choose the allocator (arenas, fixed pools, shared
// buffers), migrate a `std::make_unique<T>(...)` call to
// `pmr_make_unique<T>(resource, ...)`: same construct-in-place semantics, but
// the storage comes from `resource` and is returned to that same resource on
// destruction (the deleter carries the resource pointer, so the returned
// PmrUniquePtr is self-contained and movable like any unique_ptr).
//
// Exception model: libraries/tools build with -fno-exceptions (see the root
// CMakeLists ENABLE_EXCEPTIONS option). Under that mode a failed
// memory_resource::allocate terminates rather than throwing, and a throwing
// constructor likewise terminates, so there is no reachable leak path and this
// helper deliberately contains no try/catch (which would not compile with
// -fno-exceptions). The behavior matches std::make_unique: allocate, then
// construct in place.

#pragma once

#include <cstddef>
#include <memory>
#include <memory_resource>
#include <new>
#include <utility>

namespace statusbar::mem {

/// Deleter for an object allocated from a std::pmr::memory_resource.
///
/// Runs `~T()` then returns the storage to the resource via sized deallocation
/// (`deallocate(ptr, sizeof(T), alignof(T))`). Stores the resource pointer so a
/// `unique_ptr<T, PmrDeleter<T>>` is self-contained. Default-constructed (e.g.
/// in a moved-from unique_ptr) it refers to the default resource and only ever
/// runs against a null pointer, so it never touches the wrong allocator.
///
/// Note: there is intentionally no `PmrDeleter<Derived> -> PmrDeleter<Base>`
/// converting constructor. Sized deallocation needs the *exact* type, so a
/// base/derived conversion would deallocate with the wrong size. Hold a
/// PmrUniquePtr<T> at the concrete type T.
template <typename T>
class PmrDeleter
{
  public:
    PmrDeleter() noexcept = default;

    explicit PmrDeleter(std::pmr::memory_resource* const resource) noexcept
        : resource_{resource}
    {}

    void operator()(T* const ptr) const noexcept
    {
        if (ptr != nullptr) {
            ptr->~T();
            resource_->deallocate(ptr, sizeof(T), alignof(T));
        }
    }

    [[nodiscard]] auto resource() const noexcept -> std::pmr::memory_resource* { return resource_; }

  private:
    std::pmr::memory_resource* resource_{std::pmr::get_default_resource()};
};

/// A unique_ptr whose storage comes from a std::pmr::memory_resource.
template <typename T>
using PmrUniquePtr = std::unique_ptr<T, PmrDeleter<T>>;

/// make_unique analog that allocates `T` from `resource` and constructs it in
/// place with the forwarded arguments. The returned PmrUniquePtr carries the
/// resource, so destruction returns the storage to the same resource.
///
/// @param resource Memory resource to allocate from (must outlive the object).
/// @param args     Constructor arguments, perfectly forwarded.
/// @return A PmrUniquePtr<T> owning the constructed object.
template <typename T, typename... Args>
[[nodiscard]] auto pmr_make_unique(std::pmr::memory_resource* const resource, Args&&... args) -> PmrUniquePtr<T>
{
    void* const storage = resource->allocate(sizeof(T), alignof(T));
    // Placement-new; with -fno-exceptions a throwing ctor terminates, so no
    // cleanup path is reachable (see file header).
    T* const obj = ::new (storage) T(std::forward<Args>(args)...);
    return PmrUniquePtr<T>{obj, PmrDeleter<T>{resource}};
}

}  // namespace statusbar::mem
