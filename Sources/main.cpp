#include <3ds.h>
#include "csvc.h"
#include <CTRPluginFramework.hpp>

#include <vector>

namespace CTRPluginFramework
{
    // This patch the NFC disabling the touchscreen when scanning an amiibo, which prevents ctrpf to be used
    static void    ToggleTouchscreenForceOn(void)
    {
        static u32 original = 0;
        static u32 *patchAddress = nullptr;

        if (patchAddress && original)
        {
            *patchAddress = original;
            return;
        }

        static const std::vector<u32> pattern =
        {
            0xE59F10C0, 0xE5840004, 0xE5841000, 0xE5DD0000,
            0xE5C40008, 0xE28DD03C, 0xE8BD80F0, 0xE5D51001,
            0xE1D400D4, 0xE3510003, 0x159F0034, 0x1A000003
        };

        Result  res;
        Handle  processHandle;
        s64     textTotalSize = 0;
        s64     startAddress = 0;
        u32 *   found;

        if (R_FAILED(svcOpenProcess(&processHandle, 16)))
            return;

        svcGetProcessInfo(&textTotalSize, processHandle, 0x10002);
        svcGetProcessInfo(&startAddress, processHandle, 0x10005);
        if(R_FAILED(svcMapProcessMemoryEx(CUR_PROCESS_HANDLE, 0x14000000, processHandle, (u32)startAddress, textTotalSize)))
            goto exit;

        found = (u32 *)Utils::Search<u32>(0x14000000, (u32)textTotalSize, pattern);

        if (found != nullptr)
        {
            original = found[13];
            patchAddress = (u32 *)PA_FROM_VA((found + 13));
            found[13] = 0xE1A00000;
        }

        svcUnmapProcessMemoryEx(CUR_PROCESS_HANDLE, 0x14000000, textTotalSize);
exit:
        svcCloseHandle(processHandle);
    }

    // This function is called before main and before the game starts
    // Useful to do code edits safely
    void    PatchProcess(FwkSettings &settings)
    {
        ToggleTouchscreenForceOn();
    }

    // This function is called when the process exits
    // Useful to save settings, undo patchs or clean up things
    void    OnProcessExit(void)
    {
        ToggleTouchscreenForceOn();
    }

    // ---- 妖怪ウォッチバスターズ 白犬隊/月兎組 (offline only) -------------------
    // Addresses are for Azahar's memory layout (hardware cheat address - 0x501000).
    namespace Yk
    {
        static constexpr u32 OniDama   = 0x0BECAF2C; // u32
        static constexpr u32 Kisho     = 0x0BECAFBC; // u32
        static constexpr u32 Rank      = 0x0BECFD28; // u8
        static constexpr u32 StoryFlag = 0x0BECFBEC; // 5 x u32
        static constexpr u32 BattleItm = 0x0BECD7FC; // 0x12A entries, stride 0x10
        static constexpr u32 ToolItm   = 0x0BECB82C; // 0x1F2 entries, stride 0x10
        static constexpr u32 LvBase    = 0x0BECF45A; // 0x65 entries, stride 0x10

        static bool Ready(u32 addr, u32 size)
        {
            return Process::CheckAddress(addr) && Process::CheckAddress(addr + size - 1);
        }

        static bool Guard(u32 addr, u32 size)
        {
            if (Ready(addr, size)) return true;
            OSD::Notify("アドレスが無効です。セーブをロードしてから使って下さい");
            return false;
        }

        static void SetRank(MenuEntry *entry)
        {
            if (!Guard(Rank, 1)) return;
            u8 cur = 1;
            Process::Read8(Rank, cur);
            u32 v = cur;
            Keyboard kb("ランクを入力 (1-99)");
            kb.IsHexadecimal(false);
            if (kb.Open(v, v) != 0) return;
            if (v < 1) v = 1;
            if (v > 99) v = 99;
            Process::Write8(Rank, (u8)v);
            OSD::Notify("ランクを " + std::to_string(v) + " にしました");
        }

