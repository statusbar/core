// Copyright 2026 Jeff Koftinoff <jeff.koftinoff@statusbar.com>
// SPDX-License-Identifier: MIT

// Unit tests for Args module

#include "statusbar/args/args.hpp"

#include "statusbar/ieee/ieee.hpp"
#include "statusbar/test/test.hpp"
#include "statusbar/toml/toml_value.hpp"
#include "statusbar/tsn/tsn.hpp"

using namespace statusbar;
using namespace statusbar::args;

//
// ArgumentSpecs basic operations
//

TEST(args_specs, add_flag)
{
    ArgumentSpecs specs;
    specs.add_flag("verbose", "Enable verbose output");

    EXPECT_EQ(specs.specs().size(), 1U);
    EXPECT_EQ(specs.specs()[0].name, "verbose");
    EXPECT_EQ(specs.specs()[0].type, ArgType::Flag);
    EXPECT_EQ(specs.specs()[0].description, "Enable verbose output");
}

TEST(args_specs, add_file)
{
    ArgumentSpecs specs;
    specs.add_file("config", "Config file path", "/etc/app.toml");

    EXPECT_EQ(specs.specs().size(), 1U);
    EXPECT_EQ(specs.specs()[0].name, "config");
    EXPECT_EQ(specs.specs()[0].type, ArgType::File);
    EXPECT_EQ(specs.specs()[0].default_value, "/etc/app.toml");
}

TEST(args_specs, add_choice)
{
    ArgumentSpecs specs;
    specs.add_choice("mode", "Operating mode", {"fast", "slow", "auto"}, "auto");

    EXPECT_EQ(specs.specs().size(), 1U);
    EXPECT_EQ(specs.specs()[0].name, "mode");
    EXPECT_EQ(specs.specs()[0].type, ArgType::Choice);
    EXPECT_EQ(specs.specs()[0].default_value, "auto");
    EXPECT_EQ(specs.specs()[0].choices.size(), 3U);
    EXPECT_EQ(specs.specs()[0].choices[0], "fast");
    EXPECT_EQ(specs.specs()[0].choices[2], "auto");
}

TEST(args_specs, add_typed_int)
{
    ArgumentSpecs specs;
    specs.add<int>("port", "Port number", 8080);

    EXPECT_EQ(specs.specs().size(), 1U);
    EXPECT_EQ(specs.specs()[0].name, "port");
    EXPECT_EQ(specs.specs()[0].type, ArgType::Integer);
    EXPECT_EQ(specs.specs()[0].default_value, "8080");
}

TEST(args_specs, add_typed_double)
{
    ArgumentSpecs specs;
    specs.add<double>("gain", "Gain value", 1.0);

    EXPECT_EQ(specs.specs().size(), 1U);
    EXPECT_EQ(specs.specs()[0].name, "gain");
    EXPECT_EQ(specs.specs()[0].type, ArgType::Float);
}

TEST(args_specs, add_typed_string)
{
    ArgumentSpecs specs;
    specs.add<std::string>("name", "Device name", std::string{"default"});

    EXPECT_EQ(specs.specs().size(), 1U);
    EXPECT_EQ(specs.specs()[0].name, "name");
    EXPECT_EQ(specs.specs()[0].type, ArgType::String);
    EXPECT_EQ(specs.specs()[0].default_value, "default");
}

TEST(args_specs, is_known)
{
    ArgumentSpecs specs;
    specs.add_flag("verbose", "Verbose mode");
    specs.add<int>("port", "Port", 80);

    EXPECT_TRUE(specs.is_known("verbose"));
    EXPECT_TRUE(specs.is_known("port"));
    EXPECT_FALSE(specs.is_known("unknown"));
    EXPECT_FALSE(specs.is_known(""));
}

//
// ArgumentSpecs bindings
//

TEST(args_bindings, flag_binding)
{
    bool verbose = false;
    ArgumentSpecs specs;
    specs.add_flag("verbose", "Verbose mode", [&](bool v) { verbose = v; });

    toml::Table root;
    root.set("verbose", toml::Value{true});
    auto status = specs.apply(root);
    EXPECT_TRUE(status.has_value());
    EXPECT_TRUE(verbose);
}

TEST(args_bindings, int_binding)
{
    int port = 0;
    ArgumentSpecs specs;
    specs.add<int>("port", "Port number", 8080, [&](int v) { port = v; });

    toml::Table root;
    root.set("port", toml::Value{int64_t{9090}});
    auto status = specs.apply(root);
    EXPECT_TRUE(status.has_value());
    EXPECT_EQ(port, 9090);
}

