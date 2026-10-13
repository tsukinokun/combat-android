//-------------------------------------------------------------
//! @file   SkillAuraParticleSystem.hpp
//! @brief  取得スキルの色の光の粒をプレイヤーの足元から立ちのぼらせるシステムの宣言
//-------------------------------------------------------------
#pragma once
#include <Tsukino/Core/ECS/System/ISystem.hpp>

#include <random>
// 名前空間 : CombatAndroid::ECS
namespace CombatAndroid::ECS {
    //-------------------------------------------------------------
    //! @class  SkillAuraParticleSystem
    //! @brief  取得済みスキルのうち最もレベルの高い罪（同率なら最後に取った方）の色で、
    //!         プレイヤーの足元の輪から細長い光の粒を湧かせ、揺らしながら昇らせて消す。
    //!         レベルが上がるほど粒が増え、大きくなる。
    //!
    //!         粒はシーン生成時に作ったkAuraParticlePoolSize個のエンティティ
    //!         （AuraParticleComponent + 加算合成のワールド空間SpriteComponent）を使い回す。
    //!         深度テストが効くので、体や草の後ろに回った粒は隠れる。
    //!
    //! @note   実行順はプレイヤーの位置が確定するMovementより後、書いたpositionが
    //!         worldMatrixへ焼き込まれるTransformLateより前（SystemPriority::SkillAuraParticle）。
    //!         色の決め方はSkillAuraSystem（体の発光）と共通（Utility/Common/SkillAura.hpp）
    //-------------------------------------------------------------
    class SkillAuraParticleSystem : public Tsukino::ECS::ISystem {
    public:
        //-------------------------------------------------------------
        //! @brief 更新処理
        //-------------------------------------------------------------
        void Update(Tsukino::ECS::Registry& registry, float deltaTime) override;

    private:
        float        m_spawnAccumulator = 0.0f;                       //!< 湧かせ損ねた端数（1秒あたりの湧き数×経過時間の積み残し）
        std::mt19937 m_rng{std::random_device{}()};                   //!< 湧く位置・速さ・寿命のばらつきに使う乱数生成器
    };
}    // namespace CombatAndroid::ECS
