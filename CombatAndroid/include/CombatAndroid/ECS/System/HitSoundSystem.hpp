//-------------------------------------------------------------
//! @file   HitSoundSystem.hpp
//! @brief  HitSoundSystemクラスの宣言
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
    //! @class  HitSoundSystem
    //! @brief  WeaponHitEventを購読し、攻撃が命中するたびにプロシージャル生成した
    //!         ヒット音を鳴らすシステム。武器の質感（鈍器/刃物）によって
    //!         HitImpactBlunt.wav / HitImpactSharp.wav を鳴らし分ける。
    //! @note   DamageNumberSystemと同じ骨格。音を鳴らすだけでECSの状態は
    //!         変更しないため、AudioComponent/組み込みAudioSystemは経由せず、
    //!         AudioManagerを直接呼ぶ
    //-------------------------------------------------------------
    class HitSoundSystem : public Tsukino::ECS::ISystem {
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
        //! @note  WeaponHitEventはCombatSystemのview.eachの内側からPublishされる。
        //!        ここではAssetManager/AudioManager、そしてevent.weaponのWeaponComponentを
        //!        読む必要があるが、ハンドラにはRegistryが渡ってこないため、
        //!        イベントをそのままキューへ積むだけにして実際の再生はUpdateへ一本化する
        //-------------------------------------------------------------
        void OnWeaponHit(const WeaponHitEvent& event);

        std::vector<WeaponHitEvent>    m_pendingHits;      //!< 次のUpdateで鳴らすヒット
        Tsukino::ECS::ScopedConnection m_hitConnection;    //!< WeaponHitEventの購読

        //! 鈍器（ウォーハンマー）向けのヒット音。初回Updateで遅延ロード
        Tsukino::Asset::AssetHandle m_bluntSoundHandle;
        //! 刃物（グレートソード/バトルアックス）向けのヒット音。初回Updateで遅延ロード
        Tsukino::Asset::AssetHandle m_sharpSoundHandle;
    };
}    // namespace CombatAndroid::ECS
