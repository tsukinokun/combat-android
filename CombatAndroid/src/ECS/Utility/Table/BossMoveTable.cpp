//-------------------------------------------------------------
//! @file    BossMoveTable.cpp
//! @brief   ボスの技の表（Assets/Tables/Bosses/<名前>.json）の実装
//-------------------------------------------------------------
#include <CombatAndroid/ECS/Utility/Table/BossMoveTable.hpp>
#include <CombatAndroid/ECS/Serialization/Common/SerializationHelper.hpp>
#include <CombatAndroid/ECS/Utility/Table/TableJson.hpp>

#include <Tsukino/Core/IO/HotReload.hpp>
#include <Tsukino/Core/Log.hpp>

#include <cereal/types/vector.hpp>

#include <map>
#include <mutex>

// 名前空間 : CombatAndroid::ECS
namespace CombatAndroid::ECS {
    namespace {
        //! 範囲の形の綴り。BossAreaShapeの並びと揃えること
        constexpr const char* kAreaShapeKeys[] = {"Fan", "DiscForward", "DiscAtTarget", "Ring", "Line", "None"};

        //! 技の種類の綴り。BossMoveKindの並びと揃えること
        constexpr const char* kMoveKindKeys[] = {"Strike", "Charge", "Projectile", "Orbit"};

        BossMoveKind ParseMoveKind(const std::string& text) {
            for(int i = 0; i < static_cast<int>(std::size(kMoveKindKeys)); ++i) {
                if(text == kMoveKindKeys[i])
                    return static_cast<BossMoveKind>(i);
            }
            Tsukino::Core::Log::Error("BossMoveTable: unknown kind '" + text + "' (Strike / Charge / Projectile / Orbit)");
            return BossMoveKind::Strike;
        }

        BossAreaShape ParseAreaShape(const std::string& text) {
            for(int i = 0; i < static_cast<int>(std::size(kAreaShapeKeys)); ++i) {
                if(text == kAreaShapeKeys[i])
                    return static_cast<BossAreaShape>(i);
            }
            Tsukino::Core::Log::Error("BossMoveTable: unknown area '" + text + "' (Fan / DiscForward / DiscAtTarget / Ring / Line / None)");
            return BossAreaShape::Fan;
        }
    }    // namespace

    template <class Archive>
    void load(Archive& archive, BossMoveHitbox& hitbox) {
        LoadField(archive, "bone", hitbox.bone);
        LoadField(archive, "endBone", hitbox.endBone);
        LoadField(archive, "radius", hitbox.radius);
        LoadField(archive, "damage", hitbox.damage);
        LoadField(archive, "start", hitbox.start);
        LoadField(archive, "duration", hitbox.duration);
    }

