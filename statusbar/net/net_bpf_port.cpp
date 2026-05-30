// Copyright 2026 Jeff Koftinoff <jeff.koftinoff@statusbar.com>
// SPDX-License-Identifier: MIT

#include "statusbar/net/net_bpf_port.hpp"

#include "statusbar/net/net_error.hpp"
#include "statusbar/net/net_posix_util.hpp"

#include <fcntl.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>
#include <ctime>

#include <net/if.h>
#include <sys/ioctl.h>

#if defined(__linux__)
#    include <arpa/inet.h>
#    include <linux/if_ether.h>
#    include <linux/if_packet.h>
#    include <sys/socket.h>
#elif defined(__APPLE__)
#    include <ifaddrs.h>

#    include <net/bpf.h>
#    include <net/if_dl.h>
#    include <sys/socket.h>
#    include <sys/time.h>
#    include <sys/types.h>
#endif

namespace statusbar::net {

void BpfPortContext::swap(BpfPortContext& other) noexcept
{
    auto tmp_fd = fd_;
    fd_ = other.fd_;
    other.fd_ = tmp_fd;

    auto tmp_mac = hardware_address_;
    hardware_address_ = other.hardware_address_;
    other.hardware_address_ = tmp_mac;

    auto tmp_idx = if_index_;
    if_index_ = other.if_index_;
    other.if_index_ = tmp_idx;

    auto tmp_et = ethertype_;
    ethertype_ = other.ethertype_;
    other.ethertype_ = tmp_et;

    auto tmp_clk = rx_clock_id_;
    rx_clock_id_ = other.rx_clock_id_;
    other.rx_clock_id_ = tmp_clk;

    pool_.swap(other.pool_);
    free_list_.swap(other.free_list_);
    launch_times_.swap(other.launch_times_);

#if defined(__APPLE__)
    auto tmp_blen = bpf_buf_len_;
    bpf_buf_len_ = other.bpf_buf_len_;
    other.bpf_buf_len_ = tmp_blen;

    auto tmp_off = bpf_read_offset_;
    bpf_read_offset_ = other.bpf_read_offset_;
    other.bpf_read_offset_ = tmp_off;

    auto tmp_rlen = bpf_read_length_;
    bpf_read_length_ = other.bpf_read_length_;
    other.bpf_read_length_ = tmp_rlen;

    bpf_read_buffer_.swap(other.bpf_read_buffer_);
#endif
}

//
// ── Linux Implementation ──────────────────────────────────────────────
//

#if defined(__linux__)

[[nodiscard]] auto BpfPortContext::open(BpfPortConfig const& config, std::pmr::memory_resource* memory_resource) -> Status
{
    close();

    if (config.interface_name.empty()) {
        return failure(NetError::invalid_argument);
    }

    // Create AF_PACKET raw socket
    uint16_t const proto = config.ethertype != 0 ? config.ethertype : ETH_P_ALL;
    int const sock_fd = ::socket(AF_PACKET, SOCK_RAW, htons(proto));
    if (sock_fd < 0) {
        return failure(std::error_code(errno, std::system_category()));
    }

    // Get interface index
    unsigned int const idx = ::if_nametoindex(std::string(config.interface_name).c_str());
    if (idx == 0) {
        ::close(sock_fd);
        return failure(std::error_code(errno, std::system_category()));
    }

    // Get MAC address via ioctl
    struct ifreq ifr{};
    auto const name_len = config.interface_name.size() < IFNAMSIZ ? config.interface_name.size() : IFNAMSIZ - 1;
    for (size_t i = 0; i < name_len; ++i) {
        ifr.ifr_name[i] = config.interface_name[i];
    }
    ifr.ifr_name[name_len] = '\0';

    if (::ioctl(sock_fd, SIOCGIFHWADDR, &ifr) < 0) {
        ::close(sock_fd);
        return failure(std::error_code(errno, std::system_category()));
    }

    ieee::Eui48 mac{};
    for (int i = 0; i < 6; ++i) {
        mac.value[i] = static_cast<uint8_t>(ifr.ifr_hwaddr.sa_data[i]);
    }

    // Bind to interface
    struct sockaddr_ll saddr{};
    saddr.sll_family = AF_PACKET;
    saddr.sll_ifindex = static_cast<int>(idx);
    saddr.sll_protocol = htons(proto);

    if (::bind(sock_fd, sockaddr_cast(saddr), sizeof(saddr)) < 0) {
        ::close(sock_fd);
        return failure(std::error_code(errno, std::system_category()));
    }

    // Set non-blocking
    int const flags = ::fcntl(sock_fd, F_GETFL, 0);
    if (flags < 0 || ::fcntl(sock_fd, F_SETFL, flags | O_NONBLOCK) < 0) {
        ::close(sock_fd);
        return failure(std::error_code(errno, std::system_category()));
    }

    fd_ = sock_fd;
    hardware_address_ = mac;
    if_index_ = idx;
    ethertype_ = config.ethertype;
    rx_clock_id_ = config.rx_clock_id;

    mem_resource_ = memory_resource != nullptr ? memory_resource : std::pmr::get_default_resource();
    pool_ = std::pmr::vector<std::array<uint8_t, ETHERNET_PORT_MIN_FRAME_BUFFER>>(mem_resource_);
    free_list_ = std::pmr::vector<uint64_t>(mem_resource_);
    launch_times_ = std::pmr::vector<int64_t>(mem_resource_);

    size_t const pool_size = config.frame_pool_size > 0 ? config.frame_pool_size : 256;
    allocate_pool(pool_size);

    return success();
}

void BpfPortContext::close() noexcept
{
    if (fd_ >= 0) {
        ::close(fd_);
        fd_ = -1;
    }
    hardware_address_ = {};
    if_index_ = 0;
    ethertype_ = 0;
    pool_.clear();
    free_list_.clear();
    launch_times_.clear();
}

[[nodiscard]] auto BpfPortContext::tx_commit(uint64_t handle, size_t len) -> Status
{
    if (handle >= pool_.size()) {
        return failure(NetError::invalid_handle);
    }
    if (fd_ < 0) {
        free_list_.push_back(handle);
        return failure(NetError::not_connected);
    }

    auto const& buf = pool_[static_cast<size_t>(handle)];
    size_t const actual_len = (len > ETHERNET_PORT_MIN_FRAME_BUFFER) ? ETHERNET_PORT_MIN_FRAME_BUFFER : len;

    // Send immediately via AF_PACKET raw socket
    struct sockaddr_ll socket_address{};
    socket_address.sll_family = AF_PACKET;
    socket_address.sll_ifindex = static_cast<int>(if_index_);
    socket_address.sll_halen = 6;
    // Copy destination MAC from frame header (first 6 bytes)
    for (int i = 0; i < 6; ++i) {
        socket_address.sll_addr[i] = buf[i];
    }

    ssize_t sent = 0;
    do {
        sent = ::sendto(fd_, buf.data(), actual_len, 0, sockaddr_cast(socket_address), sizeof(socket_address));
    } while (sent < 0 && errno == EINTR);

    free_list_.push_back(handle);

    if (sent < 0) {
        return failure(std::error_code(errno, std::system_category()));
    }

    return success();
}

[[nodiscard]] auto BpfPortContext::rx_start() -> StatusValue<std::optional<EthernetRxSlot>>
{
    if (fd_ < 0) {
        return failure(NetError::not_connected);
    }

    if (free_list_.empty()) {
        return failure(NetError::buffer_full);
    }

    uint64_t const handle = free_list_.back();
    auto& buf = pool_[static_cast<size_t>(handle)];

    ssize_t n = 0;
    do {
        n = ::recv(fd_, buf.data(), buf.size(), 0);
    } while (n < 0 && errno == EINTR);

    if (n < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) {
            return success(std::optional<EthernetRxSlot>{});
        }
        return failure(std::error_code(errno, std::system_category()));
    }