TEST(args_bindings, file_binding)
{
    std::string path;
    ArgumentSpecs specs;
    specs.add_file("config", "Config", "/default.toml", [&](std::string_view v) { path = v; });

    toml::Table root;
    root.set("config", toml::Value{std::string{"/custom.toml"}});
    auto status = specs.apply(root);
    EXPECT_TRUE(status.has_value());
    EXPECT_EQ(path, "/custom.toml");
}

TEST(args_bindings, choice_binding)
{
    std::string mode;
    ArgumentSpecs specs;
    specs.add_choice("mode", "Mode", {"fast", "slow"}, "fast", [&](std::string_view v) { mode = v; });

    toml::Table root;
    root.set("mode", toml::Value{std::string{"slow"}});
    auto status = specs.apply(root);
    EXPECT_TRUE(status.has_value());
    EXPECT_EQ(mode, "slow");
}

TEST(args_bindings, apply_no_bindings)
{
    ArgumentSpecs specs;
    specs.add_flag("verbose", "Verbose");  // no binding

    toml::Table root;
    auto status = specs.apply(root);
    EXPECT_TRUE(status.has_value());
}

//
// Defaults and validation
//

TEST(args_defaults, create_default_config)
{
    ArgumentSpecs specs;
    specs.add<int>("port", "Port", 8080);
    specs.add_flag("verbose", "Verbose");
    specs.add<std::string>("name", "Name", std::string{"test"});

    auto table = specs.create_default_config();
    auto const* port_val = table.get_path("port");
    EXPECT_TRUE(port_val != nullptr);
    EXPECT_EQ(port_val->as_integer().value_or(0), int64_t{8080});

    auto const* name_val = table.get_path("name");
    EXPECT_TRUE(name_val != nullptr);
    EXPECT_EQ(name_val->as_string().value_or(""), "test");
}

TEST(args_defaults, populate_does_not_overwrite)
{
    ArgumentSpecs specs;
    specs.add<int>("port", "Port", 8080);

    toml::Table root;
    root.set("port", toml::Value{int64_t{9090}});  // existing value
    specs.populate_defaults(root);

    auto const* port_val = root.get_path("port");
    EXPECT_TRUE(port_val != nullptr);
    EXPECT_EQ(port_val->as_integer().value_or(0), int64_t{9090});  // not overwritten
}

TEST(args_defaults, find_unknown_keys)
{
    ArgumentSpecs specs;
    specs.add_flag("verbose", "Verbose");
    specs.add<int>("port", "Port", 80);

    toml::Table root;
    root.set("verbose", toml::Value{true});
    root.set("port", toml::Value{int64_t{8080}});
    root.set("unknown_key", toml::Value{int64_t{42}});

    auto unknown = specs.find_unknown_keys(root);
    EXPECT_EQ(unknown.size(), 1U);
    EXPECT_EQ(unknown[0], "unknown_key");
}

//
// Merge
//

TEST(args_merge, merge_without_prefix)
{
    ArgumentSpecs specs1;
    specs1.add_flag("verbose", "Verbose");

    ArgumentSpecs specs2;
    specs2.add<int>("port", "Port", 80);

    specs1.merge(specs2);
    EXPECT_EQ(specs1.specs().size(), 2U);
    EXPECT_TRUE(specs1.is_known("verbose"));
    EXPECT_TRUE(specs1.is_known("port"));
}

TEST(args_merge, merge_with_prefix)
{
    ArgumentSpecs main_specs;
    main_specs.add_flag("verbose", "Verbose");

    ArgumentSpecs sub_specs;
    sub_specs.add<int>("port", "Port", 80);

    main_specs.merge(sub_specs, "server.");
    EXPECT_EQ(main_specs.specs().size(), 2U);
    EXPECT_TRUE(main_specs.is_known("verbose"));
    EXPECT_TRUE(main_specs.is_known("server.port"));
    EXPECT_FALSE(main_specs.is_known("port"));
}

//
// Script generation
//

TEST(args_generate, bash_contains_program_name)
{
    ArgumentSpecs specs;
    specs.add_flag("verbose", "Verbose");
    specs.add<int>("port", "Port", 80);
    specs.add_choice("mode", "Mode", {"fast", "slow"}, "fast");

    auto script = specs.generate_bash("my-app");
    EXPECT_TRUE(script.find("my-app") != std::string::npos);
    EXPECT_TRUE(script.find("--verbose") != std::string::npos);
    EXPECT_TRUE(script.find("--port") != std::string::npos);
    EXPECT_TRUE(script.find("fast") != std::string::npos);
    EXPECT_TRUE(script.find("slow") != std::string::npos);
}

TEST(args_generate, zsh_contains_program_name)
{
    ArgumentSpecs specs;
    specs.add_flag("verbose", "Verbose");
    specs.add<int>("port", "Port", 80);

    auto script = specs.generate_zsh("my-app");
    EXPECT_TRUE(script.find("my-app") != std::string::npos);
    EXPECT_TRUE(script.find("verbose") != std::string::npos);
    EXPECT_TRUE(script.find("port") != std::string::npos);
}

