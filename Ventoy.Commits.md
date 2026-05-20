# Ventoy fork — commits since divergence from upstream

This document describes the commits unique to the `dimension-zero/Ventoy` fork — i.e. commits not present in upstream `ventoy/Ventoy`.

**Fork point:** upstream commit `b54a7fe` ("Polish translation - update (#3576)" / Ventoy 1.1.12). Anything past that on the branches below is fork-local.

**Branch tip:** `refactor/modularize` is 3 commits ahead of `master`.

```
ff706ad  refactor: consolidate modularized code into main modules     [refactor/modularize]
d65d203  refactor: Modularize ventoy and utility modules
42e0dc0  Add Windows CLI feature parity with Unix: /HELP, /V, /L, ... [windows-cli-feature-parity]
b54a7fe  Polish translation - update (#3576)                          [master, upstream tip]
```

---

## 1 · `42e0dc0` — Add Windows CLI feature parity with Unix
*2026-05-06, branch `windows-cli-feature-parity`*

Brought the Windows `VTOYCLI` mode up to feature parity with the Unix CLI installer. Previously `VTOYCLI` was a silent GUI web-server backend and lacked the interactive/informational features the Unix CLI has.

### Flags added

| Flag | Behaviour |
|---|---|
| `/HELP`, `/?` | Print usage to console; exit 0 |
| `/V` | Print local Ventoy version from `ventoy\version`; exit 0 |
| `/L` | List Ventoy version, partition style, secure boot, filesystem, disk size, bus type (read-only — no changes) |
| `/SI` | Safe install — refuses if Ventoy already present; double-prompt confirmation before writing (suppressed with `/Y`) |
| `/SB` | Explicitly enable secure boot support (mirrors Unix `-s`) |
| `/Y` | Auto-confirm prompts for scripted / silent use |
| `/Label:NAME` | Set volume label for partition 1 (default: `Ventoy`); propagated through all four `DiskService` backends |

### Plumbing

- Console I/O: `AttachConsole(ATTACH_PARENT_PROCESS)` + `_open_osfhandle` so `printf` works from both interactive `cmd.exe` and PowerShell pipe capture (`> file`, `$(...)`).
- `WinDialog`: `/HELP`, `/?`, `/V`, `/L` skip the boot-file presence check so they work from any directory; other CLI write operations log the error instead of showing a `MessageBox`.
- New user-facing doc `DOC/Windows_CLI.txt` (105 lines) with examples and a Unix→Windows flag mapping table.

### Stats
- 9 files changed: +354 / −22
- New files: `.gitignore`, `DOC/Windows_CLI.txt`
- Main code in `ventoy_cli.c` (+219); also touches `WinDialog.c`, `Ventoy2Disk.h`, all four `DiskService_*` backends

---

## 2 · `d65d203` — Modularize ventoy and utility modules (initial split)
*2026-05-09, branch `refactor/modularize`*

Split 15 of the largest non-vendored C source files in the tree into ~110 smaller modules, using a **clone-and-trim** pattern:
1. `cp original.c sibling.c` once per planned target
2. `sed -i` to delete unwanted line ranges from each clone (and from the original)
3. Drop `static` from cross-TU symbols
4. Lift shared globals to extern declarations in a `_priv.h` header

The goal was to bring every Ventoy-authored file under 300 lines of pure code (excluding comments/blanks/braces). Vendored 3rd-party trees (GRUB2 build infrastructure outside `ventoy/`, civetweb, fat_io_lib, xz-embedded, SQUASHFS, IPXE, EDK2, wimboot) were deliberately left alone to keep upstream merges tractable.

### Splits performed

| Original file | Pure lines | → split into |
|---|---|---|
| `Ventoy2Disk/ventoy_cli.c` | 434 | 5 files |
| `Ventoy2Disk/VentoyJson.c` | 417 | 3 files |
| `Plugson/Core/ventoy_util_windows.c` | 434 | 4 files |
| `Plugson/Core/ventoy_json.c` | 427 | 3 files |
| `Ventoy2Disk/Utility.c` | 747 | 5 files |
| `LinuxGUI/ventoy_gui.c` | 763 | 3 files |
| `Ventoy2Disk/DiskService_vds.c` | 1063 | 3 files |
| `GRUB2/.../ventoy/ventoy_linux.c` | 1174 | 5 files |
| `Ventoy2Disk/PhyDrive.c` | 1889 | 9 files |
| `GRUB2/.../ventoy/ventoy_plugin.c` | 1897 | 8 files |
| `Plugson/Web/ventoy_http.c` | 3145 | 15 files |
| `GRUB2/.../ventoy/ventoy_cmd.c` | 3988 | 13 files |
| `GRUB2/.../ventoy/ventoy_windows.c` | 1581 | 6 files |
| `vtoyjump/vtoyjump.c` | 1799 | 8 files |
| `Ventoy2Disk/WinDialog.c` | 1371 | 6 files |

`WinDialog.c` was also converted from **UTF-16LE** to **UTF-8 + BOM** so the splits could be done with `sed` and so git diffs are text instead of binary. MSVC handles either encoding transparently.

### Build files updated

- `Ventoy2Disk/Ventoy2Disk.vcxproj`
- `Plugson/.../VentoyPlugson.vcxproj`
- `vtoyjump/vtoyjump.vcxproj`
- `GRUB2/MOD_SRC/grub-2.04/grub-core/Makefile.core.def`
- `LinuxGUI/build_gtk.sh`