    template <class Archive>
    void load(Archive& archive, BossMoveDef& move) {
        LoadField(archive, "name", move.name);
        LoadField(archive, "clip", move.clip);
        LoadField(archive, "animationIndex", move.animationIndex);
        LoadField(archive, "minRange", move.minRange);
        LoadField(archive, "maxRange", move.maxRange);
        LoadField(archive, "weight", move.weight);
        LoadField(archive, "cooldown", move.cooldown);
        LoadField(archive, "length", move.length);
        LoadField(archive, "recovery", move.recovery);
        LoadField(archive, "hitbox", move.hitbox);

        std::string area = kAreaShapeKeys[static_cast<int>(move.area)];
        LoadField(archive, "area", area);
        move.area = ParseAreaShape(area);

        std::string kind = kMoveKindKeys[static_cast<int>(move.kind)];
        LoadField(archive, "kind", kind);
        move.kind = ParseMoveKind(kind);
        LoadField(archive, "loopClip", move.loopClip);

        LoadField(archive, "leapStart", move.leapStart);
        LoadField(archive, "leapEnd", move.leapEnd);
        LoadField(archive, "leapMaxDistance", move.leapMaxDistance);
        LoadField(archive, "summonCount", move.summonCount);
        LoadField(archive, "summonRadius", move.summonRadius);
        LoadField(archive, "summonPrefab", move.summonPrefab);
        LoadField(archive, "summonSizeScale", move.summonSizeScale);
        LoadField(archive, "summonHealthScale", move.summonHealthScale);
        LoadField(archive, "chargeStart", move.chargeStart);
        LoadField(archive, "chargeDuration", move.chargeDuration);
        LoadField(archive, "chargeSpeed", move.chargeSpeed);
        LoadField(archive, "projectileCount", move.projectileCount);
        LoadField(archive, "projectileSpread", move.projectileSpread);
        LoadField(archive, "projectileSpeed", move.projectileSpeed);
        LoadField(archive, "projectileDamage", move.projectileDamage);
        LoadField(archive, "projectileRadius", move.projectileRadius);
        LoadField(archive, "projectileRange", move.projectileRange);
        LoadField(archive, "slowSeconds", move.slowSeconds);
        LoadField(archive, "slowScale", move.slowScale);
        LoadField(archive, "retreatSpeed", move.retreatSpeed);
        LoadField(archive, "orbitSeconds", move.orbitSeconds);
        LoadField(archive, "orbitRadius", move.orbitRadius);
        LoadField(archive, "orbitSpeed", move.orbitSpeed);
        LoadField(archive, "enragedRepeat", move.enragedRepeat);
        LoadField(archive, "altitude", move.altitude);
        LoadField(archive, "projectilePrefab", move.projectilePrefab);
        LoadField(archive, "projectileAimDown", move.projectileAimDown);
    }

    template <class Archive>
    void load(Archive& archive, BossSecondForm& form) {
        LoadField(archive, "key", form.key);
        LoadWideField(archive, "displayName", form.displayName);
        LoadField(archive, "sizeMultiplier", form.sizeMultiplier);
        LoadField(archive, "healthRatio", form.healthRatio);
        LoadField(archive, "transformSeconds", form.transformSeconds);
        LoadField(archive, "collapseSeconds", form.collapseSeconds);
        LoadField(archive, "idleClip", form.idleClip);
        LoadField(archive, "walkClip", form.walkClip);
    }

    template <class Archive>
    void load(Archive& archive, BossEnrageDef& enrage) {
        LoadField(archive, "healthRatio", enrage.healthRatio);
        LoadField(archive, "move", enrage.move);
        LoadField(archive, "moveSpeedScale", enrage.moveSpeedScale);
        LoadField(archive, "cooldownScale", enrage.cooldownScale);
        LoadField(archive, "fogDensityScale", enrage.fogDensityScale);
    }

    template <class Archive>
    void load(Archive& archive, BossBreakDef& breakDef) {
        LoadField(archive, "gaugeRatio", breakDef.gaugeRatio);
        LoadField(archive, "growthPerBreak", breakDef.growthPerBreak);
        LoadField(archive, "decayRatioPerSecond", breakDef.decayRatioPerSecond);
        LoadField(archive, "decayDelay", breakDef.decayDelay);
        LoadField(archive, "downSeconds", breakDef.downSeconds);
        LoadField(archive, "clip", breakDef.clip);
        LoadField(archive, "animationIndex", breakDef.animationIndex);
        LoadField(archive, "clipEnd", breakDef.clipEnd);
        LoadField(archive, "recoverSeconds", breakDef.recoverSeconds);
        LoadField(archive, "recoverBlend", breakDef.recoverBlend);
    }

    template <class Archive>
    void load(Archive& archive, BossMoveSet& set) {
        LoadField(archive, "moves", set.moves);
        LoadField(archive, "enrage", set.enrage);
        LoadField(archive, "break", set.breakDef);
        LoadField(archive, "turnSpeed", set.turnSpeed);
        LoadField(archive, "chaseStopRange", set.chaseStopRange);
        LoadField(archive, "secondForm", set.secondForm);
        LoadField(archive, "hoverHeight", set.hoverHeight);
        LoadField(archive, "hoverBob", set.hoverBob);
        LoadField(archive, "hoverBobSpeed", set.hoverBobSpeed);
        LoadField(archive, "landAltitude", set.landAltitude);
        LoadField(archive, "climbSpeed", set.climbSpeed);
    }

