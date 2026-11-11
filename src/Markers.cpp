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

        // True if the item carries an enchantment the player has learned (same flag the vanilla
        // enchanting table uses to show "known"). Works for base and player-made enchantments.
        bool HasKnownEnchantment(RE::InventoryEntryData* a_entry)
        {
            if (!a_entry) {
                return false;
            }
            const auto* enchantment = a_entry->GetEnchantment();
            return enchantment && enchantment->GetKnown();
        }

        // Adds the marker to every list entry whose item has a known enchantment.
        // Returns true if any entry changed.
        bool MarkItems(RE::ItemList* a_list, const Config::Settings& a_settings)
        {
            bool changed = false;
            for (auto* item : a_list->items) {
                if (!item || !HasKnownEnchantment(item->data.objDesc)) {
                    continue;
                }

                RE::GFxValue text;
                if (!item->obj.GetMember("text", &text) || !text.IsString()) {
                    continue;
                }

                std::string name = text.GetString();
                const bool  already = a_settings.suffix ? name.ends_with(a_settings.marker) : name.starts_with(a_settings.marker);
                if (already) {
                    continue;
                }

                name = a_settings.suffix ? name + " " + a_settings.marker : a_settings.marker + " " + name;
                RE::GFxValue newText;
                newText.SetString(name);
                changed |= item->obj.SetMember("text", newText);
            }
            return changed;
        }

        void Apply(RE::ItemList* a_list, bool a_ownInventory)
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

            const auto settings = Config::Get();
            if (!settings.enabled || settings.marker.empty() || (a_ownInventory && !settings.showOnOwnItems)) {
                return;
            }
            if (!a_list || a_list->updatePending || a_list->items.empty()) {
                return;
            }

            if (MarkItems(a_list, settings)) {
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
                Apply(static_cast<Menu*>(a_menu)->GetRuntimeData().itemList, OwnInventory);
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
