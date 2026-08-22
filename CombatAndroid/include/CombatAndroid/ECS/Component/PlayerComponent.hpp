//-------------------------------------------------------------
//! @file   PlayerComponent.hpp
//! @brief  PlayerComponent構造体の宣言
//! @author 山﨑愛
//-------------------------------------------------------------
#pragma once
#include <Tsukino/Core/ECS/Entity/Entity.hpp>
#include <vector>
// 名前空間 : CombatAndroid::ECS
namespace CombatAndroid::ECS {
    //-------------------------------------------------------------
    //! @struct PlayerComponent
    //! @brief  プレイヤーエンティティであることを表すコンポーネント
    //-------------------------------------------------------------
    struct PlayerComponent {
        float moveSpeed = 300.0f;    //!< 水平移動速度（1ユニット≒1cm規約。軽いジョグ程度）
        float turnLerpSpeed = 12.0f; //!< 移動方向への向き直しの補間速度（大きいほど素早く向く）

        float sprintSpeedMultiplier = 1.6f;     //!< Shift押下時、moveSpeedに掛ける倍率
        bool  isSprinting            = false;    //!< 移動中にShiftが押されているか（PlayerSystemが毎フレーム更新。PlayerAnimationSystemが参照）

        Tsukino::ECS::Entity weaponEntity = entt::null;    //!< 装備中の武器エンティティ（WeaponComponentを持つ）。切り替え時はここを差し替える

        std::vector<Tsukino::ECS::Entity> weaponInventory;      //!< 浮遊武器の一覧（切り替え対象）。シーン初期化時に設定する
        int                                selectedWeaponIndex = 0;    //!< weaponInventory内の現在選択インデックス。weaponEntityと同期させる

        Tsukino::ECS::Entity pickupTarget = entt::null;    //!< 現在拾える対象（Fキーの対象）。PickupSystemが毎フレーム更新する

        //-------------------------------------------------------------
        // 回避（Sprinting Forward Roll）のチューニング値。
        // エンジンはルートモーションをTransformへ適用しないため（in_placeで殺すのみ）、
        // 回避中の前進は攻撃中の停止と同じ流儀で、PlayerAnimationSystemが
        // CharacterControllerComponent::moveInputを上書きして出す
        //-------------------------------------------------------------
        float dodgeSpeed              = 600.0f;    //!< 回避中の水平移動速度（moveSpeed=300の2倍相当）
        float dodgePlaybackSpeed      = 1.5f;      //!< 回避クリップの再生速度倍率（AttackStep::playbackSpeedと同趣旨）。大きいほど回避が短く終わり、進む距離もその分縮む
        float dodgeInvincibleDuration = 0.3f;      //!< 回避開始からこの秒数だけ無敵（実時間。dodgePlaybackSpeedを変えたら合わせて調整する）。回避全体より短くして「終わり際は被弾する」ようにする
        float dodgeCooldown           = 0.3f;      //!< 回避終了後、次の回避を受け付けない秒数
        float dodgeTimeoutSafety      = 2.0f;      //!< クリップ設定ミス等でis_finishedが立たなかった場合に回避ステートへ留まり続けないための保険値（attackTimeoutSafetyと同趣旨）

        //-------------------------------------------------------------
        // スペースキーの生入力。PlayerSystemが立て、PlayerAnimationSystemが消費してfalseへ戻す
        // （attackInputPressedと同じ流儀）。実際に回避が始まるかどうか（クールダウン中か、
        //   攻撃モーション中か）の判定はPlayerAnimationSystem側で行う
        //-------------------------------------------------------------
        bool dodgeInputPressed = false;

        bool isDodging    = false;    //!< 回避モーション再生中か。PlayerAnimationSystemが毎フレーム確定させ、PlayerSystemが向き直りの抑制に参照する
        bool isInvincible = false;    //!< 回避の無敵時間中か。PlayerAnimationSystemが確定させ、CombatSystem（後で走る）が接触ダメージのスキップに使う

        //-------------------------------------------------------------
        // 左クリックの生入力。PlayerSystemが立て、PlayerAnimationSystemが消費してfalseへ戻す。
        // WeaponComponent::attackRequestedとは意味が異なる点に注意：こちらは「入力があったか」、
        // attackRequestedは「（コンボ受付を含めた判定の結果）攻撃スイングが実際に始まったか」を表す。
        // 攻撃中の入力はすぐには反映されず、PlayerAnimationSystem側で1回だけ先行入力としてバッファされる
        //-------------------------------------------------------------
        bool attackInputPressed = false;
    };
}    // namespace CombatAndroid::ECS