        static void SetU32(u32 addr, const char *label)
        {
            if (!Guard(addr, 4)) return;
            u32 v = 0;
            Process::Read32(addr, v);
            Keyboard kb(std::string(label) + "を入力 (0-999999)");
            kb.IsHexadecimal(false);
            if (kb.Open(v, v) != 0) return;
            if (v > 999999) v = 999999;
            Process::Write32(addr, v);
            OSD::Notify(std::string(label) + "を " + std::to_string(v) + " にしました");
        }

        static void SetOniDama(MenuEntry *) { SetU32(OniDama, "鬼玉"); }
        static void SetKisho(MenuEntry *)   { SetU32(Kisho, "記章"); }

        static void FillItems(u32 base, u32 count, const char *label)
        {
            if (!Guard(base, count * 0x10)) return;
            for (u32 i = 0; i < count; i++)
                Process::Write32(base + i * 0x10, 99);
            OSD::Notify(std::string(label) + "を99個にしました");
        }

        static void BattleItems(MenuEntry *) { FillItems(BattleItm, 0x12A, "バトルアイテム&装備"); }
        static void ToolItems(MenuEntry *)   { FillItems(ToolItm, 0x1F2, "道具&素材"); }

        static void SetLevel(MenuEntry *)
        {
            if (!Guard(LvBase, 0x65 * 0x10)) return;
            u32 lv = 99;
            Keyboard kb("全妖怪のレベルを入力 (1-255)");
            kb.IsHexadecimal(false);
            if (kb.Open(lv, lv) != 0) return;
            if (lv < 1) lv = 1;
            if (lv > 255) lv = 255;
            if (!MessageBox("レベル変更", "全妖怪のレベルを " + std::to_string(lv) + " にします。\nセーブのバックアップは取りましたか？", DialogType::DialogYesNo)())
                return;
            for (u32 i = 0; i < 0x65; i++)
            {
                u32 a = LvBase + i * 0x10;
                Process::Write16(a, (u16)lv);
                Process::Write16(a + 2, 0xA1F4);
            }
            OSD::Notify("全妖怪のレベルを " + std::to_string(lv) + " にしました");
        }

        static void StorySkip(MenuEntry *)
        {
            if (!Guard(StoryFlag, 20)) return;
            if (!MessageBox("ストーリースキップ", "ストーリーの進行フラグを全て立てます。\n元に戻せない可能性があります。\n続けますか？", DialogType::DialogYesNo)())
                return;
            for (u32 i = 0; i < 5; i++)
                Process::Write32(StoryFlag + i * 4, 0xFFFFFFFF);
            OSD::Notify("ストーリースキップを適用しました");
        }
    }

    void    InitMenu(PluginMenu &menu)
    {
        menu += new MenuEntry("ランク変更", nullptr, Yk::SetRank, "A: ランクを数値で指定 (1-99)");
        menu += new MenuEntry("所持鬼玉を変更", nullptr, Yk::SetOniDama, "A: 数値を入力 (最大999999)");
        menu += new MenuEntry("所持記章を変更", nullptr, Yk::SetKisho, "A: 数値を入力 (最大999999)");
        menu += new MenuEntry("装備全入手 (バトルアイテム&装備 x99)", nullptr, Yk::BattleItems, "A: 全ての装備・バトルアイテムを99個に");
        menu += new MenuEntry("道具&素材 x99", nullptr, Yk::ToolItems, "A: 全て99個に");
        menu += new MenuEntry("全妖怪レベル変更 (初期値99)", nullptr, Yk::SetLevel, "A: 全妖怪のLvを数値で指定 (1-255) 要バックアップ");
        menu += new MenuEntry("ストーリースキップ", nullptr, Yk::StorySkip, "A: 進行フラグを全て立てる (要バックアップ)");
    }

    int     main(void)
    {
        PluginMenu *menu = new PluginMenu("妖怪ウォッチバスターズ", 0, 1, 0,
                                            "オフライン用 簡易チートメニュー (Azahar用)\nロードしたセーブで使って下さい");

        // Synnchronize the menu with frame event
        menu->SynchronizeWithFrame(true);

        // Init our menu entries & folders
        InitMenu(*menu);

        // Launch menu and mainloop
        menu->Run();

        delete menu;

        // Exit plugin
        return (0);
    }
}
