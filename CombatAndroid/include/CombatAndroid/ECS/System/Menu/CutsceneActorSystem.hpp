//-------------------------------------------------------------
//! @file   CutsceneActorSystem.hpp
//! @brief  ムービーの役者（戦わない見た目だけのキャラクター）を台本どおりに動かすシステムの宣言
//-------------------------------------------------------------
#pragma once
#include <CombatAndroid/ECS/Component/Menu/CutsceneActorComponent.hpp>

#include <Tsukino/Core/ECS/Entity/Entity.hpp>
#include <Tsukino/Core/ECS/System/ISystem.hpp>

#include <hlsl++.h>

// 前方宣言
namespace Tsukino::EngineIntegration {
    struct EngineContext;
}

// 名前空間 : CombatAndroid::ECS
namespace CombatAndroid::ECS {
    //-------------------------------------------------------------
    //! @brief  ムービーの役者を1体出す関数
    //! @param  registry   [in,out] ECSレジストリ
    //! @param  context    [in]     エンジンコンテキスト（Prefabの生成に使う）
    //! @param  prefabName [in]     見た目だけのPrefab（Assets/Prefabs/Cutscene/<名前>/）
    //! @param  position   [in]     立たせる位置
    //! @param  yaw        [in]     向き（ラジアン。0で+Z）
    //! @param  scale      [in]     Prefabの大きさに掛ける倍率（ボスの sizeScale と揃える）
    //! @param  actor      [in]     動きの台本
    //! @return 出した役者
    //! @note   ライブのカットシーン（PlayCutsceneのliveWorld）の中で使う。止めるカットシーンでは動かない
    //-------------------------------------------------------------
    Tsukino::ECS::Entity SpawnCutsceneActor(Tsukino::ECS::Registry& registry, Tsukino::EngineIntegration::EngineContext& context, const char* prefabName,
                                            const hlslpp::float3& position, float yaw, float scale, CutsceneActorComponent actor);

    //-------------------------------------------------------------
    //! @class  CutsceneActorSystem
    //! @brief  ムービーの役者を台本どおりに動かす（クリップの切り替え・前進・霧に消える）。
    //!         ムービーが指定のショットに入るか、ムービー自体が終わったら（スキップを含む）片付ける
    //! @note   役者はEnemyComponentを持たないので、敵のAI（IsGameplayFrozenで止まる）とは関係なく動く。
    //!         動かす時間はシーンのdeltaTimeなので、ライブのカットシーンの間だけ進む（SystemPriority::CutsceneActor）
    //-------------------------------------------------------------
    class CutsceneActorSystem : public Tsukino::ECS::ISystem {
    public:
        //-------------------------------------------------------------
        //! @brief システムの更新
        //-------------------------------------------------------------
        void Update(Tsukino::ECS::Registry& registry, float deltaTime) override;
    };
}    // namespace CombatAndroid::ECS
