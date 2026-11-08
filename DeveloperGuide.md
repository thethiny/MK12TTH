# Developer Guide

How to add a feature to MK12TTH: a hook found by pattern, its switch in the ini and in Settings, its live rows in MODS and the pause menu, and its place in the overlay. For the patching API itself (proxying calls, resolving addresses), see the [Memory Patch Utility Guide](MemoryPatchUtilityGuide.md).

---

## How it fits together

```
MK12TTH.ini ──► eSettingsManager (patterns, startup switches, log levels)
                    │
FeatureList.cpp ──► Features registry ──► InstallAll at startup, in dependency order
                    │                        └─ each feature's Install() finds its patterns and places its hooks
                    │
                    ├──► ModSettings: Settings > MK12TTH (startup switches, saved to MK12TTH.ini)
                    │               MODS (live rows, saved to MK12TTH_State.ini)
                    │               pause menu MOD SETTINGS (rows marked bInGame)
                    │               overlay (draws the same pages as MODS)
                    │
mk12hook.cpp ──► proxies: the game calls them, they ask Features::IsActive(...) and the feature's module
```

Every feature ends startup in exactly one state: **Installed**, **Disabled** (off in the ini), **Failed** (its patterns weren't found) or **Blocked** (something it needs isn't installed). Menus show the reason to the player.

Code asks two questions:

- `Features::IsInstalled(Id)`: the hooks are in.
- `Features::IsActive(Id)`: installed and switched on (for Live features, the live switch). Proxies check this and pass straight through to the game when it's false.

## Step 1: find the pattern

Rules for every pattern:

- Unique in **both** the Steam and the Epic executable.
- Wildcard (`?`) every rip-relative displacement, call target and struct offset that a game update could move.
- Never span into the next function.
- Prefer a call site over a function start: it survives more updates and only affects that one caller.
- Avoid the game's protected (virtualized) code.

Results are cached in `mk12tth.cache` per game build, keyed by the pattern text, so changing a pattern rescans it.

## Step 2: add the pattern key

1. `source/src/eSettingsManager.h`: add `std::string pMyThing;`.
2. `source/src/eSettingsManager.cpp`: read it under the right section:
   ```cpp
   pMyThing = ini.ReadString("Patterns.Floyd", "pMyThing", "");
   ```
3. `source/sample.ini`: add `pMyThing = 48 8B ? ? E8 ? ? ? ?` under the same section. The sample ini ships as `MK12TTH.ini` in releases.

## Step 3: declare the game function

In `source/src/mk12.h`, add a typedef and an extern, and define the pointer in `mk12.cpp`:

```cpp
// mk12.h
typedef bool (__fastcall MyThingType)(void* Self, int32_t Value); // What it does, in plain words
extern MyThingType* MyThing;

// mk12.cpp
MK12::MyThingType* MK12::MyThing = nullptr;
```

## Step 4: write the proxy and the install function

In `source/src/mk12hook.h/.cpp`:

```cpp
// MK12Hook::Proxies
bool __fastcall MyThingProxy(void* Self, int32_t Value)
{ // When the game does X: our module decides Y
	if (!Features::IsActive(Features::Id::MyFeature))
		return MK12::MyThing(Self, Value);
	return MK12::MyThing(Self, MyFeature::Adjust(Value));
}

// MK12Hook::Hooks
bool MyFeatureHooks()
{ // Hooks the call where the game does X
	printf("\n==MyFeatureHooks==\n");
	if (!GamePatcher->ProxyByPattern(SettingsMgr->pMyThing, "MyThing", 4,
		MK12Hook::Proxies::MyThingProxy, &MK12::MyThing, PATCH_CALL))
		return false; // The feature ends up Failed, menus say so
	printfSuccess("My Feature On");
	return true;
}
```

- `ProxyByPattern(pattern, name, offset, proxy, &original, PATCH_CALL)` patches the call at `pattern + offset` and gives you the original target.
- `ResolvePattern(pattern, name)` returns the address only; use `ProcessPatch::ResolveDestination` to follow a call or a rip-relative load.
- `ProxyFunctionAt(address, proxy, &original, name)` hooks a function's start (all callers).
- Keep the feature's logic in its own module (`MyFeature.cpp/.h`), not in the proxy.

## Step 5: register the feature

