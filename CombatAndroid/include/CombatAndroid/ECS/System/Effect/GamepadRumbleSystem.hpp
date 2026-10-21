//-------------------------------------------------------------
//! @file   GamepadRumbleSystem.hpp
//! @brief  戦闘の手ごたえに合わせてゲームパッドを振動させるシステムの宣言
//-------------------------------------------------------------
#pragma once
#include <CombatAndroid/ECS/Event/Combat/WeaponHitEvent.hpp>
#include <CombatAndroid/ECS/Event/Player/PlayerCounterKickEvent.hpp>
#include <CombatAndroid/ECS/Event/Player/PlayerDamagedEvent.hpp>
#include <CombatAndroid/ECS/Event/Player/PlayerFinisherEvent.hpp>
#include <CombatAndroid/ECS/Event/Player/PlayerHeavyStrikeEvent.hpp>
#include <CombatAndroid/ECS/Event/Player/PlayerJustDodgeEvent.hpp>

#include <Tsukino/Core/ECS/System/ISystem.hpp>
#include <Tsukino/Core/ECS/Event/EventBus.hpp>
#include <Tsukino/Core/ECS/Event/ScopedConnection.hpp>

#include <vector>
// 名前空間 : CombatAndroid::ECS
namespace CombatAndroid::ECS {
    //-------------------------------------------------------------
    //! @class  GamepadRumbleSystem
    //! @brief  プレイヤーの攻撃のヒット・連撃の最終段・大技・反撃キック・被弾・ジャスト回避の通知を購読し、
    //!         ゲームパッドを振動させる。強さと長さは Assets/Tables/Systems/GamepadRumble.json。
    //! @note   最終段・大技は「入った瞬間」に通知が来るので、インパクトまで待ってから震わせる。
    //!         振動中に弱い振動が来ても上書きしない（敵を何体も斬ったときの細かい振動で、被弾の重い振動が消えないように）。
    //!         オプションで振動をOFFにしたとき・ポーズやメニューで止まっている間は止める
    //-------------------------------------------------------------
    class GamepadRumbleSystem : public Tsukino::ECS::ISystem {
    public:
        //-------------------------------------------------------------
        //! @brief 更新処理
        //-------------------------------------------------------------
        void Update(Tsukino::ECS::Registry& registry, float deltaTime) override;

        //-------------------------------------------------------------
        //! @brief 各通知の購読を開始する
        //! @param eventBus [in] シーンが所有するイベントバス
        //! @note  解除は各ScopedConnectionのデストラクタに任せる（ExpOrbSystemと同じ作法）
        //-------------------------------------------------------------
        void Initialize(Tsukino::ECS::EventBus& eventBus);

    private:
        //-------------------------------------------------------------
        //! @enum   RumbleKind
        //! @brief  振動の種類（強さと長さは種類ごとにJSONで決める）
        //-------------------------------------------------------------
        enum class RumbleKind {
            Hit,           //!< プレイヤーの攻撃が敵に当たった
            HeavyStrike,   //!< 連撃の最終段のインパクト
            Finisher,      //!< 大技のインパクト
            CounterKick,   //!< ジャスト回避からの反撃キック
            Damaged,       //!< 被弾
            JustDodge,     //!< ジャスト回避
        };

        //-------------------------------------------------------------
        //! @struct PendingRumble
        //! @brief  次のUpdateで鳴らす（またはインパクトまで待っている）振動1つぶん
        //-------------------------------------------------------------
        struct PendingRumble {
            RumbleKind   kind;              //!< 種類
            float        delay = 0.0f;      //!< 震わせるまでの残り秒（実時間）
            entt::entity source = entt::null;    //!< 通知の出どころ（Hitのとき、攻撃したのがプレイヤーかを確かめる）
        };

        //-------------------------------------------------------------
        //! @brief 振動の要求を積む
        //! @param kind   [in] 種類
        //! @param delay  [in] 震わせるまでの秒
        //! @param source [in] 通知の出どころ
        //-------------------------------------------------------------
        void Enqueue(RumbleKind kind, float delay, entt::entity source);

        std::vector<PendingRumble> m_pending;    //!< 鳴らす前の振動

        float m_activeStrength  = 0.0f;    //!< いま震えている振動の強さ（弱い振動で上書きしないために覚えておく）
        float m_activeRemaining = 0.0f;    //!< いま震えている振動の残り秒

        Tsukino::ECS::ScopedConnection m_hitConnection;            //!< WeaponHitEventの購読
        Tsukino::ECS::ScopedConnection m_heavyStrikeConnection;    //!< PlayerHeavyStrikeEventの購読
        Tsukino::ECS::ScopedConnection m_finisherConnection;       //!< PlayerFinisherEventの購読
        Tsukino::ECS::ScopedConnection m_counterKickConnection;    //!< PlayerCounterKickEventの購読
        Tsukino::ECS::ScopedConnection m_damagedConnection;        //!< PlayerDamagedEventの購読
        Tsukino::ECS::ScopedConnection m_justDodgeConnection;      //!< PlayerJustDodgeEventの購読
    };
}    // namespace CombatAndroid::ECS
