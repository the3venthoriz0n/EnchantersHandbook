#include "PCH.h"
#include "Markers.h"

#include "Config.h"

namespace EnchantersHandbook::Markers
{
    namespace
    {
        using Clock = std::chrono::steady_clock;

        // How often an open menu is re-checked. The game rebuilds the item list after every
        // transaction and tab change, which drops our marker; this brings it back within a
        // fraction of a second without touching Scaleform every frame.
        constexpr auto kScanInterval = std::chrono::milliseconds(100);
        constexpr auto kConfigInterval = std::chrono::seconds(1);

        // Vanilla keyword "MagicDisallowEnchanting": the game refuses to enchant or disenchant
        // items that carry it (staves, unique artifacts).
        constexpr RE::FormID kDisallowEnchanting = 0x000C27BD;

        enum class Kind
        {
            kNone,
            kKnown,    // enchantment already learned
            kBlocked,  // enchanted, but the game will not let you disenchant it
        };

        using EffectSet = std::unordered_set<const RE::EffectSetting*>;

        // Every magic effect that appears in a known enchantment. The enchanting table lists
        // enchantment templates and learning one flags it as known, so any enchantment made only of
        // these effects is one you can make. This also covers enchantments you made yourself.
        EffectSet CollectKnownEffects()
        {
            EffectSet result;
            if (auto* data = RE::TESDataHandler::GetSingleton()) {
                for (const auto* enchantment : data->GetFormArray<RE::EnchantmentItem>()) {
                    if (!enchantment || !enchantment->GetKnown()) {
                        continue;
                    }
                    for (const auto* effect : enchantment->effects) {
                        if (effect && effect->baseEffect) {
                            result.insert(effect->baseEffect);
                        }
                    }
                }
            }
            return result;
        }

        bool IsKnown(const RE::EnchantmentItem* a_enchantment, const EffectSet& a_known)
        {
            if (!a_enchantment) {
                return false;
            }

            // The item's enchantment, or the template it was made from, is flagged as known.
            const auto* base = a_enchantment->data.baseEnchantment;
            if (a_enchantment->GetKnown() || (base && base->GetKnown())) {
                return true;
            }

            // Otherwise every effect in it is one you have already learned.
            if (a_enchantment->effects.empty()) {
                return false;
            }
            for (const auto* effect : a_enchantment->effects) {
                if (!effect || !effect->baseEffect || !a_known.contains(effect->baseEffect)) {
                    return false;
                }
            }
            return true;
        }

        bool IsBlocked(const RE::TESBoundObject* a_object)
        {
            const auto* keywords = a_object ? a_object->As<RE::BGSKeywordForm>() : nullptr;
            return keywords && keywords->HasKeywordID(kDisallowEnchanting);
        }

        Kind Classify(RE::InventoryEntryData* a_entry, const EffectSet& a_known, const Config::Settings& a_settings)
        {
            if (!a_entry) {
                return Kind::kNone;
            }
            const auto* enchantment = a_entry->GetEnchantment();
            if (!enchantment) {
                return Kind::kNone;
            }
            if (IsBlocked(a_entry->GetObject())) {
                return a_settings.markBlocked ? Kind::kBlocked : Kind::kNone;
            }
            return IsKnown(enchantment, a_known) ? Kind::kKnown : Kind::kNone;
        }

        struct Counts
        {
            int enchanted{};
            int known{};
            int blocked{};
            int marked{};
        };

        bool HasMarker(const std::string& a_name, const std::string& a_marker, bool a_suffix)
        {
            return a_suffix ? a_name.ends_with(a_marker) : a_name.starts_with(a_marker);
        }

