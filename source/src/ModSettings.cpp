#include "mkutils.h"
#include "ModSettings.h"
#include "GameMessageBox.h"
#include "PakMods.h"
#include "StateFile.h"
#include "Features.h"
#include "FloydTowerAlert.h"
#include "Announcer.h"
#include "FloydFight.h"
#include "EngineConfig.h"
#include "AttractMode.h"
#include "FloydTower.h"
#include "OnlineIds.h"
#include "GameState.h"
#include "GameToast.h"
#include "Changelog.h"
#include "mk12.h"
#include "mk12hook.h"
#include "mk12_enums.h"
#include "eSettingsManager.h"
#include "../IniReader.h"
#include <algorithm>
#include <shellapi.h>
#include <cwchar>
#include <cwctype>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace ModSettings {
	constexpr int32_t	FIRST_ROW_ID		= 0x7000; // Far from the game's own ids
	constexpr ULONGLONG	RESTART_ASK_DELAY	= 1500; // Without the screen hook: waits for the player to stop changing values
	constexpr const wchar_t* LIST_TEMPLATE_KEY		= L"RoundTimeTitle"; // Game rows we copy, both on the Gameplay page
	constexpr const wchar_t* CATEGORY_TEMPLATE_KEY	= L"FightSettingsCategory";
	constexpr const wchar_t* LINK_TEMPLATE_KEYS[]	= { L"AdjustVideoSettings", L"GraphicsAutoconfig" }; // Buttons, on the graphics pages
	constexpr const wchar_t* STEAM_RELAUNCH_URL		= L"steam://rungameid/1971870";
	constexpr const wchar_t* RESTART_SCRIPT_NAME	= L"MK12TTH_Restart.ps1"; // In the user's temp folder

	// Text table of our rows. The screen looks titles and descriptions up in it, we answer those lookups
	static const wchar_t TEXT_TABLE[] = L"MK12TTH";
	constexpr const wchar_t* DESCRIPTION_SUFFIX	= L"_desc"; // The screen adds it to a row's title for its description
	constexpr const wchar_t* PAGE_DESC_KEY		= L"MK12TTH_PageDesc";

	// Markup the game's own descriptions use (Flash style text)
	#define MK_BOLD(Text)	L"<FONT FACE=\"$BoldText\">" Text L"</font>"
	#define MK_RED_COLOR	L"#6d0a0a"
	#define MK_GREEN_COLOR	L"#1d5a1d"
	#define MK_RED(Text)	L"<FONT COLOR='" MK_RED_COLOR L"'>" Text L"</font>"
	#define MK_NEEDS(Text)	L"\n\n" MK_BOLD(L"Needs: ") Text
	#define MK_TURNS_ON(Text)	L"\n\n" MK_BOLD(L"Turns on: ") Text

	constexpr const wchar_t* PAGE_DESC = L"Settings for the MK12TTH mod, saved to MK12TTH.ini.\n\n" MK_BOLD(L"Most changes apply after a restart.");

	struct Choice {
		const wchar_t*	Label;
		const char*		IniValue;
		const wchar_t*	Description = L""; // Shown under the value while it's picked
	};

	struct Row {
		const wchar_t*		Title;
		const wchar_t*		Description = nullptr;
		const char*			Section = nullptr; // Null for category rows
		const char*			Key = nullptr;
		std::vector<Choice>	Choices;
		int32_t				Default = 0; // Index into Choices, matches sample.ini
		bool				bNeedsRestart = true;

		int32_t				Value = 0; // Index into Choices
		int32_t				StartValue = 0; // What the game is running with
		const char*			IniFile = ""; // Empty is the mod's own ini

		// Live rows: the game runs with the value right away, nothing is saved (no Section)
		std::function<int32_t()>		LiveGet;
		std::function<void(int32_t)>	LiveSet;

		// Info rows: read only, one value. Refresh fills InfoLabel and InfoText each time the screen opens
		std::function<void(Row&)>		Refresh;
		wchar_t							InfoLabel[40] = {};
		std::wstring					InfoText;

		// Any row: rebuilds the description (InfoText) each time the screen opens
		std::function<void(Row&)>		Describe;

		// Action rows: no value, Confirm runs it (the game's link rows, like its REMAP KONTROLS)
		std::function<void()>			Action;

		// Rows with a value to copy: COPY in the footer puts it on the clipboard, empty while there's none
		std::function<std::wstring()>	CopyValue;

		bool							bInGame = false; // Also in the pause menu's MOD SETTINGS during fights

		bool IsIni() const { return Section != nullptr; }
		bool IsLive() const { return (bool)LiveSet; }
		bool IsInfo() const { return (bool)Refresh; }
		bool IsAction() const { return (bool)Action; }
		bool IsCategory() const { return !IsIni() && !IsLive() && !IsInfo() && !IsAction(); }
	};

	static void Restart(const std::wstring& BeforeStart = L"");
	static void AddSettingsRow(Row row);
	static const wchar_t* Own(std::wstring Text);

	static std::vector<Choice> OffOn() { return { { L"OFF", "false" }, { L"ON", "true" } }; }
	constexpr int32_t OFF = 0;
	constexpr int32_t ON = 1;

	// Shared by the saved rows (Settings) and the live ones (MODS)
	static std::vector<Choice> AttractDelays()
	{
		return {
			{ L"GAME", "Game", L"After 30 seconds, like the game." },
			{ L"1 MINUTE", "60" },
			{ L"2 MINUTES", "120" },
			{ L"5 MINUTES", "300" },
			{ L"10 MINUTES", "600" },
			{ L"NEVER", "Never", L"Attract mode never starts." } };
	}

	static std::vector<Choice> LogLevels()
	{
		return {
			{ L"NONE", "0", L"Results only." },
			{ L"VERBOSE", "2", L"Adds addresses and each step." },
			{ L"DEBUG", "5", L"Adds debug checks." },
			{ L"FLOOD", "10", L"Adds every call of the sources picked in the FLOOD rows." },
			{ L"DROWN", "100", L"Everything, the FLOOD rows are ignored." } };
	}

	// The Settings tab: what gets hooked at startup (MK12TTH.ini), plus the startup values players may want to
	// change. Free text (urls, keys, names) and switches that make no sense in game stay in the ini
	static std::deque<Row> SettingsRows;

	// Page descriptions: the panel shows about 6 lines, the rest is cut off
	#define MK_MODS_NOTE		L"\n\n" MK_BOLD(L"Changes apply after a restart.") L"\n" MK_RED(L"Turn mods off before playing online.")
	constexpr const wchar_t* MODS_DESC			= L"Mods in Content\\Mods, loaded over the game's files." MK_MODS_NOTE;
	constexpr const wchar_t* PAKS_DESC			= L"Mods in Content\\Paks, loaded by the game in its own order." MK_MODS_NOTE;
	constexpr const wchar_t* LIVE_DESC			= L"Turn MK12TTH features off and on while you play. Saved for next time.\n\n" MK_BOLD(L"Changes apply right away.");
	constexpr const wchar_t* LOGGING_DESC		= L"What the console shows. Changes apply right away, for this session only.";
	constexpr const wchar_t* AUTHOR_URL			= L"http://thethiny.com/social/twitter";
	constexpr const wchar_t* ABOUT_DESC			= L"MK12TTH adds mod tools and features to Mortal Kombat 1. Made by @thethiny.";
	constexpr const wchar_t* PROFILE_DESC		= L"Read from your profile while you play. Select a row to see its value.";
	constexpr const wchar_t* TOKENS_DESC		= L"Read from your profile while you play. Select a row to see its value.\n\n" MK_RED(L"They are private, don't share them.");
	constexpr const wchar_t* IN_GAME_DESC		= L"MK12TTH features you can change during a fight. Changes apply right away.";
	constexpr const wchar_t* OWN_MENU_TITLE		= L"MODS"; // Our menu's title
	constexpr const wchar_t* MENU_TITLE_KEY		= L"SettingsSubMenuTitle"; // The settings screen's title, ours says MK12TTH
	constexpr int32_t	MOD_ROW_DEFAULT		= ON; // A mod missing from the ini is on

	// One page of rows of ours, and the game's rows built for them. TabKey is its label on the Q/E tab strip
	struct Page {
		const wchar_t* TabKey;
		const wchar_t* Description;
		std::vector<Row*> Rows;
		std::vector<MK12::FSettingsOption*> GameRows;
	};

	// One entry of the screen's left list. With several pages, a SECTION row at the top of each picks the page (left /
	// right); without the game's page switch each page gets its own entry instead (see Entries)
	struct Group {
		const wchar_t* TabKey;
		const wchar_t* DescKey; // The screen asks for the description by it, the page showing answers
		std::vector<Page*> Pages;
		size_t Current = 0;

		Page& Shown() const { return *Pages[Current]; }
	};
	constexpr const wchar_t* SECTION_TITLE = L"SECTION";

	static std::deque<Row> ModRows; // One per pak mod, stable addresses for the screen
	static std::deque<Row> MenuRows; // Live and info rows of our menu, stable addresses for the screen
	static std::deque<std::wstring> OwnedText; // Titles and descriptions built at runtime
	static std::deque<Page> SettingsPages; // Settings, MK12TTH: a page per category
	static Page ModsFolderPage = { L"CONTENT\\MODS", MODS_DESC };
	static Page PaksPage = { L"CONTENT\\PAKS", PAKS_DESC };
	static Page LivePage = { L"LIVE FEATURES", LIVE_DESC };
	static Page AboutPage = { L"ABOUT", ABOUT_DESC };
	static Page LoggingPage = { L"LOGGING", LOGGING_DESC };
	static Page AccountPage = { L"ACCOUNT", PROFILE_DESC };
	static Page TokensPage = { L"TOKENS", TOKENS_DESC };
	static Page FloydPage = { L"FLOYD", PROFILE_DESC };
	static Page InGamePage = { L"MK12TTH", IN_GAME_DESC }; // Rows marked bInGame, built from LIVE FEATURES
	static Page InGameDebugPage = { L"MK12TTH", IN_GAME_DESC }; // The same and the LOGGING ones, while DEBUG LOGS is on

	static Group SettingsGroup = { TabKey, PAGE_DESC_KEY };
	static Group PakModsGroup = { L"PAK MODS", L"MK12TTH_PakModsDesc", { &ModsFolderPage, &PaksPage } };
	static Group LiveGroup = { L"LIVE FEATURES", L"MK12TTH_LiveDesc", { &LivePage } };
	static Group LoggingGroup = { L"LOGGING", L"MK12TTH_LoggingDesc", { &LoggingPage } };
	static Group AboutGroup = { L"ABOUT", L"MK12TTH_AboutDesc", { &AboutPage } };
	static Group ProfileGroup = { L"PROFILE", L"MK12TTH_ProfileDesc", { &AccountPage, &TokensPage, &FloydPage } };
	static Group InGameGroup = { L"MK12TTH", L"MK12TTH_InGameDesc", { &InGamePage } };
	static Group InGameDebugGroup = { L"MK12TTH", L"MK12TTH_InGameDesc", { &InGameDebugPage } };

	static std::vector<Group*> SettingsGroups() { return { &SettingsGroup }; }
	static std::vector<Group*> MenuGroups() { return { &PakModsGroup, &LiveGroup, &ProfileGroup, &LoggingGroup, &AboutGroup }; }
	static std::vector<Group*> AllGroups() { return { &SettingsGroup, &PakModsGroup, &LiveGroup, &ProfileGroup, &LoggingGroup, &AboutGroup, &InGameGroup, &InGameDebugGroup }; }

	// Without the game's page switch, a group of several pages becomes an entry per page, "GROUP: PAGE"
	static std::deque<Group> SplitGroups;
	static bool HasSections();

	static std::vector<Group*> Entries(const std::vector<Group*>& Groups)
	{
		if (HasSections())
			return Groups;
		std::vector<Group*> Out;
		for (Group* group : Groups)
		{
			if (group->Pages.size() == 1)
			{
				Out.push_back(group);
				continue;
			}
			for (Page* page : group->Pages)
			{
				auto Same = std::find_if(SplitGroups.begin(), SplitGroups.end(), [page](const Group& Split) { return Split.Pages.front() == page; });
				if (Same == SplitGroups.end())
				{
					SplitGroups.push_back({ Own(std::wstring(group->TabKey) + L": " + page->TabKey), Own(std::wstring(group->DescKey) + L"_" + page->TabKey), { page } });
					Same = SplitGroups.end() - 1;
				}
				Out.push_back(&*Same);
			}
		}
		return Out;
	}

	static std::vector<Page*> AllPages()
	{
		std::vector<Page*> Pages;
		for (Group* group : AllGroups())
			Pages.insert(Pages.end(), group->Pages.begin(), group->Pages.end());
		return Pages;
	}
	static std::vector<Row*> AllRows;
	static std::vector<Group*> AddedGroups; // Entries added for the tab bar being built now
	static std::vector<Group*> ShownGroups; // Entries of the screen up now, after the game's own ones

	// The game's rows for ours, built once from copies of the game's own rows and never freed
	struct GameRow {
		std::unique_ptr<char[]> Memory;
		std::vector<MK12::FSettingsListValue> Values;
		std::deque<std::wstring> Texts; // Choice texts as shown
	};
	static std::deque<GameRow> GameRows;
	static bool bInitialized = false;
	static bool bBuilt = false;
	static bool bBuildFailed = false;
	static bool bOwnMenu = false; // The settings screen was opened from our main menu entry: only our pages

	static ULONGLONG LastChangeTick = 0;
	static int OpenScreens = 0; // Settings screens up right now, the restart question waits until they're all closed
	static MK12::USettingsScreen* ActiveScreen = nullptr; // The one with our pages, while it's up
	static int32_t FirstOwnPage = 0; // Our pages come after this many of the game's
	static bool bAskNow = false; // Enter on one of our pages: ask without waiting for the screen to close
	static std::vector<int32_t> LastAskedValues;

	static int32_t ReadValue(const Row& row)
	{
		CIniReader ini((char*)row.IniFile);
		char* Stored = ini.ReadString((char*)row.Section, (char*)row.Key, "");
		int32_t Value = row.Default;
		for (int32_t i = 0; i < (int32_t)row.Choices.size(); i++)
		{
			if (!_stricmp(Stored, row.Choices[i].IniValue))
			{
				Value = i;
				break;
			}
		}
		delete[] Stored;
		return Value;
	}

	static void WriteValue(const Row& row)
	{
		CIniReader ini((char*)row.IniFile);
		ini.WriteString((char*)row.Section, (char*)row.Key, (char*)row.Choices[row.Value].IniValue);
	}

	static const wchar_t* Own(std::wstring Text)
	{
		OwnedText.push_back(std::move(Text));
		return OwnedText.back().c_str();
	}

	static std::wstring Upper(std::wstring Text)
	{
		for (wchar_t& c : Text)
			c = (wchar_t)towupper(c);
		return Text;
	}

	static std::wstring Colored(const wchar_t* Color, const std::wstring& Text)
	{
		return std::wstring(L"<FONT COLOR='") + Color + L"'>" + Text + L"</font>";
	}

	static std::wstring Red(const std::wstring& Text) { return Colored(MK_RED_COLOR, Text); }
	static std::wstring Green(const std::wstring& Text) { return Colored(MK_GREEN_COLOR, Text); }

	static std::wstring Widen(const std::string& Text)
	{
		return std::wstring(Text.begin(), Text.end());
	}

	// What a mod's switch row says about how much of it the game really uses
	static std::wstring LoadText(const PakMods::Mod& mod)
	{
		const PakMods::LoadInfo& Load = mod.Load;
		std::wstring Text;
		auto Line = [&](const std::wstring& Part) { Text += L"\n\n" + Part; };
		const wchar_t* IfOn = mod.bEnabled ? L"" : L"If you turn it on: ";

		if (!Load.bContentsRead)
			Line(L"Couldn't read its file list, it may be encrypted.");
		if (!Load.LostTo.empty())
		{
			std::wstring Winners;
			for (const PakMods::Override& Winner : Load.LostTo)
				Winners += (Winners.empty() ? L"" : L", ") + Widen(Winner.Key) + L" (" + std::to_wstring(Winner.Files) + L")";
			bool bAll = Load.Lost >= Load.Files;
			Line(Red(std::wstring(IfOn) + (bAll ? L"Won't load, another mod overrides it. " : L"Partly overridden. "))
				+ std::to_wstring(Load.Lost) + L" of its " + std::to_wstring(Load.Files) + L" files come from " + Winners
				+ L", which load over it.");
		}
		if (!Load.WinsOver.empty())
		{
			std::wstring Losers;
			for (const PakMods::Override& Loser : Load.WinsOver)
				Losers += (Losers.empty() ? L"" : L", ") + Widen(Loser.Key) + L" (" + std::to_wstring(Loser.Files) + L")";
			Line(MK_BOLD(L"Overrides: ") + Losers);
		}
		if (Load.bAssetsInPakOnly)
			Line(Red(std::wstring(IfOn) + L"Probably won't load.") + L" Its game assets are in a .pak only, this game loads them from .utoc and .ucas files.");
		if (Load.bAfterGameFiles)
			Line(Red(std::wstring(IfOn) + L"Loads after some of the game's own files,") + L" so where they have the same files the game's win. "
				L"Move it into a folder that sorts after " + Load.LastGamePak + L", like pakchunk99-mods.");
		return Text;
	}

	// The short version, under the value while it's ON
	static std::wstring LoadSummary(const PakMods::Mod& mod)
	{
		const PakMods::LoadInfo& Load = mod.Load;
		if (!Load.LostTo.empty() && Load.Lost >= Load.Files)
			return L"Won't load: " + Widen(Load.LostTo.front().Key) + L" overrides it.";
		if (!Load.LostTo.empty())
			return L"Partly overridden by " + Widen(Load.LostTo.front().Key) + L".";
		if (Load.bAssetsInPakOnly)
			return L"Probably won't load: assets in a .pak only.";
		if (Load.bAfterGameFiles)
			return L"Loads after some of the game's own files.";
		return L"";
	}

	static std::wstring EngineValues(const EngineConfig::LooseFile& File)
	{
		std::wstring Text;
		for (const EngineConfig::Value& value : File.Values)
			Text += (Text.empty() ? L"" : L", ") + value.Key + L"=" + value.Text;
		return Text.empty() ? L"none" : Text;
	}

	static std::wstring WhereIs(PakMods::Source From, const std::wstring& Folder)
	{
		std::wstring Root = From == PakMods::Source::ModsFolder ? L"Content\\Mods" : L"Content\\Paks";
		return Folder.empty() ? Root : Root + L"\\" + Folder;
	}

	// How a mod's place in the load order was picked
	static std::wstring OrderText(const PakMods::Mod& mod)
	{
		if (mod.From == PakMods::Source::Paks)
			return L"\n\n" MK_BOLD(L"Load order: ") L"the game's own, by path. Content\\Mods loads over it.";
		if (mod.Priority >= 0)
			return L"\n\n" MK_BOLD(L"Load order: ") L"priority " + std::to_wstring(mod.Priority) + L" from its name (_P" + std::to_wstring(mod.Priority)
				+ L"), over the mods without one. Higher wins.";
		return L"\n\n" MK_BOLD(L"Load order: ") L"over the game's files, like every mod in Content\\Mods. When two mods change "
			L"the same file, the one whose name comes last wins. End its name with _P and a number to raise it.";
	}

	static const wchar_t* KindText(PakMods::Kind Type)
	{
		return Type == PakMods::Kind::Pak ? L".pak only"
			: Type == PakMods::Kind::IoStore ? L".pak, .utoc and .ucas"
			: L".utoc and .ucas, mounted by MK12TTH since the game only looks for .pak files";
	}

	// One pak: what it has. Several (a mod folder): each one by name
	static std::wstring PakFiles(const PakMods::Mod& mod)
	{
		if (mod.Paks.size() == 1)
			return KindText(mod.Paks.front().Type);
		std::wstring Text = std::to_wstring(mod.Paks.size()) + L" paks";
		for (const PakMods::Container& Pak : mod.Paks)
			Text += L"\n" + (Pak.Folder.empty() ? Pak.Name : Pak.Folder + L"/" + Pak.Name) + L": " + KindText(Pak.Type);
		return Text;
	}

	static std::wstring BackSlashes(std::wstring Path)
	{
		std::replace(Path.begin(), Path.end(), L'/', L'\\');
		return Path;
	}

	// A creator's text shown as it is, not as the screen's markup
	static std::wstring PlainText(const std::wstring& Text)
	{
		std::wstring Out;
		for (wchar_t c : Text)
			Out += c == L'<' ? L"&lt;" : c == L'>' ? L"&gt;" : c == L'&' ? L"&amp;" : std::wstring(1, c);
		return Out;
	}

	static std::wstring DescriptionText(const std::wstring& Description)
	{
		return Description.empty() ? L"" : L"\n\n" + PlainText(Description);
	}

	// The screen finds rows by title, so two mod rows never share one
	static std::wstring UniqueModTitle(std::wstring Title, const std::wstring& Where)
	{
		auto Taken = [](const std::wstring& Text) { return std::any_of(ModRows.begin(), ModRows.end(), [&](const Row& row) { return Text == row.Title; }); };
		if (Taken(Title))
			Title += L" (" + Upper(Where) + L")";
		for (int i = 2; Taken(Title); i++)
			Title += L" " + std::to_wstring(i);
		return Title;
	}

	static void AddModRow(Page& page, Row row, const std::wstring& Summary = L"")
	{
		row.IniFile = StateFile::FILE_NAME;
		if (!Summary.empty())
			row.Choices[ON].Description = Own(Summary);
		ModRows.push_back(std::move(row));
		page.Rows.push_back(&ModRows.back());
	}

	// Sets a row's description each time the menu opens: paks mounted since startup add their own description.txt
	static void DescribeWith(Row& row, std::function<std::wstring()> Text)
	{
		row.Describe = [Text](Row& row)
		{
			row.InfoText = Text();
			row.Description = row.InfoText.c_str();
		};
	}

	static std::wstring InsideText(const PakMods::Mod& mod, const PakMods::Switch& Entry)
	{
		if (Entry.bFolder)
		{
			std::wstring Paks;
			for (const PakMods::Switch& Pak : mod.Switches)
			{
				if (!Pak.bFolder && Pak.Path.size() > Entry.Path.size() && !_wcsnicmp(Pak.Path.c_str(), (Entry.Path + L"/").c_str(), Entry.Path.size() + 1))
					Paks += L"\n" + BackSlashes(Pak.Path.substr(Entry.Path.size() + 1));
			}
			return L"Folder " + WhereIs(mod.From, BackSlashes(mod.Folder + L"/" + Entry.Path)) + L"." + DescriptionText(Entry.Description)
				+ L"\n\n" MK_BOLD(L"Paks: ") + Paks + L"\n\nTurning it off keeps all its paks from mounting after a restart.";
		}
		const PakMods::Container& Pak = mod.Paks[Entry.Pak];
		std::wstring Off = Pak.bLoads || !Entry.bEnabled ? L"" : L"\n\n" + Red(L"Doesn't mount: a folder above it or its mod is off.");
		return L"In " + WhereIs(mod.From, BackSlashes(mod.Folder + (Pak.Folder.empty() ? L"" : L"/" + Pak.Folder))) + L"."
			+ DescriptionText(Pak.Description) + L"\n\n" MK_BOLD(L"Files: ") + KindText(Pak.Type) + Off
			+ L"\n\nMounts only when the folders above it and its mod are on too. Turning it off keeps the game from mounting it after a restart.";
	}

	// The folders and paks inside a mod with several paks, each with its own switch, under the mod's row
	static void AddInsideRows(Page& page, const PakMods::Mod& mod)
	{
		for (const PakMods::Switch& Entry : mod.Switches)
		{
			std::wstring Indent;
			for (int i = 0; i < Entry.Depth; i++)
				Indent += L"- ";
			Row row = { Own(UniqueModTitle(Indent + Upper(Entry.Name), BackSlashes(mod.Name + L"/" + Entry.Path))), nullptr,
				StateFile::MODS, Entry.Key.c_str(), OffOn(), MOD_ROW_DEFAULT };
			const PakMods::Switch* Where = &Entry;
			DescribeWith(row, [&mod, Where] { return InsideText(mod, *Where); });
			AddModRow(page, std::move(row));
		}
	}

	// A mod's own row. A single pak shows the description.txt inside it, a folder of paks its folder's
	static std::wstring ModText(const PakMods::Mod& mod)
	{
		std::wstring Engine;
		for (const EngineConfig::LooseFile* File : EngineConfig::FilesFor(mod))
			Engine += EngineValues(*File);
		if (!Engine.empty())
			Engine = L"\n\n" MK_BOLD(L"Engine settings: ") + Engine;
		std::wstring Description = mod.Paks.size() == 1 ? mod.Paks.front().Description : mod.Description;
		std::wstring Inside = mod.Switches.empty() ? L"" : L"\n\nEach pak, and each folder with several paks, has its own switch below.";
		return L"In " + WhereIs(mod.From, BackSlashes(mod.Folder)) + L"." + DescriptionText(Description) + L"\n\n" MK_BOLD(L"Files: ") + PakFiles(mod)
			+ Engine + OrderText(mod) + LoadText(mod) + Inside + L"\n\nTurning it off keeps the game from mounting it after a restart.";
	}

	// Content\Mods and Content\Paks always show, so players know they're there
	static void AddNoModsRow(Page& page, const wchar_t* Description)
	{
		Row row = { L"NO MODS YET", Description };
		row.bNeedsRestart = false;
		row.Refresh = [](Row& row) { wcsncpy_s(row.InfoLabel, L"EMPTY", _TRUNCATE); };
		ModRows.push_back(std::move(row));
		ModRows.back().Choices = { { ModRows.back().InfoLabel, "" } }; // The label lives in the row, after the move
		page.Rows.push_back(&ModRows.back());
	}

	static void AddActionRow(Page& page, const wchar_t* Title, const wchar_t* Description, std::function<void()> Action)
	{
		Row row = { Title, Description };
		row.bNeedsRestart = false;
		row.Action = std::move(Action);
		ModRows.push_back(std::move(row));
		page.Rows.push_back(&ModRows.back());
	}

	static std::wstring PowerShellQuoted(const std::wstring& Text)
	{
		std::wstring Out = L"'";
		for (wchar_t c : Text)
			Out += c == L'\'' ? std::wstring(L"''") : std::wstring(1, c);
		return Out + L"'";
	}

	// Content\Paks mods move to Content\Mods with their folder: a pak right in Paks becomes a mod of its own, a folder of
	// paks one mod. The game has them open, so they move once it has closed, before it starts again
	static std::wstring MovePaksModsScript(size_t& Moved)
	{
		std::wstring Script;
		Moved = 0;
		for (const PakMods::Mod& mod : PakMods::List())
		{
			if (mod.From != PakMods::Source::Paks)
				continue;
			std::filesystem::path To = PakMods::Root(PakMods::Source::ModsFolder) / mod.Folder;
			bool bAny = false;
			for (const wchar_t* Extension : { L".pak", L".utoc", L".ucas", L".sig" })
			{
				std::filesystem::path From = PakMods::Root(PakMods::Source::Paks) / mod.Folder / (mod.Name + Extension);
				std::error_code Error;
				if (!std::filesystem::exists(From, Error))
					continue;
				bAny = true;
				Script += L"New-Item -ItemType Directory -Force -Path " + PowerShellQuoted(To.wstring()) + L" | Out-Null\r\n";
				Script += L"Move-Item -LiteralPath " + PowerShellQuoted(From.wstring()) + L" -Destination " + PowerShellQuoted(To.wstring()) + L" -ErrorAction SilentlyContinue\r\n";
			}
			Moved += bAny;
		}
		return Script;
	}

	static void AddModsActions()
	{
		AddActionRow(ModsFolderPage, L"RESTART NOW", L"Restarts the game, so mods you turned on or off apply.", []
		{
			GameMessageBox::Ask(L"Restart the game now?", [](bool Yes) { if (Yes) Restart(); }, L"MK12TTH");
		});
		AddActionRow(ModsFolderPage, L"MOVE MODS TO MODS FOLDER", L"Moves your mods from Content\\Paks to Content\\Mods, then restarts the game. "
			L"There MK12TTH orders them and shows what each one overrides.\n\nA pak right in Content\\Paks becomes a mod of its own, "
			L"a folder of paks becomes one mod with a switch for each pak.", []
		{
			size_t Count = 0;
			MovePaksModsScript(Count);
			if (!Count)
			{
				GameMessageBox::Confirm(L"Content\\Paks has no mods to move.", nullptr, L"MK12TTH");
				return;
			}
			GameMessageBox::Ask(L"Move " + std::to_wstring(Count) + L" mods from Content\\Paks to Content\\Mods and restart the game?", [](bool Yes)
			{
				size_t Moved = 0;
				if (Yes)
					Restart(MovePaksModsScript(Moved));
			}, L"MK12TTH");
		});
	}

	// A row per pak mod, the title is its file name, with its folder when two share a name. A mod with several paks has
	// rows for its folders and paks under it. Content\Mods and Content\Paks are tabs of their own
	static void AddModRows()
	{
		const auto& Mods = PakMods::List();
		AddModsActions();
		auto Category = [](Page& page, const wchar_t* Title)
		{
			ModRows.push_back({ Title });
			page.Rows.push_back(&ModRows.back());
		};
		Category(ModsFolderPage, L"MODS");
		Category(PaksPage, L"PAKS");
		auto Has = [&](PakMods::Source From) { return std::any_of(Mods.begin(), Mods.end(), [From](const PakMods::Mod& mod) { return mod.From == From; }); };
		if (!Has(PakMods::Source::ModsFolder))
			AddNoModsRow(ModsFolderPage, L"Put mods in Content\\Mods: each folder is one mod, with its .pak, .utoc and .ucas files and its Engine.ini. "
				L"A pak right in Content\\Mods is a mod of its own.\n\nMods there load over the game's files and over Content\\Paks, "
				L"MK12TTH orders them, and each can be turned off here.");
		if (!Has(PakMods::Source::Paks))
			AddNoModsRow(PaksPage, L"No mods in Content\\Paks. Mods go in Content\\Mods, where MK12TTH orders them.");
		for (const PakMods::Mod& mod : Mods)
		{
			Page& page = mod.From == PakMods::Source::ModsFolder ? ModsFolderPage : PaksPage;
			std::wstring Place = mod.Folder.empty() ? (mod.From == PakMods::Source::ModsFolder ? L"Mods" : L"Paks") : mod.Folder;
			Row row = { Own(UniqueModTitle(Upper(mod.Name), Place)), nullptr, StateFile::MODS, mod.Key.c_str(), OffOn(), MOD_ROW_DEFAULT };
			const PakMods::Mod* Of = &mod;
			DescribeWith(row, [Of] { return ModText(*Of); });
			AddModRow(page, std::move(row), LoadSummary(mod));
			AddInsideRows(page, mod);
		}
	}

	static Row& AddMenuRow(Page& page, Row row)
	{
		MenuRows.push_back(std::move(row));
		page.Rows.push_back(&MenuRows.back());
		return MenuRows.back();
	}

	static void AddCategory(Page& page, const wchar_t* Title)
	{
		AddMenuRow(page, { Title });
	}

	// A live on/off row. Off when its hook isn't in, the description says what to turn on
	static Row& AddLiveRow(Page& page, const wchar_t* Title, const wchar_t* Description, std::function<bool()> Get, std::function<void(bool)> Set,
		bool bAvailable = true, const wchar_t* Needs = nullptr)
	{
		Row row = { Title, Description };
		row.Choices = OffOn();
		row.bNeedsRestart = false;
		if (!bAvailable)
		{
			// The game has no greyed out rows: one value can't be changed, the description says why
			row.Choices = { { L"UNAVAILABLE", "" } };
			if (Needs)
				row.Description = Own(std::wstring(Description) + L"\n\n" MK_RED(L"Needs ") + Needs + MK_RED(L" in Settings, MK12TTH, then a restart."));
			row.LiveGet = [] { return 0; };
			row.LiveSet = [](int32_t) {};
			return AddMenuRow(page, std::move(row));
		}
		row.LiveGet = [Get] { return Get() ? ON : OFF; };
		row.LiveSet = [Set, bAvailable](int32_t Value) { if (bAvailable) Set(Value == ON); };
		return AddMenuRow(page, std::move(row));
	}

	// What a feature needs: green when it's running, red when it isn't
	static std::wstring NeedsText(Features::Id Which)
	{
		std::wstring Text;
		for (Features::Id Need : Features::Get(Which).Needs)
		{
			std::wstring Title = Features::Get(Need).Title;
			Text += (Text.empty() ? L"\n\n" MK_BOLD(L"Needs: ") : L", ") + (Features::IsInstalled(Need) ? Green(Title) : Red(Title));
		}
		return Text;
	}

	// A feature's live switch (MODS). Unavailable unless it's installed, the description says why
	static Row& AddFeatureRow(Page& page, Features::Id Which)
	{
		const Features::Definition& Feature = Features::Get(Which);
		bool bInstalled = Features::IsInstalled(Which);
		const wchar_t* Description = bInstalled ? Feature.Description
			: Own(std::wstring(Feature.Description) + NeedsText(Which) + L"\n\n" MK_RED(L"Not available: ") + Features::Explain(Which));
		return AddLiveRow(page, Feature.Title, Description,
			[Which] { return Features::IsActive(Which); },
			[Which](bool bOn) { Features::SetActive(Which, bOn); },
			bInstalled);
	}

	// A feature's startup switch (Settings tab, MK12TTH.ini). The description says what it needs and how it went
	static void AddFeatureSetting(Features::Id Which)
	{
		if (!Features::HasStartupSwitch(Which))
			return;
		const Features::Definition& Feature = Features::Get(Which);
		std::wstring Text = Feature.Description;
		if (Feature.Type == Features::Kind::Live)
			Text += L"\n\nTurn it off and on while playing in MODS, LIVE FEATURES.";
		Text += NeedsText(Which);
		std::wstring Why = Features::Explain(Which);
		if (!Why.empty() && Features::GetState(Which) != Features::State::Disabled)
			Text += L"\n\n" MK_RED(L"Not running: ") + Why;
		std::vector<Choice> Choices = OffOn();
		if (!Feature.Modes.empty())
		{
			Choices.clear();
			for (const auto& Mode : Feature.Modes)
				Choices.push_back({ Mode.first, Mode.second });
		}
		AddSettingsRow({ Feature.Title, Own(Text), Feature.IniSection, Feature.IniKey, Choices, Feature.bIniDefault ? ON : OFF });
	}

	// A header row inside the current page
	static void AddSettingsCategory(const wchar_t* Title)
	{
		SettingsRows.push_back({ Title });
		SettingsPages.back().Rows.push_back(&SettingsRows.back());
	}

	// A new page of the Settings tab, its rows follow
	static void AddSettingsPage(const wchar_t* Title)
	{
		SettingsPages.push_back({ Title, PAGE_DESC });
		SettingsGroup.Pages.push_back(&SettingsPages.back());
	}

	static void AddSettingsRow(Row row)
	{
		SettingsRows.push_back(std::move(row));
		SettingsPages.back().Rows.push_back(&SettingsRows.back());
	}

	static void AddSettingsRows()
	{
		using Features::Id;
		AddSettingsPage(L"GENERAL");
		AddFeatureSetting(Id::ModLoader);
		AddFeatureSetting(Id::PakMods);
		AddFeatureSetting(Id::EngineConfig);
		AddFeatureSetting(Id::FastBoot);
		AddFeatureSetting(Id::KeepRunning);
		AddFeatureSetting(Id::AttractMode);
		AddSettingsCategory(L"NAMES AND AUDIO");
		AddFeatureSetting(Id::NameTable);
		AddFeatureSetting(Id::StringSwap);
		AddFeatureSetting(Id::Announcer);

		AddSettingsPage(L"FLOYD");
		AddFeatureSetting(Id::FloydTracking);
		AddSettingsRow({ L"FLOYD TOWER TIMES",
			L"Where the Floyd Tower times come from.",
			"Patches", "szFloydTowerMode", {
				{ L"GAME", "Game", L"The game's own schedule, read once you open Living Towers. Shows up once the Floyd Tower is unlocked." },
				{ L"CALCULATED", "Calculated", L"Worked out from your profile, the same way the game does." } }, 0 });
		AddFeatureSetting(Id::FloydTowerAlert);
		AddFeatureSetting(Id::FloydFight);
		AddFeatureSetting(Id::FightMetadata);

		AddSettingsPage(L"ONLINE");
		AddFeatureSetting(Id::ProfileGetter);
		AddFeatureSetting(Id::Curl);
		AddFeatureSetting(Id::PrivateServer);

		AddSettingsPage(L"DEBUG");
		AddSettingsRow({ L"CONSOLE WINDOW", L"Opens a console window with the mod's logs.",
			"Settings.Debug", "bEnableConsoleWindow", OffOn(), OFF });
		AddSettingsRow({ L"LOG LEVEL", L"How much the console shows when the game starts. Change it while playing in MODS, LOGGING.",
			"Settings", "iLogLevel", LogLevels(), 0 });
		AddSettingsRow({ L"DEBUG LOGS", L"Shows logs up to the DEBUG level whatever the LOG LEVEL is.",
			"Settings.Debug", "bDebug", OffOn(), OFF });
		AddSettingsRow({ L"PAUSE ON START", L"Pauses the game at startup until you press OK, so a debugger can attach.",
			"Settings.Debug", "bPauseOnStart", OffOn(), OFF });
		AddSettingsRow({ L"FLOYD DEBUG", L"Shows Floyd dev logs: hash inputs, seeds and extracted IDs." MK_NEEDS(L"FLOYD TRACKING"),
			"Settings.Debug", "bFloydDebug", OffOn(), OFF });
		AddSettingsRow({ L"PROFILE DEBUG", L"Shows profile dev logs: platform, platform ID and save key." MK_NEEDS(L"PROFILE GETTER"),
			"Settings.Debug", "bProfileDebug", OffOn(), OFF });
		AddSettingsRow({ L"FNAME FLOOD", L"At LOG LEVEL FLOOD, logs every name lookup.",
			"Settings.Debug.FloodToggles", "bFName", OffOn(), OFF });
		AddSettingsRow({ L"CURL FLOOD", L"At LOG LEVEL FLOOD, logs every web request.",
			"Settings.Debug.FloodToggles", "bCurl", OffOn(), OFF });
	}

	static Row& AddFlagRow(Page& page, const wchar_t* Title, const wchar_t* Description, bool& Flag, bool bAvailable = true, const wchar_t* Needs = nullptr)
	{
		bool* Target = &Flag;
		return AddLiveRow(page, Title, Description, [Target] { return *Target; }, [Target](bool bOn) { *Target = bOn; }, bAvailable, Needs);
	}

	static void SetInfo(Row& row, const std::wstring& Label, const std::wstring& Text)
	{
		wcsncpy_s(row.InfoLabel, Label.c_str(), _TRUNCATE);
		row.InfoText = Text;
		row.Description = row.InfoText.c_str();
	}

	static Row& AddInfoRow(Page& page, const wchar_t* Title, std::function<void(Row&)> Refresh)
	{
		Row& row = AddMenuRow(page, { Title });
		row.bNeedsRestart = false;
		row.Refresh = std::move(Refresh);
		row.Choices = { { row.InfoLabel, "" } }; // The screen reads the label when it opens, after Refresh
		return row;
	}

	static std::wstring Widen(const char* Text)
	{
		return Text ? std::wstring(Text, Text + strlen(Text)) : std::wstring();
	}

	static std::wstring Short(const std::wstring& Text, size_t Max)
	{
		return Text.size() > Max ? Text.substr(0, Max - 3) + L"..." : Text;
	}

	constexpr const wchar_t* NOT_READ = L"NOT READ YET";

	// A value read by one of our hooks. Long values show in full in the description only, private ones only there
	static void AddValueRow(Page& page, const wchar_t* Title, const wchar_t* About, std::function<std::wstring()> Read, Features::Id Source,
		bool bPrivate = false, std::function<std::wstring()> WhenRead = [] { return std::wstring(L"Not read yet, the game reads it as you play."); })
	{
		AddInfoRow(page, Title, [=](Row& row)
		{
			std::wstring Value = Read();
			std::wstring Text = About;
			if (!Value.empty())
			{
				SetInfo(row, bPrivate ? L"SELECT TO SEE" : Short(Value, 20), Text + L"\n\n" MK_BOLD(L"Value: ") + Short(Value, 160));
				return;
			}
			if (!Features::IsInstalled(Source))
				Text += L"\n\n" MK_RED(L"Not available: ") + Features::Explain(Source);
			else if (!Features::IsActive(Source))
				Text += L"\n\n" MK_RED(L"Turned off in LIVE FEATURES.");
			else
				Text += L"\n\n" + WhenRead();
			SetInfo(row, NOT_READ, Text);
		}).CopyValue = Read;
	}

	// Floyd's challenges for the player, sorted, with their names. Green when done, red when not, plain when the
	// done ones aren't read yet
	static std::wstring ChallengeList(const std::vector<uint8_t>& Challenges, const FloydTowerAlert::FloydStats& Stats)
	{
		std::vector<uint8_t> Sorted = Challenges;
		std::sort(Sorted.begin(), Sorted.end());
		std::wstring Text;
		for (uint8_t Challenge : Sorted)
		{
			std::wstring Line = std::to_wstring(Challenge) + L". " + Widen(MK12::EnumMaps::Find(MK12::EnumMaps::FloydChallengeMap, (int)Challenge));
			if (Stats.bRead)
				Line = FloydTowerAlert::IsChallengeDone(Stats, Challenge) ? Green(Line + L", done") : Red(Line);
			Text += (Text.empty() ? L"" : L"\n") + Line;
		}
		return Text;
	}

	// The player's challenges, or the done ones alone when the game didn't pick them yet this session
	static std::vector<uint8_t> KnownChallenges(const FloydTowerAlert::FloydStats& Stats)
	{
		const auto& Floyd = HookMetadata::CurrentFloydInfo;
		if (Features::IsActive(Features::Id::FloydTracking) && Floyd.HasClues)
			return Floyd.Clues;
		std::vector<uint8_t> Done;
		for (uint8_t Challenge = 1; Stats.bRead && Challenge <= FloydTower::CHALLENGE_COUNT; Challenge++)
		{
			if (FloydTowerAlert::IsChallengeDone(Stats, Challenge))
				Done.push_back(Challenge);
		}
		return Done;
	}

	static int DoneCount(const std::vector<uint8_t>& Challenges, const FloydTowerAlert::FloydStats& Stats)
	{
		return (int)std::count_if(Challenges.begin(), Challenges.end(), [&](uint8_t Challenge) { return FloydTowerAlert::IsChallengeDone(Stats, Challenge); });
	}

	static std::wstring FloydTowerText(std::wstring& Label)
	{
		time_t Now = time(nullptr);
		FloydTower::Window window;
		FloydTower::Status status = FloydTower::Find(Now, HookMetadata::CurrentTacoInfo.UserProfileHash, SettingsMgr->bFloydTowerFromGame, window);
		if (status == FloydTower::Status::NotUnlocked)
		{
			// The game drops the tower from the schedule after you've had your chance at it, not only before the first win
			const FloydTowerAlert::FloydStats& Stats = FloydTowerAlert::LastStats();
			if (Stats.bRead && Stats.Values[(size_t)FloydTowerAlert::Stat::Victories] > 0)
			{
				Label = L"GONE";
				return L"Not in the game's schedule. The Floyd Tower shows up once you've beaten Floyd, and is gone after it showed up.";
			}
			Label = L"LOCKED";
			return L"Not in the game's schedule. Beat Floyd once to unlock it.";
		}
		if (status == FloydTower::Status::NoSchedule)
		{
			Label = NOT_READ;
			return SettingsMgr->bFloydTowerFromGame ? L"Open Living Towers to read the schedule." : L"Beat Floyd once, then open Living Towers.";
		}

		char When[32], Duration[32];
		bool bUp = window.Start <= Now;
		FloydTower::FormatLocalTime(bUp ? window.End : window.Start, When, sizeof(When));
		FloydTower::FormatDuration(bUp ? window.End - Now : window.Start - Now, Duration, sizeof(Duration));
		Label = Upper((bUp ? L"UP, ENDS IN " : L"IN ") + Widen(Duration));
		return (bUp ? L"Up now, ends in " : L"Next in ") + Widen(Duration) + L" (" + Widen(When) + L").\n\n"
			MK_BOLD(L"Slot: ") + Widen(FloydTower::SlotName(window.Slot));
	}

	// The delay picked here is saved for next time too
	static void AddAttractRow()
	{
		const wchar_t* About = L"How long you idle on the main menu before attract mode, the demo, starts.";
		if (!AttractMode::IsReady())
		{
			AddLiveRow(LivePage, L"ATTRACT MODE", Own(std::wstring(About) + L"\n\n" MK_RED(L"Not available: ") + Features::Explain(Features::Id::AttractMode)),
				nullptr, nullptr, false);
			return;
		}
		Row row = { L"ATTRACT MODE", About };
		row.Choices = AttractDelays();
		row.bNeedsRestart = false;
		row.LiveGet = []
		{
			std::vector<Choice> Delays = AttractDelays();
			for (int32_t i = 0; i < (int32_t)Delays.size(); i++)
			{
				if (AttractMode::FromIni(Delays[i].IniValue) == AttractMode::Delay())
					return i;
			}
			return 0;
		};
		row.LiveSet = [](int32_t Value)
		{
			const char* Picked = AttractDelays()[Value].IniValue;
			AttractMode::SetDelay(AttractMode::FromIni(Picked));
			CIniReader ini("");
			ini.WriteString((char*)"Patches", (char*)"szAttractModeDelay", (char*)Picked);
		};
		AddMenuRow(LivePage, std::move(row));
	}

	static Row& AddAnnouncerRow()
	{
		const wchar_t* About = L"The announcer's voice.\n"
			L"GAME: your own setting.\n"
			L"AUTO: in fights, the announcer of the fighter you play. A fighter without one gets your own setting.\n"
			L"Or any announcer, everywhere.";
		if (!Features::IsInstalled(Features::Id::Announcer) || !Announcer::IsReady())
		{
			return AddLiveRow(LivePage, L"ANNOUNCER", Own(std::wstring(About) + L"\n\n" MK_RED(L"Not available: ") + Features::Explain(Features::Id::Announcer)),
				nullptr, nullptr, false);
		}
		Row row = { L"ANNOUNCER", About };
		for (const Announcer::Option& Option : Announcer::Options())
			row.Choices.push_back({ Option.Label, Option.Value });
		row.bNeedsRestart = false;
		row.LiveGet = [] { return Announcer::Picked(); };
		row.LiveSet = [](int32_t Value) { Announcer::Pick(Value); };
		return AddMenuRow(LivePage, std::move(row));
	}

	// A list row of FLOYD FIGHT, unavailable without its hooks
	static void AddFloydFightRow(const wchar_t* Title, const wchar_t* About, const std::vector<FloydFight::Option>& Options,
		std::function<int32_t()> Get, std::function<void(int32_t)> Set)
	{
		if (!Features::IsInstalled(Features::Id::FloydFight) || !FloydFight::IsReady())
		{
			AddLiveRow(LivePage, Title, Own(std::wstring(About) + L"\n\n" MK_RED(L"Not available: ") + Features::Explain(Features::Id::FloydFight)),
				nullptr, nullptr, false);
			return;
		}
		Row row = { Title, About };
		for (const FloydFight::Option& Option : Options)
			row.Choices.push_back({ Option.Label, Option.Value });
		row.bNeedsRestart = false;
		row.LiveGet = std::move(Get);
		row.LiveSet = std::move(Set);
		AddMenuRow(LivePage, std::move(row)).bInGame = true;
	}

	static void AddFloydFightRows()
	{
		AddFloydFightRow(L"FLOYD DIFFICULTY LEVEL", L"Floyd's AI level, from 1 to 19, higher is harder. Used from his next fight, retries included.\n"
			L"GAME: the game's own, 13 at first and lower after each loss.",
			FloydFight::LevelOptions(), [] { return FloydFight::PickedLevel(); }, [](int32_t Value) { FloydFight::PickLevel(Value); });
		AddFloydFightRow(L"FLOYD RETRIES", L"How many times you can retry after losing to Floyd before he leaves.\n"
			L"GAME: the game's own, 2.\n"
			L"NONE: one fight, no retry.",
			FloydFight::RetryOptions(), [] { return FloydFight::PickedRetries(); }, [](int32_t Value) { FloydFight::PickRetries(Value); });
	}

	static void AddSpawnTowerRow()
	{
		const wchar_t* TowerNow = L"Makes today's Floyd Tower show up the next time you open Living Towers, for as long as the game's own "
			L"would. Once per press: only that visit, and gone if you restart the game first. Needs a Floyd win, like the game's own tower.\n"
			L"Rewards from it might not count, WB's servers know when the tower is really due.";
		bool bTower = Features::IsInstalled(Features::Id::FloydFight) && Features::IsInstalled(Features::Id::FloydTracking);
		if (!bTower)
		{
			AddLiveRow(LivePage, L"SPAWN FLOYD TOWER NOW", Own(std::wstring(TowerNow) + L"\n\n" MK_RED(L"Not available: ")
				+ Features::Explain(Features::IsInstalled(Features::Id::FloydFight) ? Features::Id::FloydTracking : Features::Id::FloydFight)), nullptr, nullptr, false);
			return;
		}
		AddActionRow(LivePage, L"SPAWN FLOYD TOWER NOW", TowerNow, []
		{
			FloydFight::SpawnTower();
			GameMessageBox::Confirm(L"The Floyd Tower shows up the next time you open Living Towers.", nullptr, L"SPAWN FLOYD TOWER NOW");
		});
	}

	static void AddSummonRow()
	{
		if (!FloydTowerAlert::CanSummon())
			return;
		AddActionRow(LivePage, L"SUMMON FLOYD", L"Marks all of Floyd's challenges done in your profile on this PC only, nothing is sent "
			L"to WB's servers. Floyd shows up at the end of your next match where he can, and the game syncs as usual after you fight him.", []
		{
			GameMessageBox::Ask(L"Mark all of Floyd's challenges done?", [](bool Yes)
			{
				if (!Yes)
					return;
				using FloydTowerAlert::SummonResult;
				SummonResult Result = FloydTowerAlert::Summon();
				const wchar_t* Text = Result == SummonResult::Summoned ? L"Done. Floyd shows up at the end of your next match where he can."
					: Result == SummonResult::AlreadyDone ? L"All of Floyd's challenges were already done."
					: Result == SummonResult::NoProfile ? L"Your profile isn't loaded yet. Try again from the main menu."
					: L"Not available in this version of the game.";
				GameMessageBox::Confirm(Text, nullptr, L"SUMMON FLOYD");
			}, L"SUMMON FLOYD");
		});
	}

	static std::wstring FloydStatText(FloydTowerAlert::Stat Which)
	{
		return std::to_wstring(FloydTowerAlert::LastStats().Values[(size_t)Which]);
	}

	// Floyd profile stats, read when Living Towers opens
	static void AddFloydStatRow(const wchar_t* Title, const wchar_t* About, std::function<void(Row&, const FloydTowerAlert::FloydStats&)> Show)
	{
		AddInfoRow(FloydPage, Title, [About, Show](Row& row)
		{
			const FloydTowerAlert::FloydStats& Stats = FloydTowerAlert::LastStats();
			if (!Stats.bRead)
			{
				std::wstring Why = Features::IsInstalled(Features::Id::FloydTowerAlert) ? L"Read from your profile when you open Living Towers."
					: std::wstring(MK_RED(L"Not available: ")) + Features::Explain(Features::Id::FloydTowerAlert);
				SetInfo(row, NOT_READ, std::wstring(About) + L"\n\n" + Why);
				return;
			}
			Show(row, Stats);
		});
	}

	static void AddAboutRows()
	{
		AddInfoRow(AboutPage, L"VERSION", [](Row& row)
		{
			SetInfo(row, ModVersion(), L"The MK12TTH version you're running.");
		});
		AddInfoRow(AboutPage, L"GAME VERSION", [](Row& row)
		{
			int32_t Changelist = MK12::GetChangelist ? MK12::GetChangelist() : 0;
			std::wstring Build = MK12::GetBuildVersion ? MK12::GetBuildVersion() : L"";
			SetInfo(row, Changelist ? std::to_wstring(Changelist) : NOT_READ,
				L"The game build MK12TTH is running on." + (Build.empty() ? L"" : L"\n\n" MK_BOLD(L"Build: ") + Build));
		});
		AddActionRow(AboutPage, L"MADE BY @THETHINY", L"MK12TTH is made and maintained by @thethiny. Opens @thethiny on X in your browser.", []
		{
			ShellExecuteW(nullptr, L"open", AUTHOR_URL, nullptr, nullptr, SW_SHOWNORMAL);
		});
		AddActionRow(AboutPage, L"WHAT'S NEW", L"Shows what changed in this version of MK12TTH.", []
		{
			GameMessageBox::Confirm(Changelog::Current, nullptr, L"MK12TTH " + ModVersion());
		});
	}

	static void AddMenuRows()
	{
		using Features::Id;
		AddAboutRows();

		AddCategory(LivePage, L"FLOYD");
		AddFeatureRow(LivePage, Id::FloydTracking);
		AddFeatureRow(LivePage, Id::FloydTowerAlert);
		AddFloydFightRows();
		AddFeatureRow(LivePage, Id::FightMetadataLog);
		AddSummonRow();
		AddSpawnTowerRow();
		AddCategory(LivePage, L"ONLINE");
		AddFeatureRow(LivePage, Id::ProfileGetter);
		AddFeatureRow(LivePage, Id::Curl);
		AddCategory(LivePage, L"NAMES AND AUDIO");
		AddFeatureRow(LivePage, Id::StringSwap).bInGame = true;
		AddAnnouncerRow().bInGame = true;
		AddCategory(LivePage, L"GAME");
		AddAttractRow();
		AddFeatureRow(LivePage, Id::KeepRunning).bInGame = true;

		bool bFloyd = Features::IsInstalled(Id::FloydTracking);
		bool bProfile = Features::IsInstalled(Id::ProfileGetter);
		AddCategory(LoggingPage, L"CONSOLE");
		Row Level = { L"LOG LEVEL", L"How much the console shows." };
		Level.Choices = LogLevels();
		Level.bNeedsRestart = false;
		Level.LiveGet = []
		{
			std::vector<Choice> Levels = LogLevels();
			int32_t Picked = 0;
			for (int32_t i = 0; i < (int32_t)Levels.size(); i++)
			{
				if (SettingsMgr->iLogLevel >= atoi(Levels[i].IniValue))
					Picked = i;
			}
			return Picked;
		};
		Level.LiveSet = [](int32_t Value) { SettingsMgr->iLogLevel = atoi(LogLevels()[Value].IniValue); };
		AddMenuRow(LoggingPage, std::move(Level)).bInGame = true;
		AddFlagRow(LoggingPage, L"DEBUG LOGS", L"Shows logs up to the DEBUG level whatever the LOG LEVEL is.", SettingsMgr->bDebug).bInGame = true;
		AddFlagRow(LoggingPage, L"FLOYD DEBUG", L"Shows Floyd dev logs: hash inputs, seeds and extracted IDs.", SettingsMgr->bFloydDebug, bFloyd, L"FLOYD TRACKING").bInGame = true;
		AddFlagRow(LoggingPage, L"PROFILE DEBUG", L"Shows profile dev logs: platform, platform ID and save key.", SettingsMgr->bProfileDebug, bProfile, L"PROFILE GETTER");
		AddCategory(LoggingPage, L"FLOOD");
		AddFlagRow(LoggingPage, L"FNAME FLOOD", L"At LOG LEVEL FLOOD, logs every name lookup.", SettingsMgr->Floods.bFName).bInGame = true;
		AddFlagRow(LoggingPage, L"CURL FLOOD", L"At LOG LEVEL FLOOD, logs every web request.", SettingsMgr->Floods.bCurl);
		AddFlagRow(LoggingPage, L"ENDPOINT FLOOD", L"At LOG LEVEL FLOOD, logs every server endpoint lookup.", SettingsMgr->Floods.bEndpoint);

		auto& Info = HookMetadata::UserProfileInfo;
		auto Text = [](wchar_t** Field) { return [Field] { return *Field ? std::wstring(*Field) : std::wstring(); }; };
		AddValueRow(AccountPage, L"PLATFORM", L"The platform your profile is on.", Text(&Info.Platform), Id::ProfileGetter);
		AddValueRow(AccountPage, L"PLATFORM ID", L"Your account ID on that platform, online or offline.", Text(&Info.PlatformId), Id::ProfileGetter, false,
			[] { return std::wstring(L"Read when your save loads, or on the main menu."); });
		auto IdRead = []
		{
			if (OnlineIds::HasOnlineCheck() && !OnlineIds::IsOnline())
				return std::wstring(L"Offline: the game isn't signed in to MK1's servers. It shows once it is.");
			return std::wstring(OnlineIds::GaveUp() ? L"The game couldn't sign in online. It shows once it does, back on the main menu."
				: L"Read from the game as it signs in online, on the main menu.");
		};
		AddInfoRow(AccountPage, L"ONLINE", [](Row& row)
		{
			std::wstring About = L"Whether the game is signed in to MK1's servers. Offline, Floyd's challenges come from your platform account alone.";
			if (!OnlineIds::HasOnlineCheck() || !Features::IsActive(Id::ProfileGetter))
			{
				SetInfo(row, NOT_READ, About + L"\n\n" + (Features::IsActive(Id::ProfileGetter) ? L"Can't be read in this version of the game."
					: MK_RED(L"Needs PROFILE GETTER on.")));
				return;
			}
			bool bOnline = OnlineIds::IsOnline();
			SetInfo(row, bOnline ? L"SIGNED IN" : L"OFFLINE", About);
		});
		AddValueRow(AccountPage, L"HYDRA ID", L"Your MK1 online account ID.", Text(&Info.HydraId), Id::ProfileGetter, false, IdRead);
		AddValueRow(AccountPage, L"WB ID", L"Your WB account ID, when it's linked.", Text(&Info.WBId), Id::ProfileGetter, false, IdRead);

		AddValueRow(TokensPage, L"SAVE KEY", L"The key your save is encrypted with.", Text(&Info.SaveKey), Id::ProfileGetter, true);
		AddInfoRow(TokensPage, L"ACCESS TOKEN", [](Row& row)
		{
			std::wstring Token = Widen(HookMetadata::sUserKeys.AccessToken.c_str());
			std::wstring About = L"The key the game signs in to its server with this session.";
			if (!Token.empty())
				SetInfo(row, L"SELECT TO SEE", About + L"\n\n" MK_BOLD(L"Value: ") + Short(Token, 160) + L"\n\nThe full token is in the MK12TTH overlay menu.");
			else if (!Features::IsInstalled(Id::Curl))
				SetInfo(row, NOT_READ, About + L"\n\n" MK_RED(L"Not available: ") + Features::Explain(Id::Curl));
			else if (!Features::IsActive(Id::Curl))
				SetInfo(row, NOT_READ, About + L"\n\nTurn on WEB CAPTURE in LIVE FEATURES, it's read the next time the game signs in.");
			else
				SetInfo(row, NOT_READ, About + L"\n\nNot read yet, it's read when the game signs in.");
		}).CopyValue = [] { return Widen(HookMetadata::sUserKeys.AccessToken.c_str()); };

		AddInfoRow(FloydPage, L"FLOYD TOWER", [](Row& row)
		{
			std::wstring About = L"When the Floyd Tower shows up in Living Towers.";
			if (!Features::IsActive(Id::FloydTracking))
			{
				SetInfo(row, NOT_READ, About + L"\n\n" MK_RED(L"Needs FLOYD TRACKING on."));
				return;
			}
			std::wstring Label;
			std::wstring Text = FloydTowerText(Label);
			SetInfo(row, Label, About + L"\n\n" + Text);
		}).bInGame = true;
		AddInfoRow(FloydPage, L"FLOYD CHALLENGES", [](Row& row)
		{
			const auto& Floyd = HookMetadata::CurrentFloydInfo;
			const auto& Stats = FloydTowerAlert::LastStats();
			std::wstring About = L"The 10 challenges Floyd's clues point to, numbered like the Floyd tracker.";
			if (!Features::IsActive(Id::FloydTracking) || !Floyd.HasClues)
			{
				SetInfo(row, NOT_READ, About + L"\n\n" + (Features::IsActive(Id::FloydTracking)
					? L"Open Living Towers or start a tower to read them." : MK_RED(L"Needs FLOYD TRACKING on.")));
				return;
			}
			std::wstring Label = Stats.bRead ? std::to_wstring(DoneCount(Floyd.Clues, Stats)) + L" OF 10 DONE" : std::to_wstring(Floyd.Clues.size()) + L" CHALLENGES";
			// The game picks a new list after meeting Floyd, the next time a match starts: until then this one is old
			std::wstring Picked = L"\n\n" MK_BOLD(L"Picked at encounter: ") + std::to_wstring(Floyd.FloydEncounters);
			uint64_t TimesMet = Stats.Values[(size_t)FloydTowerAlert::Stat::Encounters];
			if (Stats.bRead && TimesMet != Floyd.FloydEncounters)
			{
				Label = L"OUT OF DATE";
				Picked += L"\n" + Red(L"Open a match to update your stats.");
			}
			SetInfo(row, Label, About + L"\n\n" + ChallengeList(Floyd.Clues, Stats) + Picked);
		}).bInGame = true;
		AddInfoRow(FloydPage, L"PROFILE HASH", [](Row& row)
		{
			const auto& Floyd = HookMetadata::CurrentFloydInfo;
			wchar_t Hash[16];
			swprintf_s(Hash, L"%08X", Floyd.UserProfileHash);
			bool bRead = Features::IsActive(Id::FloydTracking) && Floyd.HasClues;
			SetInfo(row, bRead ? Hash : NOT_READ, L"The hash Floyd's challenges are picked from.");
		}).CopyValue = []
		{
			const auto& Floyd = HookMetadata::CurrentFloydInfo;
			if (!Features::IsActive(Id::FloydTracking) || !Floyd.HasClues)
				return std::wstring();
			wchar_t Hash[16];
			swprintf_s(Hash, L"%08X", Floyd.UserProfileHash);
			return std::wstring(Hash);
		};

		using FloydTowerAlert::Stat;
		AddFloydStatRow(L"FLOYD WINS", L"How many times you beat Floyd. The Floyd Tower only shows up once you have.",
			[](Row& row, const FloydTowerAlert::FloydStats&) { SetInfo(row, FloydStatText(Stat::Victories), L"How many times you beat Floyd. The Floyd Tower only shows up once you have."); });
		AddFloydStatRow(L"TIMES MET FLOYD", L"How many times Floyd showed up.",
			[](Row& row, const FloydTowerAlert::FloydStats&) { SetInfo(row, FloydStatText(Stat::Encounters), L"How many times Floyd showed up."); });
		AddFloydStatRow(L"CHALLENGES DONE", L"Floyd's challenges you completed, out of your 10.",
			[](Row& row, const FloydTowerAlert::FloydStats& Stats)
			{
				std::vector<uint8_t> Challenges = KnownChallenges(Stats);
				std::wstring Text = L"Floyd's challenges you completed, out of your 10.\n\n" + ChallengeList(Challenges, Stats);
				if (!HookMetadata::CurrentFloydInfo.HasClues)
					Text += L"\n\nThe ones left show once Floyd's challenges are read: open Living Towers or start a tower.";
				SetInfo(row, std::to_wstring(DoneCount(Challenges, Stats)) + L" OF 10", Text);
			});
		AddFloydStatRow(L"LAST FLOYD FIGHT", L"How your last fight with Floyd went.",
			[](Row& row, const FloydTowerAlert::FloydStats& Stats)
			{
				uint32_t Value = (uint32_t)Stats.Values[(size_t)Stat::LastBattle];
				const wchar_t* Label = Value == 9 ? L"LOST" : Value == 12 ? L"WON" : Value == 11 ? L"DIDN'T FINISH" : L"NONE YET";
				SetInfo(row, Label, L"How your last fight with Floyd went.");
			});
		AddFloydStatRow(L"NEXT CLUE", L"When Floyd may show up again to give you a clue.",
			[](Row& row, const FloydTowerAlert::FloydStats& Stats)
			{
				uint32_t Since = (uint32_t)(Stats.Values[(size_t)Stat::MatchesSinceSeen] % 501);
				std::wstring Text = Since < 100
					? L"At least " + std::to_wstring(100 - Since) + L" more fights before Floyd may show up to give you a clue."
					: L"Floyd shows up within the next " + std::to_wstring(500 - Since) + L" fights. Even then, it may not be the challenge you need.";
				SetInfo(row, Since < 100 ? L"IN " + std::to_wstring(100 - Since) + L"+ FIGHTS" : L"WITHIN " + std::to_wstring(500 - Since), Text);
			});
	}

	static void RefreshInfoRows()
	{
		if (bInitialized)
			FloydTowerAlert::Refresh(); // Floyd fights change them, read them as a screen opens (not at startup)
		for (Row* row : AllRows)
		{
			if (row->IsInfo())
				row->Refresh(*row);
			if (row->Describe)
				row->Describe(*row);
		}
	}

	// The pause menu's page: the rows marked for fights, each under its own header
	static void AddInGamePage()
	{
		Row* Header = nullptr;
		for (Row* row : LivePage.Rows)
		{
			if (row->IsCategory())
				Header = row;
			else if (row->bInGame)
			{
				for (Page* page : { &InGamePage, &InGameDebugPage })
				{
					if (Header)
						page->Rows.push_back(Header);
					page->Rows.push_back(row);
				}
				Header = nullptr;
			}
		}
		// Floyd's rows to look at, under a FLOYD header
		bool bFloydHeader = false;
		for (Row* row : FloydPage.Rows)
		{
			if (row->IsCategory() || !row->bInGame)
				continue;
			if (!bFloydHeader)
			{
				MenuRows.push_back({ L"FLOYD" });
				InGamePage.Rows.push_back(&MenuRows.back());
				InGameDebugPage.Rows.push_back(&MenuRows.back());
				bFloydHeader = true;
			}
			InGamePage.Rows.push_back(row);
			InGameDebugPage.Rows.push_back(row);
		}
		// Logging rows only for debugging, so they can't be changed by mistake: one LOGGING header for all of them
		bool bHeader = false;
		for (Row* row : LoggingPage.Rows)
		{
			if (row->IsCategory() || !row->bInGame)
				continue;
			if (!bHeader)
			{
				MenuRows.push_back({ L"LOGGING" });
				InGameDebugPage.Rows.push_back(&MenuRows.back());
				bHeader = true;
			}
			InGameDebugPage.Rows.push_back(row);
		}
	}

	// A SECTION row first on each page of a group with several: its value is the page showing, left / right switch.
	// The switch waits for the next frame, the screen is still handling the press when the value changes
	static Group* PendingGroup = nullptr;
	static size_t PendingSection = 0;

	static void AddSectionRows()
	{
		for (Group* group : AllGroups())
		{
			if (group->Pages.size() < 2)
				continue;
			Row row = { SECTION_TITLE, L"What this page shows. Left and right switch it." };
			row.bNeedsRestart = false;
			for (const Page* page : group->Pages)
				row.Choices.push_back({ page->TabKey, "" });
			row.LiveGet = [group] { return (int32_t)group->Current; };
			row.LiveSet = [group](int32_t Value) { PendingGroup = group; PendingSection = (size_t)Value; };
			MenuRows.push_back(std::move(row));
			Row* Section = &MenuRows.back();
			for (Page* page : group->Pages)
			{
				// A header under it, so the page reads as its own part: SECTION GENERAL, then GENERAL
				bool bOwnHeader = !page->Rows.empty() && page->Rows.front()->IsCategory() && !wcscmp(page->Rows.front()->Title, page->TabKey);
				if (!bOwnHeader)
				{
					MenuRows.push_back({ page->TabKey });
					page->Rows.insert(page->Rows.begin(), &MenuRows.back());
				}
				page->Rows.insert(page->Rows.begin(), Section);
			}
		}
	}

	void Init(bool bWithMods)
	{
		AddSettingsRows();
		if (bWithMods)
			AddModRows();
		AddMenuRows();
		AddInGamePage();
		AddSectionRows();

		for (Page* page : AllPages())
		{
			for (Row* row : page->Rows)
			{
				if (std::find(AllRows.begin(), AllRows.end(), row) == AllRows.end())
					AllRows.push_back(row);
			}
		}
		for (Row* row : AllRows)
		{
			if (row->IsIni())
				row->Value = row->StartValue = ReadValue(*row);
		}
		RefreshInfoRows();
		bInitialized = true;
	}

	bool IsReady()
	{
		return bBuilt;
	}

	bool IsAvailable()
	{
		return bInitialized;
	}

	bool HasOwnMenu()
	{
		return bInitialized;
	}

	void SetOwnMenu(bool bOpen)
	{
		bOwnMenu = bOpen;
	}

	static std::wstring ModVersionText;
	static std::wstring OwnVersionText; // Read by the screen right after we hand it over, rebuilt on each open
	static MK12::FString OwnVersion = { nullptr, 0, 0 };

	void SetModVersion(const char* Version)
	{
		ModVersionText.assign(Version, Version + strlen(Version));
	}

	const std::wstring& ModVersion()
	{
		return ModVersionText;
	}

	MK12::FString* VersionText(MK12::FString* GameVersion)
	{
		if (!bOwnMenu || ModVersionText.empty())
			return GameVersion;

		OwnVersionText = L"MK12TTH " + ModVersionText;
		OwnVersion = { OwnVersionText.data(), (int32_t)OwnVersionText.size() + 1, (int32_t)OwnVersionText.size() + 1 };
		return &OwnVersion; // The screen copies it
	}

	// Called by the screen with our Row as the context
	static int32_t __fastcall GetValue(void* Context, int64_t*)
	{
		Row& row = *(Row*)Context;
		return row.IsLive() ? row.LiveGet() : row.Value;
	}

	static void __fastcall SetValue(void* Context, int32_t* Value, int64_t*)
	{
		Row& row = *(Row*)Context;
		if (!Value || *Value < 0 || *Value >= (int32_t)row.Choices.size() || (row.IsIni() && *Value == row.Value))
			return;

		row.Value = *Value;
		if (row.IsInfo())
			return;
		if (row.IsLive())
		{
			row.LiveSet(*Value);
			return;
		}
		WriteValue(row);
		if (row.bNeedsRestart)
			LastChangeTick = GetTickCount64();
	}

	static int32_t __fastcall GetDefault(void* Context)
	{
		return ((Row*)Context)->Default;
	}

	// The game's rows are static objects: found once without a screen, or on the screen up now
	static MK12::FSettingsOption* FindGameRow(MK12::USettingsScreen* Screen, int32_t Type, const wchar_t* Key)
	{
		if (MK12::FSettingsOption* Static = MK12::FindStaticOption(Key, Type))
			return Static;
		for (int32_t p = 0; Screen && p < Screen->Pages.ElementsCount; p++)
		{
			MK12::FSettingsPage& Page = Screen->Pages.Data[p];
			for (int32_t o = 0; o < Page.Count; o++)
			{
				MK12::FSettingsOption* Option = Page.Options[o];
				if (Option && Option->Type == Type && Option->Key && !wcscmp(Option->Key, Key))
					return Option;
			}
		}
		return nullptr;
	}

	// Confirm on one of our action rows, on the game thread while the screen ticks
	static bool __fastcall ActivateRow(void* Context, void*)
	{
		Row& row = *(Row*)Context;
		if (row.Action)
			row.Action();
		return true;
	}

	struct Templates {
		MK12::FSettingsOption* List = nullptr;
		MK12::FSettingsOption* Category = nullptr;
		MK12::FSettingsOption* Link = nullptr; // Null when the screen has none, action rows say so instead
	};

	static MK12::FSettingsOption* BuildGameRow(Row& row, int32_t Id, const Templates& From)
	{
		GameRows.emplace_back();
		GameRow& game = GameRows.back();
		bool bLink = row.IsAction() && From.Link;
		size_t Size = row.IsCategory() ? MK12::FSettingsOption::Size : bLink ? MK12::FSettingsLinkOption::Size : MK12::FSettingsListOption::Size;
		game.Memory.reset(new char[Size]);
		memcpy(game.Memory.get(), row.IsCategory() ? (void*)From.Category : bLink ? (void*)From.Link : (void*)From.List, Size);

		auto* Option = (MK12::FSettingsOption*)game.Memory.get();
		Option->Table = TEXT_TABLE;
		Option->Key = row.Title;
		Option->bLocalizedTitle = false;
		Option->Id = Id;
		memset(Option->Callbacks, 0, sizeof(Option->Callbacks));

		if (bLink)
		{
			auto* Link = (MK12::FSettingsLinkOption*)Option;
			Link->Activate = ActivateRow;
			Link->Context = &row;
			return Option;
		}
		if (row.IsAction())
			row.Choices = { { L"UNAVAILABLE", "", L"This version of the game has no buttons to copy." } };

		if (!row.IsCategory())
		{
			for (int32_t v = 0; v < (int32_t)row.Choices.size(); v++)
			{
				// The game puts the picked choice's text right after the row's, a blank line keeps them apart
				const wchar_t* Text = row.Choices[v].Description;
				if (Text && *Text && row.Description && *row.Description)
				{
					game.Texts.push_back(L"\n\n" + std::wstring(Text));
					Text = game.Texts.back().c_str();
				}
				game.Values.push_back({ nullptr, row.Choices[v].Label, Text, v, false });
			}

			auto* List = (MK12::FSettingsListOption*)Option;
			List->Values.Data = game.Values.data();
			List->Values.ElementsCount = (int32_t)game.Values.size();
			List->Values.Size = (int32_t)game.Values.size();
			List->bLocalizedValues = false;
			List->bSkipLast = false;
			List->Get = GetValue;
			List->GetContext = &row;
			List->Set = SetValue;
			List->SetContext = &row;
			List->Default = GetDefault;
			List->DefaultContext = &row;
		}
		return Option;
	}

	static bool Build(MK12::USettingsScreen* Screen)
	{
		Templates From;
		From.List = FindGameRow(Screen, MK12::SettingsOptionType::List, LIST_TEMPLATE_KEY);
		From.Category = FindGameRow(Screen, MK12::SettingsOptionType::Category, CATEGORY_TEMPLATE_KEY);
		if (!From.List || !From.Category)
		{
			printfError("Mod Settings: Couldn't find the game's rows to copy, no MK12TTH tab");
			return false;
		}
		for (const wchar_t* Key : LINK_TEMPLATE_KEYS)
		{
			if (!From.Link)
				From.Link = FindGameRow(Screen, MK12::SettingsOptionType::Link, Key);
		}
		if (!From.Link)
			printfWarning("Mod Settings: Couldn't find the game's buttons to copy, MODS has no buttons");

		int32_t Id = FIRST_ROW_ID;
		for (Page* page : AllPages())
		{
			for (Row* row : page->Rows)
				page->GameRows.push_back(BuildGameRow(*row, Id++, From));
		}
		return true;
	}

	// Same growth the game uses: bump the count, grow when it passes the capacity, then fill the new slot
	template <typename T>
	static T& AddSlot(MK12::TArray<T>& Array)
	{
		int32_t Index = Array.ElementsCount++;
		if (Array.ElementsCount > Array.Size)
			MK12::GrowArray(&Array);
		return Array.Data[Index];
	}

	static MK12::FSettingsPage GamePage(const Page& page)
	{
		return { const_cast<MK12::FSettingsOption**>(page.GameRows.data()), (int32_t)page.GameRows.size(), 0 };
	}

	static void AddPage(MK12::USettingsScreen* Screen, Group& group)
	{
		if (ShownGroups.empty())
			FirstOwnPage = Screen->Pages.ElementsCount;
		ActiveScreen = Screen;
		// The game frees page descriptions with the screen, so the key goes in the game's own memory
		int32_t KeyCount = (int32_t)wcslen(group.DescKey) + 1;
		MK12::FString Key = { nullptr, 0, 0 };
		MK12::StringReserve(&Key, KeyCount);
		memcpy(Key.Data, group.DescKey, KeyCount * sizeof(wchar_t));
		Key.Count = KeyCount;

		AddSlot(Screen->Pages) = GamePage(group.Shown());
		AddSlot(Screen->PageDescriptions) = Key;
		AddedGroups.push_back(&group);
		ShownGroups.push_back(&group);
	}

	// Our menu shows only our pages. The game's pages point at static rows, only their description keys are freed
	static void DropGamePages(MK12::USettingsScreen* Screen)
	{
		for (int32_t i = 0; i < Screen->PageDescriptions.ElementsCount; i++)
		{
			if (Screen->PageDescriptions.Data[i].Data)
				MK12::MemoryFree(Screen->PageDescriptions.Data[i].Data);
		}
		Screen->PageDescriptions.ElementsCount = 0;
		Screen->Pages.ElementsCount = 0;
	}

	// Our rows for the game, built once. Without a screen it needs the game's static rows
	static bool EnsureBuilt(MK12::USettingsScreen* Screen)
	{
		if (!bInitialized || bBuildFailed)
			return false;
		if (!bBuilt)
		{
			bBuilt = Build(Screen);
			bBuildFailed = !bBuilt && Screen; // Without a screen it tries again on the next one
		}
		return bBuilt;
	}

	void OnPagesBuilt(MK12::USettingsScreen* Screen)
	{
		AddedGroups.clear();
		ShownGroups.clear();
		if (!Screen || !MK12::GrowArray || !EnsureBuilt(Screen))
			return;

		if (bOwnMenu && MK12::MemoryFree)
		{
			DropGamePages(Screen);
			RefreshInfoRows();
			for (Group* group : Entries(MenuGroups()))
			{
				if (!group->Shown().Rows.empty())
					AddPage(Screen, *group);
			}
			return;
		}
		for (Group* group : Entries(SettingsGroups()))
			AddPage(Screen, *group);
	}

	static void ToGameText(MK12::FText* Out, const wchar_t* Text)
	{
		int32_t Count = (int32_t)wcslen(Text) + 1;
		MK12::FString String = { (wchar_t*)Text, Count, Count };
		MK12::TextFromString(Out, &String); // Copies the string
	}

	// The screen only gives us the title. The Settings tab and our menu share some titles, so the pages showing
	// right now are searched first
	static const Row* FindRow(const wchar_t* Title, size_t Length)
	{
		auto Matches = [&](const Row* row) { return wcslen(row->Title) == Length && !wcsncmp(row->Title, Title, Length); };
		for (const Group* group : ShownGroups)
		{
			for (const Page* page : group->Pages)
			{
				for (const Row* row : page->Rows)
				{
					if (Matches(row))
						return row;
				}
			}
		}
		for (const Row* row : AllRows)
		{
			if (Matches(row))
				return row;
		}
		return nullptr;
	}

	bool FindText(MK12::FText* Out, const wchar_t* Table, const wchar_t* Key)
	{
		if (!Key)
			return false;

		if (Table != TEXT_TABLE)
		{
			const wchar_t* Text = nullptr;
			std::vector<Group*> Groups = ShownGroups;
			for (Group* group : AllGroups())
				Groups.push_back(group);
			for (Group& group : SplitGroups)
				Groups.push_back(&group);
			for (const Group* group : Groups)
			{
				if (!Text && Key[0] == L'M' && !wcscmp(Key, group->DescKey))
					Text = group->Shown().Description;
			}
			if (bOwnMenu && Key[0] == L'S' && !wcscmp(Key, MENU_TITLE_KEY))
				Text = OWN_MENU_TITLE;
			if (!Text)
				return false;
			ToGameText(Out, Text);
			return true;
		}

		// Title, or title + "_desc" for the description panel
		size_t Length = wcslen(Key);
		size_t SuffixLength = wcslen(DESCRIPTION_SUFFIX);
		bool bDescription = Length > SuffixLength && !wcscmp(Key + Length - SuffixLength, DESCRIPTION_SUFFIX);
		const Row* row = FindRow(Key, bDescription ? Length - SuffixLength : Length);

		const wchar_t* Text = L"";
		if (row)
			Text = bDescription ? (row->Description ? row->Description : L"") : row->Title;
		ToGameText(Out, Text);
		return true;
	}

	void CopyTabs(void* Out, MK12::FSettingsTab* Tabs, int32_t Count, int32_t ExtraA, int32_t ExtraB)
	{
		if (AddedGroups.empty())
		{
			MK12::SettingsCopyTabs(Out, Tabs, Count, ExtraA, ExtraB);
			return;
		}

		// The game copies the tabs, so ours live on a copy of the list and go away after. A tab shows the page
		// with the same index, so in our menu only our tabs are passed
		std::vector<MK12::FSettingsTab> WithOurs;
		if (!bOwnMenu)
			WithOurs.assign(Tabs, Tabs + Count);
		size_t FirstOurs = WithOurs.size();
		for (size_t i = 0; i < AddedGroups.size(); i++)
		{
			MK12::FMenuEntry Entry = {};
			MK12::FMenuEntryCallback NoCallback;
			MK12::MenuEntryConstruct(&Entry, AddedGroups[i]->TabKey, &NoCallback);
			Entry.bLocalized = false; // Shows the key as the label

			WithOurs.push_back({});
			MK12::SettingsTabConstruct(&WithOurs.back(), &Entry, TabOrder + (int32_t)i);
		}
		AddedGroups.clear();

		MK12::SettingsCopyTabs(Out, WithOurs.data(), (int32_t)WithOurs.size(), ExtraA, ExtraB);
		for (size_t i = FirstOurs; i < WithOurs.size(); i++)
			MK12::SettingsTabDestruct(&WithOurs[i]);
	}

	static bool HasSections()
	{
		return MK12::SettingsUnfocusRow && MK12::SettingsShowPage && MK12::SettingsRefreshFooter;
	}

	static MK12::USettingsScreen* ScreenUp()
	{
		return ActiveScreen && ActiveScreen->bOpen ? ActiveScreen : nullptr;
	}

	static Group* GroupOnScreen(MK12::USettingsScreen* Screen)
	{
		int32_t Index = Screen->CurrentPage - FirstOwnPage;
		return Index >= 0 && Index < (int32_t)ShownGroups.size() ? ShownGroups[Index] : nullptr;
	}

	// The game's own page switch steps: rows of the page showing are swapped for the section's
	static void ShowSection(MK12::USettingsScreen* Screen, Group& group, size_t Section)
	{
		MK12::SettingsUnfocusRow(Screen);
		group.Current = Section;
		Screen->Pages.Data[Screen->CurrentPage] = GamePage(group.Shown());
		MK12::SettingsShowPage(Screen);
		if (MK12::SettingsFocusRows && !Screen->bNoRowFocusA && !Screen->bNoRowFocusB && !Screen->bNoRowFocusC)
			MK12::SettingsFocusRows(Screen, true);
		MK12::SettingsRefreshFooter(Screen);
	}

	static void TickSections()
	{
		Group* group = PendingGroup;
		PendingGroup = nullptr;
		MK12::USettingsScreen* Screen = group && HasSections() ? ScreenUp() : nullptr;
		if (Screen && GroupOnScreen(Screen) == group && PendingSection < group->Pages.size() && PendingSection != group->Current)
			ShowSection(Screen, *group, PendingSection);
	}

	// COPY: J on keyboard, X / Square on a controller. Free on our pages, the game only uses it on recording rows
	constexpr uint32_t COPY_INPUT = 0x40;
	constexpr uint8_t COPY_INPUT_BIT = 6;

	// Our row the screen has highlighted, null on the game's pages
	static Row* FocusedOwnRow(MK12::USettingsScreen* Screen)
	{
		Group* group = Screen && Screen == ScreenUp() ? GroupOnScreen(Screen) : nullptr;
		if (!group)
			return nullptr;
		const Page& page = group->Shown();
		int32_t Index = Screen->FocusedRow;
		return Index >= 0 && Index < (int32_t)page.Rows.size() ? page.Rows[Index] : nullptr;
	}

	static std::wstring CopyText(Row* row)
	{
		return row && row->CopyValue ? row->CopyValue() : std::wstring();
	}

	static bool ToClipboard(const std::wstring& Text)
	{
		if (!OpenClipboard(nullptr))
			return false;
		EmptyClipboard();
		size_t Bytes = (Text.size() + 1) * sizeof(wchar_t);
		HGLOBAL Memory = GlobalAlloc(GMEM_MOVEABLE, Bytes);
		bool bDone = false;
		if (Memory)
		{
			memcpy(GlobalLock(Memory), Text.c_str(), Bytes);
			GlobalUnlock(Memory);
			bDone = SetClipboardData(CF_UNICODETEXT, Memory) != nullptr;
			if (!bDone)
				GlobalFree(Memory);
		}
		CloseClipboard();
		return bDone;
	}

	// Grows a game array by one with the game's allocator, the new slot zeroed
	template <typename T>
	static T* AppendRaw(MK12::TArray<T>& Array)
	{
		if (Array.ElementsCount >= Array.Size)
		{
			int32_t Max = Array.ElementsCount + 4;
			T* Data = (T*)MK12::Realloc(Array.Data, (size_t)Max * sizeof(T), 0);
			if (!Data)
				return nullptr;
			Array.Data = Data;
			Array.Size = Max;
		}
		T* Slot = &Array.Data[Array.ElementsCount++];
		memset(Slot, 0, sizeof(T));
		return Slot;
	}

	void OnFooterBuilt(MK12::USettingsScreen* Screen, MK12::TArray<MK12::FText>* Labels, void* FocusedRow, MK12::TArray<uint8_t>* Bits)
	{
		if (!Labels || !Bits || !FocusedRow || !MK12::PromptText || !MK12::Realloc || !MK12::TextFromString)
			return;
		Row* row = FocusedOwnRow(Screen);
		if (CopyText(row).empty())
			return;
		MK12::FText Label = GameMessageBox::ToGameText(GameMessageBox::Text(L"COPY"));
		MK12::FText* Prompt = AppendRaw(*Labels);
		uint8_t* Bit = Prompt ? AppendRaw(*Bits) : nullptr;
		if (Prompt && Bit)
		{
			MK12::PromptText(Prompt, COPY_INPUT, &Label);
			*Bit = COPY_INPUT_BIT;
		}
		else if (Prompt)
			Labels->ElementsCount--; // Both or neither, the footer pairs them by index
		Label.Release();
	}

	static void TickCopy(MK12::FPlayerInput* Input)
	{
		uint32_t Pressed = Input->Down & ~Input->Previous & COPY_INPUT;
		if (!Pressed)
			return;
		Row* row = FocusedOwnRow(ActiveScreen);
		std::wstring Text = CopyText(row);
		if (Text.empty())
			return;
		Input->Previous |= Pressed; // Ours, the screen doesn't see it
		if (ToClipboard(Text))
			GameToast::Show(GameMessageBox::Text(L"Copied"), GameMessageBox::Text(std::wstring(row->Title) + L" is on your clipboard."), nullptr);
	}

	bool HasInGameRows()
	{
		return bBuilt && !InGameDebugPage.Rows.empty() && MK12::MemoryFree && MK12::GrowArray;
	}

	void OnPausePanelSetUp(MK12::USettingsScreen* Screen)
	{
		if (!Screen || !HasInGameRows())
			return;
		if (MK12::SettingsUnfocusRow)
			MK12::SettingsUnfocusRow(Screen);
		DropGamePages(Screen);
		AddedGroups.clear();
		ShownGroups.clear();
		RefreshInfoRows();
		// The panel isn't a settings screen run of its own: nothing tells us it closed, so it never becomes the active one
		MK12::USettingsScreen* Active = ActiveScreen;
		Group& InGame = SettingsMgr->bDebug || InGamePage.Rows.empty() ? InGameDebugGroup : InGameGroup;
		AddPage(Screen, InGame);
		ActiveScreen = Active;
		AddedGroups.clear();
		Screen->CurrentPage = 0;
		if (MK12::SettingsShowPage && MK12::SettingsRefreshFooter)
		{
			MK12::SettingsShowPage(Screen);
			MK12::SettingsRefreshFooter(Screen);
		}
	}

	static std::vector<int32_t> RestartValues()
	{
		std::vector<int32_t> Values;
		for (const Row* row : AllRows)
		{
			if (row->IsIni() && row->bNeedsRestart)
				Values.push_back(row->Value);
		}
		return Values;
	}

	static bool NeedsRestart()
	{
		for (const Row* row : AllRows)
		{
			if (row->IsIni() && row->bNeedsRestart && row->Value != row->StartValue)
				return true;
		}
		return false;
	}

	// The store the game was started from, so it comes back the same way
	static std::wstring RelaunchUrl()
	{
		const wchar_t* CommandLine = GetCommandLineW();
		const wchar_t* EpicApp = CommandLine ? wcsstr(CommandLine, L"-epicapp=") : nullptr;
		if (!EpicApp)
			return STEAM_RELAUNCH_URL;

		EpicApp += wcslen(L"-epicapp=");
		std::wstring App;
		while (*EpicApp && *EpicApp != L' ' && *EpicApp != L'"')
			App += *EpicApp++;
		return L"com.epicgames.launcher://apps/" + App + L"?action=launch&silent=true";
	}

	// Asks the game to exit, a hidden helper starts it again once this process is gone
	// The helper's script: waits for this process to end, runs BeforeStart, starts the game again. A file, so any
	// number of steps and any path fit. UTF-8 with a BOM, the way Windows PowerShell reads non-ASCII paths
	static std::wstring WriteRestartScript(const std::wstring& BeforeStart)
	{
		std::wstring Script = L"Wait-Process -Id " + std::to_wstring(GetCurrentProcessId()) + L" -ErrorAction SilentlyContinue\r\n"
			+ BeforeStart + L"Start-Process " + PowerShellQuoted(RelaunchUrl()) + L"\r\n";
		wchar_t Temp[MAX_PATH] = {};
		if (!GetTempPathW(MAX_PATH, Temp))
			return L"";
		std::wstring Path = std::wstring(Temp) + RESTART_SCRIPT_NAME;
		int Size = WideCharToMultiByte(CP_UTF8, 0, Script.c_str(), (int)Script.size(), nullptr, 0, nullptr, nullptr);
		std::string Bytes = "\xEF\xBB\xBF" + std::string(Size, '\0');
		WideCharToMultiByte(CP_UTF8, 0, Script.c_str(), (int)Script.size(), Bytes.data() + 3, Size, nullptr, nullptr);
		HANDLE File = CreateFileW(Path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
		if (File == INVALID_HANDLE_VALUE)
			return L"";
		DWORD Written = 0;
		bool bOk = WriteFile(File, Bytes.data(), (DWORD)Bytes.size(), &Written, nullptr) && Written == Bytes.size();
		CloseHandle(File);
		return bOk ? Path : L"";
	}

	static void Restart(const std::wstring& BeforeStart)
	{
		std::wstring ScriptPath = MK12::lpEngineExitRequested ? WriteRestartScript(BeforeStart) : L"";
		if (ScriptPath.empty())
		{
			GameMessageBox::Confirm(L"Couldn't restart the game. Close it and start it again to apply the changes.", nullptr, L"MK12TTH");
			return;
		}

		std::wstring Command = L"powershell.exe -NoProfile -ExecutionPolicy Bypass -WindowStyle Hidden -File \"" + ScriptPath + L"\"";
		STARTUPINFOW Startup = { sizeof(Startup) };
		PROCESS_INFORMATION Process = {};
		if (!CreateProcessW(nullptr, Command.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW | DETACHED_PROCESS, nullptr, nullptr, &Startup, &Process))
		{
			printfError("Mod Settings: Couldn't start the restart helper (%lu)", GetLastError());
			GameMessageBox::Confirm(L"Couldn't restart the game. Close it and start it again to apply the changes.", nullptr, L"MK12TTH");
			return;
		}
		CloseHandle(Process.hThread);
		CloseHandle(Process.hProcess);

		printfInfo("Mod Settings: Restarting the game");
		*MK12::lpEngineExitRequested = 1;
	}

	void OnScreenOpened()
	{
		OpenScreens++;
	}

	void OnScreenClosed()
	{
		if (OpenScreens > 0)
			OpenScreens--;
		if (!OpenScreens)
		{
			ActiveScreen = nullptr;
			PendingGroup = nullptr;
		}
	}

	void OnInputUpdate(MK12::FPlayerInput* Input)
	{
		if (!Input)
			return;
		// A section picked last frame. Input runs every frame the screen is up
		TickSections();
		TickCopy(Input);
		if (!ActiveScreen || !LastChangeTick || ActiveScreen->CurrentPage < FirstOwnPage)
			return;
		if ((Input->Down & ~Input->Previous) & MK12::MessageBoxInput::Confirm)
			bAskNow = true;
	}

	// ---- The overlay's copy of our pages
	constexpr ULONGLONG OVERLAY_REFRESH_MS = 500; // While it draws
	constexpr ULONGLONG OVERLAY_IDLE_MS = 1000; // It stopped asking: closed

	static std::mutex OverlayLock;
	static std::vector<OverlayPage> OverlayCopy;
	static std::vector<std::pair<const void*, int32_t>> OverlayChanges; // Choice, or -1 to press an action row
	static ULONGLONG OverlayAskedAt = 0;

	// Game text to the overlay's: markup tags dropped, escapes undone, UTF-8
	static std::string OverlayText(const std::wstring& Text)
	{
		std::wstring Out;
		bool bTag = false;
		for (wchar_t c : Text)
		{
			if (c == L'<')
				bTag = true;
			else if (c == L'>')
				bTag = false;
			else if (!bTag)
				Out += c;
		}
		// The escapes our own text uses for the game
		for (auto [From, To] : { std::pair<const wchar_t*, const wchar_t*>{ L"&lt;", L"<" }, { L"&gt;", L">" }, { L"&amp;", L"&" } })
		{
			for (size_t At = Out.find(From); At != std::wstring::npos; At = Out.find(From, At + 1))
				Out.replace(At, wcslen(From), To);
		}
		int Size = WideCharToMultiByte(CP_UTF8, 0, Out.c_str(), (int)Out.size(), nullptr, 0, nullptr, nullptr);
		std::string Narrow(Size, '\0');
		WideCharToMultiByte(CP_UTF8, 0, Out.c_str(), (int)Out.size(), Narrow.data(), Size, nullptr, nullptr);
		return Narrow;
	}

	static OverlayRow ToOverlay(Row& row)
	{
		OverlayRow Out;
		Out.Id = &row;
		Out.Title = OverlayText(row.Title);
		Out.Text = OverlayText(row.IsInfo() ? row.InfoText : (row.Description ? row.Description : L""));
		Out.bCategory = row.IsCategory();
		Out.bAction = row.IsAction();
		if (row.IsInfo())
			Out.Value = OverlayText(row.InfoLabel);
		else if (row.IsLive() && row.LiveGet)
		{
			Out.Picked = row.LiveGet();
			for (const Choice& choice : row.Choices)
				Out.Choices.push_back(OverlayText(choice.Label));
		}
		else if (row.IsIni() && row.Value >= 0 && row.Value < (int32_t)row.Choices.size())
			Out.Value = OverlayText(row.Choices[row.Value].Label) + (row.bNeedsRestart ? " (change it in game)" : "");
		return Out;
	}

	static void TickOverlay()
	{
		std::vector<std::pair<const void*, int32_t>> Changes;
		ULONGLONG AskedAt;
		{
			std::lock_guard<std::mutex> Guard(OverlayLock);
			Changes.swap(OverlayChanges);
			AskedAt = OverlayAskedAt;
		}
		for (auto& [Id, Value] : Changes)
		{
			auto Found = std::find(AllRows.begin(), AllRows.end(), (Row*)Id);
			if (Found == AllRows.end())
				continue;
			Row& row = **Found;
			if (Value < 0 && row.Action)
				row.Action();
			else if (Value >= 0 && row.LiveSet && Value < (int32_t)row.Choices.size())
				row.LiveSet(Value);
		}

		static ULONGLONG LastCopy = 0;
		ULONGLONG Now = GetTickCount64();
		if (!bInitialized || Now - AskedAt > OVERLAY_IDLE_MS || (Changes.empty() && Now - LastCopy < OVERLAY_REFRESH_MS))
			return;
		LastCopy = Now;
		RefreshInfoRows();
		std::vector<OverlayPage> Pages;
		for (Group* group : MenuGroups())
		{
			for (Page* page : group->Pages)
			{
				OverlayPage Out;
				Out.Title = OverlayText(group->Pages.size() > 1 ? std::wstring(group->TabKey) + L": " + page->TabKey : std::wstring(group->TabKey));
				for (Row* row : page->Rows)
				{
					if (row->Title && !wcscmp(row->Title, SECTION_TITLE))
						continue; // The overlay lists every page, no switch needed
					Out.Rows.push_back(ToOverlay(*row));
				}
				Pages.push_back(std::move(Out));
			}
		}
		std::lock_guard<std::mutex> Guard(OverlayLock);
		OverlayCopy.swap(Pages);
	}

	std::vector<OverlayPage> OverlayPages()
	{
		std::lock_guard<std::mutex> Guard(OverlayLock);
		OverlayAskedAt = GetTickCount64();
		return OverlayCopy;
	}

	void OverlayPick(const void* Id, int32_t Choice)
	{
		std::lock_guard<std::mutex> Guard(OverlayLock);
		OverlayChanges.push_back({ Id, Choice });
	}

	void OverlayPress(const void* Id)
	{
		std::lock_guard<std::mutex> Guard(OverlayLock);
		OverlayChanges.push_back({ Id, -1 });
	}

	void Tick()
	{
		TickOverlay();

		// The game's static rows are there from startup: build ours once the main menu is up, so the pause menu
		// has them even when Settings was never opened
		static bool bTriedStatic = false;
		if (!bTriedStatic && bInitialized && GameState::IsMainMenuUp())
		{
			bTriedStatic = true;
			EnsureBuilt(nullptr);
		}

		if (!LastChangeTick)
			return;
		if (!bAskNow && OpenScreens > 0)
			return;
		if (!bAskNow && !MK12::SettingsRun && GetTickCount64() - LastChangeTick < RESTART_ASK_DELAY)
			return;
		bAskNow = false;
		LastChangeTick = 0;

		std::vector<int32_t> Values = RestartValues();
		if (!NeedsRestart() || Values == LastAskedValues)
			return;
		LastAskedValues = Values;

		if (!GameMessageBox::IsReady())
		{
			printfInfo("Mod Settings: Restart the game to apply the changes");
			return;
		}
		GameMessageBox::Ask(L"Some changes apply after a restart.\nRestart the game now?", [](bool Yes)
		{
			if (Yes)
				Restart();
		}, L"MK12TTH");
	}
}
