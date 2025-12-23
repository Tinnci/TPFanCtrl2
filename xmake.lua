-- Project Information
set_project("TPFanCtrl2")

-- Dynamic versioning from Git tags (Xmake 3.0 compatible)
set_version("2.2.0", {build = function ()
    local os = import("core.base.os")
    local ok, v = pcall(function() 
        return os.iorun("git describe --tags --always"):trim():gsub("^v", "")
    end)
    return ok and v or nil
end})

-- Add dependencies
add_requires("gtest")
add_requires("imgui master", {configs = {win32 = true, vulkan = true, freetype = true}})
add_requires("vulkan-loader")
add_requires("freetype")
add_requires("vulkan-memory-allocator")

-- Set x86 architecture as default (due to TVicPort driver limitations)
set_arch("x86")

-- Define build modes
add_rules("mode.debug", "mode.release")

-- Global settings
set_languages("c++17")
add_defines("WIN32", "_MBCS")
add_cxflags("/J", "/utf-8", {tools = "msvc"})

if is_plat("windows") then
    add_cxflags("/W4", {tools = "msvc"}) -- Enable strict warning level 4
end

-- Target: TPFanCtrl2 (Main GUI App)
target("TPFanCtrl2")
    set_kind("binary")
    set_plat("windows")

    add_packages("imgui", "vulkan-loader", "freetype", "vulkan-memory-allocator")
    
    -- Set subsystem to Console for debugging (change to WINDOWS for final release)
    add_ldflags("/SUBSYSTEM:CONSOLE", {force = true})
    
    -- Precompiled Header
    set_pcxxheader("fancontrol/_prec.h")
    
    -- Source files
    add_files("fancontrol/*.cpp")
    add_files("fancontrol/res/fancontrol.rc")
    add_files("fancontrol/res/app.manifest")
    
    -- Include directories
    add_includedirs("fancontrol")
    
    -- Link libraries
    add_linkdirs("fancontrol")
    add_links("TVicPort")
    add_links("comctl32", "user32", "gdi32", "advapi32", "shell32", "ole32", "oleaut32", "uuid", "dwmapi")
    
    -- Output directory
    set_targetdir("bin")

    -- Optimizations for Release mode
    if is_mode("release") then
        set_optimize("fastest")
        set_symbols("hidden")
        set_strip("all")
        add_cxflags("/GL", {tools = "msvc"})
        add_ldflags("/LTCG", {tools = "msvc"})
        add_vectorexts("sse2")
    end

-- Target: logic_test (Unit Tests)
target("logic_test")
    set_kind("binary")
    set_plat("windows")
    add_packages("gtest")
    
    -- Console application
    add_ldflags("/SUBSYSTEM:CONSOLE", {force = true})
    
    -- Source files (only logic components)
    add_files("tests/logic_test.cpp")
    add_files("fancontrol/ECManager.cpp")
    add_files("fancontrol/SensorManager.cpp")
    add_files("fancontrol/FanController.cpp")
    add_files("fancontrol/ConfigManager.cpp")
    
    -- Include directories
    add_includedirs("fancontrol")
    
    -- Output directory
    set_targetdir("bin")
