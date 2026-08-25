//-------------------------------------------------------------
//! @file   SkillSelectSystem.cpp
//! @brief  SkillSelectSystemクラスの実装
//-------------------------------------------------------------
#include <CombatAndroid/ECS/System/SkillSelectSystem.hpp>
#include <CombatAndroid/ECS/Component/SkillSelectComponent.hpp>
#include <CombatAndroid/ECS/Component/PlayerSkillComponent.hpp>
#include <CombatAndroid/ECS/Component/PlayerComponent.hpp>

#include <Tsukino/BuiltIn/ECS/Component/TransformComponent.hpp>
#include <Tsukino/BuiltIn/ECS/Component/SpriteComponent.hpp>
#include <Tsukino/BuiltIn/ECS/Component/FontComponent.hpp>
#include <Tsukino/BuiltIn/ECS/Component/CharacterControllerComponent.hpp>

#include <Tsukino/EngineIntegration/EngineContext.hpp>

#include <Tsukino/Engine/Asset/AssetManager.hpp>
#include <Tsukino/Engine/Asset/Texture/TextureAsset.hpp>

#include <Tsukino/Core/Input/InputSystem.hpp>
#include <Tsukino/Core/Path.hpp>
#include <Tsukino/Core/Window.hpp>

#include <hlsl++.h>
#include <entt/entt.hpp>

#include <algorithm>
#include <string>
// 名前空間 : CombatAndroid::ECS
namespace CombatAndroid::ECS {
    namespace {
        //-------------------------------------------------------------
        // カードの見た目のチューニング値（全て画面ピクセル単位）。
        // 位置はメニューを開くたびにウィンドウサイズから計算し直す
        //-------------------------------------------------------------
        constexpr float kCardWidth        = 760.0f;
        constexpr float kCardHeight       = 150.0f;
        constexpr float kCardGap          = 24.0f;                     //!< カード同士の縦の隙間
        constexpr float kCardPitch        = kCardHeight + kCardGap;    //!< カード1枚ぶんの送り
        constexpr float kHighlightInflate = 8.0f;                      //!< 選択中カードの強調枠が四辺へはみ出す量
        constexpr float kTextInsetX       = 32.0f;                     //!< カード左端からテキストまでの余白
        constexpr float kNameOffsetY      = -26.0f;                    //!< カード中心から見たスキル名のY
        constexpr float kDescOffsetY      = 30.0f;                     //!< カード中心から見た説明文のY
        constexpr float kTitleOffsetY     = -310.0f;                   //!< 画面中心から見た「LEVEL UP!」のY

        constexpr float kTitleFontScale = 2.2f;
        constexpr float kNameFontScale  = 1.3f;
        constexpr float kDescFontScale  = 0.85f;

        const hlslpp::float4 kBackdropColor  = hlslpp::float4(0.0f, 0.0f, 0.0f, 0.65f);      //!< 画面全体の暗転
        const hlslpp::float4 kHighlightColor = hlslpp::float4(1.0f, 0.92f, 0.35f, 0.95f);    //!< 選択中カードの枠（黄色）
        const hlslpp::float4 kTitleColor     = hlslpp::float4(1.0f, 0.95f, 0.6f, 1.0f);
        const hlslpp::float4 kNameColor      = hlslpp::float4(1.0f, 1.0f, 1.0f, 1.0f);
        const hlslpp::float4 kDescColor      = hlslpp::float4(0.92f, 0.92f, 0.92f, 1.0f);

        constexpr float kUnselectedPanelAlpha = 0.78f;    //!< 非選択カードは少し沈ませる

        //-------------------------------------------------------------
        //! @brief  エンティティを非表示にする関数
        //! @param  registry [in] ECSレジストリ
        //! @param  entity   [in] 対象のエンティティ
        //! @note   SpriteRenderSystemは極小スケールを、FontRendererSystemは空文字を
        //!         それぞれ描画対象から外すため、この2つを書けば消える（HPバー等と同じ流儀）
        //-------------------------------------------------------------
        void HideEntity(Tsukino::ECS::Registry& registry, Tsukino::ECS::Entity entity) {
            if(entity == entt::null)
                return;

            if(auto* transform = registry.try_get<Tsukino::BuiltIn::ECS::TransformComponent>(entity)) {
                transform->scale = hlslpp::float3(0.0f, 0.0f, 0.0f);
                transform->dirty = true;
            }
            if(auto* font = registry.try_get<Tsukino::BuiltIn::ECS::FontComponent>(entity))
                font->text = L"";
        }

