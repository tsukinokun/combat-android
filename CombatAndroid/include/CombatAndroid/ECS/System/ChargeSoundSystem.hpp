//-------------------------------------------------------------
//! @file   ChargeSoundSystem.hpp
//! @brief  ChargeSoundSystemクラスの宣言
//-------------------------------------------------------------
#pragma once
#include <CombatAndroid/ECS/Event/PlayerChargeEvent.hpp>

#include <Tsukino/Core/ECS/System/ISystem.hpp>
#include <Tsukino/Core/ECS/Event/EventBus.hpp>
#include <Tsukino/Core/ECS/Event/ScopedConnection.hpp>
#include <Tsukino/Engine/Asset/AssetHandle.hpp>
// 名前空間 : CombatAndroid::ECS
namespace CombatAndroid::ECS {
    //-------------------------------------------------------------
    //! @class  ChargeSoundSystem
    //! @brief  溜め攻撃の効果音を鳴らすシステム。溜めを解放したあと、
    //!         斬撃弾が飛び出す瞬間に「ドン」（ChargeFire.wav）を鳴らす
    //! @note   音を鳴らすだけでECSの状態は変更しないため、AudioComponent/組み込み
    //!         AudioSystemは経由せずAudioManagerを直接呼ぶ（HitSoundSystemと同じ）。
    //!         溜めている間の音は、合成音では耳障りになりやすかったため鳴らしていない
    //-------------------------------------------------------------
    class ChargeSoundSystem : public Tsukino::ECS::ISystem {
    public:
        //-------------------------------------------------------------
        //! @brief 更新処理
        //! @param registry  [in] エンジンのECSレジストリのラッパー
        //! @param deltaTime [in] デルタタイム
        //-------------------------------------------------------------
        void Update(Tsukino::ECS::Registry& registry, float deltaTime) override;

        //-------------------------------------------------------------
        //! @brief PlayerChargeReleasedEventの購読を開始する
        //! @param eventBus [in] シーンが所有するイベントバス
        //! @note  EventBusはSystemManagerより先に宣言されており購読者より長生きするため、
        //!        解除はm_releasedConnectionのデストラクタに任せてよい
        //-------------------------------------------------------------
        void Initialize(Tsukino::ECS::EventBus& eventBus);

    private:
        //-------------------------------------------------------------
        //! @brief 解放通知のハンドラ
        //! @note  PlayerChargeReleasedEventはPlayerAnimationSystemのview.eachの内側から
        //!        Publishされる。ここではAssetManager/AudioManagerが要るがRegistryが
        //!        渡ってこないため、残り時間を控えるだけにして再生はUpdateへ一本化する
        //-------------------------------------------------------------
        void OnChargeReleased(const PlayerChargeReleasedEvent& event);

        Tsukino::ECS::ScopedConnection m_releasedConnection;    //!< PlayerChargeReleasedEventの購読

        //! 解放してから斬撃弾が飛び出すまでの残り秒数。0より大きい間だけ発射待ち
        float m_fireTimer = 0.0f;

        //! 発射の「ドン」。初回に遅延ロード
        Tsukino::Asset::AssetHandle m_fireSoundHandle;
    };
}    // namespace CombatAndroid::ECS