    namespace {
        //-------------------------------------------------------------
        //! @brief  Bosses/<key>.json を読み、書き間違いを警告する関数
        //! @param  key [in]  ボスの名前
        //! @param  set [out] 読み込み先（既定値のもの）
        //! @return 読めたか
        //-------------------------------------------------------------
        bool LoadBossMoveSet(const std::string& key, BossMoveSet& set) {
            if(!LoadTableJson(("Bosses/" + key + ".json").c_str(), "Boss", set))
                return false;
            if(set.moves.empty())
                Tsukino::Core::Log::Error("BossMoveTable: no moves in Bosses/" + key + ".json");

            // 技の判定が終わるより先に技を終えると判定が出ない。表の書き間違いを早めに知らせる
            for(const BossMoveDef& move : set.moves) {
                if(move.kind == BossMoveKind::Charge && move.chargeStart + move.chargeDuration > move.length)
                    Tsukino::Core::Log::Warn("BossMoveTable: " + key + "." + move.name + " ends before its charge (length < chargeStart + chargeDuration)");
                if(move.kind == BossMoveKind::Strike && move.hitbox.start + move.hitbox.duration > move.length)
                    Tsukino::Core::Log::Warn("BossMoveTable: " + key + "." + move.name + " ends before its hitbox (length < start + duration)");
            }
            return true;
        }

        //-------------------------------------------------------------
        //! @brief  技の数と名前の並びが同じかを調べる関数
        //! @note   湧いたボスは技の番号でクリップ・クールタイムを持つ（BossMoveSetComponent）。
        //!         読み直しで並びが変わるとその番号がずれるので、並びの変更は再起動を求める
        //-------------------------------------------------------------
        bool HasSameMoveList(const BossMoveSet& lhs, const BossMoveSet& rhs) {
            if(lhs.moves.size() != rhs.moves.size())
                return false;
            for(size_t i = 0; i < lhs.moves.size(); ++i) {
                if(lhs.moves[i].name != rhs.moves[i].name || lhs.moves[i].clip != rhs.moves[i].clip)
                    return false;
            }
            return true;
        }
    }    // namespace

    //-------------------------------------------------------------
    //! @brief ボスの技の表を得る
    //-------------------------------------------------------------
    const BossMoveSet& GetBossMoveSet(const std::string& key) {
        static std::mutex                         s_mutex;
        static std::map<std::string, BossMoveSet> s_sets;    // mapの要素は差し替えても動かないので、返した参照はそのまま使える

        std::lock_guard<std::mutex> lock(s_mutex);
        auto it = s_sets.find(key);
        if(it != s_sets.end())
            return it->second;

        BossMoveSet set;
        (void)LoadBossMoveSet(key, set);
        BossMoveSet& stored = s_sets.emplace(key, std::move(set)).first->second;

        // 保存し直したら読み直す（Debugのみ）。技の数値はその場で効き、技の並びの変更は拒否する
        Tsukino::IO::HotReload::Watch(TablePath("Bosses/" + key + ".json"), [key, &stored] {
            BossMoveSet reloaded;
            if(!LoadBossMoveSet(key, reloaded) || reloaded.moves.empty())
                return false;
            if(!HasSameMoveList(stored, reloaded)) {
                Tsukino::Core::Log::Warn("BossMoveTable: moves were added, removed, renamed or got another clip in Bosses/" + key
                                         + ".json. Restart required");
                return false;
            }

            std::lock_guard<std::mutex> reloadLock(s_mutex);
            stored = std::move(reloaded);
            return true;
        });
        return stored;
    }

    //-------------------------------------------------------------
    //! @brief 技の名前から番号を引く
    //-------------------------------------------------------------
    int FindBossMove(const BossMoveSet& set, const std::string& name) {
        for(size_t i = 0; i < set.moves.size(); ++i) {
            if(set.moves[i].name == name)
                return static_cast<int>(i);
        }
        return -1;
    }
}    // namespace CombatAndroid::ECS
