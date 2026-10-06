//-------------------------------------------------------------
//! @file   AttackMotionBlurSystem.cpp
//! @brief  攻撃中にモーションブラーを強めるシステムの実装
//! @author 山﨑愛
//-------------------------------------------------------------
#include <CombatAndroid/ECS/System/Effect/AttackMotionBlurSystem.hpp>
#include <CombatAndroid/ECS/Serialization/Common/SerializationHelper.hpp>
#include <CombatAndroid/ECS/Utility/Table/TableJson.hpp>
#include <CombatAndroid/ECS/Component/Player/PlayerComponent.hpp>
#include <CombatAndroid/ECS/Component/Weapon/WeaponComponent.hpp>
#include <Tsukino/BuiltIn/ECS/Component/MotionBlurComponent.hpp>

#include <entt/entt.hpp>

// 名前空間 : CombatAndroid::ECS
namespace CombatAndroid::ECS {
    namespace {
        //-------------------------------------------------------------
        //! @struct AttackMotionBlurParams
        //! @brief  見た目と挙動のチューニング値（Assets/Tables/Systems/AttackMotionBlur.json。ここの初期値はJSONにキーが無いときの既定値）
        //-------------------------------------------------------------
        struct AttackMotionBlurParams {
            //-------------------------------------------------------------
            // 演出パラメータ
            //-------------------------------------------------------------
            //! @brief 非攻撃時のブラー強度
            //! @note  0にすると走っているときも一切ブレなくなる。
            //!        通常移動でもうっすら効かせておくと、攻撃時の跳ね上がりが
            //!        「急に別の絵になる」感じにならず馴染む。
            float baseStrength = 0.6f;

            //! @brief 攻撃ピーク時のブラー強度
            float attackStrength = 3.0f;
        };

        template <class Archive>
        void load(Archive& archive, AttackMotionBlurParams& params) {
            LoadField(archive, "baseStrength", params.baseStrength);
            LoadField(archive, "attackStrength", params.attackStrength);
        }

        //-------------------------------------------------------------
        //! @brief  チューニング値を得る関数（初回の呼び出しで1度だけ読む）
        //-------------------------------------------------------------
        const AttackMotionBlurParams& GetParams() {
            static const AttackMotionBlurParams s_params = LoadSystemParams<AttackMotionBlurParams>("AttackMotionBlur");
            return s_params;
        }
    }    // namespace

    //-------------------------------------------------------------
    //! @brief システムの更新
    //-------------------------------------------------------------
    void AttackMotionBlurSystem::Update(Tsukino::ECS::Registry& registry, float deltaTime) {
        const AttackMotionBlurParams& params = GetParams();

        (void)deltaTime;

        //-------------------------------------------------------------
        // プレイヤーの攻撃ブレンド値を取得する
        //
        // attackBlend は CombatSystem が毎フレーム 0（非攻撃）↔1（攻撃中）へ
        // 連続的に補間している値なので、そのまま強度の補間係数に使える。
        // 自前でタイマーを持つ必要はない。
        //-------------------------------------------------------------
        float attackBlend = 0.0f;

        auto playerView = registry.View<PlayerComponent>();
        playerView.each([&](entt::entity entity, const PlayerComponent& player) {
            if(player.weaponEntity == entt::null)
                return;

            auto* weapon = registry.try_get<WeaponComponent>(player.weaponEntity);
            if(!weapon)
                return;

            // 複数プレイヤーがいる場合は最も攻撃中のものを採用する
            if(weapon->attackBlend > attackBlend)
                attackBlend = weapon->attackBlend;
        });

        //-------------------------------------------------------------
        // カメラに付いている MotionBlurComponent へ強度を書き込む
        //-------------------------------------------------------------
        const float strength = params.baseStrength + attackBlend * (params.attackStrength - params.baseStrength);

        auto blurView = registry.View<Tsukino::BuiltIn::ECS::MotionBlurComponent>();
        blurView.each([&](entt::entity entity, Tsukino::BuiltIn::ECS::MotionBlurComponent& blur) {
            blur.strength = strength;
        });
    }

}    // namespace CombatAndroid::ECS
