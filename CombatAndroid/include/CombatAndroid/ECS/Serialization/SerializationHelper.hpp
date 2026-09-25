//-------------------------------------------------------------
//! @file   SerializationHelper.hpp
//! @brief  ゲーム側Componentのcerealシリアライズ定義が共通で使うヘルパ
//-------------------------------------------------------------
#pragma once
#include <cereal/cereal.hpp>

#include <type_traits>

// 名前空間 : CombatAndroid::ECS
namespace CombatAndroid::ECS {
    //-------------------------------------------------------------
    //! @brief  Archiveがcerealの本物の入力アーカイブか
    //! @note   PrefabFactory::InstantiateGroupは、EntityRefの解決のためにload()へ擬似アーカイブ
    //!         （EntityRefResolverArchive）を渡して再訪問する（AssetRefは読み込み中に解決されるので再訪問しない）。
    //!         擬似アーカイブはstd::vectorなどcerealのサイズタグを使うコンテナを処理できないため、
    //!         参照を含まないコンテナ（SpringBoneのchainDefsなど）は本物のアーカイブのときだけ読む
    //-------------------------------------------------------------
    template <class Archive>
    inline constexpr bool kIsRealInputArchive = std::is_base_of_v<cereal::detail::InputArchiveBase, Archive>;

    //-------------------------------------------------------------
    //! @brief  名前付きで1フィールドを読む。JSONにキーが無ければ既定値のまま残す
    //! @param  archive [in,out] 入力アーカイブ
    //! @param  name    [in]     JSONのキー名
    //! @param  value   [out]    読み込み先
    //! @note   JSONInputArchiveはキーが無いとthrowし、そのComponentが丸ごと読めなくなる。
    //!         フィールドを後から足しても古いPrefab JSONを壊さないよう、loadでは全フィールドをこれで読む
    //!         （throwはアーカイブの位置を進める前に起きるので、握りつぶして次のフィールドへ進んでよい）
    //-------------------------------------------------------------
    template <class Archive, class T>
    void LoadField(Archive& archive, const char* name, T& value) {
        try {
            archive(cereal::make_nvp(name, value));
        } catch(const cereal::Exception&) {
            // キー無し：既定値のまま
        }
    }
}    // namespace CombatAndroid::ECS