        //-------------------------------------------------------------
        //! @brief  スプライトを指定のピクセル矩形いっぱいに広げる関数
        //! @param  registry  [in] ECSレジストリ
        //! @param  context   [in] エンジンコンテキスト（テクスチャの実寸を引くのに使う）
        //! @param  entity    [in] 対象のエンティティ
        //! @param  centerX   [in] 矩形中心のスクリーンX
        //! @param  centerY   [in] 矩形中心のスクリーンY
        //! @param  width     [in] 矩形の幅（ピクセル）
        //! @param  height    [in] 矩形の高さ（ピクセル）
        //! @param  tintColor [in] スプライトに乗算する色
        //! @note   SpriteRenderSystemは「テクスチャの実ピクセル数 × transform.scale」を
        //!         最終的な大きさとして使うため、テクスチャサイズで割った値をscaleへ入れる。
        //!         HPバーのようにWhitePixel.png（4x4）決め打ちにせずアセットから実寸を
        //!         引いているので、スキル専用の背景画像へ差し替えてもレイアウトが崩れない
        //-------------------------------------------------------------
        void StretchSprite(Tsukino::ECS::Registry& registry, Tsukino::EngineIntegration::EngineContext& context, Tsukino::ECS::Entity entity,
                           float centerX, float centerY, float width, float height, const hlslpp::float4& tintColor) {
            if(entity == entt::null)
                return;

            auto* transform = registry.try_get<Tsukino::BuiltIn::ECS::TransformComponent>(entity);
            auto* sprite    = registry.try_get<Tsukino::BuiltIn::ECS::SpriteComponent>(entity);
            if(!transform || !sprite)
                return;

            float textureWidth  = 1.0f;
            float textureHeight = 1.0f;
            if(context.assetManager) {
                std::shared_ptr<Tsukino::Asset::TextureAsset> textureAsset =
                    std::static_pointer_cast<Tsukino::Asset::TextureAsset>(context.assetManager->Get(sprite->textureHandle));
                if(textureAsset && textureAsset->width > 0 && textureAsset->height > 0) {
                    textureWidth  = static_cast<float>(textureAsset->width);
                    textureHeight = static_cast<float>(textureAsset->height);
                }
            }

            transform->position = hlslpp::float3(centerX, centerY, 0.0f);
            transform->scale    = hlslpp::float3(width / textureWidth, height / textureHeight, 1.0f);
            transform->dirty    = true;

            sprite->tintColor = tintColor;
        }

        //-------------------------------------------------------------
        //! @brief  文字エンティティの位置・大きさ・内容を書く関数
        //! @param  registry  [in] ECSレジストリ
        //! @param  entity    [in] 対象のエンティティ
        //! @param  x         [in] スクリーンX
        //! @param  y         [in] スクリーンY
        //! @param  fontScale [in] フォントの拡大率（TransformComponent::scale.xがそのまま文字サイズになる）
        //! @param  text      [in] 表示する文字列
        //! @param  color     [in] 文字色
        //-------------------------------------------------------------
        void PlaceText(Tsukino::ECS::Registry& registry, Tsukino::ECS::Entity entity, float x, float y, float fontScale,
                       const std::wstring& text, const hlslpp::float4& color) {
            if(entity == entt::null)
                return;

            auto* transform = registry.try_get<Tsukino::BuiltIn::ECS::TransformComponent>(entity);
            auto* font      = registry.try_get<Tsukino::BuiltIn::ECS::FontComponent>(entity);
            if(!transform || !font)
                return;

            transform->position = hlslpp::float3(x, y, 0.0f);
            transform->scale    = hlslpp::float3(fontScale, fontScale, 1.0f);
            transform->dirty    = true;

            font->text  = text;
            font->color = color;
        }

        //-------------------------------------------------------------
        //! @brief  index枚目のカードの中心Yを求める関数
        //! @param  screenCenterY  [in] 画面中央のY
        //! @param  index          [in] 何枚目か（0始まり）
        //! @param  candidateCount [in] 今回並べるカードの枚数
        //! @return カード中心のスクリーンY
        //! @note   枚数が1〜3のどれでも画面中央に対して対称に並ぶ1本の式にしてある
        //-------------------------------------------------------------
        [[nodiscard]]
        float CalculateCardCenterY(float screenCenterY, int index, int candidateCount) {
            return screenCenterY + (static_cast<float>(index) - static_cast<float>(candidateCount - 1) * 0.5f) * kCardPitch;
        }