    if (n == 0) {
        return success(std::optional<EthernetRxSlot>{});
    }

    free_list_.pop_back();

    // Get timestamp
    struct timespec ts{};
    ::clock_gettime(rx_clock_id_, &ts);
    int64_t const timestamp_ns = (static_cast<int64_t>(ts.tv_sec) * 1'000'000'000LL) + ts.tv_nsec;

    return success(
        std::optional<EthernetRxSlot>{EthernetRxSlot{
            .handle = handle,
            .buffer = make_const_span(buf).first(static_cast<size_t>(n)),
            .timestamp_ns = timestamp_ns,
        }});
}

[[nodiscard]] auto BpfPortContext::join_multicast(ieee::Eui48 const& addr) -> Status
{
    if (fd_ < 0) {
        return failure(NetError::not_connected);
    }

    struct packet_mreq mreq{};
    mreq.mr_ifindex = static_cast<int>(if_index_);
    mreq.mr_type = PACKET_MR_MULTICAST;
    mreq.mr_alen = 6;
    for (int i = 0; i < 6; ++i) {
        mreq.mr_address[i] = addr.value[i];
    }

    if (::setsockopt(fd_, SOL_PACKET, PACKET_ADD_MEMBERSHIP, &mreq, sizeof(mreq)) < 0) {
        return failure(std::error_code(errno, std::system_category()));
    }

    return success();
}

#elif defined(__APPLE__)

//
// ── macOS Implementation ──────────────────────────────────────────────
//

[[nodiscard]] auto BpfPortContext::open(BpfPortConfig const& config, std::pmr::memory_resource* memory_resource) -> Status
{
    close();

    if (config.interface_name.empty()) {
        return failure(NetError::invalid_argument);
    }

    // Find and open an available BPF device
    int bpf_fd = -1;
    char dev_path[32];
    for (int i = 0; i < 100; ++i) {
        snprintf(dev_path, sizeof(dev_path), "/dev/bpf%d", i);
        bpf_fd = ::open(dev_path, O_RDWR);
        if (bpf_fd >= 0) {
            break;
        }
        if (errno != EBUSY) {
            continue;
        }
    }

    if (bpf_fd < 0) {
        return failure(std::error_code(errno, std::system_category()));
    }

    // Bind to interface
    struct ifreq ifr{};
    auto const name_len = config.interface_name.size() < IFNAMSIZ ? config.interface_name.size() : IFNAMSIZ - 1;
    for (size_t i = 0; i < name_len; ++i) {
        ifr.ifr_name[i] = config.interface_name[i];
    }
    ifr.ifr_name[name_len] = '\0';

    if (::ioctl(bpf_fd, BIOCSETIF, &ifr) < 0) {
        ::close(bpf_fd);
        return failure(std::error_code(errno, std::system_category()));
    }

    // Get BPF buffer length
    u_int buf_len = 0;
    if (::ioctl(bpf_fd, BIOCGBLEN, &buf_len) < 0) {
        ::close(bpf_fd);
        return failure(std::error_code(errno, std::system_category()));
    }

    // Enable immediate mode
    u_int immediate = 1;
    if (::ioctl(bpf_fd, BIOCIMMEDIATE, &immediate) < 0) {
        ::close(bpf_fd);
        return failure(std::error_code(errno, std::system_category()));
    }

    // Enable header complete mode (we provide the full Ethernet header)
    u_int hdrcmplt = 1;
    if (::ioctl(bpf_fd, BIOCSHDRCMPLT, &hdrcmplt) < 0) {
        ::close(bpf_fd);
        return failure(std::error_code(errno, std::system_category()));
    }

    // Don't see our own packets
    u_int seesent = 0;
    ::ioctl(bpf_fd, BIOCSSEESENT, &seesent);  // non-fatal

    // Set BPF filter for ethertype if specified
    if (config.ethertype != 0) {
        struct bpf_insn filter[] = {
            BPF_STMT(BPF_LD + BPF_H + BPF_ABS, 12),
            BPF_JUMP(BPF_JMP + BPF_JEQ + BPF_K, config.ethertype, 0, 1),
            BPF_STMT(BPF_RET + BPF_K, 0xFFFFFFFF),
            BPF_STMT(BPF_RET + BPF_K, 0),
        };

        struct bpf_program prog{};
        prog.bf_len = sizeof(filter) / sizeof(filter[0]);
        prog.bf_insns = filter;

        if (::ioctl(bpf_fd, BIOCSETF, &prog) < 0) {
            ::close(bpf_fd);
            return failure(std::error_code(errno, std::system_category()));
        }
    }

    // Get interface index
    unsigned int const idx = ::if_nametoindex(ifr.ifr_name);
    if (idx == 0) {
        ::close(bpf_fd);
        return failure(std::error_code(errno, std::system_category()));
    }

    // Get MAC address via getifaddrs
    ieee::Eui48 mac{};
    struct ifaddrs* ifaddr = nullptr;
    if (::getifaddrs(&ifaddr) == 0) {
        for (struct ifaddrs const* ifa = ifaddr; ifa != nullptr; ifa = ifa->ifa_next) {
            if (ifa->ifa_addr == nullptr || ifa->ifa_addr->sa_family != AF_LINK) {
                continue;
            }
            if (std::strncmp(ifa->ifa_name, ifr.ifr_name, IFNAMSIZ) != 0) {
                continue;
            }
            // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast) — macOS sockaddr_dl overlay
            auto* sdl = reinterpret_cast<struct sockaddr_dl*>(ifa->ifa_addr);
            if (sdl->sdl_alen == 6) {
                // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast) — LLADDR returns char*
                auto const* lladdr = reinterpret_cast<uint8_t const*>(LLADDR(sdl));
                for (int j = 0; j < 6; ++j) {
                    mac.value[j] = lladdr[j];
                }
                break;
            }
        }
        ::freeifaddrs(ifaddr);
    }

    // Set non-blocking
    int const flags = ::fcntl(bpf_fd, F_GETFL, 0);
    if (flags < 0 || ::fcntl(bpf_fd, F_SETFL, flags | O_NONBLOCK) < 0) {
        ::close(bpf_fd);
        return failure(std::error_code(errno, std::system_category()));
    }

    fd_ = bpf_fd;
    hardware_address_ = mac;
    if_index_ = idx;
    ethertype_ = config.ethertype;
    rx_clock_id_ = config.rx_clock_id;
    bpf_buf_len_ = buf_len;

    mem_resource_ = memory_resource != nullptr ? memory_resource : std::pmr::get_default_resource();
    pool_ = std::pmr::vector<std::array<uint8_t, ETHERNET_PORT_MIN_FRAME_BUFFER>>(mem_resource_);
    free_list_ = std::pmr::vector<uint64_t>(mem_resource_);
    launch_times_ = std::pmr::vector<int64_t>(mem_resource_);
    bpf_read_buffer_ = std::pmr::vector<uint8_t>(mem_resource_);

    bpf_read_buffer_.resize(buf_len);
    bpf_read_offset_ = 0;
    bpf_read_length_ = 0;

    size_t const pool_size = config.frame_pool_size > 0 ? config.frame_pool_size : 256;
    allocate_pool(pool_size);

    return success();
}

void BpfPortContext::close() noexcept
{
    if (fd_ >= 0) {
        ::close(fd_);
        fd_ = -1;
    }
    hardware_address_ = {};
    if_index_ = 0;
    ethertype_ = 0;
    pool_.clear();
    free_list_.clear();
    launch_times_.clear();
    bpf_buf_len_ = 0;
    bpf_read_buffer_.clear();
    bpf_read_offset_ = 0;
    bpf_read_length_ = 0;
}

[[nodiscard]] auto BpfPortContext::tx_commit(uint64_t handle, size_t len) -> Status
{
    if (handle >= pool_.size()) {
        return failure(NetError::invalid_handle);
    }
    if (fd_ < 0) {
        free_list_.push_back(handle);
        return failure(NetError::not_connected);
    }

    auto const& buf = pool_[static_cast<size_t>(handle)];
    size_t const actual_len = (len > ETHERNET_PORT_MIN_FRAME_BUFFER) ? ETHERNET_PORT_MIN_FRAME_BUFFER : len;

    // Write frame to BPF device
    ssize_t sent = 0;
    do {
        sent = ::write(fd_, buf.data(), actual_len);
    } while (sent < 0 && errno == EINTR);

    free_list_.push_back(handle);

    if (sent < 0) {
        return failure(std::error_code(errno, std::system_category()));
    }

    return success();
}

[[nodiscard]] auto BpfPortContext::rx_start() -> StatusValue<std::optional<EthernetRxSlot>>
{
    if (fd_ < 0) {
        return failure(NetError::not_connected);
    }

    if (free_list_.empty()) {
        return failure(NetError::buffer_full);
    }

    // Check if we have buffered data from a previous read
    while (bpf_read_offset_ < bpf_read_length_) {
        // Verify the BPF header itself fits in the remaining buffer before
        // reinterpret_casting and dereferencing it.
        if (bpf_read_length_ - bpf_read_offset_ < sizeof(struct bpf_hdr)) {
            break;
        }
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast) — macOS BPF packet walker
        auto* bh = reinterpret_cast<struct bpf_hdr*>(bpf_read_buffer_.data() + bpf_read_offset_);

        // Verify the header-declared frame fits in the remaining buffer.
        size_t const total = static_cast<size_t>(bh->bh_hdrlen) + static_cast<size_t>(bh->bh_caplen);
        if (total > bpf_read_length_ - bpf_read_offset_) {
            break;
        }

        uint8_t const* frame_data = bpf_read_buffer_.data() + bpf_read_offset_ + bh->bh_hdrlen;
        uint32_t const caplen = bh->bh_caplen;

        // Advance to next packet. Guard against the corrupt-header case where
        // the aligned step is zero — without this the loop would spin forever.
        size_t const advance = BPF_WORDALIGN(total);
        if (advance == 0) {
            break;
        }
        bpf_read_offset_ += advance;

        if (caplen < 14) {
            continue;  // Skip runt frames
        }

        // Copy frame into a pool buffer
        uint64_t const handle = free_list_.back();
        free_list_.pop_back();

        auto& buf = pool_[static_cast<size_t>(handle)];
        size_t const copy_len = (caplen > ETHERNET_PORT_MIN_FRAME_BUFFER) ? ETHERNET_PORT_MIN_FRAME_BUFFER : caplen;
        std::copy_n(frame_data, copy_len, buf.data());

        // Timestamp from BPF header
        int64_t const timestamp_ns = (static_cast<int64_t>(bh->bh_tstamp.tv_sec) * 1'000'000'000LL) +
            (static_cast<int64_t>(bh->bh_tstamp.tv_usec) * 1'000LL);

        return success(
            std::optional<EthernetRxSlot>{EthernetRxSlot{
                .handle = handle,
                .buffer = make_const_span(buf).first(copy_len),
                .timestamp_ns = timestamp_ns,
            }});
    }

    // Need to read more data from BPF
    ssize_t n = 0;
    do {
        n = ::read(fd_, bpf_read_buffer_.data(), bpf_read_buffer_.size());
    } while (n < 0 && errno == EINTR);

    if (n < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) {
            return success(std::optional<EthernetRxSlot>{});
        }
        return failure(std::error_code(errno, std::system_category()));
    }

    if (n == 0) {
        return success(std::optional<EthernetRxSlot>{});
    }

    bpf_read_offset_ = 0;
    bpf_read_length_ = static_cast<size_t>(n);

    // Process the newly read data (recursive call to handle the buffer)
    return rx_start();
}

[[nodiscard]] auto BpfPortContext::join_multicast(ieee::Eui48 const& addr) -> Status
{
    if (fd_ < 0) {
        return failure(NetError::not_connected);
    }

    // Use SIOCADDMULTI to join multicast group on the interface
    struct ifreq ifr{};
    // We need the interface name; get it from the index
    char ifname[IFNAMSIZ]{};
    if (::if_indextoname(if_index_, ifname) == nullptr) {
        return failure(std::error_code(errno, std::system_category()));
    }
    std::strncpy(ifr.ifr_name, ifname, IFNAMSIZ - 1);

    ifr.ifr_addr.sa_family = AF_UNSPEC;
    for (int i = 0; i < 6; ++i) {
        ifr.ifr_addr.sa_data[i] = static_cast<char>(addr.value[i]);
    }

    // Create a temporary socket for the ioctl (BPF fd doesn't support SIOCADDMULTI)
    int const tmp_sock = ::socket(AF_INET, SOCK_DGRAM, 0);
    if (tmp_sock < 0) {
        return failure(std::error_code(errno, std::system_category()));
    }

    int const result = ::ioctl(tmp_sock, SIOCADDMULTI, &ifr);
    int const saved_errno = errno;
    ::close(tmp_sock);

    if (result < 0) {
        return failure(std::error_code(saved_errno, std::system_category()));
    }

    return success();
}

#else

//
// ── Fallback stub (neither Linux nor macOS) ───────────────────────────
//

[[nodiscard]] auto BpfPortContext::open(BpfPortConfig const&, std::pmr::memory_resource*) -> Status
{
    return failure(NetError::not_supported);
}

void BpfPortContext::close() noexcept
{
    fd_ = -1;
    hardware_address_ = {};
    if_index_ = 0;
    ethertype_ = 0;
    pool_.clear();
    free_list_.clear();
    launch_times_.clear();
}

[[nodiscard]] auto BpfPortContext::tx_commit(uint64_t handle, size_t) -> Status
{
    if (handle < pool_.size()) {
        free_list_.push_back(handle);
    }
    return failure(NetError::not_supported);
}

[[nodiscard]] auto BpfPortContext::rx_start() -> StatusValue<std::optional<EthernetRxSlot>>
{
    return failure(NetError::not_supported);
}

[[nodiscard]] auto BpfPortContext::join_multicast(ieee::Eui48 const&) -> Status
{
    return failure(NetError::not_supported);
}

#endif

}  // namespace statusbar::net
