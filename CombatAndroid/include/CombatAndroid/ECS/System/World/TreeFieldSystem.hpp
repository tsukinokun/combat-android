//-------------------------------------------------------------
//! @file   TreeFieldSystem.hpp
//! @brief  TreeFieldSystemクラスの宣言
//-------------------------------------------------------------
#pragma once
#include <CombatAndroid/ECS/Event/World/GrassWaveEvent.hpp>
#include <CombatAndroid/ECS/Event/World/TreeHitEvent.hpp>

#include <Tsukino/Core/ECS/System/ISystem.hpp>
#include <Tsukino/Core/ECS/Event/ScopedConnection.hpp>
#include <Tsukino/Renderer/DX11/InstanceBuffer.hpp>
#include <Tsukino/Renderer/DX11/MeshBuffer.hpp>
#include <Tsukino/Renderer/DX11/UserConstantBuffer.hpp>

#include <entt/entt.hpp>
#include <hlsl++.h>

#include <cstdint>
#include <optional>
#include <unordered_map>
#include <unordered_set>
#include <vector>
// 名前空間 : CombatAndroid::ECS
namespace CombatAndroid::ECS {
    struct TreeFieldComponent;

    //! 攻撃で揺らせる木の数（同時に強く揺れている木のうち、揺れの大きい順にこの数まで）
    //! @note Assets/Shaders/TreeSway.hlsli の kMaxShakingTrees と一致させること
    inline constexpr std::uint32_t kMaxShakingTrees = 8;

    //-------------------------------------------------------------
    //! @struct CBufferTree
    //! @brief  木を風で揺らすパラメータ（ゲーム予約枠 b12）
    //! @note   Assets/Shaders/TreeSway.hlsli の CBufferTree と1バイト単位で一致させること（全メンバfloat4で192バイト）
    //-------------------------------------------------------------
    struct CBufferTree {
        hlslpp::float4 windParams;                      //!< xz: 風向き（水平・正規化済み）, yw: 予約
        hlslpp::float4 swayParams;                      //!< x: 幹のしなりの強さ, y: しなりの速さ, z: 葉のそよぎの強さ（0で無し）, w: そよぎの速さ
        hlslpp::float4 gustParams;                      //!< x: 突風の波長, y: 突風の速さ, z: 突風の強さ, w: しなりの基準の高さ
        hlslpp::float4 shakeSlots[kMaxShakingTrees];    //!< 攻撃で揺れている木ごとに xy: 根元のXZ, zw: しなりのずれのXZ
        hlslpp::float4 shakeParams;                     //!< x: 使っているスロット数, y: ずれ1あたりに葉のそよぎを強める量, zw: 予約
    };

    //-------------------------------------------------------------
    //! @class  TreeFieldSystem
    //! @brief  フィールドにプログラム生成の木を生やし、描画・幹の当たり判定・攻撃への反応を受け持つシステム
    //! @note   最初の更新でTreeFieldComponentの値から木の形を何種類か作り（ProceduralTree）、以後は使い回す。
    //!         毎フレーム、プレイヤーの周りの升目を調べて、升目の座標から決まる乱数で木を置き、
    //!         1本ずつ幹と葉の描画コマンドをGBufferパスへ積む。1本ずつ描くのは、エンジンの影の描き直しが
    //!         「1描画＝1つのワールド行列」で、まとめ描画（インスタンス描画）だと影が正しく落ちないため。
    //!         当たり判定はプレイヤーの近くの木にだけ、幹のカプセルを持つエンティティを作り、離れたら消す。
    //!         風の揺れは自前の頂点シェーダー（TreeSway.vs.hlsl）で付け、影も同じ揺れの影用シェーダー
    //!         （TreeSwayShadow.vs.hlsl。DrawCommand::shadowPipeline）で描くので一緒に揺れる。
    //!         攻撃が木に当たる（TreeHitEvent）か、範囲攻撃の衝撃波（GrassWaveEvent）が届くと、
    //!         その木をばねで大きくしならせ、葉を舞い落とす（落ちる葉はインスタンス描画。FallingLeaf.vs.hlsl）
    //-------------------------------------------------------------
    class TreeFieldSystem : public Tsukino::ECS::ISystem {
    public:
        //-------------------------------------------------------------
        //! @brief 更新処理
        //! @param registry  [in] エンジンのECSレジストリのラッパー
        //! @param deltaTime [in] デルタタイム（攻撃の揺れと落ちる葉を進める。メニュー中は0）
        //-------------------------------------------------------------
        void Update(Tsukino::ECS::Registry& registry, float deltaTime) override;

    private:
        //-------------------------------------------------------------
        //! @struct TreeVariant
        //! @brief  作っておいた木の形1種類ぶん
        //-------------------------------------------------------------
        struct TreeVariant {
            Tsukino::Renderer::MeshBuffer trunk;                 //!< 幹と枝
            Tsukino::Renderer::MeshBuffer canopy;                //!< 葉カード
            float                         trunkRadius  = 0.0f;   //!< 主幹の根元の半径（当たり判定の太さに使う）
            float                         height       = 0.0f;   //!< 木全体の高さ（落ちる葉を出す高さに使う）
            float                         canopyRadius = 0.0f;   //!< 葉の茂みの水平の広がり（落ちる葉を出す範囲に使う）
        };

