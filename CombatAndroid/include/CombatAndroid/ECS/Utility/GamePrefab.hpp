//-------------------------------------------------------------
//! @file   GamePrefab.hpp
//! @brief  ゲーム側ComponentのPrefab登録と、Prefabからの共通エンティティ生成の宣言
//-------------------------------------------------------------
#pragma once
#include <Tsukino/Core/ECS/Registry/Registry.hpp>

#include <string>
#include <string_view>

namespace Tsukino::Engine::ECS::Prefab {
    class PrefabFactory;
}

namespace Tsukino::EngineIntegration {
    struct EngineContext;
}

// 名前空間 : CombatAndroid::ECS
namespace CombatAndroid::ECS {
    //-------------------------------------------------------------
    //! @brief  シリアライズ対応済みのゲーム側ComponentをPrefabFactoryに登録する
    //! @param  factory [in,out] 登録先（EngineContext::prefabFactory）
    //! @note   エンジン初期化の直後、最初のシーンを開始する前に1回だけ呼ぶ。
    //!         登録名はComponentの型名と同じにする（Prefab JSONのキーになる）。
    //!
    //!         登録していないのは実行時状態のみのComponent（BehaviorTree・ScreenFade・各種プール・
    //!         メニュー/HUDのEntityハンドルの束など）。保存しても意味が無いので対象外
    //-------------------------------------------------------------
    void RegisterGameComponents(Tsukino::Engine::ECS::Prefab::PrefabFactory& factory);

    //-------------------------------------------------------------
    //! @brief  Prefab名からPrefabのフォルダのパスを得る
    //! @param  name [in] Assets/Prefabs/ からの相対名（例: "UI/Rect"）
    //! @return "CombatAndroid/Assets/Prefabs/<name>"
    //-------------------------------------------------------------
    [[nodiscard]]
    std::string GetPrefabDirectory(std::string_view name);

    //-------------------------------------------------------------
    //! @brief  Prefab名からエンティティを1つ生成する
    //! @param  registry [in,out] ECSレジストリ
    //! @param  context  [in]     エンジンコンテキスト
    //! @param  name     [in]     Assets/Prefabs/ からの相対名（例: "UI/Rect"。<name>/Prefab.json を読む）
    //! @return 生成したエンティティ（Prefabが無ければentt::null。ログにエラーが出る）
    //! @note   ゲームのエンティティは全てこれ（かPrefabFactory::Instantiate）で作る。
    //!         Prefabが持つのは型ごとの値で、位置・所有者・描画順などの個体ごとの値は呼び出し側が生成後に上書きする
    //-------------------------------------------------------------
    Tsukino::ECS::Entity InstantiatePrefab(Tsukino::ECS::Registry& registry, Tsukino::EngineIntegration::EngineContext& context,
                                           std::string_view name);

    //-------------------------------------------------------------
    //! @brief  環境（地面・光・空・フォグ・環境パーティクル・草）をPrefabから生成する
    //! @param  registry  [in,out] ECSレジストリ
    //! @param  context   [in]     エンジンコンテキスト
    //! @param  sceneName [in]     "Combat" または "Title"（Assets/Prefabs/Environment/<sceneName>/ を読む）
    //! @note   空だけは両シーン共通（Assets/Prefabs/Environment/Sky）
    //-------------------------------------------------------------
    void InstantiateEnvironment(Tsukino::ECS::Registry& registry, Tsukino::EngineIntegration::EngineContext& context, const char* sceneName);

    //-------------------------------------------------------------
    //! @brief  画面固定UI用の2DカメラをPrefab（Assets/Prefabs/Environment/UiCamera2D）から生成する
    //! @param  registry [in,out] ECSレジストリ
    //! @param  context  [in]     エンジンコンテキスト
    //-------------------------------------------------------------
    void InstantiateUiCamera2D(Tsukino::ECS::Registry& registry, Tsukino::EngineIntegration::EngineContext& context);
}    // namespace CombatAndroid::ECS
