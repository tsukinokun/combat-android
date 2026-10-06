//-------------------------------------------------------------
//! @file   CutsceneSystem.hpp
//! @brief  CutsceneSystemクラスの宣言
//-------------------------------------------------------------
#pragma once
#include <CombatAndroid/ECS/Component/Menu/CutsceneComponent.hpp>

#include <Tsukino/Core/ECS/Entity/Entity.hpp>
#include <Tsukino/Core/ECS/System/ISystem.hpp>

#include <vector>
// 前方宣言
namespace Tsukino::ECS {
    class Registry;
}
namespace Tsukino::EngineIntegration {
    struct EngineContext;
}
// 名前空間 : CombatAndroid::ECS
namespace CombatAndroid::ECS {
    //-------------------------------------------------------------
    //! @brief  カットシーンの再生状態と、レターボックス／スキップ案内UIを
    //!         非表示で作る関数
    //! @param  registry     [in] ECSレジストリ
    //! @param  context      [in] エンジンコンテキスト（UIエンティティのテクスチャロードに使う）
    //! @param  playerEntity [in] CutsceneComponentを付けるプレイヤーエンティティ
    //! @note   シーン初期化で1回だけ呼ぶ（PauseMenuComponent等と同じ作法）
    //-------------------------------------------------------------
    void CreateCutscenePlayback(Tsukino::ECS::Registry& registry, Tsukino::EngineIntegration::EngineContext& context,
                                Tsukino::ECS::Entity playerEntity);

    //-------------------------------------------------------------
    //! @brief  カットシーンを1本再生する関数
    //! @param  registry [in] ECSレジストリ
    //! @param  shots    [in] 再生するショット列（空なら何もしない）
    //! @note   既に何か再生中、またはスキル選択・ポーズ・リザルトなど他の停止要因が
    //!         既に有効な場合は何もせず警告ログを出す（同時に2つ動く事態を避ける）
    //-------------------------------------------------------------
    void PlayCutscene(Tsukino::ECS::Registry& registry, std::vector<CutsceneShot> shots);

    //-------------------------------------------------------------
    //! @brief  今カットシーンを再生中かを問い合わせる関数
    //! @param  registry [in] ECSレジストリ
    //! @return true: 再生中
    //-------------------------------------------------------------
    [[nodiscard]]
    bool IsCutsceneActive(Tsukino::ECS::Registry& registry);

    //-------------------------------------------------------------
    //! @class  CutsceneSystem
    //! @brief  演出カメラでカットシーンを再生するシステム。
    //!         再生中はTPSカメラのTransform/CameraComponent::fovを直接書き換え、
    //!         TpsCameraSystem（Camera3D）は何もしない
    //! @note   対象（プレイヤー・湧いたエリート等）の今フレームの位置を使うため
    //!         Movement/TransformLateの後、TpsCameraSystemより前の優先度で登録する
    //-------------------------------------------------------------
    class CutsceneSystem : public Tsukino::ECS::ISystem {
    public:
        //-------------------------------------------------------------
        //! @brief 更新処理
        //! @param registry  [in] エンジンのECSレジストリのラッパー
        //! @param deltaTime [in] デルタタイム（演出はWorldTimeContext::realDeltaTimeで進めるため未使用）
        //-------------------------------------------------------------
        void Update(Tsukino::ECS::Registry& registry, float deltaTime) override;
    };
}    // namespace CombatAndroid::ECS
