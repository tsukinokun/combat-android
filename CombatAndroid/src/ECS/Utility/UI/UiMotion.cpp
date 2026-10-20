//-------------------------------------------------------------
//! @file    UiMotion.cpp
//! @brief   UIの動きの定型の実装
//-------------------------------------------------------------
#include <CombatAndroid/ECS/Utility/UI/UiMotion.hpp>
#include <CombatAndroid/ECS/Utility/UI/InputPromptWidget.hpp>
#include <CombatAndroid/ECS/Serialization/Common/SerializationHelper.hpp>
#include <CombatAndroid/ECS/Utility/Table/TableJson.hpp>

#include <Tsukino/BuiltIn/ECS/Component/UIMotionComponent.hpp>
#include <Tsukino/BuiltIn/ECS/Component/UITweenComponent.hpp>
#include <Tsukino/BuiltIn/ECS/UI/UITweenUtility.hpp>

#include <Tsukino/Core/Math/Easing.hpp>

#include <entt/entt.hpp>

#include <algorithm>
#include <cstdint>
// 名前空間 : CombatAndroid::ECS
namespace CombatAndroid::ECS {
    namespace {
        using Tsukino::BuiltIn::ECS::UITween;
        using Tsukino::BuiltIn::ECS::UITweenLoop;
        using Tsukino::BuiltIn::ECS::UITweenProperty;
        using Tsukino::Core::Math::Easing::EaseType;
        namespace UITweenUtility = Tsukino::BuiltIn::ECS::UITweenUtility;

        //-------------------------------------------------------------
        //! @enum   UiMotionTag
        //! @brief  トゥイーンの目印。同じ目印を再生し直すと置き換わる。
        //!         出現と消えるは同じ目印を使い、途中で切り替えても重ならないようにする
        //-------------------------------------------------------------
        enum class UiMotionTag : std::uint32_t {
            VisibilityAlpha = 0x55490001,    // ゲーム側の目印だと分かる値から始める（0は「目印なし」）
            VisibilityOffsetX,
            VisibilityOffsetY,
            VisibilityScale,
            Punch,
            Pulse,
            SlideX,
            SlideY,
            IdleBob,
            IdleTilt,
        };

        constexpr std::uint32_t ToTag(UiMotionTag tag) {
            return static_cast<std::uint32_t>(tag);
        }

        //-------------------------------------------------------------
        //! @brief  トゥイーン1本を組み立てる
        //-------------------------------------------------------------
        UITween MakeTween(UiMotionTag tag, UITweenProperty property, float from, float to, float duration, float delay, EaseType ease,
                          bool unscaledTime) {
            UITween tween;
            tween.tag          = ToTag(tag);
            tween.property     = property;
            tween.from         = from;
            tween.to           = to;
            tween.duration     = std::max(duration, 0.01f);
            tween.delay        = std::max(delay, 0.0f);
            tween.ease         = ease;
            tween.unscaledTime = unscaledTime;
            return tween;
        }

        //-------------------------------------------------------------
        //! @brief  出現・消えるの目印のトゥイーンをすべて外す
        //-------------------------------------------------------------
        void StopVisibility(Tsukino::ECS::Registry& registry, Tsukino::ECS::Entity entity) {
            UITweenUtility::Stop(registry, entity, ToTag(UiMotionTag::VisibilityAlpha));
            UITweenUtility::Stop(registry, entity, ToTag(UiMotionTag::VisibilityOffsetX));
            UITweenUtility::Stop(registry, entity, ToTag(UiMotionTag::VisibilityOffsetY));
            UITweenUtility::Stop(registry, entity, ToTag(UiMotionTag::VisibilityScale));
        }
    }    // namespace

