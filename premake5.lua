-- TsukinoEngine のヘルパーを先に読み込む（workspace 宣言より前）。
-- include/link/配布物の定義はエンジン側の1箇所にあり、ここへは書き写さない
include "External/TsukinoEngine/Tools/premake/tsukino.lua"

workspace "CombatAndroid"
    startproject "CombatAndroid"
    location ".build"

    -- アーキテクチャ・構成・警告まわりはエンジンと共通の設定を使う
    tsukino_workspace_defaults()

include "External/TsukinoEngine"

project "CombatAndroid"
    location ".build/CombatAndroid"
    kind "WindowedApp"
    language "C++"
    cppdialect "C++20"
    forceincludes { "pch.h" }

    -- /bigobj: GamePrefab.cpp は全コンポーネントの Prefab 読み込みを1つの翻訳単位でインスタンス化するため、
    -- 既定の COFF のセクション数の上限（約65,000）を超える（C1128）
    filter "action:vs*"
        buildoptions { "/permissive-", "/bigobj" }
    filter {}

    pchheader "pch.h"
    pchsource "CombatAndroid/pch.cpp"

    targetdir ("bin/%{cfg.buildcfg}")
    objdir ("bin-int/%{cfg.buildcfg}")

    files {
        "CombatAndroid/src/**.cpp",
        "CombatAndroid/include/**.hpp",
        "CombatAndroid/pch.cpp",
    }

    includedirs { "CombatAndroid/include" }

    -- エンジンの include・lib・NuGet
    tsukino_link()

    -- 実行時の基準ディレクトリと、エンジンが持ち込む Release 配布物
    -- （組み込み Assets / Tools / ライセンス条文）
    tsukino_release_payload()

    -- exe・ウィンドウのアイコン。Assets の外に置く（Release では Assets が丸ごと配布物へコピーされるため）。
    -- 作り直しは CombatAndroid/Resources/Icon/MakeIcon.ps1
    tsukino_icon("CombatAndroid/Resources/Icon/App.ico")

    -- ゲーム自身の Assets は自分でコピーする。実行時に読まないものは配布物に入れない:
    --   *.efkproj … Effekseer のエディタ用プロジェクト（約41MB）。実行時は .efk / .efkefc だけ読む
    --   *.py / *.js / *.md … アセット作成用のスクリプトと説明書き
    filter "configurations:Release"
        postbuildcommands(tsukino_mirror_commands("CombatAndroid/Assets", "CombatAndroid/Assets", {
            excludeFiles = { "*.efkproj", "*.py", "*.js", "*.md" },
        }))
    filter {}
