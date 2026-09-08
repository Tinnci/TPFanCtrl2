-- Project Information
set_project("TPFanCtrl2")
set_version("2.8.0", {build = function () 
    return try { function() return os.ioread("git rev-parse --short HEAD"):trim() end } or "unknown"
end})

-- Build options
option("gui")
    set_default(true)
    set_showmenu(true)
    set_description("Build GUI application (TPFanCtrl2)")
option_end()

option("tests")
    set_default(true)
    set_showmenu(true)
    set_description("Build test targets (logic_test, core_test)")
option_end()

-- Add dependencies
add_requires("nlohmann_json")
add_requires("spdlog")

if has_config("gui") then
    add_requires("imgui master", {configs = {win32 = true, vulkan = true, freetype = true}})
    add_requires("vulkan-loader")
    add_requires("freetype")
    add_requires("vulkan-memory-allocator")
end

if has_config("tests") then
    add_requires("gtest")
end

-- Native 64-bit architecture by default
if not get_config("arch") then
    set_arch("x64")
end

-- Define version info rule
rule("version_info")
    on_load(function (target)
        local git_commit = try { function() local out = os.iorun("git rev-parse --short HEAD") return out and out:trim() or "dev" end } or "dev"
        local git_date = try { function() local out = os.iorun("git log -1 --format=%cd --date=short") return out and out:trim() or "2026-09-08" end } or "2026-09-08"
        target:add("defines", 'TPFC_VERSION="2.8.0"')
        target:add("defines", 'TPFC_COMMIT="' .. git_commit .. '"')
        target:add("defines", 'TPFC_BUILD_DATE="' .. git_date .. '"')
    end)
rule_end()

-- Sync default architecture output to artifacts/bin for convenient direct CLI execution
rule("sync_bin_root")
    after_build(function (target)
        import("core.project.config")
        local arch = config.get("arch") or "x64"
        if arch == "x64" then
            os.cp(target:targetfile(), "artifacts/bin/")
        end
    end)
rule_end()

-- Define build modes
add_rules("mode.debug", "mode.release", "version_info", "sync_bin_root")

-- Global settings
set_languages("c++20")
add_defines("WIN32", "_MBCS")

-- Compiler flags: support both MSVC and Clang/Zig
add_cxflags("/J", "/utf-8", {tools = "msvc"})
add_cxflags("-funsigned-char", "-finput-charset=UTF-8", {tools = {"clang", "zig"}})

if is_plat("windows") then
    add_cxflags("/W4", {tools = "msvc"})
    add_cxflags("-Wall", "-Wextra", {tools = {"clang", "zig"}})
end

if has_config("gui") then
-- Target: TPFanCtrl2 (Main GUI App)
target("TPFanCtrl2")
    set_kind("binary")
    set_plat("windows")

    add_packages("imgui", "vulkan-loader", "freetype", "vulkan-memory-allocator", "spdlog", "nlohmann_json")
    
    -- Keep the GUI binary detached from a console window in every build mode.
    add_ldflags("/SUBSYSTEM:WINDOWS", "/ENTRY:mainCRTStartup", {force = true})
    add_ldflags("/SUBSYSTEM:WINDOWS", "/ENTRY:mainCRTStartup", {force = true, tools = "msvc"})
    add_ldflags("-Wl,/SUBSYSTEM:WINDOWS", "-Wl,/ENTRY:mainCRTStartup", {force = true, tools = {"clang", "zig"}})
    
    -- Precompiled Header (Disable in CodeQL environment to avoid PCH issues)
    if os.getenv("XMAKE_PCH") ~= "false" and not os.getenv("CODEQL_ACTION_INIT_HAS_RUN") then
        set_pcxxheader("fancontrol/_prec.h")
    end
    
    -- Source files
    add_files("fancontrol/Application.cpp")
    add_files("fancontrol/ConfigManager.cpp")
    add_files("fancontrol/ECManager.cpp")
    add_files("fancontrol/SensorManager.cpp")
    add_files("fancontrol/FanController.cpp")
    add_files("fancontrol/PawnIOProvider.cpp")
    add_files("fancontrol/I18nManager.cpp")
    add_files("fancontrol/dynamicicon.cpp")
    add_files("fancontrol/imgui_main.cpp")
    add_files("fancontrol/_prec.cpp")
    add_files("fancontrol/Core/*.cpp")  -- Core library
    add_files("fancontrol/res/fancontrol.rc")
    add_files("fancontrol/res/app.manifest")
    
    -- Include directories
    add_includedirs("fancontrol")
    add_includedirs("fancontrol/Core")
    
    -- Link libraries
    add_links("comctl32", "user32", "gdi32", "advapi32", "shell32", "ole32", "oleaut32", "uuid", "dwmapi")
    
    -- Output directory
    set_targetdir("artifacts/bin/" .. (get_config("arch") or "x64"))

    -- Optimizations for Release mode
    if is_mode("release") then
        set_optimize("fastest")
        set_symbols("hidden")
        set_strip("all")
        -- MSVC: Link Time Code Generation
        add_cxflags("/GL", {tools = "msvc"})
        add_ldflags("/LTCG", {tools = "msvc"})
        -- Clang/Zig: Link Time Optimization
        add_cxflags("-flto", {tools = {"clang", "zig"}})
        add_ldflags("-flto", {tools = {"clang", "zig"}})
        add_vectorexts("sse2")
    end