    //-------------------------------------------------------------
    //! @brief  UiMotion.jsonの読み込み（UiMotionParamsと同じ名前空間に置き、cerealから見つかるようにする）
    //-------------------------------------------------------------
    template <class Archive>
    void load(Archive& archive, UiMotionParams& params) {
        LoadField(archive, "appearDuration", params.appearDuration);
        LoadField(archive, "appearSlide", params.appearSlide);
        LoadField(archive, "appearStagger", params.appearStagger);
        LoadField(archive, "popScaleFrom", params.popScaleFrom);
        LoadField(archive, "popDuration", params.popDuration);
        LoadField(archive, "disappearDuration", params.disappearDuration);
        LoadField(archive, "disappearSlide", params.disappearSlide);
        LoadField(archive, "punchStrength", params.punchStrength);
        LoadField(archive, "punchDuration", params.punchDuration);
        LoadField(archive, "pulseAmount", params.pulseAmount);
        LoadField(archive, "pulsePeriod", params.pulsePeriod);
        LoadField(archive, "slideDuration", params.slideDuration);
        LoadField(archive, "idleBob", params.idleBob);
        LoadField(archive, "idlePeriod", params.idlePeriod);
        LoadField(archive, "idleTilt", params.idleTilt);
        LoadField(archive, "idleTiltPeriod", params.idleTiltPeriod);
    }

    //-------------------------------------------------------------
    //! @brief  UIの動きの既定値を得る関数
    //-------------------------------------------------------------
    const UiMotionParams& GetUiMotionParams() {
        return GetSystemParams<UiMotionParams>("UiMotion");
    }

    //-------------------------------------------------------------
    //! @brief  出現させる
    //-------------------------------------------------------------
    void UiAppear(Tsukino::ECS::Registry& registry, Tsukino::ECS::Entity entity, const UiAppearDesc& desc) {
        if(entity == entt::null || !registry.IsValid(entity))
            return;

        const UiMotionParams& params   = GetUiMotionParams();
        const bool            pop      = desc.scaleFrom < 0.999f;
        const float           duration = (desc.duration > 0.0f) ? desc.duration : (pop ? params.popDuration : params.appearDuration);

        // 前の出現・消えるを外してから始める（消えるの途中から出し直しても透明が残らない）
        StopVisibility(registry, entity);

        // 透明度は少し早めに上がりきるようにして、滑り込みの終わりにはもうはっきり見えているようにする
        UITweenUtility::Play(registry, entity,
                             MakeTween(UiMotionTag::VisibilityAlpha, UITweenProperty::Alpha, 0.0f, 1.0f, duration * 0.7f, desc.delay, EaseType::OutQuad,
                                       desc.unscaledTime));
        if(desc.slideX != 0.0f)
            UITweenUtility::Play(registry, entity,
                                 MakeTween(UiMotionTag::VisibilityOffsetX, UITweenProperty::OffsetX, desc.slideX, 0.0f, duration, desc.delay,
                                           EaseType::OutCubic, desc.unscaledTime));
        if(desc.slideY != 0.0f)
            UITweenUtility::Play(registry, entity,
                                 MakeTween(UiMotionTag::VisibilityOffsetY, UITweenProperty::OffsetY, desc.slideY, 0.0f, duration, desc.delay,
                                           EaseType::OutCubic, desc.unscaledTime));
        if(pop)
            UITweenUtility::Play(registry, entity,
                                 MakeTween(UiMotionTag::VisibilityScale, UITweenProperty::Scale, desc.scaleFrom, 1.0f, duration, desc.delay,
                                           EaseType::OutBack, desc.unscaledTime));
    }

    //-------------------------------------------------------------
    //! @brief  下から滑り込んで出す
    //-------------------------------------------------------------
    void UiAppearFromBelow(Tsukino::ECS::Registry& registry, Tsukino::ECS::Entity entity, float delay, bool unscaledTime) {
        UiAppearDesc desc;
        desc.slideY       = GetUiMotionParams().appearSlide;
        desc.delay        = delay;
        desc.unscaledTime = unscaledTime;
        UiAppear(registry, entity, desc);
    }

