// Copyright 2026 Jeff Koftinoff <jeff.koftinoff@statusbar.com>
// SPDX-License-Identifier: MIT

// Embed the compiled BPF object using C23 #embed.
// This lives in a plain .cpp (not a .cppm module interface) to work around a
// clang-19 bug where #embed data in module interface units is placed in BSS
// (zero-initialized) instead of .rodata.

#if defined(__linux__) && defined(HAVE_XDP)

// extern is required: in C++, const at namespace scope implies internal linkage.
extern unsigned char const xdp_filter_bpf_o[] = {
#    embed "xdp_filter.bpf.o"
};
extern unsigned int const xdp_filter_bpf_o_len = sizeof(xdp_filter_bpf_o);

#endif
