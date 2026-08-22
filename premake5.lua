workspace "CombatAndroid"
    architecture "x64"
    configurations { "Debug", "Release" }
    startproject "CombatAndroid"
    location ".build"
    multiprocessorcompile "On"
    exceptionhandling "On"

    filter "configurations:*"
        defines { "JPH_DEBUG_RENDERER" }
    filter {}

    filter "configurations:Debug"
        optimize "Off"
        symbols "On"
    filter {}

    filter "action:vs*"
        buildoptions { "/utf-8" }
    filter {}

    filter "configurations:Release"
        optimize "Full"
        symbols "On"
        defines { "NDEBUG" }
    filter {}

    filter "configurations:*"
        linkoptions { "/IGNORE:4006" }
    filter {}

include "External/TsukinoEngine"

project "CombatAndroid"
    location ".build/CombatAndroid"
    kind "WindowedApp"
    language "C++"
    cppdialect "C++20"
    forceincludes { "pch.h" }

    filter "action:vs*"
        buildoptions { "/permissive-" }
    filter {}

    pchheader "pch.h"
    pchsource "CombatAndroid/pch.cpp"

    targetdir ("bin/%{cfg.buildcfg}")
    objdir ("bin-int/%{cfg.buildcfg}")

    filter "configurations:Debug"
        debugdir "%{wks.location}/.."
    filter "configurations:Release"
        debugdir "%{cfg.targetdir}"
        postbuildcommands {
            "{COPYDIR} %{wks.location}/../CombatAndroid/Assets %{cfg.targetdir}/CombatAndroid/Assets",
            -- Tsukino::Core::FileSystem::GetAssetRootPath/GetEngineAssetRootPath はRelease時
            -- exeの場所を基準にするため、エンジン組み込みアセットとツールもexeの隣へコピーする
            -- (Debug時はTSUKINO_ENGINE_ROOTでエンジンのソースツリーを直接参照するため不要)
            "{COPYDIR} %{wks.location}/../External/TsukinoEngine/Tsukino.BuiltIn/Assets %{cfg.targetdir}/Tsukino.BuiltIn/Assets",
            "{COPYDIR} %{wks.location}/../External/TsukinoEngine/Tools %{cfg.targetdir}/Tools",
        }
    filter {}

    files {
        "CombatAndroid/src/**.cpp",
        "CombatAndroid/include/**.hpp",
        "CombatAndroid/pch.cpp",
    }

    includedirs {
        "CombatAndroid/include",
        "External/TsukinoEngine/Tsukino.Audio/include",
        "External/TsukinoEngine/Tsukino.GraphicsCommon/include",
        "External/TsukinoEngine/Tsukino.Engine/include",
        "External/TsukinoEngine/Tsukino.Renderer/include",
        "External/TsukinoEngine/Tsukino.BuiltIn/include",
        "External/TsukinoEngine/Tsukino.EngineIntegration/include",
        "External/TsukinoEngine/Tsukino.Core/include",
        "External/TsukinoEngine/External/cereal/include",
        "External/TsukinoEngine/External/hlslpp/include",
        "External/TsukinoEngine/External/entt/single_include",
        "External/TsukinoEngine/External/JoltPhysics",
        "External/TsukinoEngine/External/Effekseer/Dev/Cpp",
        "External/TsukinoEngine/External/Effekseer/Dev/Cpp/Effekseer",
        "External/TsukinoEngine/External/Effekseer/Dev/Cpp/EffekseerRendererDX11",
        "External/TsukinoEngine/External/Effekseer/Dev/Cpp/EffekseerRendererCommon",
        "External/TsukinoEngine/External/Effekseer/Dev/Cpp/3rdParty",
    }

    links {
        "Tsukino.Engine",
        "Tsukino.Renderer",
        "Tsukino.GraphicsCommon",
        "Tsukino.Audio",
        "Tsukino.BuiltIn",
        "Tsukino.EngineIntegration",
        "JoltPhysics",
        "Tsukino.Core",
        "EffekseerRendererDX11",
        "EffekseerRendererCommon",
        "Effekseer",
        "d3d11",
        "dxgi",
        "d3dcompiler",
        "dwrite",
    }

    nuget { "directxtk_desktop_win10:2026.4.1.1",
            "AssimpCpp:5.0.1.6",
    }