    //-------------------------------------------------------------
    //! @brief  横から滑り込んで出す
    //-------------------------------------------------------------
    void UiAppearFromSide(Tsukino::ECS::Registry& registry, Tsukino::ECS::Entity entity, int index, bool unscaledTime) {
        const UiMotionParams& params = GetUiMotionParams();
        UiAppearDesc          desc;
        desc.slideX       = -params.appearSlide;
        desc.delay        = params.appearStagger * static_cast<float>(std::max(index, 0));
        desc.unscaledTime = unscaledTime;
        UiAppear(registry, entity, desc);
    }

    //-------------------------------------------------------------
    //! @brief  ポップで出す
    //-------------------------------------------------------------
    void UiPopIn(Tsukino::ECS::Registry& registry, Tsukino::ECS::Entity entity, float delay, bool unscaledTime) {
        UiAppearDesc desc;
        desc.scaleFrom    = GetUiMotionParams().popScaleFrom;
        desc.delay        = delay;
        desc.unscaledTime = unscaledTime;
        UiAppear(registry, entity, desc);
    }

    //-------------------------------------------------------------
    //! @brief  消す
    //-------------------------------------------------------------
    void UiDisappear(Tsukino::ECS::Registry& registry, Tsukino::ECS::Entity entity, bool unscaledTime) {
        if(entity == entt::null || !registry.IsValid(entity))
            return;

        const UiMotionParams& params = GetUiMotionParams();
        StopVisibility(registry, entity);

        UITween alpha = MakeTween(UiMotionTag::VisibilityAlpha, UITweenProperty::Alpha, 1.0f, 0.0f, params.disappearDuration, 0.0f, EaseType::InQuad,
                                  unscaledTime);
        alpha.holdAtEnd = true;    // 消えたあと一瞬元に戻って見えないよう、透明のまま保つ
        UITweenUtility::Play(registry, entity, alpha);

        UITween slide = MakeTween(UiMotionTag::VisibilityOffsetY, UITweenProperty::OffsetY, 0.0f, params.disappearSlide, params.disappearDuration,
                                  0.0f, EaseType::InQuad, unscaledTime);
        slide.holdAtEnd = true;
        UITweenUtility::Play(registry, entity, slide);
    }

    //-------------------------------------------------------------
    //! @brief  出現・消えるの動きが残っているか
    //-------------------------------------------------------------
    bool UiIsVisibilityAnimating(Tsukino::ECS::Registry& registry, Tsukino::ECS::Entity entity) {
        if(entity == entt::null || !registry.IsValid(entity) || !registry.HasComponent<Tsukino::BuiltIn::ECS::UITweenComponent>(entity))
            return false;

        const auto& tweens = registry.GetComponent<Tsukino::BuiltIn::ECS::UITweenComponent>(entity).tweens;
        return std::any_of(tweens.begin(), tweens.end(), [](const UITween& tween) {
            const bool isVisibility = tween.tag >= ToTag(UiMotionTag::VisibilityAlpha) && tween.tag <= ToTag(UiMotionTag::VisibilityScale);
            return isVisibility && tween.elapsed < tween.delay + tween.duration;
        });
    }

    //-------------------------------------------------------------
    //! @brief  出現・消えるの動きを外す
    //-------------------------------------------------------------
    void UiResetVisibility(Tsukino::ECS::Registry& registry, Tsukino::ECS::Entity entity) {
        if(entity == entt::null || !registry.IsValid(entity))
            return;
        StopVisibility(registry, entity);
    }

