//-------------------------------------------------------------
//! @file   WeaponLevelDebugSystem.cpp
//! @brief  WeaponLevelDebugSystemクラスの実装
//! @author 山﨑愛
//-------------------------------------------------------------
#include <CombatAndroid/ECS/System/WeaponLevelDebugSystem.hpp>
#include <CombatAndroid/ECS/Component/WeaponLevelDebugComponent.hpp>
#include <CombatAndroid/ECS/Component/PlayerComponent.hpp>
#include <CombatAndroid/ECS/Component/WeaponComponent.hpp>
#include <CombatAndroid/ECS/Utility/WeaponTable.hpp>

#include <Tsukino/BuiltIn/ECS/Component/FontComponent.hpp>

#include <sstream>
// 名前空間 : CombatAndroid::ECS
namespace CombatAndroid::ECS {
    //-------------------------------------------------------------
    //! @brief システムの更新
    //-------------------------------------------------------------
    void WeaponLevelDebugSystem::Update(Tsukino::ECS::Registry& registry, float deltaTime) {
        //-------------------------------------------------------------
        // プレイヤーを取得（単一プレイヤー前提。PickupSystemと同じ流儀）
        //-------------------------------------------------------------
        const PlayerComponent* player = nullptr;

        auto playerView = registry.View<PlayerComponent>();
        for(auto entity : playerView) {
            player = &playerView.get<PlayerComponent>(entity);
            break;
        }

        //-------------------------------------------------------------
        // HUDテキストの更新。所持武器を種類名＋レベルで1行ずつ並べる
        //-------------------------------------------------------------
        auto hudView = registry.View<WeaponLevelDebugComponent, Tsukino::BuiltIn::ECS::FontComponent>();
        hudView.each([&](entt::entity, WeaponLevelDebugComponent&, Tsukino::BuiltIn::ECS::FontComponent& hudFont) {
            if(!player || player->weaponInventory.empty()) {
                hudFont.text.clear();    // 空文字ならFontRendererSystemが描画をスキップする
                return;
            }

            std::wostringstream wss;
            wss << L"[武器Lv]";

            for(entt::entity weaponEntity : player->weaponInventory) {
                if(!registry.HasComponent<WeaponComponent>(weaponEntity))
                    continue;

                const WeaponComponent&  weapon = registry.GetComponent<WeaponComponent>(weaponEntity);
                const WeaponTableEntry& entry  = GetWeaponEntry(weapon.weaponId);
                wss << L"\n" << entry.displayName << L" Lv." << weapon.level;
            }

            hudFont.text = wss.str();
        });
    }
}    // namespace CombatAndroid::ECS