TEST(args_generate, fish_contains_program_name)
{
    ArgumentSpecs specs;
    specs.add_flag("verbose", "Verbose");
    specs.add_file("config", "Config file", "/etc/app.toml");

    auto script = specs.generate_fish("my-app");
    EXPECT_TRUE(script.find("my-app") != std::string::npos);
    EXPECT_TRUE(script.find("verbose") != std::string::npos);
    EXPECT_TRUE(script.find("config") != std::string::npos);
}

TEST(args_generate, bash_handles_device_type)
{
    ArgumentSpecs specs;
    std::string device;
    specs.add_device("interface", "Network interface", "eth0", [&](std::string_view v) { device = v; });

    auto script = specs.generate_bash("net-tool");
    EXPECT_TRUE(script.find("--interface") != std::string::npos);
    // Device type should trigger network interface completion
    EXPECT_TRUE(script.find("ifconfig") != std::string::npos || script.find("ip ") != std::string::npos);
}

//
// ArgTypeTraits - type specializations
//

TEST(args_type_traits, uint64_get)
{
    toml::Table root;
    root.set("val", toml::Value{int64_t{1000}});
    ArgumentSpecs specs;
    uint64_t out = 0;
    specs.add<uint64_t>("val", "test", uint64_t{0}, [&](uint64_t v) { out = v; });
    (void)specs.apply(root);
    EXPECT_EQ(out, uint64_t{1000});
}

TEST(args_type_traits, uint32_get)
{
    toml::Table root;
    root.set("val", toml::Value{int64_t{500}});
    ArgumentSpecs specs;
    uint32_t out = 0;
    specs.add<uint32_t>("val", "test", uint32_t{0}, [&](uint32_t v) { out = v; });
    (void)specs.apply(root);
    EXPECT_EQ(out, uint32_t{500});
}

TEST(args_type_traits, uint16_get)
{
    toml::Table root;
    root.set("val", toml::Value{int64_t{300}});
    ArgumentSpecs specs;
    uint16_t out = 0;
    specs.add<uint16_t>("val", "test", uint16_t{0}, [&](uint16_t v) { out = v; });
    (void)specs.apply(root);
    EXPECT_EQ(out, uint16_t{300});
}

TEST(args_type_traits, int16_get)
{
    toml::Table root;
    root.set("val", toml::Value{int64_t{-100}});
    ArgumentSpecs specs;
    int16_t out = 0;
    specs.add<int16_t>("val", "test", int16_t{0}, [&](int16_t v) { out = v; });
    (void)specs.apply(root);
    EXPECT_EQ(out, int16_t{-100});
}

TEST(args_type_traits, uint8_get)
{
    toml::Table root;
    root.set("val", toml::Value{int64_t{200}});
    ArgumentSpecs specs;
    uint8_t out = 0;
    specs.add<uint8_t>("val", "test", uint8_t{0}, [&](uint8_t v) { out = v; });
    (void)specs.apply(root);
    EXPECT_EQ(out, uint8_t{200});
}

TEST(args_type_traits, int8_get)
{
    toml::Table root;
    root.set("val", toml::Value{int64_t{-50}});
    ArgumentSpecs specs;
    int8_t out = 0;
    specs.add<int8_t>("val", "test", int8_t{0}, [&](int8_t v) { out = v; });
    (void)specs.apply(root);
    EXPECT_EQ(out, int8_t{-50});
}

// Error-path coverage for ArgTypeTraits<integral T>: the implementation does
// a plain `static_cast<T>(...)` after pulling an `int64_t` out of the toml
// node, so values outside T's range silently wrap. These tests pin down that
// behaviour so any future "reject on overflow" change is intentional.

TEST(args_type_traits, uint8_overflow_wraps)
{
    toml::Table root;
    root.set("val", toml::Value{int64_t{300}});  // > UINT8_MAX
    ArgumentSpecs specs;
    uint8_t out = 0;
    specs.add<uint8_t>("val", "test", uint8_t{0}, [&](uint8_t v) { out = v; });
    (void)specs.apply(root);
    EXPECT_EQ(out, static_cast<uint8_t>(300));  // 300 & 0xFF == 44
}

TEST(args_type_traits, int16_overflow_wraps)
{
    toml::Table root;
    root.set("val", toml::Value{int64_t{100000}});  // > INT16_MAX
    ArgumentSpecs specs;
    int16_t out = 0;
    specs.add<int16_t>("val", "test", int16_t{0}, [&](int16_t v) { out = v; });
    (void)specs.apply(root);
    EXPECT_EQ(out, static_cast<int16_t>(100000));
}