        //-------------------------------------------------------------
        //! @brief  メニュー全体の見た目を書き直す関数
        //! @param  registry [in] ECSレジストリ
        //! @param  context  [in] エンジンコンテキスト
        //! @param  select   [in] メニューの状態
        //! @param  skills   [in] 取得済みスキル（「Lv.2 / 5」の表示に使う）
        //! @note   開いた時とカーソルが動いた時に呼ぶ。毎フレーム呼んでも問題はないが、
        //!         文字列を組み立てるので変化があった時だけにしている
        //-------------------------------------------------------------
        void RefreshUi(Tsukino::ECS::Registry& registry, Tsukino::EngineIntegration::EngineContext& context, SkillSelectComponent& select,
                       const PlayerSkillComponent& skills) {
            const float screenWidth   = context.window ? static_cast<float>(context.window->GetWidth()) : 1700.0f;
            const float screenHeight  = context.window ? static_cast<float>(context.window->GetHeight()) : 1000.0f;
            const float screenCenterX = screenWidth * 0.5f;
            const float screenCenterY = screenHeight * 0.5f;

            //-------------------------------------------------------------
            // 画面全体の暗転とタイトル
            //-------------------------------------------------------------
            StretchSprite(registry, context, select.backdropEntity, screenCenterX, screenCenterY, screenWidth, screenHeight, kBackdropColor);
            PlaceText(registry, select.titleEntity, screenCenterX, screenCenterY + kTitleOffsetY, kTitleFontScale, L"LEVEL UP!", kTitleColor);

            //-------------------------------------------------------------
            // 選択中カードの強調枠。カード矩形を四辺へ少しはみ出させた板を1枚、
            // カードより奥（sortOrderが小さい）に敷いて縁のように見せている
            //-------------------------------------------------------------
            const int cursorIndex = std::clamp(select.cursorIndex, 0, std::max(select.candidateCount - 1, 0));
            StretchSprite(registry, context, select.highlightEntity, screenCenterX,
                          CalculateCardCenterY(screenCenterY, cursorIndex, select.candidateCount), kCardWidth + kHighlightInflate * 2.0f,
                          kCardHeight + kHighlightInflate * 2.0f, kHighlightColor);

            //-------------------------------------------------------------
            // カード本体
            //-------------------------------------------------------------
            const float textLeftX = screenCenterX - kCardWidth * 0.5f + kTextInsetX;

            for(int i = 0; i < kSkillChoiceMax; ++i) {
                SkillSelectCardEntities& card = select.cards[static_cast<size_t>(i)];

                if(i >= select.candidateCount) {
                    // カンストで選択肢が3つに満たない回。余ったカードは消しておく
                    HideEntity(registry, card.panelEntity);
                    HideEntity(registry, card.nameEntity);
                    HideEntity(registry, card.descEntity);
                    continue;
                }

                const SkillId          id       = select.candidates[static_cast<size_t>(i)];
                const SkillTableEntry& entry    = GetSkillEntry(id);
                const int              level    = skills.levels[static_cast<size_t>(id)];
                const float            centerY  = CalculateCardCenterY(screenCenterY, i, select.candidateCount);
                const bool             selected = (i == cursorIndex);

                //-------------------------------------------------------------
                // 背景。テクスチャはテーブルのパスから引く（AssetManagerがパスで
                // キャッシュするため、開くたびにLoadを呼んでも実際の読み込みは1回きり）
                //-------------------------------------------------------------
                if(card.panelEntity != entt::null && context.assetManager) {
                    if(auto* panelSprite = registry.try_get<Tsukino::BuiltIn::ECS::SpriteComponent>(card.panelEntity))
                        panelSprite->textureHandle = context.assetManager->Load(Tsukino::Core::Path(entry.backgroundTexturePath));
                }

                // 非選択のカードはアルファだけ下げて沈ませる（PlayerDamageEffectSystemと同じ組み立て方）
                const float          panelAlpha = selected ? entry.panelColor.w : entry.panelColor.w * kUnselectedPanelAlpha;
                const hlslpp::float4 panelColor =
                    hlslpp::float4(entry.panelColor.x, entry.panelColor.y, entry.panelColor.z, panelAlpha);

                StretchSprite(registry, context, card.panelEntity, screenCenterX, centerY, kCardWidth, kCardHeight, panelColor);

                //-------------------------------------------------------------
                // 文言。levelは取得済みの段階数なので、今回取ると level+1 段階目になる
                //-------------------------------------------------------------
                std::wstring nameText = entry.displayName;
                nameText += L"   Lv.";
                nameText += std::to_wstring(level + 1);
                nameText += L" / ";
                nameText += std::to_wstring(kMaxSkillLevel);

                PlaceText(registry, card.nameEntity, textLeftX, centerY + kNameOffsetY, kNameFontScale, nameText, kNameColor);
                PlaceText(registry, card.descEntity, textLeftX, centerY + kDescOffsetY, kDescFontScale,
                          entry.levels[static_cast<size_t>(level)].description, kDescColor);
            }
        }

