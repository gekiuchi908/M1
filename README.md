# M1: Arcade Video and Pinball Sound Emulator (Modern Build & Fixes)

This is a fork/modified version of **M1** (arcade video and pinball sound emulator by R. Belmont), updated for compatibility with modern compilers and platforms.

---

## Changes & Fixes

- **Compiler Compatibility & Bug Fixes:**  
  Fixed several bugs and compile issues identified with the assistance of **agy (Google Antigravity CLI)**, ensuring clean builds on modern GCC toolchains.
- **Build System Updates:**  
  Updated build configurations for Linux and synced flags/changes across Makefiles.

---

## Build & Test Environment

### Verified
- **Compiler:** GCC / G++ 15.3.0
- **OS:** Linux Mint 22.3 (Cinnamon)

```bash
# Build on Linux
make
```
```markdown
Windows Status (Unverified)
Relevant build changes and updates have been reflected in the Windows Makefile (makefile.dll).
Note: The Windows build has not been compiled or tested in this release. Pull requests or test feedback are welcome!

Credits & Acknowledgments
Original Author: R. Belmont (M1 project)
Assistance: agy (Google Antigravity CLI)



# M1: Arcade Video and Pinball Sound Emulator (修正版)

R. Belmont 氏によるアーケード／ピンボール音源エミュレータ「**M1**」を、現代の開発環境向けにビルド可能にし、不具合を修正したフォーク版です。

---

## 主な変更点

- **不具合修正 & 現代的コンパイラへの適合:**  
  agy (Google Antigravity CLI) の支援を得て特定された各種不具合や警告・エラーを修正しました。
- **Makefile の更新:**  
  最新の GCC 環境に合わせてビルドスクリプトを調整しています。

---

## 動作確認・ビルド環境

- **コンパイラ:** GCC / G++ 15.3.0
- **テスト環境:** Linux Mint 22.3

### ビルド手順 (Linux)

```bash
make
```
```markdown
Windows 版 (makefile.dll) について
Linux 側で加えた変更・反映事項は Windows 用の makefile.dll にも適用済みです。
注意: Windows 環境でのビルド確認および実機テストは行っておりません（未テスト状態です）。動作報告や PR を歓迎します。

クレジット・謝辞
オリジナル版開発者: R. Belmont 氏
解析・修正支援: agy (Google Antigravity CLI)
