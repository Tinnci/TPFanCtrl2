-- Project Information
set_project("TPFanCtrl2")
set_version("2.2.0")

-- Add GoogleTest dependency
add_requires("gtest")

-- Set x86 architecture as default (due to TVicPort driver limitations)
set_arch("x86")

-- Define build modes
add_rules("mode.debug", "mode.release")

-- Global settings
set_languages("c++17")
add_defines("WIN32", "_MBCS")
add_cxflags("/J", "/source-charset:utf-8", {tools = "msvc"})

-- Target: TPFanCtrl2 (Main GUI App)
target("TPFanCtrl2")
    set_kind("binary")
    set_plat("windows")
    
    -- Set subsystem to Windows (GUI)
    add_ldflags("/SUBSYSTEM:WINDOWS", {force = true})
    
    -- Precompiled Header
    set_pcxxheader("fancontrol/_prec.h")
    
    -- Source files
    add_files("fancontrol/*.cpp")
    add_files("fancontrol/res/fancontrol.rc")
    
    -- Include directories
    add_includedirs("fancontrol")
    
    -- Link libraries
    add_linkdirs("fancontrol")
    add_links("TVicPort")
    add_links("comctl32", "user32", "gdi32", "advapi32", "shell32", "ole32", "oleaut32", "uuid")
    
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