        // Adds the marker to every list entry that needs one. Returns true if any entry changed.
        bool MarkItems(RE::ItemList* a_list, const Config::Settings& a_settings, Counts& a_counts)
        {
            const auto known = CollectKnownEffects();
            bool       changed = false;

            for (auto* item : a_list->items) {
                if (!item) {
                    continue;
                }
                const auto kind = Classify(item->data.objDesc, known, a_settings);
                if (kind == Kind::kNone) {
                    continue;
                }
                kind == Kind::kKnown ? ++a_counts.known : ++a_counts.blocked;

                // Experimental: tint the item-type icon at the left of the row (SkyUI reads "iconColor").
                if (a_settings.tintIcons) {
                    constexpr double kGreen = 0x5BC85B;
                    constexpr double kRed = 0xE04B4B;
                    const double     wanted = kind == Kind::kKnown ? kGreen : kRed;

                    RE::GFxValue current;
                    if (!item->obj.GetMember("iconColor", &current) || !current.IsNumber() || current.GetNumber() != wanted) {
                        RE::GFxValue color;
                        color.SetNumber(wanted);
                        if (item->obj.SetMember("iconColor", color)) {
                            ++a_counts.marked;
                            changed = true;
                        }
                    }
                }

                const std::string& marker = kind == Kind::kKnown ? a_settings.marker : a_settings.blockedMarker;
                if (marker.empty()) {
                    continue;
                }

                RE::GFxValue text;
                if (!item->obj.GetMember("text", &text) || !text.IsString()) {
                    continue;
                }

                std::string name = text.GetString();
                if (HasMarker(name, marker, a_settings.suffix)) {
                    continue;
                }

                name = a_settings.suffix ? name + " " + marker : marker + " " + name;
                RE::GFxValue newText;
                newText.SetString(name);
                if (item->obj.SetMember("text", newText)) {
                    ++a_counts.marked;
                    changed = true;
                }
            }
            return changed;
        }

        void Apply(const char* a_menuName, RE::ItemList* a_list, bool a_ownInventory)
        {
            static Clock::time_point lastScan{};
            static Clock::time_point lastConfig{};
            static std::string       lastReport;

            const auto now = Clock::now();
            if (now - lastScan < kScanInterval) {
                return;
            }
            lastScan = now;

            if (now - lastConfig >= kConfigInterval) {
                lastConfig = now;
                Config::ReloadIfChanged();
            }

            const auto settings = Config::Get();
            if (!settings.enabled || (a_ownInventory && !settings.showOnOwnItems)) {
                return;
            }
            if (!a_list || a_list->updatePending || a_list->items.empty()) {
                return;
            }

            Counts     counts;
            const bool changed = MarkItems(a_list, settings, counts);

            if (settings.debug) {
                const auto report = std::format("{}: known={} blocked={}", a_menuName, counts.known, counts.blocked);
                if (report != lastReport) {
                    lastReport = report;
                    spdlog::debug("{}", report);
                }
            }

            if (changed) {
                // Tell the (SkyUI) list its data changed so it redraws with the new names.
                a_list->root.Invoke("InvalidateData");
            }
        }

        // Replaces IMenu::AdvanceMovie (vtable slot 5) of one menu class: run the original, then mark.
        template <class Menu, bool OwnInventory>
        struct MenuHook
        {
            static void Thunk(RE::IMenu* a_menu, float a_interval, std::uint32_t a_currentTime)
            {
                original(a_menu, a_interval, a_currentTime);
                Apply(Menu::MENU_NAME.data(), static_cast<Menu*>(a_menu)->GetRuntimeData().itemList, OwnInventory);
            }

            static void Install()
            {
                REL::Relocation<std::uintptr_t> vtable{ Menu::VTABLE[0] };
                original = reinterpret_cast<decltype(original)>(vtable.write_vfunc(0x05, Thunk));
            }

            static inline void (*original)(RE::IMenu*, float, std::uint32_t){};
        };
    }

    void Install()
    {
        MenuHook<RE::BarterMenu, false>::Install();
        MenuHook<RE::ContainerMenu, false>::Install();
        MenuHook<RE::GiftMenu, false>::Install();
        MenuHook<RE::InventoryMenu, true>::Install();
        spdlog::info("Item menu hooks installed (barter, container, gift, inventory)");
    }
}