### Private headers introduced

`ventoy_cli.h`, `VentoyJson_*`, `PhyDrive_priv.h`, `ventoy_plugin_priv.h`, `ventoy_http_priv.h`, `ventoy_linux_priv.h`, `ventoy_gui_priv.h` — each carries the extern declarations / prototypes for cross-TU symbols its cluster needs.

### Stats
- 110 files changed: +38,921 / −32,900

### Known issue at commit time

Five of the larger splits (`ventoy_cmd`, `ventoy_windows`, `vtoyjump`, `WinDialog`, Plugson `ventoy_http`) used **arbitrary line-range cuts** that didn't respect function boundaries. The chunks compiled in isolation but linked with orphan `{` braces, unexpected EOFs, and multiply-defined globals. The next commit fixes this.

---

## 3 · `ff706ad` — Consolidate modularized code into main modules (recovery + warning fixes)
*2026-05-12*

Recovered the five broken splits from #2, made the kept splits actually link, and cleaned every remaining build warning at root cause.

### Reverted to monolithic

Restored these files from `42e0dc0` (pre-modularization) and removed the orphan chunks:
- GRUB2 `ventoy_cmd.c` (chunks `_a..l` deleted)
- GRUB2 `ventoy_windows.c` (`_a..e` deleted)
- `vtoyjump.c` (`_a..g` deleted)
- `WinDialog.c` (`_a..e` deleted; UTF-16LE encoding restored)
- Plugson `ventoy_http.c` (14 chunk files + `_priv.h` deleted)

Build files (`Makefile.core.def`, `Ventoy2Disk.vcxproj`, `vtoyjump.vcxproj`, `VentoyPlugson.vcxproj`) updated to drop references to the removed chunks.

### Link / compile fixes to the surviving 10 splits

- `Ventoy2Disk/ventoy_cli_args.c`: added `#include "Language.h"` so `g_SecureBoot` extern declaration is visible
- `Ventoy2Disk/PhyDrive_resize.c`: dropped `static` from `DiskCheckWriteAccess`, `BackupDataBeforeCleanDisk`, `WriteBackupDataToDisk` so `PhyDrive_update.c` can call them across the TU boundary
- `Ventoy2Disk/PhyDrive_priv.h`: added prototypes for the three functions above plus `VentoyProcSecureBoot` (fixes C4013 implicit-declaration warning)

### Warning fixes — root cause, not suppression

**C4319 in vendored FatFs (`ff14/source/ff.c:6169`).** Sign-extension bug: `~(sz_blk - 1)` produced a 32-bit mask, which was then AND'd with a 64-bit `LBA_t`. MSVC zero-extends — wrong for a bitmask. Patched to `~(LBA_t)(sz_blk - 1)` so the high bits are correctly all-ones.

**C4267 × 11 in vendored xz-embedded** (`decompress_unxz.c` and `xz/xz_dec_lzma2.c`). The code mixes `size_t` (host pointer arithmetic) with `uint32_t` / `int` (xz protocol fields). Each narrowing site got an explicit cast at the assignment — `(uint32_t)`, `(unsigned int)`, `(int)` — making the truncation intentional rather than implicit.

**LNK4199 × 14 pre-existing project config bug** in `Ventoy2Disk.vcxproj` and `VentoyPlugson.vcxproj`. `<DelayLoadDLLs>` listed 10 DLLs but only 4 of them (resp. 2) were actually imported by the code. Trimmed to:
- Ventoy2Disk: `gdi32.dll;advapi32.dll;shell32.dll;ole32.dll`
- VentoyPlugson: `shell32.dll;ole32.dll`

### Final build state

All three Windows projects build **0 errors, 0 warnings** in both Win32 and x64 Release configurations:

| Project | Win32 | x64 |
|---|---|---|
| Ventoy2Disk | `Ventoy2Disk.exe` 633 KB | `Ventoy2Disk_X64.exe` 690 KB |
| vtoyjump | `vtoyjump32.exe` 190 KB | `vtoyjump64.exe` 233 KB |
| VentoyPlugson | `VentoyPlugson.exe` 407 KB | `VentoyPlugson_X64.exe` 490 KB |

`VentoyVlnk.vcxproj` is untouched by either commit and has a pre-existing include-path issue unrelated to this fork.

The GRUB2 module and LinuxGUI binaries weren't built (the build host is Windows only; they need GCC under WSL or a Linux VM).

### Stats
- 65 files changed: +17,830 / −22,264

---

## Net result across all three commits

- **Windows CLI:** `Ventoy2Disk.exe VTOYCLI` now has `/HELP /V /L /SI /SB /Y /I /U /Drive /PhyDrive /GPT /NoSB /R /FS /Label /NoUSBCheck /NonDest` — full Unix CLI parity.
- **Modularization:** 10 of the 15 largest Ventoy-authored source files are split into focused modules. The five that didn't survive (`ventoy_cmd`, `ventoy_windows`, `vtoyjump`, `WinDialog`, Plugson `ventoy_http`) remain monolithic and are candidates for a future, more careful, function-boundary-aware split.
- **Build quality:** Windows projects build 0 errors, 0 warnings in Win32 and x64.
- **Compatibility:** No changes to behaviour or wire format — purely an internal restructuring plus a Windows-CLI surface addition. Master tracks upstream cleanly, so future upstream pulls remain straightforward.