        //-------------------------------------------------------------
        //! @brief  メニューを構成するエンティティを全て消す関数
        //! @param  registry [in] ECSレジストリ
        //! @param  select   [in] メニューの状態
        //-------------------------------------------------------------
        void HideUi(Tsukino::ECS::Registry& registry, SkillSelectComponent& select) {
            HideEntity(registry, select.backdropEntity);
            HideEntity(registry, select.highlightEntity);
            HideEntity(registry, select.titleEntity);

            for(SkillSelectCardEntities& card : select.cards) {
                HideEntity(registry, card.panelEntity);
                HideEntity(registry, card.nameEntity);
                HideEntity(registry, card.descEntity);
            }
        }

        //-------------------------------------------------------------
        //! @brief  全キャラクタの移動入力を打ち消す関数
        //! @param  registry [in] ECSレジストリ
        //! @note   PhysicsSystemはdeltaTimeが0以下でも1/60秒ぶん必ずステップする。
        //!         そのためシーン側でdeltaTime=0にしても、moveInputが残っていると
        //!         CharacterVirtualは毎フレーム動き続けてしまう。移動入力を書くSystem群は
        //!         メニュー中は早期リターンして何も書かないので、ここで潰した値が保たれる
        //-------------------------------------------------------------
        void SuppressMoveInput(Tsukino::ECS::Registry& registry) {
            auto view = registry.View<Tsukino::BuiltIn::ECS::CharacterControllerComponent>();
            view.each([](Tsukino::BuiltIn::ECS::CharacterControllerComponent& characterController) {
                characterController.moveInput = hlslpp::float3(0.0f, 0.0f, 0.0f);
            });
        }
    }    // namespace

    //-------------------------------------------------------------
    //! @brief 今スキル選択で進行を止めているかを問い合わせる
    //-------------------------------------------------------------
    bool IsSkillSelectActive(Tsukino::ECS::Registry& registry) {
        auto view = registry.View<SkillSelectComponent>();
        for(entt::entity entity : view) {
            const SkillSelectComponent& select = view.get<SkillSelectComponent>(entity);
            return select.isActive || select.pendingLevelUps > 0 || select.closingBlockFrames > 0;
        }
        return false;
    }