        //-------------------------------------------------------------
        //! @struct TreeInstance
        //! @brief  升目に立っている木1本（升目の座標から決まる）
        //-------------------------------------------------------------
        struct TreeInstance {
            float  x       = 0.0f;
            float  z       = 0.0f;
            size_t variant = 0;
            float  yaw     = 0.0f;
            float  scale   = 1.0f;
        };

        //-------------------------------------------------------------
        //! @struct TreeShake
        //! @brief  攻撃で揺れている木1本ぶんのばねの状態
        //-------------------------------------------------------------
        struct TreeShake {
            float          x = 0.0f;                                     //!< 根元のX
            float          z = 0.0f;                                     //!< 根元のZ
            hlslpp::float3 offset   = hlslpp::float3(0.0f, 0.0f, 0.0f);  //!< しなりのずれ（基準の高さでの量。yは使わない）
            hlslpp::float3 velocity = hlslpp::float3(0.0f, 0.0f, 0.0f);  //!< 上記の速度
        };

        //-------------------------------------------------------------
        //! @struct FallingLeaf
        //! @brief  舞い落ちる葉1枚
        //-------------------------------------------------------------
        struct FallingLeaf {
            hlslpp::float3 position = hlslpp::float3(0.0f, 0.0f, 0.0f);
            hlslpp::float3 velocity = hlslpp::float3(0.0f, 0.0f, 0.0f);
            float          yaw = 0.0f, pitch = 0.0f, roll = 0.0f;               //!< 向き（ラジアン）
            float          yawSpeed = 0.0f, pitchSpeed = 0.0f, rollSpeed = 0.0f;    //!< 回る速さ
            float          size        = 1.0f;     //!< 一辺の長さ
            float          tile        = 0.0f;     //!< 葉の絵のマス（0〜3）
            float          swayPhase   = 0.0f;     //!< ゆらゆら流れる動きの位相
            float          age         = 0.0f;     //!< 出てからの秒数
            float          restTimer   = -1.0f;    //!< 地面に着いてからの秒数（負なら落ちている途中）
        };

        //! 升目の木を求める（生えていなければ無し）
        [[nodiscard]]
        std::optional<TreeInstance> ResolveTree(const TreeFieldComponent& field, int cellX, int cellZ) const;

        //! 木に攻撃を当てる（揺らして、葉を落とす）
        void HitTree(const TreeFieldComponent& field, std::uint64_t cellKey, const TreeInstance& tree, const hlslpp::float3& direction,
                     float impulse, float strength);

        //! 攻撃の揺れと落ちる葉を進める
        void UpdateReactions(const TreeFieldComponent& field, float deltaTime, float windX, float windZ);

        //! 落ちる葉を描く
        void DrawFallingLeaves(Tsukino::ECS::Registry& registry, const TreeFieldComponent& field);

        std::vector<TreeVariant> m_variants;          //!< 木の形（最初の更新で作る。作った後は数を変えない＝描画コマンドが指す先を動かさない）
        bool                     m_buildFailed = false;    //!< 形を作れなかった（以後は何もしない）

        //! 揺れのパラメータ。幹（葉のそよぎ無し）と葉（そよぎ有り）で分けて持つ
        //! @note 描画コマンドはバッファを指すだけで、実際の描画は後でまとめて行われるので、1本を書き換えて使い回せない
        Tsukino::Renderer::UserConstantBuffer m_trunkSwayBuffer;
        Tsukino::Renderer::UserConstantBuffer m_leafSwayBuffer;

        //! 当たり判定のエンティティ。キーは升目の座標（木は1升に多くて1本）
        std::unordered_map<std::uint64_t, Tsukino::ECS::Entity> m_colliders;

        //! このフレームに当たり判定が要る升目（毎フレーム作り直す。確保し直さないよう使い回す）
        std::unordered_set<std::uint64_t> m_wantedColliders;

        //-------------------------------------------------------------
        // 攻撃への反応
        //-------------------------------------------------------------
        std::unordered_map<std::uint64_t, TreeShake> m_shakes;          //!< 揺れている木（キーは升目）
        std::vector<FallingLeaf>                     m_leaves;          //!< 舞い落ちている葉
        std::uint32_t                                m_leafRandom = 0x2545f491u;    //!< 葉を出すときの乱数の状態

        std::vector<TreeHitEvent>   m_pendingHits;     //!< 次の更新で処理する、木への当たり
        std::vector<GrassWaveEvent> m_pendingWaves;    //!< 次の更新で処理する、範囲攻撃の衝撃波
        Tsukino::ECS::ScopedConnection m_hitConnection;     //!< TreeHitEventの購読
        Tsukino::ECS::ScopedConnection m_waveConnection;    //!< GrassWaveEventの購読
        bool                           m_subscribed = false;    //!< 購読を張ったか（最初の更新で張る）

        Tsukino::Renderer::MeshBuffer     m_leafQuad;          //!< 落ちる葉の四角形（全部の葉で共用）
        Tsukino::Renderer::InstanceBuffer m_leafInstances;     //!< 落ちる葉ごとの位置・大きさ・回転（毎フレーム書き直す）
    };
}    // namespace CombatAndroid::ECS
