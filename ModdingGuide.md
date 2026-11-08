# Modding Guide

How to install, make and package mods that MK12TTH loads, and how to give them settings and descriptions players can read in game.

---

## Where mods go

Put mods in `MK1Folder/MK12/Content/Mods/`. Create the `Mods` folder if it isn't there.

```
Content/
├── Paks/                       ← the game's own files (old mods here still work, see below)
└── Mods/
    ├── MyCoolSkin/             ← one folder = one mod
    │   ├── MyCoolSkin.pak
    │   ├── MyCoolSkin.utoc
    │   ├── MyCoolSkin.ucas
    │   └── description.txt
    ├── BigAudioPack/
    │   ├── description.txt
    │   ├── Voices/             ← a folder with several paks gets its own switch
    │   │   ├── description.txt
    │   │   ├── Voices_A.pak
    │   │   └── Voices_B.pak
    │   └── Music.pak
    └── QuickFix.pak            ← a pak right in Mods is a mod of its own
```

- Every folder directly in `Content/Mods` is one mod. It can hold any number of paks, in any subfolders.
- A pak placed directly in `Content/Mods` is a mod on its own.
- A pak can be a `.pak`, a `.utoc` + `.ucas` pair, or all three with the same name. A `.utoc`/`.ucas` pair without a `.pak` loads too.
- No signature files (`.sig`) are needed.

## Turning mods on and off

Open **MODS → PAK MODS** from the main menu.

- Every mod has an on/off switch. A mod with several paks also gets a switch per pak, and per folder that holds more than one pak. A pak only loads when it, its folders and its mod are all on.
- Changes apply after a restart: the game can't unload a mod while it runs. MK12TTH offers to restart for you.
- Your choices are saved, and a new mod is on until you turn it off.
- **Turn mods off before playing online.**

Each mod's description in the menu also shows:

- which files it has, and which other mods it overrides or is overridden by;
- **"Won't load"** when another mod replaces all of its files;
- **"Probably won't load"** when its game assets are only in a `.pak`. This game loads assets (`.uasset`, `.uexp`, `.ubulk`, `.umap`) from `.utoc`/`.ucas`, so ship them that way.

## Load order

- All mods in `Content/Mods` load over the game's own files.
- When two mods change the same file, the mod whose name sorts **last** wins (`Zeta` wins over `Alpha`).
- To make a mod win regardless of its name, end its folder (or file) name with `_P` and a number, like `MyCoolSkin_P10`. A higher number wins, and any `_P<number>` mod wins over every mod without one. A plain `_P` with no number (the engine's patch naming) changes nothing.
- Files in a `.pak` and files in a `.utoc` are looked up separately, so a `.pak`-only mod and a `.utoc` mod never fight over the same file.

## Mods in Content/Paks

Mods left in `Content/Paks` still load, the way the game loads its own files: by path, with the same priority as the game's files. A mod there that sorts before some of the game's own paks can lose to them, and the menu says so.

**MOVE MODS TO MODS FOLDER** in **MODS → PAK MODS → CONTENT\PAKS** moves them all into `Content/Mods`, keeping their folders. Prefer `Content/Mods` for anything new.

## Descriptions (description.txt)

A `description.txt` is shown with your mod in **MODS → PAK MODS**. It's plain text, UTF-8 or UTF-16. Only the first 1500 characters are shown.

Where to put it:

| Your mod | Where its description goes |
|---|---|
| A single pak (alone in its folder, or right in `Content/Mods`) | Inside the pak, as `description.txt` at its root |
| A folder with several paks | `description.txt` in the mod's folder |
| A subfolder with several paks | `description.txt` in that subfolder |

A `description.txt` inside a pak goes in the `.pak` (loose files aren't stored in `.utoc`/`.ucas`). If a pak holds several, the one closest to its root is used.

## Engine settings (Engine.ini)

Some mods need engine settings, for example a console variable or a renderer switch. MK12TTH applies them for you while the mod is on, and never writes them to the player's own config. Turning the mod off simply stops applying them.

There are two ways to ship them:

1. **A loose `Engine.ini` anywhere inside your mod's folder** in `Content/Mods`:
   ```ini
   [SystemSettings]
   r.SomeSetting=1

   [/Script/Engine.RendererSettings]
   r.OtherSetting=0
   ```
2. **Inside your pak**, at `MK12/Config/Mods/<ModName>.ini`, where `<ModName>` is your mod's folder name (or the pak's file name for a single-file mod). Same format. This also works for mods in `Content/Paks`.

Rules:

- Only keys whose value differs from the game's own are applied.
- Only plain `Key=Value` lines inside a `[Section]` are applied. Lines starting with `+`, `-`, `.` or `!`, and any key that appears more than once in the file (a list, like `Paths=`), are left alone.
- A loose `Engine.ini` only counts in a mod folder that has paks.
- When two mods set the same key, the mod that wins the load order wins.
- `Engine.ini` files anywhere else (directly in `Content/Mods`, in `Content/Paks`) are reported in the console and ignored.
- The settings are applied as the engine starts, so they take effect after a restart, like the mod itself.

## String swaps (string_swaps.txt)

`string_swaps.txt` next to MK12TTH (in `MK12/Binaries/Win64/`) swaps one game name for another, for example to make one skin load another skin's files. One swap per line:

```
/Game/Path/To/Original.Original_C -> /Game/Path/To/Replacement.Replacement_C
```

Swaps are read at startup. Turn them on or off live in **MODS → LIVE FEATURES → STRING SWAP**.

## Packaging checklist

- [ ] One folder per mod, named the way you want it shown (and sorted).
- [ ] Assets in `.utoc`/`.ucas`, loose files (`description.txt`) in the `.pak`.
- [ ] A `description.txt` that says what the mod changes.
- [ ] Engine settings in a loose `Engine.ini` or in `MK12/Config/Mods/<ModName>.ini`.
- [ ] `_P<number>` only if your mod must win over others.
- [ ] Tell players to unzip into `Content/Mods` and restart.
