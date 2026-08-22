//-------------------------------------------------------------
//! @file   DamageNumberSystem.hpp
//! @brief  DamageNumberSystemクラスの宣言
//-------------------------------------------------------------
#pragma once
#include <CombatAndroid/ECS/Event/WeaponHitEvent.hpp>

#include <Tsukino/Core/ECS/System/ISystem.hpp>
#include <Tsukino/Core/ECS/Entity/Entity.hpp>
#include <Tsukino/Core/ECS/Event/EventBus.hpp>
#include <Tsukino/Core/ECS/Event/ScopedConnection.hpp>

#include <hlsl++.h>
#include <vector>
// 名前空間 : CombatAndroid::ECS
namespace CombatAndroid::ECS {
    //-------------------------------------------------------------
    //! @class  DamageNumberSystem
    //! @brief  WeaponHitEventを購読し、ヒット位置のワールド座標にダメージ数値を
    //!         ポップさせるシステム。数値はヒットした地点に貼り付いたまま画面上方向へ
    //!         上昇し、フェードアウトして消える。
    //!         表示にはシーン生成時に用意したkDamageNumberPoolSize個のエンティティを
    //!         使い回し、実行時のエンティティ生成・破棄は一切行わない
    //-------------------------------------------------------------
    class DamageNumberSystem : public Tsukino::ECS::ISystem {
    public:
        //-------------------------------------------------------------
        //! @brief 更新処理
        //-------------------------------------------------------------
        void Update(Tsukino::ECS::Registry& registry, float deltaTime) override;

        //-------------------------------------------------------------
        //! @brief WeaponHitEventの購読を開始する
        //! @param eventBus [in] シーンが所有するイベントバス
        //! @note  EventBusはSystemManagerより先に宣言されており購読者より長生きするため、
        //!        解除はm_hitConnectionのデストラクタに任せてよい（OnExitで行うことは無い）
        //-------------------------------------------------------------
        void Initialize(Tsukino::ECS::EventBus& eventBus);

    private:
        //-------------------------------------------------------------
        //! @struct PendingDamageNumber
        //! @brief  まだスロットへ割り当てていないヒット1件分の情報
        //-------------------------------------------------------------
        struct PendingDamageNumber {
            Tsukino::ECS::Entity target;         //!< ヒットを受けた敵（表示高さの算出に使う）
            hlslpp::float3       hitPosition;    //!< ヒット位置（ワールド空間、敵の足元原点）
            float                damage;         //!< 与えたダメージ量
        };

        //-------------------------------------------------------------
        //! @brief ヒット通知のハンドラ
        //! @note  WeaponHitEventはCombatSystemのview.eachの内側からPublishされるため、
        //!        ここでコンポーネントを追加・削除するとEnTTのイテレータが壊れる。
        //!        キューへ積むだけにして、ECSの変更はUpdateへ一本化する
        //-------------------------------------------------------------
        void OnWeaponHit(const WeaponHitEvent& event);

        std::vector<PendingDamageNumber> m_pending;          //!< 次のUpdateで処理するヒット
        Tsukino::ECS::ScopedConnection   m_hitConnection;    //!< WeaponHitEventの購読
    };
}    // namespace CombatAndroid::ECS
