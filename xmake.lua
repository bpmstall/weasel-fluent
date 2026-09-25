set_project("weasel-fluent-cpp-qt")
set_version("1.0.0")

set_languages("cxx17")
add_rules("mode.debug", "mode.release")

-- Target: FluentQt (Static Library)
target("FluentQt")
    set_kind("static")
    add_rules("qt.static")
    add_frameworks("QtWidgets", "QtGui", "QtCore")

    add_defines("UNICODE", "_UNICODE", "_SILENCE_STDEXT_ARR_ITERS_DEPRECATION_WARNING")
    if is_plat("windows") then
        add_cxflags("/utf-8", "/FS", "/MP", {tools = {"cl", "clang_cl"}})
    end

    add_includedirs("3rdparty/Fluent-Qt/include", {public = true})
    add_includedirs("3rdparty/Fluent-Qt/src", {public = true})

    add_files("3rdparty/Fluent-Qt/src/**.cpp")
    add_files("3rdparty/Fluent-Qt/resources.qrc")

    on_load(function (target)
        if not target:data("qt") then
            import("detect.sdks.find_qt")
            import("core.project.config")
            local sdkdir = config.get("qt")
            if not sdkdir and os.isdir("C:/Qt/6.8.2/msvc2022_64") then
                sdkdir = "C:/Qt/6.8.2/msvc2022_64"
            end
            local qt = find_qt(sdkdir)
            if qt then
                target:data_set("qt", qt)
            end
        end

        for _, file in ipairs(os.files("3rdparty/Fluent-Qt/src/**.h")) do
            local content = io.readfile(file)
            if content and (content:find("Q_OBJECT") or content:find("Q_GADGET") or content:find("Q_NAMESPACE")) then
                target:add("files", file)
            else
                target:add("headerfiles", file)
            end
        end
    end)

-- Target: weasel-fluent-cpp-qt (Executable)
target("weasel-fluent-cpp-qt")
    set_kind("binary")
    add_rules("qt.widgetapp")
    add_frameworks("QtWidgets", "QtGui", "QtCore", "QtSvg")

    add_deps("FluentQt")

    add_defines("UNICODE", "_UNICODE")
    if is_plat("windows") then
        add_cxflags("/utf-8", "/FS", "/MP", {tools = {"cl", "clang_cl"}})
        add_ldflags("/SUBSYSTEM:CONSOLE", {force = true})
        add_syslinks("User32", "Gdi32", "Dwmapi", "Winmm", "Psapi", "Advapi32")
    end

    add_includedirs("src")

    add_files("src/*.cpp")
    add_files("src/*.h")
    add_files("assets/emojis.qrc")

    on_load(function (target)
        if not target:data("qt") then
            import("detect.sdks.find_qt")
            import("core.project.config")
            local sdkdir = config.get("qt")
            if not sdkdir and os.isdir("C:/Qt/6.8.2/msvc2022_64") then
                sdkdir = "C:/Qt/6.8.2/msvc2022_64"
            end
            local qt = find_qt(sdkdir)
            if qt then
                target:data_set("qt", qt)
            end
        end
    end)

    after_build(function (target)
        local targetfile = target:targetfile()
        if os.isfile(targetfile) and os.isdir("build/Release") then
            os.cp(targetfile, "build/Release/weasel-fluent-cpp-qt.exe")
        end
        if os.isfile(targetfile) and os.isdir("dist/weasel-fluent") then
            local dst = "dist/weasel-fluent/weasel-fluent-cpp-qt.exe"
            local old = "dist/weasel-fluent/weasel-fluent-cpp-qt.exe.old"
            os.tryrm(old)
            os.trymv(dst, old)
            os.trycp(targetfile, dst)
        end
    end)

-- Target: weasel-fluent-tsf (Windows TSF COM TIP DLL)
target("weasel-fluent-tsf")
    set_kind("shared")
    set_languages("cxx17")
    add_defines("UNICODE", "_UNICODE", "WEASEL_TSF_EXPORTS")
    if is_plat("windows") then
        add_cxflags("/utf-8", "/FS", "/MP", {tools = {"cl", "clang_cl"}})
        add_syslinks("User32", "Gdi32", "Advapi32", "Ole32", "OleAut32", "Dwmapi")
    end
    add_files("tsf/*.cpp")
    add_files("tsf/weasel_tsf.def")

    after_build(function (target)
        local targetfile = target:targetfile()
        if os.isfile(targetfile) and os.isdir("build/Release") then
            os.trycp(targetfile, "build/Release/weasel-fluent-tsf.dll")
        end
        if os.isfile(targetfile) and os.isdir("dist/weasel-fluent") then
            local dst = "dist/weasel-fluent/weasel-fluent-tsf.dll"
            local old = "dist/weasel-fluent/weasel-fluent-tsf.dll.old"
            os.tryrm(old)
            os.trymv(dst, old)
            os.trycp(targetfile, dst)
        end
    end)