    //-------------------------------------------------------------
    //! @brief  前の位置からなめらかに移ってきたように見せる
    //-------------------------------------------------------------
    void UiSlideFrom(Tsukino::ECS::Registry& registry, Tsukino::ECS::Entity entity, float fromX, float fromY, bool unscaledTime) {
        if(entity == entt::null || !registry.IsValid(entity))
            return;

        const UiMotionParams& params = GetUiMotionParams();

        //-------------------------------------------------------------
        // まだ前の寄せが残っていれば、今見えているずれ（残り）に今回のずれを足して続ける。
        // こうしないと、素早く2段動かしたときに1段ぶん飛んで見える
        //-------------------------------------------------------------
        auto remaining = [&](UiMotionTag tag) {
            if(!registry.HasComponent<Tsukino::BuiltIn::ECS::UITweenComponent>(entity))
                return 0.0f;
            for(const UITween& tween : registry.GetComponent<Tsukino::BuiltIn::ECS::UITweenComponent>(entity).tweens) {
                if(tween.tag != ToTag(tag))
                    continue;
                const float t = std::clamp((tween.elapsed - tween.delay) / std::max(tween.duration, 0.01f), 0.0f, 1.0f);
                return tween.from + (tween.to - tween.from) * Tsukino::Core::Math::Easing::Evaluate(tween.ease, t);
            }
            return 0.0f;
        };

        const float startX = remaining(UiMotionTag::SlideX) + fromX;
        const float startY = remaining(UiMotionTag::SlideY) + fromY;
        if(startX != 0.0f)
            UITweenUtility::Play(registry, entity,
                                 MakeTween(UiMotionTag::SlideX, UITweenProperty::OffsetX, startX, 0.0f, params.slideDuration, 0.0f, EaseType::OutCubic,
                                           unscaledTime));
        if(startY != 0.0f)
            UITweenUtility::Play(registry, entity,
                                 MakeTween(UiMotionTag::SlideY, UITweenProperty::OffsetY, startY, 0.0f, params.slideDuration, 0.0f, EaseType::OutCubic,
                                           unscaledTime));
    }

    //-------------------------------------------------------------
    //! @brief  弾ませる
    //-------------------------------------------------------------
    void UiPunch(Tsukino::ECS::Registry& registry, Tsukino::ECS::Entity entity, float strength, bool unscaledTime) {
        if(entity == entt::null || !registry.IsValid(entity))
            return;

        const UiMotionParams& params = GetUiMotionParams();
        // Punchのイージングは両端で0になるので、終わると拡大率1へ戻る
        UITweenUtility::Play(registry, entity,
                             MakeTween(UiMotionTag::Punch, UITweenProperty::Scale, 1.0f, 1.0f + params.punchStrength * strength, params.punchDuration,
                                       0.0f, EaseType::Punch, unscaledTime));
    }

    //-------------------------------------------------------------
    //! @brief  呼吸させる
    //-------------------------------------------------------------
    void UiPulse(Tsukino::ECS::Registry& registry, Tsukino::ECS::Entity entity, float amountScale, bool unscaledTime) {
        if(entity == entt::null || !registry.IsValid(entity) || UiIsPulsing(registry, entity))
            return;

        const UiMotionParams& params = GetUiMotionParams();
        UITween tween = MakeTween(UiMotionTag::Pulse, UITweenProperty::Scale, 1.0f, 1.0f + params.pulseAmount * amountScale, params.pulsePeriod, 0.0f,
                                  EaseType::InOutSine, unscaledTime);
        tween.loop = UITweenLoop::PingPong;
        UITweenUtility::Play(registry, entity, tween);
    }

