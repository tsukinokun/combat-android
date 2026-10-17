//-------------------------------------------------------------
//! @file   BossDirectorSystem.hpp
//! @brief  BossDirectorSystemクラスの宣言
//-------------------------------------------------------------
#pragma once
#include <CombatAndroid/ECS/Event/Enemy/BossProjectileEvent.hpp>
#include <CombatAndroid/ECS/Event/Enemy/BossSummonEvent.hpp>

#include <Tsukino/Core/ECS/System/ISystem.hpp>
#include <Tsukino/Core/ECS/Event/EventBus.hpp>
#include <Tsukino/Core/ECS/Event/ScopedConnection.hpp>

#include <random>
#include <vector>

// 名前空間 : CombatAndroid::ECS
namespace CombatAndroid::ECS {
    //-------------------------------------------------------------
    //! @class  BossDirectorSystem
    //! @brief  走行の山場（中ボス → ラスボス → クリア）を進めるシステム。
    //!         決まった時間にボスを湧かせ、出現の演出（カットシーン・告知・夜への切り替え）を始め、
    //!         撃破を見届けて次へ進める。クリアの判定そのものはRunResultSystemが
    //!         BossProgressComponent::phase を読んで行う
    //! @note   調整値は Assets/Tables/Systems/BossDirector.json
    //-------------------------------------------------------------
    class BossDirectorSystem : public Tsukino::ECS::ISystem {
    public:
        //-------------------------------------------------------------
        //! @brief 更新処理
        //-------------------------------------------------------------
        void Update(Tsukino::ECS::Registry& registry, float deltaTime) override;

        //-------------------------------------------------------------
        //! @brief BossSummonEvent（ボスが雑魚を呼ぶ）とBossProjectileEvent（ボスが弾を撃つ）の購読を始める
        //! @param eventBus [in] イベントバス
        //-------------------------------------------------------------
        void Initialize(Tsukino::ECS::EventBus& eventBus);

    private:
        //-------------------------------------------------------------
        //! @brief  呼ばれた雑魚を湧かせる（ビヘイビアツリーの外＝Updateで行う）
        //-------------------------------------------------------------
        void SpawnSummons(Tsukino::ECS::Registry& registry, const BossSummonEvent& summon, int dangerRank);

        //-------------------------------------------------------------
        //! @brief  ボスの弾を作る（ビヘイビアツリーの外＝Updateで行う）
        //-------------------------------------------------------------
        void SpawnProjectile(Tsukino::ECS::Registry& registry, const BossProjectileEvent& shot);

        Tsukino::ECS::ScopedConnection   m_summonConnection;        //!< BossSummonEventの購読
        Tsukino::ECS::ScopedConnection   m_projectileConnection;    //!< BossProjectileEventの購読
        std::vector<BossSummonEvent>     m_pendingSummons;          //!< 次のUpdateで湧かせる呼び出し
        std::vector<BossProjectileEvent> m_pendingProjectiles;      //!< 次のUpdateで作る弾
        std::mt19937                   m_rng{std::random_device{}()};
    };

    //-------------------------------------------------------------
    //! @brief  ボスと戦っている最中かを問い合わせる
    //! @param  registry [in] ECSレジストリ
    //! @return 中ボスかラスボスが生きていればtrue（湧き潰しが雑魚の湧きを強める・HUDが時間を呼吸させるのに使う）
    //-------------------------------------------------------------
    [[nodiscard]]
    bool IsBossBattleActive(Tsukino::ECS::Registry& registry);

    //-------------------------------------------------------------
    //! @brief  ラスボスを倒し終えたかを問い合わせる
    //! @param  registry [in] ECSレジストリ
    //! @return 撃破の演出待ちまで終わっていればtrue（RunResultSystemのクリア条件）
    //-------------------------------------------------------------
    [[nodiscard]]
    bool IsFinalBossDefeated(Tsukino::ECS::Registry& registry);
}    // namespace CombatAndroid::ECS