TEST(args_type_traits, integer_get_falls_back_to_default_on_string_value)
{
    // toml::Value::as_integer() returns nullopt when the node is not an
    // integer — the trait then uses value_or(default).
    toml::Table root;
    root.set("val", toml::Value{std::string{"not-a-number"}});
    ArgumentSpecs specs;
    int32_t out = 0;
    specs.add<int32_t>("val", "test", int32_t{-7}, [&](int32_t v) { out = v; });
    (void)specs.apply(root);
    EXPECT_EQ(out, int32_t{-7});
}

TEST(args_type_traits, float_get_falls_back_to_default_on_string_value)
{
    toml::Table root;
    root.set("val", toml::Value{std::string{"not-a-float"}});
    ArgumentSpecs specs;
    float out = 0.0f;
    specs.add<float>("val", "test", 3.5f, [&](float v) { out = v; });
    (void)specs.apply(root);
    EXPECT_TRUE(out > 3.4f && out < 3.6f);
}

TEST(args_type_traits, float_get)
{
    toml::Table root;
    root.set("val", toml::Value{2.5});
    ArgumentSpecs specs;
    float out = 0.0f;
    specs.add<float>("val", "test", 0.0f, [&](float v) { out = v; });
    (void)specs.apply(root);
    EXPECT_TRUE(out > 2.4f && out < 2.6f);
}

TEST(args_type_traits, string_view_get)
{
    toml::Table root;
    root.set("val", toml::Value{std::string{"hello_sv"}});
    ArgumentSpecs specs;
    std::string out;
    specs.add<std::string_view>("val", "test", std::string_view{"default"}, [&](std::string_view v) { out = std::string{v}; });
    (void)specs.apply(root);
    EXPECT_TRUE(out == "hello_sv");
}

TEST(args_type_traits, string_get)
{
    toml::Table root;
    root.set("val", toml::Value{std::string{"hello_str"}});
    ArgumentSpecs specs;
    std::string out;
    specs.add<std::string>("val", "test", std::string{"default"}, [&](std::string v) { out = std::move(v); });
    (void)specs.apply(root);
    EXPECT_TRUE(out == "hello_str");
}

TEST(args_type_traits, eui48_get)
{
    toml::Table root;
    statusbar::ieee::Eui48 mac{};
    mac.value = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF};
    root.set("mac", toml::Value{ieee::to_string(mac)});
    ArgumentSpecs specs;
    statusbar::ieee::Eui48 out{};
    specs.add<statusbar::ieee::Eui48>("mac", "test", statusbar::ieee::Eui48{}, [&](statusbar::ieee::Eui48 v) { out = v; });
    (void)specs.apply(root);
    EXPECT_TRUE(out == mac);
}

TEST(args_type_traits, eui64_get)
{
    toml::Table root;
    statusbar::ieee::Eui64 eui{0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77};
    root.set("eui", toml::Value{ieee::to_string(eui)});
    ArgumentSpecs specs;
    statusbar::ieee::Eui64 out{};
    specs.add<statusbar::ieee::Eui64>("eui", "test", statusbar::ieee::Eui64{}, [&](statusbar::ieee::Eui64 v) { out = v; });
    (void)specs.apply(root);
    EXPECT_TRUE(out == eui);
}

TEST(args_type_traits, stream_id_get)
{
    toml::Table root;
    statusbar::ieee::Eui48 sid_mac{};
    sid_mac.value = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55};
    statusbar::tsn::StreamId sid{sid_mac, 0x6677};
    root.set("sid", toml::Value{tsn::to_string(sid)});
    ArgumentSpecs specs;
    statusbar::tsn::StreamId out{};
    specs.add<statusbar::tsn::StreamId>("sid", "test", statusbar::tsn::StreamId{}, [&](statusbar::tsn::StreamId v) { out = v; });
    (void)specs.apply(root);
    EXPECT_TRUE(out == sid);
}

//
// CLI arg type direct get_value
//

TEST(args_cli_types, long_get_value)
{
    toml::Table root;
    root.set("count", toml::Value{int64_t{42}});
    auto val = ArgTypeTraits<int64_t>::get_value(root, "count", int64_t{0});
    EXPECT_EQ(val, int64_t{42});
}

TEST(args_cli_types, double_get_value)
{
    toml::Table root;
    root.set("rate", toml::Value{48000.0});
    auto val = ArgTypeTraits<double>::get_value(root, "rate", 0.0);
    EXPECT_TRUE(val > 47999.0 && val < 48001.0);
}

TEST(args_cli_types, bool_get_value)
{
    toml::Table root;
    root.set("flag", toml::Value{true});
    auto val = ArgTypeTraits<bool>::get_value(root, "flag", false);
    EXPECT_TRUE(val);
}

TEST_MAIN(statusbar_args, args_test)