    //-------------------------------------------------------------
    //! @brief システムの更新
    //-------------------------------------------------------------
    void SkillSelectSystem::Update(Tsukino::ECS::Registry& registry, float deltaTime) {
        // メニューはアニメーションを持たないため経過時間を使わない
        //（そもそも表示中はシーンがdeltaTime=0を渡してくる）
        (void)deltaTime;

        auto* ctx = registry.GetContext<Tsukino::EngineIntegration::EngineContext*>();
        if(!ctx)
            return;

        auto view = registry.View<PlayerComponent, SkillSelectComponent, PlayerSkillComponent>();
        for(entt::entity entity : view) {
            auto& player = view.get<PlayerComponent>(entity);
            auto& select = view.get<SkillSelectComponent>(entity);
            auto& skills = view.get<PlayerSkillComponent>(entity);

            if(!select.isActive && select.pendingLevelUps <= 0) {
                //-------------------------------------------------------------
                // 決定した次のフレーム。この1フレームだけ停止を延長して、
                // 決定に使ったスペースの押し込みが完全に流れきるのを待つ
                // （IsKeyPressedは立ち上がり検出なので、1フレーム挟めば消える）
                //-------------------------------------------------------------
                if(select.closingBlockFrames > 0) {
                    --select.closingBlockFrames;
                    SuppressMoveInput(registry);
                }
                continue;    // 平常時。何もしない
            }

            select.closingBlockFrames = 0;

            // 停止中はプレイヤーも敵も移動入力を持たない状態に固定する
            SuppressMoveInput(registry);

            //-------------------------------------------------------------
            // メニューを開く
            //-------------------------------------------------------------
            if(!select.isActive) {
                const int candidateCount = PickSkillCandidates(m_rng, skills.levels, select.candidates);
                if(candidateCount <= 0) {
                    // 全スキルがカンスト済み。出す物が無いので予約ごと捨て、そのままゲームを続ける
                    select.pendingLevelUps = 0;
                    continue;
                }

                select.candidateCount  = candidateCount;
                select.cursorIndex     = 0;
                select.isActive        = true;
                select.openedThisFrame = true;

                //-------------------------------------------------------------
                // 進行中のヒットストップを打ち切る。CombatAndroidScene::OnUpdateは
                // メニュー中はdeltaTimeを0で上書きするためhitStopTimerを減らす分岐を
                // 通らず、放っておくとメニューを閉じた後にスローモーションが残ってしまう
                //-------------------------------------------------------------
                ctx->hitStopTimer = 0.0f;

                RefreshUi(registry, *ctx, select, skills);
                continue;    // 表示した直後のフレームでそのまま決定入力を拾わない
            }

            if(select.openedThisFrame) {
                select.openedThisFrame = false;
                continue;
            }

            if(!ctx->inputSystem)
                continue;

            //-------------------------------------------------------------
            // カーソルの上下移動。W/Sの押し込みとマウスホイールを同じ「1段ぶん」として扱う
            //（ホイールは1ノッチ=±1.0。上へ回す＝上のカードへ＝インデックスは減る）
            //-------------------------------------------------------------
            int step = 0;
            if(ctx->inputSystem->IsKeyPressed(Tsukino::Input::KeyCode::W))
                --step;
            if(ctx->inputSystem->IsKeyPressed(Tsukino::Input::KeyCode::S))
                ++step;

            const float wheelDelta = ctx->inputSystem->GetWheelDelta();
            if(wheelDelta > 0.0f)
                --step;
            else if(wheelDelta < 0.0f)
                ++step;

            if(step != 0) {
                const int nextIndex = std::clamp(select.cursorIndex + step, 0, select.candidateCount - 1);
                if(nextIndex != select.cursorIndex) {
                    select.cursorIndex = nextIndex;
                    RefreshUi(registry, *ctx, select, skills);
                }
            }

            //-------------------------------------------------------------
            // 決定。取得段階を1つ進めて実効値を計算し直し、メニューを閉じる。
            // pendingLevelUpsがまだ残っていれば次のフレームで2回目のメニューが開く
            //（IsSkillSelectActiveはpendingLevelUpsも見ているので停止は続いたまま）
            //-------------------------------------------------------------
            if(ctx->inputSystem->IsKeyPressed(Tsukino::Input::KeyCode::Space)) {
                const SkillId acquiredId = select.candidates[static_cast<size_t>(select.cursorIndex)];

                int& acquiredLevel = skills.levels[static_cast<size_t>(acquiredId)];
                acquiredLevel      = std::min(acquiredLevel + 1, kMaxSkillLevel);
                RecalculateSkillStats(skills);

                --select.pendingLevelUps;
                select.isActive = false;
                HideUi(registry, select);

                // 次のレベルアップが残っていない場合、このフレームの後半でPlayerSystemが
                // 走ってしまうため、あと1フレームだけ停止を延長する
                select.closingBlockFrames = 1;

                //-------------------------------------------------------------
                // メニューへ入る前に積まれていた先行入力を捨てる。残したままだと
                // 閉じた次のフレームにPlayerAnimationSystemがそれを消費し、
                // 攻撃や回避が意図せず暴発する
                //-------------------------------------------------------------
                player.attackInputPressed = false;
                player.dodgeInputPressed  = false;
            }
        }
    }
}    // namespace CombatAndroid::ECS
