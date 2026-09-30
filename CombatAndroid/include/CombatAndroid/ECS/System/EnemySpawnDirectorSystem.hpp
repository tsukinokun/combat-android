//-------------------------------------------------------------
//! @file    EnemySpawnDirectorSystem.hpp
//! @brief   サバイバー型の敵湧き潰しシステムの宣言
//! @author  山﨑愛
//-------------------------------------------------------------
#pragma once

#include <Tsukino/Core/ECS/Registry/Registry.hpp>
#include <Tsukino/Core/ECS/System/ISystem.hpp>

#include <hlsl++.h>

#include <array>
#include <random>
#include <vector>

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
        //! 湧きの方角を数える扇形の分割数
        static constexpr int kSectorCount = 8;

        //! 扇形ごとの敵の数
        using SectorCounts = std::array<int, kSectorCount>;

        //-------------------------------------------------------------
        //! @brief 敵を1体、抽選テーブルに従って湧かせる関数
        //! @param registry       [in]     エンティティレジストリ
        //! @param context        [in]     エンジンコンテキスト
        //! @param playerPosition  [in]     プレイヤーの現在位置
        //! @param elapsedSeconds  [in]     走行の経過秒数（種類の解禁判定に使う）
        //! @param dangerRank      [in]     現在の危険度ランク（EnemyDifficultyTableの倍率を引くのに使う）
        //! @param sectorCounts    [in,out] 扇形ごとの敵の数（湧かせた扇形を+1する）
        //-------------------------------------------------------------
        void SpawnOne(Tsukino::ECS::Registry& registry,
                      Tsukino::EngineIntegration::EngineContext& context,
                      const hlslpp::float3& playerPosition,
                      float elapsedSeconds,
                      int dangerRank,
                      SectorCounts& sectorCounts);

        //-------------------------------------------------------------
        //! @brief  プレイヤーを中心に、フォグの外側の湧き位置を1つ決める関数
        //! @param  playerPosition [in]     プレイヤーの現在位置
        //! @param  sectorCounts   [in,out] 扇形ごとの敵の数（湧かせた扇形を+1する）
        //! @return 湧き位置
        //! @note   敵の少ない扇形から順に試す。地面（±kGroundLimit）の外を引いた場合は引き直す。
        //!         clampで内側へ押し込むと地面の端でプレイヤーの目の前に湧いてしまうため
        //-------------------------------------------------------------
        [[nodiscard]]
        hlslpp::float3 ResolveSpawnPosition(const hlslpp::float3& playerPosition, SectorCounts& sectorCounts);

        //-------------------------------------------------------------
        //! @brief  囲み半径（kSurroundRadius）内の生存敵を、プレイヤーから見た方角の扇形ごとに数える関数
        //! @param  registry       [in] エンティティレジストリ
        //! @param  playerPosition [in] プレイヤーの現在位置
        //! @return 扇形ごとの敵の数
        //-------------------------------------------------------------
        [[nodiscard]]
        SectorCounts CountEnemiesPerSector(Tsukino::ECS::Registry& registry, const hlslpp::float3& playerPosition) const;

        //-------------------------------------------------------------
        //! @brief  囲み半径の外に取り残された、本Systemが湧かせた雑魚を遠い順に集める関数
        //! @param  registry       [in] エンティティレジストリ
        //! @param  playerPosition [in] プレイヤーの現在位置
        //! @return 取り残された個体（遠い順）
        //! @note   エリートは告知済みなので黙って消さない（kDespawnRadiusの間引きだけに任せる）
        //-------------------------------------------------------------
        [[nodiscard]]
        std::vector<Tsukino::ECS::Entity> CollectStragglers(Tsukino::ECS::Registry& registry, const hlslpp::float3& playerPosition) const;

        //-------------------------------------------------------------
        //! @brief 遠くへ離れた、本Systemが湧かせた個体を間引く関数
        //! @param registry       [in] エンティティレジストリ
        //! @param playerPosition [in] プレイヤーの現在位置
        //! @note  シーン手置きの敵や負荷試験が湧かせた敵はSpawnedEnemyComponentを
        //!        持たないため対象にならない
        //-------------------------------------------------------------
        void CullDistantEnemies(Tsukino::ECS::Registry& registry, const hlslpp::float3& playerPosition);

        //-------------------------------------------------------------
        //! @brief 本Systemが湧かせた敵1体を、HPバー・持ち武器ごと破棄予約する関数
        //! @param registry [in] エンティティレジストリ
        //! @param entity   [in] 破棄する敵
        //-------------------------------------------------------------
        static void DespawnSpawnedEnemy(Tsukino::ECS::Registry& registry, Tsukino::ECS::Entity entity);

        //-------------------------------------------------------------
        //! @brief 湧いたばかりの敵を、霧の中から浮かび上がるようにフェードインさせる関数
        //! @param registry       [in] エンティティレジストリ
        //! @param playerPosition [in] プレイヤーの現在位置
        //! @param deltaTime      [in] デルタタイム
        //! @note  霧はノイズで濃淡が揺れるため、湧き半径でも薄い所ではシルエットが透ける。
        //!        そのまま出すとパッと生成されたように見える
        //-------------------------------------------------------------
        void UpdateSpawnFadeIn(Tsukino::ECS::Registry& registry, const hlslpp::float3& playerPosition, float deltaTime);

        //-------------------------------------------------------------
        //! @brief 敵本体と持っている武器の不透明度をまとめて書く関数
        //! @param registry [in] エンティティレジストリ
        //! @param entity   [in] 敵
        //! @param opacity  [in] 不透明度（0〜1）
        //! @note  本体だけ消すと武器が宙に浮いて見えるので、Paladinの武器にも同じ値を書く
        //-------------------------------------------------------------
        static void ApplyEnemyOpacity(Tsukino::ECS::Registry& registry, Tsukino::ECS::Entity entity, float opacity);

        //-------------------------------------------------------------
        //! @brief 生存中（死亡演出中を除く）の敵の総数を数える関数
        //! @param registry [in] エンティティレジストリ
        //! @return 生存数
        //! @note  死亡演出中の個体を数えてしまうと、倒した直後だけ湧きが止まり
        //!        「倒すほど湧きが遅くなる」逆向きの挙動になるため除外する
        //-------------------------------------------------------------
        [[nodiscard]]
        int CountLiveEnemies(Tsukino::ECS::Registry& registry);

        //-------------------------------------------------------------
        //! @brief 生存中（死亡演出中を除く）のエリートを数える関数
        //! @param registry [in] エンティティレジストリ
        //! @return 生存数（同時出現数の上限判定に使う）
        //-------------------------------------------------------------
        [[nodiscard]]
        int CountLiveElites(Tsukino::ECS::Registry& registry) const;

        //---------------------------------------------------------
        // 湧き位置
        //---------------------------------------------------------
        //! 湧き半径の内側。Combatの霧（Fog/FogComponent.json：開始600・density 0.0035）で
        //! 約88%隠れる距離。霧の設定を変えたらここも見直すこと
        static constexpr float kSpawnRadiusMin = 1200.0f;

        //! 湧き半径の外側（霧で約96%隠れる）。SmallZombie(moveSpeed=100)基準で到達まで約15秒。
        //! これ以上遠くすると湧いた敵が戦闘に絡むまでの待ち時間が間延びする
        static constexpr float kSpawnRadiusMax = 1500.0f;

        //! 間引き半径。湧き外周より十分外に置く。近すぎると、湧いた直後に
        //! プレイヤーがほんの少し逆走しただけで即座に消えて湧き直しが延々と続く
        static constexpr float kDespawnRadius = 2400.0f;

        //! 囲み半径。これより内側の敵を「プレイヤーを囲んでいる」とみなして方角ごとに数える。
        //! 外側の雑魚は霧の奥に取り残された個体として、上限に達しているときに湧きと入れ替える。
        //! 湧き外周（kSpawnRadiusMax）より外なので、入れ替えで消える瞬間は霧の中で見えない。
        //! これが無いと、一方向へ逃げたときに背後へ置き去りにした敵が上限を埋め、行く手に何も湧かなくなる
        static constexpr float kSurroundRadius = 1700.0f;

        //! 湧いてから不透明になりきるまでの秒数
        static constexpr float kFadeInSeconds = 2.5f;

        //! この距離まで近づいたら、経過時間に関係なく不透明にしきる。
        //! 湧いた敵へ向かって走ると（接近速度 約400/秒）時間だけでは目の前で半透明のままになるため
        static constexpr float kFadeInOpaqueDistance = 800.0f;

        //! 地面（±5000の板）から落とさないための実効境界。
        //! 端に余白を取るのは、境界ちょうどに湧くとカプセルが床の縁からはみ出すため
        static constexpr float kGroundLimit = 4500.0f;

        //! 生成時の浮かせ量。EnemyStressTestSystem::kSpawnHeightと同値
        static constexpr float kSpawnHeight = 20.0f;

        //! 扇形1つあたりの引き直し回数。地面外へ出た場合に同じ扇形の中で引き直し、
        //! それでも外れたら次に敵の少ない扇形を試す
        static constexpr int kSpawnAttemptCount = 3;

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

        //! クリア前のラスト1分（kRunFinalStretchSeconds）に湧き間隔へ掛ける倍率。
        //! 0.6秒 → 0.36秒で、湧く速さはおよそ1.7倍になる。同時に居られる数の上限（kMaxLiveEnemies）は変えない
        static constexpr float kFinalStretchIntervalScale = 0.6f;

        //! 1回の湧きで出す数。間隔だけを詰めると1体ずつ細く来る絵になるため、
        //! まとまりで出して「群れが押し寄せる」画を作る
        static constexpr int kSpawnBatchSize = 2;

        //! 同時に存在してよい敵の数の上限（シーン手置き・負荷試験分も含めた総数で判定する）。
        //! 負荷試験では500体でも60fpsに余裕があったので、これは性能上限ではなく
        //! ゲーム性（囲まれ具合）の設定値
        static constexpr int kMaxLiveEnemies = 60;

        //! 乱数生成器。エンジン側に共通の乱数ユーティリティが無いため本Systemが自前で持つ
        std::mt19937 m_rng{std::random_device{}()};

        //! 次の湧きまでの残り秒数。
        //! 経過秒数は本Systemでは持たず、RunClockComponent（RunClockSystemが進める）を読む。
        //! HUDへ出す危険度と、実際に湧く敵へ適用される危険度を必ず一致させるため
        float m_spawnTimer = 0.0f;
    };
}    // namespace CombatAndroid::ECS
