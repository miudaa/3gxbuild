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
        static constexpr u32 LvBase    = 0x0BECF45A; // 0x65 entries, stride 0x10 (soul levels)
        static constexpr u32 YokaiLv   = 0x0BEBACD5; // u8, 0x1A2 slots, stride 0x88 (owned yokai)

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

        static void SetYokaiLevel(MenuEntry *)
        {
            if (!Guard(YokaiLv, 0x1A1 * 0x88 + 1)) return;
            u32 lv = 99;
            Keyboard kb("全妖怪のレベルを入力 (1-255)");
            kb.IsHexadecimal(false);
            if (kb.Open(lv, lv) != 0) return;
            if (lv < 1) lv = 1;
            if (lv > 255) lv = 255;
            if (!MessageBox("レベル変更", "所持している全妖怪のレベルを " + std::to_string(lv) + " にします。\nセーブのバックアップは取りましたか？", DialogType::DialogYesNo)())
                return;
            for (u32 i = 0; i < 0x1A2; i++)
                Process::Write8(YokaiLv + i * 0x88, (u8)lv);
            OSD::Notify("全妖怪のレベルを " + std::to_string(lv) + " にしました");
        }

        // ---- 大辞典で選んだ妖怪を手持ちの枠に入れる -----------------------------
        static constexpr u32 YokaiSlot = 0x0BEBAC94; // yokai ID (u32), stride 0x88, 400 slots
        static constexpr u32 SlotCount = 400;

        struct Known { const char *name; u32 id; };
        static const Known kKnown[] =
        {
            { "ジバニャン",     0xA3A68AA8 },
            { "ふぶきちゃん",   0x9A4949D5 },
            { "百鬼姫",         0x8D41292C },
            { "ブシニャン",     0x97122416 },
            { "鬼蜘蛛",         0x849948FD },
            { "鬼ガマ",         0xE616A81F },
            { "ツチノコ星人",   0x2E908076 },
            { "エンマ大王",     0x31172F1E },
            { "ブシ王",         0xEBE2C9D1 },
        };

        static std::vector<u32> gCands;
        static bool gHaveCands = false;

        static void ScanHeap(u32 id, std::vector<u32> &out)
        {
            for (u32 page = 0x08000000; page < 0x0C800000; page += 0x1000)
            {
                if (!Process::CheckAddress(page)) continue;
                const u32 *p = (const u32 *)page;
                for (u32 i = 0; i < 0x400; i++)
                    if (p[i] == id) out.push_back(page + i * 4);
            }
        }

        static void FindDictAddr(MenuEntry *)
        {
            std::vector<std::string> opts;
            for (const Known &k : kKnown) opts.push_back(k.name);
            Keyboard kb("大辞典で今カーソルを合わせている妖怪を選んで下さい", opts);
            int ch = kb.Open();
            if (ch < 0) return;
            u32 id = kKnown[ch].id;

            std::vector<u32> res;
            if (!gHaveCands)
                ScanHeap(id, res);
            else
                for (u32 a : gCands)
                    if (Process::CheckAddress(a) && *(const u32 *)a == id) res.push_back(a);

            if (res.empty())
            {
                gCands.clear(); gHaveCands = false;
                OSD::Notify("候補が0件になりました。最初からやり直して下さい");
                return;
            }
            gCands = res; gHaveCands = true;
            if (res.size() == 1)
            {
                char buf[64];
                snprintf(buf, sizeof(buf), "確定: %08X", res[0]);
                OSD::Notify(buf);
            }
            else
                OSD::Notify("候補 " + std::to_string(res.size()) + " 件。別の妖怪にカーソルを動かして、もう一度実行して下さい");
        }

        static void ResetDictAddr(MenuEntry *)
        {
            gCands.clear(); gHaveCands = false;
            OSD::Notify("探索結果をリセットしました");
        }

        static void DictToParty(MenuEntry *)
        {
            if (!gHaveCands || gCands.size() != 1)
            {
                OSD::Notify("先に「大辞典の選択アドレス探索」で1件に絞って下さい");
                return;
            }
            u32 src = gCands[0];
            if (!Process::CheckAddress(src)) { OSD::Notify("アドレスが無効です。探索し直して下さい"); return; }
            u32 id = *(const u32 *)src;
            if (id == 0 || id == 0xFFFFFFFF) { OSD::Notify("大辞典で妖怪を選んでから実行して下さい"); return; }

            u32 slot = 1;
            Keyboard kb("手持ちの何番目の枠に入れますか (1-400)\n※その枠の妖怪は置き換わります");
            kb.IsHexadecimal(false);
            if (kb.Open(slot, slot) != 0) return;
            if (slot < 1 || slot > SlotCount) { OSD::Notify("枠の番号が範囲外です"); return; }
            u32 dst = YokaiSlot + (slot - 1) * 0x88;
            if (!Guard(dst, 4)) return;

            char body[160];
            snprintf(body, sizeof(body), "枠%u の妖怪を、大辞典で選択中の妖怪(ID %08X)に置き換えます。\nセーブのバックアップは取りましたか？", (unsigned)slot, (unsigned)id);
            if (!MessageBox("妖怪変更", body, DialogType::DialogYesNo)()) return;
            Process::Write32(dst, id);
            OSD::Notify("枠" + std::to_string(slot) + " を変更しました。セーブして再ロードして下さい");
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
        menu += new MenuEntry("全妖怪レベル変更 (初期値99)", nullptr, Yk::SetYokaiLevel, "A: 所持している全妖怪のLvを数値で指定 (1-255) 要バックアップ");
        menu += new MenuEntry("全魂レベル変更 (初期値99)", nullptr, Yk::SetLevel, "A: 全ての魂のLvを数値で指定 (1-255) 要バックアップ");
        menu += new MenuEntry("妖怪変更1: 大辞典の選択アドレス探索", nullptr, Yk::FindDictAddr, "大辞典で妖怪にカーソルを合わせて実行。候補が1件になるまで、別の妖怪で繰り返す");
        menu += new MenuEntry("妖怪変更2: 大辞典の妖怪を手持ちの枠に入れる", nullptr, Yk::DictToParty, "大辞典で選択中の妖怪を、指定した枠に入れる (要バックアップ)");
        menu += new MenuEntry("妖怪変更: 探索結果をリセット", nullptr, Yk::ResetDictAddr, "探索をやり直すとき");
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