1. `source/src/Features.h`: add `MyFeature` to `enum class Id` (before `Count`).
2. `source/src/FeatureList.cpp`: define it after everything it needs:
   ```cpp
   Define({ Id::MyFeature, "bMyFeature", true, Kind::Live, { Id::GameState }, MyFeatureHooks,
   	L"MY FEATURE", L"What it does, written for the player.", false });
   ```
   The fields, in order:

   | Field | Meaning |
   |---|---|
   | `Which` | Its id |
   | `IniKey` | Its startup switch in `MK12TTH.ini`; also names its live switch in the state file |
   | `bIniDefault` | On or off when the ini key is missing |
   | `Type` | `Kind::Hidden` (ini only), `Kind::Startup` (row in Settings, applies after a restart) or `Kind::Live` (also a live switch in MODS) |
   | `Needs` | Features that must be installed first, otherwise it's Blocked |
   | `Install` | The install function from step 4, `nullptr` if it has nothing to hook |
   | `Title`, `Description` | Its rows in the menus |
   | `bLiveDefault` | Live features: the live switch when the state file has none |
   | `IniSection` | Defaults to `Patches`; `nullptr` means no startup switch at all |

   Optional fields after `Define`: `OnSwitch` (runs at install and on each live change, with the new state) and `Modes` (several startup choices instead of OFF / ON, like FAST BOOT):
   ```cpp
   Definition MyFeature = { Id::MyFeature, "szMyFeature", true, Kind::Startup, {}, MyFeatureHooks, L"MY FEATURE", L"" };
   MyFeature.Modes = { { L"OFF", "Off" }, { L"SOME", "Some" }, { L"ALL", "All" } };
   MyFeature.Description = L"What it does.\nOFF: ...\nSOME: ...\nALL: ...";
   Define(MyFeature);
   ```
   Read the picked mode with `Features::StartupMode(Id::MyFeature)` (0 is off).
3. `source/sample.ini`: add `bMyFeature = true` under `[Patches]`, in the matching group, with a one-line comment.

## Step 6: the Settings row (startup switch)

In `ModSettings.cpp`, `AddSettingsRows()`, add the row to the right page:

```cpp
AddFeatureSetting(Id::MyFeature);
```

The row is built from the definition: title, description, OFF / ON (or its modes), the reason when it isn't running, and the restart question when it changes. It saves to `MK12TTH.ini`.

## Step 7: rows in MODS (live)

In `AddMenuRows()`, add rows to a page (`LivePage`, `LoggingPage`, `AccountPage`, `TokensPage`, `FloydPage`, `AboutPage`):

| You want | Use |
|---|---|
| The feature's own live switch | `AddFeatureRow(LivePage, Id::MyFeature);` |
| Another on/off switch | `AddLiveRow(page, Title, Description, Get, Set, bAvailable);` |
| A list of choices | a `Row` with `Choices`, `LiveGet`, `LiveSet` and `bNeedsRestart = false`, added with `AddMenuRow(page, std::move(row))` |
| A read-only value | `AddInfoRow(page, Title, [](Row& row) { SetInfo(row, Label, Text); });` (refreshed each time a screen opens) |
| A button | `AddActionRow(page, Title, Description, [] { ... });` |
| A header | `AddCategory(page, Title);` |

Extras on any returned row:

- `.bInGame = true`: also shown in the fight pause menu's **MOD SETTINGS**.
- `.CopyValue = [] { return std::wstring(...); };`: shows **COPY** in the footer and copies the full value.

Live values the player picks are saved in `MK12TTH_State.ini`, section `[Live]` (`StateFile::LIVE`). Feature live switches are saved for you; your own choices save themselves in their `Set`/`LiveSet`, like `Announcer::Pick`.

Description text supports `\n` line breaks and the `MK_BOLD(...)` / `MK_RED(...)` markup. When a row offers choices, put each choice on its own line.

## Step 8: the pause menu and the overlay

- **Pause menu:** rows with `.bInGame = true` show there automatically. Logging rows only show while DEBUG LOGS is on.
- **Overlay (MK1Hook's ImGui tab):** draws every MODS page automatically, with live rows as dropdowns, buttons as buttons and info rows as text. Its changes run on the game thread through the same `LiveSet`/`Action`. The Features section lists each feature's state. You only touch `dllmain.cpp` (`TabFunction`) for something MODS can't show.

## Step 9: logging

- `printfInfo`, `printfSuccess`, `printfWarning`, `printfError` for lines every player should see (keep them few).
- `if (SettingsMgr->ShouldLog(Log::Debug)) printf(...)` for testing output.
- `SettingsMgr->ShouldLogFloyd()` / `ShouldLogProfile()` for those areas' dev logs.
- Never print tokens, tickets or keys below `Log::Verbose`.

## Rules of the codebase

- **Ask the game.** Take values from the game's own functions or data, never recalculate what the game computes.
- **Comments say what the code does,** in plain words. No addresses, `sub_` names, disassembler or tool references, or local paths. Private reverse-engineering notes stay out of the repo.
- **Player-facing text** says what the player sees and can do, not how it works inside: no internal ids, thresholds or which check fired.
- **Threads:** proxies run on whatever thread the game calls them from. Menu rows, `LiveSet` and `Action` run on the game thread. Guard anything shared across threads.
- **Fail soft:** a missing pattern logs a warning and leaves the feature Failed. Reads of game memory that could be stale go in `__try`/`__except`.

## Checklist

- [ ] Pattern unique on Steam and Epic, displacements wildcarded
- [ ] `eSettingsManager` key, `sample.ini` pattern line
- [ ] Typedef + pointer in `mk12.h/.cpp`
- [ ] Proxy checks `Features::IsActive` and passes through
- [ ] Install function returns false on a missing pattern
- [ ] `Features::Id` entry and `Define` in `FeatureList.cpp`, `sample.ini` switch
- [ ] `AddFeatureSetting` in Settings, rows in MODS, `bInGame` where it helps in fights
- [ ] Builds clean: `msbuild ASIMK12.vcxproj /p:Configuration=Release /p:Platform=x64`
