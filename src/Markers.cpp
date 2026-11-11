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

        // The enchanting table lists enchantment *templates* ("Fire Damage", "Fortify Smithing"...).
        // Every enchanted item's enchantment points at its template (data.baseEnchantment), and
        // disenchanting is believed to flag that template as known. Player-made enchantments point at
        // the template they were made from. If there is no template the enchantment is its own.
        const RE::EnchantmentItem* TemplateOf(const RE::EnchantmentItem* a_enchantment)
        {
            if (!a_enchantment) {
                return nullptr;
            }
            const auto* base = a_enchantment->data.baseEnchantment;
            return base ? base : a_enchantment;
        }

        using EffectSet = std::unordered_set<const RE::EffectSetting*>;

        // Every magic effect that appears in a known enchantment template. This is what the enchanting
        // table lets you reuse, so any enchantment made only of these effects is one you can make.
        // Covers enchantments you made yourself, which do not point at a known template.
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

        bool EffectsKnown(const RE::EnchantmentItem* a_enchantment, const EffectSet& a_known)
        {
            if (!a_enchantment || a_enchantment->effects.empty()) {
                return false;
            }
            for (const auto* effect : a_enchantment->effects) {
                if (!effect || !effect->baseEffect || !a_known.contains(effect->baseEffect)) {
                    return false;
                }
            }
            return true;
        }

        bool IsKnown(const RE::EnchantmentItem* a_enchantment, const EffectSet& a_known)
        {
            const auto* templ = TemplateOf(a_enchantment);
            return templ && (templ->GetKnown() || a_enchantment->GetKnown() || EffectsKnown(a_enchantment, a_known));
        }

        bool HasKnownEnchantment(RE::InventoryEntryData* a_entry, const EffectSet& a_known)
        {
            return a_entry && IsKnown(a_entry->GetEnchantment(), a_known);
        }

        // Diagnostics: one line describing an enchanted item that was NOT marked.
        std::string DescribeUnmarked(RE::InventoryEntryData* a_entry, const EffectSet& a_known)
        {
            const auto* enchantment = a_entry->GetEnchantment();
            const auto* base = enchantment->data.baseEnchantment;
            std::string effects;
            for (const auto* effect : enchantment->effects) {
                if (effect && effect->baseEffect) {
                    effects += std::format(" {:08X}:{}{}", effect->baseEffect->GetFormID(), effect->baseEffect->GetName(),
                        a_known.contains(effect->baseEffect) ? "(known)" : "");
                }
            }
            return std::format("    unmarked '{}' ench={:08X}:'{}' base={} effects:{}",
                a_entry->GetDisplayName() ? a_entry->GetDisplayName() : "?",
                enchantment->GetFormID(), enchantment->GetName(),
                base ? std::format("{:08X}:'{}' known={}", base->GetFormID(), base->GetName(), base->GetKnown()) : std::string("none"),
                effects);
        }

        // Diagnostics: how many enchantments and magic effects in the whole game carry the "known"
        // flag, with a few names. Shows which flag the game really sets when you disenchant.
        template <class T>
        std::string DescribeKnown(const char* a_label)
        {
            auto* data = RE::TESDataHandler::GetSingleton();
            if (!data) {
                return std::string(a_label) + ": no data handler";
            }
            int         total = 0;
            int         known = 0;
            int         rawBit = 0;
            std::string names;
            for (const auto* form : data->GetFormArray<T>()) {
                if (!form) {
                    continue;
                }
                ++total;
                rawBit += (static_cast<std::uint32_t>(form->formFlags) >> 6) & 1;
                if (form->GetKnown()) {
                    if (++known <= 8) {
                        names += std::format(" {:08X}:{}", form->GetFormID(), form->GetName());
                    }
                }
            }
            return std::format("{}: total={} known={} rawBit6={}{}", a_label, total, known, rawBit, names);
        }

        struct Counts
        {
            int total{};
            int enchanted{};
            int known{};
            int enchantmentFlagKnown{};  // diagnostics: the old per-enchantment flag
            int templateKnown{};         // diagnostics: the template carries the flag
            int noText{};
            int alreadyMarked{};
            int marked{};
            int setFailed{};
            int samples{};
            std::string sampleText;

            
        };

        // Adds the marker to every list entry whose item has a known enchantment.
        // Returns true if any entry changed.
        bool MarkItems(RE::ItemList* a_list, const Config::Settings& a_settings, const EffectSet& a_known, Counts& a_counts)
        {
            bool changed = false;
            for (auto* item : a_list->items) {
                ++a_counts.total;
                if (!item || !item->data.objDesc) {
                    continue;
                }
                if (const auto* enchantment = item->data.objDesc->GetEnchantment()) {
                    ++a_counts.enchanted;
                    a_counts.enchantmentFlagKnown += enchantment->GetKnown() ? 1 : 0;
                    a_counts.templateKnown += (TemplateOf(enchantment) && TemplateOf(enchantment)->GetKnown()) ? 1 : 0;
                }
                if (!HasKnownEnchantment(item->data.objDesc, a_known)) {
                    if (item->data.objDesc->GetEnchantment() && a_counts.samples < 12) {
                        ++a_counts.samples;
                        a_counts.sampleText += '\n' + DescribeUnmarked(item->data.objDesc, a_known);
                    }
                    continue;
                }
                ++a_counts.known;

                RE::GFxValue text;
                if (!item->obj.GetMember("text", &text) || !text.IsString()) {
                    ++a_counts.noText;
                    continue;
                }

                std::string name = text.GetString();
                const bool  already = a_settings.suffix ? name.ends_with(a_settings.marker) : name.starts_with(a_settings.marker);
                if (already) {
                    ++a_counts.alreadyMarked;
                    continue;
                }

                name = a_settings.suffix ? name + " " + a_settings.marker : a_settings.marker + " " + name;
                RE::GFxValue newText;
                newText.SetString(name);
                if (item->obj.SetMember("text", newText)) {
                    ++a_counts.marked;
                    changed = true;
                } else {
                    ++a_counts.setFailed;
                }
            }
            return changed;
        }

        void Apply(const char* a_menuName, RE::ItemList* a_list, bool a_ownInventory)
        {
            static Clock::time_point lastScan{};
            static Clock::time_point lastConfig{};

            const auto now = Clock::now();
            if (now - lastScan < kScanInterval) {
                return;
            }
            lastScan = now;

            if (now - lastConfig >= kConfigInterval) {
                lastConfig = now;
                Config::ReloadIfChanged();
            }

            // Diagnostics: log what a menu scan sees, but only when it changes, so the log stays short.
            static std::string lastState;
            const auto         report = [&](const std::string& a_state) {
                if (a_state != lastState) {
                    lastState = a_state;
                    spdlog::info("{}: {}", a_menuName, a_state);
                    spdlog::info("  game-wide {}", DescribeKnown<RE::EnchantmentItem>("enchantments"));
                    spdlog::info("  game-wide {}", DescribeKnown<RE::EffectSetting>("magic effects"));
                }
            };

            const auto settings = Config::Get();
            if (!settings.enabled || settings.marker.empty() || (a_ownInventory && !settings.showOnOwnItems)) {
                report("skipped by settings");
                return;
            }
            if (!a_list) {
                report("no item list");
                return;
            }
            if (a_list->updatePending) {
                report("item list update pending");
                return;
            }
            if (a_list->items.empty()) {
                report("item list empty");
                return;
            }

            Counts     counts;
            const bool changed = MarkItems(a_list, settings, CollectKnownEffects(), counts);
            report(std::format(
                "items={} enchanted={} known={} (enchantmentFlag={} template={}) marked={} alreadyMarked={} noText={} setFailed={}",
                counts.total, counts.enchanted, counts.known, counts.enchantmentFlagKnown, counts.templateKnown, counts.marked, counts.alreadyMarked, counts.noText, counts.setFailed) + counts.sampleText);

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
