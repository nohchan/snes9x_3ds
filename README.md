# Snes9x for 3DS

ここは、matbo87さんが作成してくれた「snes9x_3ds」を自分用にカスタマイズする場所です。


## 現在対応したこと
* タイトルロゴを１５００個まで対応させた
* 私用と分かるようにバージョン表記に「＋」をつけた
* ゲーム選択画面を日本語ファイルでも表示出来るようになった

## アセット(画像およびチート)
* 日本語ファイル対応アセットは専用のアセットリポジトリである[snes9x_3ds-assets](https://github.com/nohchan/snes9x_3ds-assets)に提供されています。

## Screenshots

<table>
  <tr>
    <td width="50%" align="center"><img src="screenshots/dark-mode-file-menu.png" alt="Start screen" valign="bottom"></td>
    <td width="50%" align="center"><img src="screenshots/retroarch-pause-screen.png" alt="Super Mario World" valign="bottom"></td>
  </tr>
  <tr>
    <td valign="top" width="50%" align="center">日本語ファイル名表示, Title enabled</td>
    <td valign="top" width="50%" align="center">Pause screen, per-game overlay enabled</td>
  </tr>
  <tr><td colspan="2"></td></tr>
  <tr></tr>
  <tr>
    <td width="50%" align="center"><img src="screenshots/aladdin-pp-cheats.png" alt="Aladdin" valign="bottom"></td>
    <td width="50%" align="center"><img src="screenshots/dkc-hotkeys.png" alt="Donkey Kong Country" valign="bottom"></td>
  </tr>
  <tr>
    <td valign="top" width="50%" align="center">Cropped top & bottom, cheats enabled</td>
    <td valign="top" width="50%" align="center">Applied hotkeys</td>
  </tr>
  <tr><td colspan="2"></td></tr>
  <tr></tr>
  <tr>
    <td width="50%" align="center"><img src="screenshots/sf2-cropped-border-cover.png" alt="Super Street Fighter II" valign="bottom"></td>
    <td width="50%" align="center"><img src="screenshots/issd-screen-swap.png" alt="International Superstar Soccer Deluxe" valign="bottom"></td>
  </tr>
  <tr>
    <td valign="top" width="50%" align="center">Crop & overscan, scanlines enabled</td>
    <td valign="top" width="50%" align="center">Swapped screen</td>
 </tr>
 <tr>
    <td width="50%" align="center"><img src="screenshots/tg2-hdma.png" alt="Top Gear II" valign="bottom"></td>
    <td width="50%" align="center"><img src="screenshots/savestate-preview-bsx.png" alt="Excitebike - Bunbun Mario Battle" valign="bottom"></td>
  </tr>
  <tr>
    <td valign="top" width="50%" align="center">In-Frame Palette Changes enabled</td>
    <td valign="top" width="50%" align="center">BS-X game, savestate preview</td>
 </tr>
 </table>
 <br>


## Credits

* The Snes9x team for the SNES emulator core, and the libretro Snes9x core maintainers for ongoing reference work
* bubble2k, original author of [snes9x_3ds](https://github.com/bubble2k16/snes9x_3ds), for creating the excellent base this fork builds on
* matbo87, [snes9x_3ds](https://github.com/matbo87/snes9x_3ds), for creating the excellent base this fork builds on
* Wyatt-James for the SuperFX optimizations and fixes ([#85](https://github.com/matbo87/snes9x_3ds/pull/85)), and for the safety, audio and stability fixes adapted from the [snes9x_3ds fork](https://github.com/Wyatt-James/snes9x_3ds)
* ramzinouri's [snes9x_3ds fork](https://github.com/ramzinouri/snes9x_3ds) inspired the image border/background and theme support
* willjow's [snes9x_3ds fork](https://github.com/willjow/snes9x_3ds) revived the project after development had gone quiet
* [RetroAchievements](https://retroachievements.org) for the achievement service, the rcheevos library and the community-made achievement sets
* The Citra/Azahar teams for making 3DS emulator testing and debugging practical
* Everyone reporting issues, testing games and suggesting improvements
