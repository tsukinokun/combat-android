//-------------------------------------------------------------
//! @file   HitImpactEffectSystem.hpp
//! @brief  HitImpactEffectSystemクラスの宣言
//-------------------------------------------------------------
#pragma once
#include <CombatAndroid/ECS/Event/WeaponHitEvent.hpp>

#include <Tsukino/Core/ECS/System/ISystem.hpp>
#include <Tsukino/Core/ECS/Event/EventBus.hpp>
#include <Tsukino/Core/ECS/Event/ScopedConnection.hpp>
#include <Tsukino/Engine/Asset/AssetHandle.hpp>

#include <vector>
// 名前空間 : CombatAndroid::ECS
namespace CombatAndroid::ECS {
    //-------------------------------------------------------------
    //! @class  HitImpactEffectSystem
    //! @brief  WeaponHitEventを購読し、攻撃が命中するたびにヒット位置へ
    //!         Effekseerエフェクト（attackImpact.efkefc）を再生するシステム。
    //! @note   HitSoundSystemと同じ骨格。ECSの状態は変更せず、EffectSystemを
    //!         直接呼ぶだけなので、EffectComponentは経由しない
    //-------------------------------------------------------------
    class HitImpactEffectSystem : public Tsukino::ECS::ISystem {
    public:
        //-------------------------------------------------------------
        //! @brief 更新処理
        //-------------------------------------------------------------
        void Update(Tsukino::ECS::Registry& registry, float deltaTime) override;

        //-------------------------------------------------------------
        //! @brief WeaponHitEventの購読を開始する
        //! @param eventBus [in] シーンが所有するイベントバス
        //! @note  EventBusはSystemManagerより先に宣言されており購読者より長生きするため、
        //!        解除はm_hitConnectionのデストラクタに任せてよい
        //-------------------------------------------------------------
        void Initialize(Tsukino::ECS::EventBus& eventBus);

    private:
        //-------------------------------------------------------------
        //! @brief ヒット通知のハンドラ
        //! @note  WeaponHitEventはCombatSystem/ProjectileSystemのview.eachの内側から
        //!        Publishされる。ハンドラにはRegistryが渡ってこないため、ここでは
        //!        イベントをそのままキューへ積むだけにして、実際の再生はUpdateへ一本化する
        //-------------------------------------------------------------
        void OnWeaponHit(const WeaponHitEvent& event);

        std::vector<WeaponHitEvent>    m_pendingHits;      //!< 次のUpdateで再生するヒット
        Tsukino::ECS::ScopedConnection m_hitConnection;    //!< WeaponHitEventの購読

        //! ヒットエフェクト。初回Updateで遅延ロード
        Tsukino::Asset::AssetHandle m_effectHandle;
    };
}    // namespace CombatAndroid::ECS