    //-------------------------------------------------------------
    //! @brief  ゆらゆらさせる
    //-------------------------------------------------------------
    void UiIdle(Tsukino::ECS::Registry& registry, Tsukino::ECS::Entity entity, float amountScale, bool unscaledTime) {
        if(entity == entt::null || !registry.IsValid(entity) || UITweenUtility::IsPlaying(registry, entity, ToTag(UiMotionTag::IdleBob)))
            return;

        const UiMotionParams& params = GetUiMotionParams();

        // 上下は0から上へ浮いて戻る（始めた瞬間に位置が飛ばないように0から始める）
        UITween bob = MakeTween(UiMotionTag::IdleBob, UITweenProperty::OffsetY, 0.0f, -params.idleBob * amountScale, params.idlePeriod, 0.0f,
                                EaseType::InOutSine, unscaledTime);
        bob.loop = UITweenLoop::PingPong;
        UITweenUtility::Play(registry, entity, bob);

        // 傾きは左右へ振る。振れ幅が小さいので、始めの一瞬のずれは目に付かない
        constexpr float kDegToRad = 3.14159265f / 180.0f;
        const float     tilt      = params.idleTilt * amountScale * kDegToRad;
        UITween sway = MakeTween(UiMotionTag::IdleTilt, UITweenProperty::Rotation, -tilt, tilt, params.idleTiltPeriod, 0.0f, EaseType::InOutSine,
                                 unscaledTime);
        sway.loop = UITweenLoop::PingPong;
        UITweenUtility::Play(registry, entity, sway);
    }

    //-------------------------------------------------------------
    //! @brief  ゆらゆらを止める
    //-------------------------------------------------------------
    void UiStopIdle(Tsukino::ECS::Registry& registry, Tsukino::ECS::Entity entity) {
        if(entity == entt::null || !registry.IsValid(entity))
            return;
        UITweenUtility::Stop(registry, entity, ToTag(UiMotionTag::IdleBob));
        UITweenUtility::Stop(registry, entity, ToTag(UiMotionTag::IdleTilt));
    }

    //-------------------------------------------------------------
    //! @brief  呼吸を止める
    //-------------------------------------------------------------
    void UiStopPulse(Tsukino::ECS::Registry& registry, Tsukino::ECS::Entity entity) {
        if(entity == entt::null || !registry.IsValid(entity))
            return;
        UITweenUtility::Stop(registry, entity, ToTag(UiMotionTag::Pulse));
    }

    //-------------------------------------------------------------
    //! @brief  呼吸しているか
    //-------------------------------------------------------------
    bool UiIsPulsing(Tsukino::ECS::Registry& registry, Tsukino::ECS::Entity entity) {
        return entity != entt::null && UITweenUtility::IsPlaying(registry, entity, ToTag(UiMotionTag::Pulse));
    }

    //-------------------------------------------------------------
    //! @brief  部品を親役のエンティティの動きに追従させる
    //-------------------------------------------------------------
    void UiFollow(Tsukino::ECS::Registry& registry, Tsukino::ECS::Entity follower, Tsukino::ECS::Entity source) {
        if(follower == entt::null || source == entt::null || !registry.IsValid(follower) || follower == source)
            return;

        auto& motion = registry.HasComponent<Tsukino::BuiltIn::ECS::UIMotionComponent>(follower)
                           ? registry.GetComponent<Tsukino::BuiltIn::ECS::UIMotionComponent>(follower)
                           : registry.AddComponent<Tsukino::BuiltIn::ECS::UIMotionComponent>(follower);
        motion.source = source;
    }

    //-------------------------------------------------------------
    //! @brief  複数の部品をまとめて追従させる
    //-------------------------------------------------------------
    void UiFollowAll(Tsukino::ECS::Registry& registry, std::span<const Tsukino::ECS::Entity> followers, Tsukino::ECS::Entity source) {
        for(Tsukino::ECS::Entity follower : followers)
            UiFollow(registry, follower, source);
    }

    //-------------------------------------------------------------
    //! @brief  キー表示の部品をまとめて追従させる
    //-------------------------------------------------------------
    void UiFollowPrompt(Tsukino::ECS::Registry& registry, const InputPromptWidget& prompt, Tsukino::ECS::Entity source) {
        const Tsukino::ECS::Entity parts[] = {prompt.capBorderEntity,   prompt.capFaceEntity,     prompt.glyphEntity,
                                              prompt.chevronArmAEntity, prompt.chevronArmBEntity, prompt.captionEntity};
        UiFollowAll(registry, parts, source);
        UiFollowAll(registry, prompt.ringEntities, source);
    }
}    // namespace CombatAndroid::ECS
