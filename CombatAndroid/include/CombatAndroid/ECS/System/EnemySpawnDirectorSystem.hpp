//-------------------------------------------------------------
//! @file    EnemySpawnDirectorSystem.hpp
//! @brief   サバイバー型の敵湧き潰しシステムの宣言
//! @author  山﨑愛
//-------------------------------------------------------------
#pragma once

#include <Tsukino/Core/ECS/Registry/Registry.hpp>
#include <Tsukino/Core/ECS/System/ISystem.hpp>

#include <hlsl++.h>

#include <random>

// 前方宣言
namespace Tsukino::EngineIntegration {
    struct EngineContext;
}

// 名前空間 : CombatAndroid::ECS
namespace CombatAndroid::ECS {
    //-------------------------------------------------------------
    //! @class  EnemySpawnDirectorSystem
    //! @brief  プレイヤーを中心にフォグの外（視認できない距離）から雑魚敵を
    //!         継続的に湧かせ、遠く離れた個体は間引いて母数を一定に保つシステム
    //! @note   湧かせる種類の比重は EnemySpawnTable.cpp が持つ。本Systemは
    //!         「いつ・どこに・何体」だけを決め、「何を」はテーブルへ委ねる
    //-------------------------------------------------------------
    class EnemySpawnDirectorSystem : public Tsukino::ECS::ISystem {
    public:
        //-------------------------------------------------------------
        //! @brief システムの更新
        //! @param registry  [in] エンティティレジストリ
        //! @param deltaTime [in] デルタタイム
        //-------------------------------------------------------------
        void Update(Tsukino::ECS::Registry& registry, float deltaTime) override;

    private:
        //-------------------------------------------------------------
        //! @brief 敵を1体、抽選テーブルに従って湧かせる関数
        //! @param registry       [in] エンティティレジストリ
        //! @param context        [in] エンジンコンテキスト
        //! @param playerPosition [in] プレイヤーの現在位置
        //-------------------------------------------------------------
        void SpawnOne(Tsukino::ECS::Registry& registry,
                      Tsukino::EngineIntegration::EngineContext& context,
                      const hlslpp::float3& playerPosition);

        //-------------------------------------------------------------
        //! @brief  プレイヤーを中心に、フォグの外側の湧き位置を1つ決める関数
        //! @param  playerPosition [in] プレイヤーの現在位置
        //! @return 湧き位置
        //! @note   地面（±kGroundLimit）の外を引いた場合は角度を引き直す。
        //!         clampで内側へ押し込むと地面の端でプレイヤーの目の前に湧いてしまうため
        //-------------------------------------------------------------
        [[nodiscard]]
        hlslpp::float3 ResolveSpawnPosition(const hlslpp::float3& playerPosition);

        //-------------------------------------------------------------
        //! @brief 遠くへ離れた、本Systemが湧かせた個体を間引く関数
        //! @param registry       [in] エンティティレジストリ
        //! @param playerPosition [in] プレイヤーの現在位置
        //! @note  シーン手置きの敵や負荷試験が湧かせた敵はSpawnedEnemyComponentを
        //!        持たないため対象にならない
        //-------------------------------------------------------------
        void CullDistantEnemies(Tsukino::ECS::Registry& registry, const hlslpp::float3& playerPosition);

        //-------------------------------------------------------------
        //! @brief 生存中（死亡演出中を除く）の敵の総数を数える関数
        //! @param registry [in] エンティティレジストリ
        //! @return 生存数
        //! @note  死亡演出中の個体を数えてしまうと、倒した直後だけ湧きが止まり
        //!        「倒すほど湧きが遅くなる」逆向きの挙動になるため除外する
        //-------------------------------------------------------------
        [[nodiscard]]
        int CountLiveEnemies(Tsukino::ECS::Registry& registry);

        //---------------------------------------------------------
        // 湧き位置
        //---------------------------------------------------------
        //! 湧き半径の内側。距離フォグ・高さフォグを合わせた不透明度で、
        //! この距離ならシルエットがほぼ見えなくなる想定値
        static constexpr float kSpawnRadiusMin = 900.0f;

        //! 湧き半径の外側。SmallZombie(moveSpeed=100)基準で到達まで約13秒。
        //! これ以上遠くすると湧いた敵が戦闘に絡むまでの待ち時間が間延びする
        static constexpr float kSpawnRadiusMax = 1300.0f;

        //! 間引き半径。湧き外周より十分外に置く。近すぎると、湧いた直後に
        //! プレイヤーがほんの少し逆走しただけで即座に消えて湧き直しが延々と続く
        static constexpr float kDespawnRadius = 2400.0f;

        //! 地面（±5000の板）から落とさないための実効境界。
        //! 端に余白を取るのは、境界ちょうどに湧くとカプセルが床の縁からはみ出すため
        static constexpr float kGroundLimit = 4500.0f;

        //! 生成時の浮かせ量。EnemyStressTestSystem::kSpawnHeightと同値
        static constexpr float kSpawnHeight = 20.0f;

        //! 角度の引き直し回数。地面外へ出た場合に別の方角を試す
        static constexpr int kSpawnAttemptCount = 8;

        //! 湧かせる敵に与える索敵距離。湧き半径より十分大きくないとBTのMoveToPlayerが
        //! Failureを返し、その場で足踏みしたまま近づいてこない
        //! （EnemyComponent/EnemySpawnConfigの既定値600のままでは湧き半径に届かない）。
        //! 地面の対角（約14000）より大きくして「絶対に見失わない」ようにしている
        static constexpr float kChaseDetectRange = 20000.0f;

        //---------------------------------------------------------
        // 湧きの間隔と上限
        //---------------------------------------------------------
        //! シーン開始からこの秒数は湧かせない（初回のモデル/クリップのロードと
        //! パイプラインキャッシュ充填が終わるのを待つ）
        static constexpr float kWarmupSeconds = 2.0f;

        //! 開始直後の湧き間隔（秒）
        static constexpr float kIntervalStart = 3.0f;

        //! 詰めきったときの湧き間隔（秒）
        static constexpr float kIntervalEnd = 0.6f;

        //! kIntervalStart から kIntervalEnd まで線形に詰めきるのにかける時間（秒）
        static constexpr float kIntervalRampSeconds = 300.0f;

        //! 1回の湧きで出す数。間隔だけを詰めると1体ずつ細く来る絵になるため、
        //! まとまりで出して「群れが押し寄せる」画を作る
        static constexpr int kSpawnBatchSize = 2;

        //! 同時に存在してよい敵の数の上限（シーン手置き・負荷試験分も含めた総数で判定する）。
        //! 負荷試験では500体でも60fpsに余裕があったので、これは性能上限ではなく
        //! ゲーム性（囲まれ具合）の設定値
        static constexpr int kMaxLiveEnemies = 60;

        //! 乱数生成器。エンジン側に共通の乱数ユーティリティが無いため本Systemが自前で持つ
        std::mt19937 m_rng{std::random_device{}()};

        float m_elapsedSeconds = 0.0f;    //!< シーン開始からの経過秒数（比重の解禁と間隔の詰めに使う）
        float m_spawnTimer     = 0.0f;    //!< 次の湧きまでの残り秒数
    };
}    // namespace CombatAndroid::ECS