end

-- Target: TPFanCtrl2-cli (PowerShell/console hardware control)
target("TPFanCtrl2-cli")
    set_kind("binary")
    set_plat("windows")
    add_packages("spdlog", "nlohmann_json")

    add_ldflags("/SUBSYSTEM:CONSOLE", {force = true, tools = "msvc"})
    add_ldflags("-Wl,/SUBSYSTEM:CONSOLE", {force = true, tools = {"clang", "zig"}})

    if os.getenv("XMAKE_PCH") ~= "false" and not os.getenv("CODEQL_ACTION_INIT_HAS_RUN") then
        set_pcxxheader("fancontrol/_prec.h")
    end

    -- Reuse the same EC and fan-control implementation as the GUI.
    add_files("fancontrol/cli_main.cpp")
    add_files("fancontrol/ECManager.cpp")
    add_files("fancontrol/FanController.cpp")
    add_files("fancontrol/PawnIOProvider.cpp")

    add_includedirs("fancontrol")
    add_links("comctl32", "user32", "advapi32")
    set_targetdir("artifacts/bin/" .. (get_config("arch") or "x64"))

if has_config("tests") then
-- Target: logic_test (Unit Tests - Legacy)
target("logic_test")
    set_kind("binary")
    set_plat("windows")
    add_packages("gtest", "spdlog", "nlohmann_json")
    
    -- Console application
    add_ldflags("/SUBSYSTEM:CONSOLE", {force = true, tools = "msvc"})
    add_ldflags("-Wl,/SUBSYSTEM:CONSOLE", {force = true, tools = {"clang", "zig"}})
    
    -- Source files (only logic components)
    add_files("tests/logic_test.cpp")
    add_files("fancontrol/ECManager.cpp")
    add_files("fancontrol/SensorManager.cpp")
    add_files("fancontrol/FanController.cpp")
    add_files("fancontrol/ConfigManager.cpp")
    add_files("fancontrol/Core/*.cpp")  -- Core library
    
    -- Include directories
    add_includedirs("fancontrol")
    add_includedirs("fancontrol/Core")
    
    -- Output directory
    set_targetdir("artifacts/bin/" .. (get_config("arch") or "x64"))

-- Target: core_test (Unit Tests - Core Library)
target("core_test")
    set_kind("binary")
    set_plat("windows")
    add_packages("gtest", "spdlog", "nlohmann_json")
    
    -- Console application
    add_ldflags("/SUBSYSTEM:CONSOLE", {force = true, tools = "msvc"})
    add_ldflags("-Wl,/SUBSYSTEM:CONSOLE", {force = true, tools = {"clang", "zig"}})
    
    -- Source files
    add_files("tests/core_test.cpp")
    add_files("fancontrol/ECManager.cpp")
    add_files("fancontrol/SensorManager.cpp")
    add_files("fancontrol/FanController.cpp")
    add_files("fancontrol/ConfigManager.cpp")
    add_files("fancontrol/Core/*.cpp")
    
    -- Include directories
    add_includedirs("fancontrol")
    add_includedirs("fancontrol/Core")
    
    -- Output directory
    set_targetdir("artifacts/bin/" .. (get_config("arch") or "x64"))
end
