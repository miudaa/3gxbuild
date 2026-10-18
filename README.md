# 3gxbuild

CTRPluginFramework の空テンプレート（[PabloMK7/CTRPluginFramework-BlankTemplate](https://github.com/PabloMK7/CTRPluginFramework-BlankTemplate) ベース）を
GitHub Actions でビルドして、Azahar で読める `3GX$0002` 形式の `.3gx` を作るリポジトリ。

## ビルド

1. `git push` するか、Actions タブの「Build 3GX」で `Run workflow` を押す
2. 完了したら、実行結果の Artifacts から `3gxbuild-3gx` をダウンロード（zip の中身が `3gxbuild.3gx`）
3. ビルドの最後に先頭8バイトが `3GX$0002` かを自動チェックする（違うと赤くなる）

## Azahar での使い方

| 置くもの | 場所 |
| --- | --- |
| `3gxbuild.3gx` | `Azahar/sdmc/luma/plugins/<タイトルID>/`（全ゲーム共通なら `Azahar/sdmc/luma/plugins/default.3gx`） |
| チートの txt | `Azahar/sdmc/cheats/<タイトルID>.txt` |

- 設定で「3GXプラグインローダーを有効化」にチェックを入れる
- ゲームを完全に起動し直す（プラグインは起動時にしか読み込まれない）
- 妖怪ウォッチバスターズ 赤猫団 は `000400000016C700`、白犬隊 は `000400000016C600`

## ログの見方

`Azahar/log/` の最新ログに `Service.PLGLDR` の行が出る。
`Outdated or invalid 3GX plugin` は形式が古いとき、読み込みに成功していればエラー行は出ない。
