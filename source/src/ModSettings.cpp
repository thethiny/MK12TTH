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
#include <string>
#include <vector>

namespace ModSettings {
	constexpr int32_t			FIRST_ROW_ID			= 0x7000; // Far from the game's own ids
	constexpr ULONGLONG			RESTART_ASK_DELAY		= 1500; // Without the screen hook: waits for the player to stop changing values
	constexpr const wchar_t*	LIST_TEMPLATE_KEY		= L"RoundTimeTitle"; // Game rows we copy, both on the Gameplay page
	constexpr const wchar_t*	CATEGORY_TEMPLATE_KEY	= L"FightSettingsCategory";
	constexpr const wchar_t*	LINK_TEMPLATE_KEYS[]	= { L"AdjustVideoSettings", L"GraphicsAutoconfig" }; // Buttons, on the graphics pages
	constexpr const wchar_t*	STEAM_RELAUNCH_URL		= L"steam://rungameid/1971870";
	constexpr const wchar_t*	RESTART_SCRIPT_NAME		= L"MK12TTH_Restart.ps1"; // In the user's temp folder

	// Our rows' text table, the screen looks titles and descriptions up in it and we answer
	static const wchar_t TEXT_TABLE[] = L"MK12TTH";
	constexpr const wchar_t*	DESCRIPTION_SUFFIX		= L"_desc"; // Added to a row's title for its description
	constexpr const wchar_t*	PAGE_DESC_KEY			= L"MK12TTH_PageDesc";

	// The markup the game's own descriptions use (Flash text)
	#define MK_BOLD(Text)		L"<FONT FACE=\"$BoldText\">" Text L"</font>"
	#define MK_RED_COLOR		L"#6d0a0a"
	#define MK_GREEN_COLOR		L"#1d5a1d"
	#define MK_RED(Text)		L"<FONT COLOR='" MK_RED_COLOR L"'>" Text L"</font>"
	#define MK_NEEDS(Text)		L"\n\n" MK_BOLD(L"Needs: ") Text
	#define MK_TURNS_ON(Text)	L"\n\n" MK_BOLD(L"Turns on: ") Text

	constexpr const wchar_t* PAGE_DESC = L"Settings for the MK12TTH mod, saved to MK12TTH.ini.\n\n" MK_BOLD(L"Most changes apply after a restart.");

	struct Choice {
		const wchar_t*	Label;
		const char*		IniValue;
		const wchar_t*	Description = L""; // Under the value while it's picked
	};

	struct Row;
	struct Group;
	typedef int32_t			(LiveGetType)(Row& row);
	typedef void			(LiveSetType)(Row& row, int32_t Value);
	typedef bool			(SwitchGetType)(Row& row);
	typedef void			(SwitchSetType)(Row& row, bool bOn);
	typedef void			(RowType)(Row& row);
	typedef void			(ActionType)();
	typedef std::wstring	(RowTextType)(Row& row);
	typedef std::wstring	(TextType)();
	typedef void			(StatShowType)(Row& row, const FloydTowerAlert::FloydStats& Stats);

	struct Row {
		const wchar_t*			Title;
		const wchar_t*			Description		= nullptr;
		const char*				Section			= nullptr; // Null for category rows
		const char*				Key				= nullptr;
		std::vector<Choice>		Choices;
		int32_t					Default			= 0; // Index into Choices, matches sample.ini
		bool					bNeedsRestart	= true;

		int32_t					Value			= 0; // Index into Choices
		int32_t					StartValue		= 0; // What the game runs with
		const char*				IniFile			= ""; // Empty is the mod's own ini

		LiveGetType*			LiveGet			= nullptr; // Live rows: the game runs with the value right away, not saved
		LiveSetType*			LiveSet			= nullptr;
		RowType*				Refresh			= nullptr; // Info rows: read only, fills InfoLabel and InfoText as the screen opens
		wchar_t					InfoLabel[40]	= {};
		std::wstring			InfoText;
		RowType*				Describe		= nullptr; // Any row: rebuilds the description (InfoText) as the screen opens
		ActionType*				Action			= nullptr; // Action rows: no value, Confirm runs it
		RowTextType*			CopyValue		= nullptr; // COPY in the footer, empty while there's nothing to copy
		bool					bInGame			= false; // Also in the pause menu's MOD SETTINGS

		// What the callbacks work on
		SwitchGetType*			GetSwitch		= nullptr;
		SwitchSetType*			SetSwitch		= nullptr;
		Features::Id			Feature			= Features::Id::Count;
		bool*					Flag			= nullptr;
		const PakMods::Mod*		Mod				= nullptr;
		const PakMods::Switch*	ModSwitch		= nullptr;
		Group*					SectionGroup	= nullptr;
		const wchar_t*			About			= nullptr;
		wchar_t**				Field			= nullptr;
		TextType*				ReadValue		= nullptr;
		TextType*				WhenRead		= nullptr;
		bool					bPrivate		= false;
		StatShowType*			ShowStat		= nullptr;

		bool IsIni() const { return Section != nullptr; }
		bool IsLive() const { return LiveSet != nullptr; }
		bool IsInfo() const { return Refresh != nullptr; }
		bool IsAction() const { return Action != nullptr; }
		bool IsCategory() const { return !IsIni() && !IsLive() && !IsInfo() && !IsAction(); }
	};

	static void Restart(const std::wstring& BeforeStart = L"");
	static void AddSettingsRow(const Row& row);
	static const wchar_t* Own(const std::wstring& Text);

	static std::vector<Choice> OffOn()
	{
		std::vector<Choice> Choices;
		Choices.push_back({ L"OFF", "false" });
		Choices.push_back({ L"ON", "true" });
		return Choices;
	}
	constexpr int32_t OFF = 0;
	constexpr int32_t ON = 1;

	// Shared by the saved rows (Settings) and the live ones (MODS)
	static std::vector<Choice> AttractDelays()
	{
		std::vector<Choice> Choices;
		Choices.push_back({ L"GAME", "Game", L"After 30 seconds, like the game." });
		Choices.push_back({ L"1 MINUTE", "60" });
		Choices.push_back({ L"2 MINUTES", "120" });
		Choices.push_back({ L"5 MINUTES", "300" });
		Choices.push_back({ L"10 MINUTES", "600" });
		Choices.push_back({ L"NEVER", "Never", L"Attract mode never starts." });
		return Choices;
	}

	static std::vector<Choice> LogLevels()
	{
		std::vector<Choice> Choices;
		Choices.push_back({ L"NONE", "0", L"Results only." });
		Choices.push_back({ L"VERBOSE", "2", L"Adds addresses and each step." });
		Choices.push_back({ L"DEBUG", "5", L"Adds debug checks." });
		Choices.push_back({ L"FLOOD", "10", L"Adds every call of the sources picked in the FLOOD rows." });
		Choices.push_back({ L"DROWN", "100", L"Everything, the FLOOD rows are ignored." });
		return Choices;
	}

	// The Settings tab: what's hooked at startup (MK12TTH.ini) and the startup values players may want to change.
	// Free text (urls, keys, names) and switches that make no sense in game stay in the ini
	static std::vector<Row*> SettingsRows;

	// Page descriptions: the panel shows about 6 lines
	#define MK_MODS_NOTE		L"\n\n" MK_BOLD(L"Changes apply after a restart.") L"\n" MK_RED(L"Turn mods off before playing online.")
	constexpr const wchar_t*	MODS_DESC			= L"Mods in Content\\Mods, loaded over the game's files." MK_MODS_NOTE;
	constexpr const wchar_t*	PAKS_DESC			= L"Mods in Content\\Paks, loaded by the game in its own order." MK_MODS_NOTE;
	constexpr const wchar_t*	LIVE_DESC			= L"Turn MK12TTH features off and on while you play. Saved for next time.\n\n" MK_BOLD(L"Changes apply right away.");
	constexpr const wchar_t*	LOGGING_DESC		= L"What the console shows. Changes apply right away, for this session only.";
	constexpr const wchar_t*	AUTHOR_URL			= L"http://thethiny.com/social/twitter";
	constexpr const wchar_t*	ABOUT_DESC			= L"MK12TTH adds mod tools and features to Mortal Kombat 1. Made by @thethiny.";
	constexpr const wchar_t*	PROFILE_DESC		= L"Read from your profile while you play. Select a row to see its value.";
	constexpr const wchar_t*	TOKENS_DESC			= L"Read from your profile while you play. Select a row to see its value.\n\n" MK_RED(L"They are private, don't share them.");
	constexpr const wchar_t*	IN_GAME_DESC		= L"MK12TTH features you can change during a fight. Changes apply right away.";
	constexpr const wchar_t*	OWN_MENU_TITLE		= L"MODS";
	constexpr const wchar_t*	MENU_TITLE_KEY		= L"SettingsSubMenuTitle"; // The settings screen's title, ours says MODS
	constexpr int32_t			MOD_ROW_DEFAULT		= ON; // A mod missing from the ini is on

	// A page of our rows and the game's rows built for them. TabKey is its label on the Q/E tab strip
	struct Page {
		const wchar_t*						TabKey;
		const wchar_t*						Description;
		std::vector<Row*>					Rows;
		std::vector<MK12::FSettingsOption*>	GameRows;
	};

	// An entry of the screen's left list. With several pages, a SECTION row at the top of each picks the page (left /
	// right). Without the game's page switch each page gets its own entry (Entries)
	struct Group {
		const wchar_t*		TabKey;
		const wchar_t*		DescKey; // The screen asks for the description by it, the page showing answers
		std::vector<Page*>	Pages;
		size_t				Current = 0;

		Page& Shown() const { return *Pages[Current]; }
	};
	constexpr const wchar_t* SECTION_TITLE = L"SECTION";

	// Rows and pages are never freed, the screen keeps pointers to them
	static std::vector<Row*>	ModRows; // One per pak mod
	static std::vector<Row*>	MenuRows; // Live and info rows of our menu
	static std::vector<Page*>	SettingsPages; // Settings, MK12TTH: a page per category
	static Page ModsFolderPage	= { L"CONTENT\\MODS", MODS_DESC };
	static Page PaksPage		= { L"CONTENT\\PAKS", PAKS_DESC };
	static Page LivePage		= { L"LIVE FEATURES", LIVE_DESC };
	static Page AboutPage		= { L"ABOUT", ABOUT_DESC };
	static Page LoggingPage		= { L"LOGGING", LOGGING_DESC };
	static Page AccountPage		= { L"ACCOUNT", PROFILE_DESC };
	static Page TokensPage		= { L"TOKENS", TOKENS_DESC };
	static Page FloydPage		= { L"FLOYD", PROFILE_DESC };
	static Page InGamePage		= { L"MK12TTH", IN_GAME_DESC }; // Rows marked bInGame, from LIVE FEATURES
	static Page InGameDebugPage	= { L"MK12TTH", IN_GAME_DESC }; // The same plus LOGGING's, while DEBUG LOGS is on

	static Group SettingsGroup		= { TabKey, PAGE_DESC_KEY };
	static Group PakModsGroup		= { L"PAK MODS", L"MK12TTH_PakModsDesc", { &ModsFolderPage, &PaksPage } };
	static Group LiveGroup			= { L"LIVE FEATURES", L"MK12TTH_LiveDesc", { &LivePage } };
	static Group LoggingGroup		= { L"LOGGING", L"MK12TTH_LoggingDesc", { &LoggingPage } };
	static Group AboutGroup			= { L"ABOUT", L"MK12TTH_AboutDesc", { &AboutPage } };
	static Group ProfileGroup		= { L"PROFILE", L"MK12TTH_ProfileDesc", { &AccountPage, &TokensPage, &FloydPage } };
	static Group InGameGroup		= { L"MK12TTH", L"MK12TTH_InGameDesc", { &InGamePage } };
	static Group InGameDebugGroup	= { L"MK12TTH", L"MK12TTH_InGameDesc", { &InGameDebugPage } };

	static std::vector<Group*> SettingsGroups()
	{
		std::vector<Group*> Groups;
		Groups.push_back(&SettingsGroup);
		return Groups;
	}

	static std::vector<Group*> MenuGroups()
	{
		std::vector<Group*> Groups;
		Groups.push_back(&PakModsGroup);
		Groups.push_back(&LiveGroup);
		Groups.push_back(&ProfileGroup);
		Groups.push_back(&LoggingGroup);
		Groups.push_back(&AboutGroup);
		return Groups;
	}

	static std::vector<Group*> AllGroups()
	{
		std::vector<Group*> Groups;
		Groups.push_back(&SettingsGroup);
		Groups.push_back(&PakModsGroup);
		Groups.push_back(&LiveGroup);
		Groups.push_back(&ProfileGroup);
		Groups.push_back(&LoggingGroup);
		Groups.push_back(&AboutGroup);
		Groups.push_back(&InGameGroup);
		Groups.push_back(&InGameDebugGroup);
		return Groups;
	}

	// Without the game's page switch a group of several pages becomes an entry per page, "GROUP: PAGE"
	static std::vector<Group*> SplitGroups;
	static bool HasSections();

	static std::vector<Group*> Entries(const std::vector<Group*>& Groups)
	{
		if (HasSections())
			return Groups;
		std::vector<Group*> Out;
		for (size_t g = 0; g < Groups.size(); g++)
		{
			Group* group = Groups[g];
			if (group->Pages.size() == 1)
			{
				Out.push_back(group);
				continue;
			}
			for (size_t p = 0; p < group->Pages.size(); p++)
			{
				Page* page = group->Pages[p];
				Group* Same = nullptr;
				for (size_t s = 0; s < SplitGroups.size() && !Same; s++)
				{
					if (SplitGroups[s]->Pages.front() == page)
						Same = SplitGroups[s];
				}
				if (!Same)
				{
					Same = new Group;
					Same->TabKey = Own(std::wstring(group->TabKey) + L": " + page->TabKey);
					Same->DescKey = Own(std::wstring(group->DescKey) + L"_" + page->TabKey);
					Same->Pages.push_back(page);
					SplitGroups.push_back(Same);
				}
				Out.push_back(Same);
			}
		}
		return Out;
	}

	static std::vector<Page*> AllPages()
	{
		std::vector<Page*> Pages;
		std::vector<Group*> Groups = AllGroups();
		for (size_t g = 0; g < Groups.size(); g++)
			Pages.insert(Pages.end(), Groups[g]->Pages.begin(), Groups[g]->Pages.end());
		return Pages;
	}
	static std::vector<Row*> AllRows;
	static std::vector<Group*> AddedGroups; // Entries added for the tab bar being built now
	static std::vector<Group*> ShownGroups; // Entries of the screen up now, after the game's own

	// The game's rows for ours, copies of the game's own rows. Never freed
	struct GameRow {
		char*								Memory;
		std::vector<MK12::FSettingsListValue>	Values;
	};
	static std::vector<GameRow*> GameRows;
	static bool bInitialized = false;
	static bool bBuilt = false;
	static bool bBuildFailed = false;
	static bool bOwnMenu = false; // The screen was opened from our main menu entry: only our pages

	static ULONGLONG LastChangeTick = 0;
	static int OpenScreens = 0; // The restart question waits until they're all closed
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

	static const wchar_t* Own(const std::wstring& Text)
	{ // Text built at runtime, kept for good
		wchar_t* Copy = new wchar_t[Text.size() + 1];
		wcscpy_s(Copy, Text.size() + 1, Text.c_str());
		return Copy;
	}

	static std::wstring Upper(std::wstring Text)
	{
		for (size_t i = 0; i < Text.size(); i++)
			Text[i] = (wchar_t)towupper(Text[i]);
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

	static std::wstring OverrideList(const std::vector<PakMods::Override>& Mods)
	{ // "Mod (12), Other (3)"
		std::wstring Text;
		for (size_t i = 0; i < Mods.size(); i++)
			Text += (Text.empty() ? L"" : L", ") + Widen(Mods[i].Key) + L" (" + std::to_wstring(Mods[i].Files) + L")";
		return Text;
	}

	// What a mod's row says about how much of it the game really uses
	static std::wstring LoadText(const PakMods::Mod& mod)
	{
		const PakMods::LoadInfo& Load = mod.Load;
		std::wstring Text;
		const wchar_t* IfOn = mod.bEnabled ? L"" : L"If you turn it on: ";

		if (!Load.bContentsRead)
			Text += L"\n\nCouldn't read its file list, it may be encrypted.";
		if (!Load.LostTo.empty())
		{
			bool bAll = Load.Lost >= Load.Files;
			Text += L"\n\n" + Red(std::wstring(IfOn) + (bAll ? L"Won't load, another mod overrides it. " : L"Partly overridden. "))
				+ std::to_wstring(Load.Lost) + L" of its " + std::to_wstring(Load.Files) + L" files come from " + OverrideList(Load.LostTo)
				+ L", which load over it.";
		}
		if (!Load.WinsOver.empty())
			Text += L"\n\n" + (MK_BOLD(L"Overrides: ") + OverrideList(Load.WinsOver));
		if (Load.bAssetsInPakOnly)
			Text += L"\n\n" + Red(std::wstring(IfOn) + L"Probably won't load.") + L" Its game assets are in a .pak only, this game loads them from .utoc and .ucas files.";
		if (Load.bAfterGameFiles)
			Text += L"\n\n" + Red(std::wstring(IfOn) + L"Loads after some of the game's own files,") + L" so where they have the same files the game's win. "
				L"Move it into a folder that sorts after " + Load.LastGamePak + L", like pakchunk99-mods.";
		return Text;
	}

	static std::wstring LoadSummary(const PakMods::Mod& mod)
	{ // The short version, under the value while it's ON
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
		for (size_t i = 0; i < File.Values.size(); i++)
			Text += (Text.empty() ? L"" : L", ") + File.Values[i].Key + L"=" + File.Values[i].Text;
		return Text.empty() ? L"none" : Text;
	}

	static std::wstring WhereIs(PakMods::Source From, const std::wstring& Folder)
	{
		std::wstring Root = From == PakMods::Source::ModsFolder ? L"Content\\Mods" : L"Content\\Paks";
		return Folder.empty() ? Root : Root + L"\\" + Folder;
	}

	static std::wstring OrderText(const PakMods::Mod& mod)
	{ // How the mod's place in the load order was picked
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

	static std::wstring PakFiles(const PakMods::Mod& mod)
	{ // One pak: what it has. Several: each by name
		if (mod.Paks.size() == 1)
			return KindText(mod.Paks.front().Type);
		std::wstring Text = std::to_wstring(mod.Paks.size()) + L" paks";
		for (size_t i = 0; i < mod.Paks.size(); i++)
		{
			const PakMods::Container& Pak = mod.Paks[i];
			Text += L"\n" + (Pak.Folder.empty() ? Pak.Name : Pak.Folder + L"/" + Pak.Name) + L": " + KindText(Pak.Type);
		}
		return Text;
	}

	static std::wstring BackSlashes(std::wstring Path)
	{
		std::replace(Path.begin(), Path.end(), L'/', L'\\');
		return Path;
	}

	static std::wstring PlainText(const std::wstring& Text)
	{ // A creator's text shown as is, not as markup
		std::wstring Out;
		for (size_t i = 0; i < Text.size(); i++)
		{
			wchar_t c = Text[i];
			Out += c == L'<' ? L"&lt;" : c == L'>' ? L"&gt;" : c == L'&' ? L"&amp;" : std::wstring(1, c);
		}
		return Out;
	}

	static std::wstring DescriptionText(const std::wstring& Description)
	{
		return Description.empty() ? L"" : L"\n\n" + PlainText(Description);
	}

	static bool IsModTitleTaken(const std::wstring& Title)
	{
		for (size_t i = 0; i < ModRows.size(); i++)
		{
			if (Title == ModRows[i]->Title)
				return true;
		}
		return false;
	}

	static std::wstring UniqueModTitle(std::wstring Title, const std::wstring& Where)
	{ // The screen finds rows by title, two mod rows never share one
		if (IsModTitleTaken(Title))
			Title += L" (" + Upper(Where) + L")";
		for (int i = 2; IsModTitleTaken(Title); i++)
			Title += L" " + std::to_wstring(i);
		return Title;
	}

	static Row& AddModRow(Page& page, const Row& row, const std::wstring& Summary = L"")
	{
		Row* Added = new Row(row);
		Added->IniFile = StateFile::FileName();
		if (!Summary.empty())
			Added->Choices[ON].Description = Own(Summary);
		ModRows.push_back(Added);
		page.Rows.push_back(Added);
		return *Added;
	}

	static std::wstring InsideText(const PakMods::Mod& mod, const PakMods::Switch& Entry)
	{
		if (Entry.bFolder)
		{
			std::wstring Paks;
			for (size_t i = 0; i < mod.Switches.size(); i++)
			{
				const PakMods::Switch& Pak = mod.Switches[i];
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

	static void DescribeInside(Row& row)
	{ // As the menu opens: paks mounted since startup bring their own description.txt
		row.InfoText = InsideText(*row.Mod, *row.ModSwitch);
		row.Description = row.InfoText.c_str();
	}

	// The folders and paks of a mod with several paks, each with its own switch, under the mod's row
	static void AddInsideRows(Page& page, const PakMods::Mod& mod)
	{
		for (size_t i = 0; i < mod.Switches.size(); i++)
		{
			const PakMods::Switch& Entry = mod.Switches[i];
			std::wstring Indent;
			for (int d = 0; d < Entry.Depth; d++)
				Indent += L"- ";
			Row row = { Own(UniqueModTitle(Indent + Upper(Entry.Name), BackSlashes(mod.Name + L"/" + Entry.Path))), nullptr,
				StateFile::MODS, Entry.Key.c_str(), OffOn(), MOD_ROW_DEFAULT };
			row.Mod = &mod;
			row.ModSwitch = &Entry;
			row.Describe = DescribeInside;
			AddModRow(page, row);
		}
	}

	// A mod's own row. A single pak shows its description.txt, a folder of paks its folder's
	static std::wstring ModText(const PakMods::Mod& mod)
	{
		std::wstring Engine;
		std::vector<const EngineConfig::LooseFile*> Files = EngineConfig::FilesFor(mod);
		for (size_t i = 0; i < Files.size(); i++)
			Engine += EngineValues(*Files[i]);
		if (!Engine.empty())
			Engine = L"\n\n" MK_BOLD(L"Engine settings: ") + Engine;
		// A single pak's own description.txt first, else the one in its folder
		std::wstring Description = mod.Paks.size() == 1 && !mod.Paks.front().Description.empty() ? mod.Paks.front().Description : mod.Description;
		std::wstring Inside = mod.Switches.empty() ? L"" : L"\n\nEach pak, and each folder with several paks, has its own switch below.";
		return L"In " + WhereIs(mod.From, BackSlashes(mod.Folder)) + L"." + DescriptionText(Description) + L"\n\n" MK_BOLD(L"Files: ") + PakFiles(mod)
			+ Engine + OrderText(mod) + LoadText(mod) + Inside + L"\n\nTurning it off keeps the game from mounting it after a restart.";
	}

	static void DescribeMod(Row& row)
	{
		row.InfoText = ModText(*row.Mod);
		row.Description = row.InfoText.c_str();
	}

	static void RefreshEmpty(Row& row)
	{
		wcsncpy_s(row.InfoLabel, L"EMPTY", _TRUNCATE);
	}

	// Content\Mods and Content\Paks always show, so players know they're there
	static void AddNoModsRow(Page& page, const wchar_t* Description)
	{
		Row* row = new Row;
		row->Title = L"NO MODS YET";
		row->Description = Description;
		row->bNeedsRestart = false;
		row->Refresh = RefreshEmpty;
		row->Choices.push_back({ row->InfoLabel, "" }); // The label lives in the row
		ModRows.push_back(row);
		page.Rows.push_back(row);
	}

	static void AddActionRow(Page& page, const wchar_t* Title, const wchar_t* Description, ActionType* Action)
	{
		Row* row = new Row;
		row->Title = Title;
		row->Description = Description;
		row->bNeedsRestart = false;
		row->Action = Action;
		ModRows.push_back(row);
		page.Rows.push_back(row);
	}

	static std::wstring PowerShellQuoted(const std::wstring& Text)
	{
		std::wstring Out = L"'";
		for (size_t i = 0; i < Text.size(); i++)
			Out += Text[i] == L'\'' ? std::wstring(L"''") : std::wstring(1, Text[i]);
		return Out + L"'";
	}

	// Content\Paks mods move to Content\Mods with their folder: a pak right in Paks becomes a mod of its own, a folder of
	// paks one mod. The game has them open, they move once it closed, before it starts again
	static std::wstring MovePaksModsScript(size_t& Moved)
	{
		static const wchar_t* Extensions[] = { L".pak", L".utoc", L".ucas", L".sig" };
		std::wstring Script;
		Moved = 0;
		const std::vector<PakMods::Mod>& Mods = PakMods::List();
		for (size_t m = 0; m < Mods.size(); m++)
		{
			const PakMods::Mod& mod = Mods[m];
			if (mod.From != PakMods::Source::Paks)
				continue;
			std::wstring To = PakMods::PathAppend(PakMods::Root(PakMods::Source::ModsFolder), mod.Folder);
			bool bAny = false;
			for (size_t e = 0; e < _countof(Extensions); e++)
			{
				std::wstring From = PakMods::PathAppend(PakMods::PathAppend(PakMods::Root(PakMods::Source::Paks), mod.Folder), mod.Name + Extensions[e]);
				if (GetFileAttributesW(From.c_str()) == INVALID_FILE_ATTRIBUTES)
					continue;
				bAny = true;
				Script += L"New-Item -ItemType Directory -Force -Path " + PowerShellQuoted(To) + L" | Out-Null\r\n";
				Script += L"Move-Item -LiteralPath " + PowerShellQuoted(From) + L" -Destination " + PowerShellQuoted(To) + L" -ErrorAction SilentlyContinue\r\n";
			}
			Moved += bAny;
		}
		return Script;
	}

	static void RestartIfYes(bool bYes)
	{
		if (bYes)
			Restart();
	}

	static void AskRestart()
	{
		GameMessageBox::Ask(L"Restart the game now?", RestartIfYes, L"MK12TTH");
	}

	static void MoveModsIfYes(bool bYes)
	{
		size_t Moved = 0;
		if (bYes)
			Restart(MovePaksModsScript(Moved));
	}

	static void AskMoveMods()
	{
		size_t Count = 0;
		MovePaksModsScript(Count);
		if (!Count)
		{
			GameMessageBox::Confirm(L"Content\\Paks has no mods to move.", nullptr, L"MK12TTH");
			return;
		}
		GameMessageBox::Ask(L"Move " + std::to_wstring(Count) + L" mods from Content\\Paks to Content\\Mods and restart the game?", MoveModsIfYes, L"MK12TTH");
	}

	static void AddModsActions()
	{
		AddActionRow(ModsFolderPage, L"RESTART NOW", L"Restarts the game, so mods you turned on or off apply.", AskRestart);
		AddActionRow(ModsFolderPage, L"MOVE MODS TO MODS FOLDER", L"Moves your mods from Content\\Paks to Content\\Mods, then restarts the game. "
			L"There MK12TTH orders them and shows what each one overrides.\n\nA pak right in Content\\Paks becomes a mod of its own, "
			L"a folder of paks becomes one mod with a switch for each pak.", AskMoveMods);
	}

	static void AddModCategory(Page& page, const wchar_t* Title)
	{
		Row* row = new Row;
		row->Title = Title;
		ModRows.push_back(row);
		page.Rows.push_back(row);
	}

	static bool HasModsIn(PakMods::Source From)
	{
		const std::vector<PakMods::Mod>& Mods = PakMods::List();
		for (size_t i = 0; i < Mods.size(); i++)
		{
			if (Mods[i].From == From)
				return true;
		}
		return false;
	}

	// A row per pak mod titled with its name, with its folder when two share one. A mod with several paks has rows for its
	// folders and paks under it
	static void AddModRows()
	{
		const std::vector<PakMods::Mod>& Mods = PakMods::List();
		AddModsActions();
		AddModCategory(ModsFolderPage, L"MODS");
		AddModCategory(PaksPage, L"PAKS");
		if (!HasModsIn(PakMods::Source::ModsFolder))
			AddNoModsRow(ModsFolderPage, L"Put mods in Content\\Mods: each folder is one mod, with its .pak, .utoc and .ucas files and its Engine.ini. "
				L"A pak right in Content\\Mods is a mod of its own.\n\nMods there load over the game's files and over Content\\Paks, "
				L"MK12TTH orders them, and each can be turned off here.");
		if (!HasModsIn(PakMods::Source::Paks))
			AddNoModsRow(PaksPage, L"No mods in Content\\Paks. Mods go in Content\\Mods, where MK12TTH orders them.");
		for (size_t i = 0; i < Mods.size(); i++)
		{
			const PakMods::Mod& mod = Mods[i];
			Page& page = mod.From == PakMods::Source::ModsFolder ? ModsFolderPage : PaksPage;
			std::wstring Place = mod.Folder.empty() ? (mod.From == PakMods::Source::ModsFolder ? L"Mods" : L"Paks") : mod.Folder;
			Row row = { Own(UniqueModTitle(Upper(mod.Name), Place)), nullptr, StateFile::MODS, mod.Key.c_str(), OffOn(), MOD_ROW_DEFAULT };
			row.Mod = &mod;
			row.Describe = DescribeMod;
			AddModRow(page, row, LoadSummary(mod));
			AddInsideRows(page, mod);
		}
	}

	static Row& AddMenuRow(Page& page, const Row& row)
	{
		Row* Added = new Row(row);
		MenuRows.push_back(Added);
		page.Rows.push_back(Added);
		return *Added;
	}

	static void AddCategory(Page& page, const wchar_t* Title)
	{
		Row row = { Title };
		AddMenuRow(page, row);
	}

	// Live on/off rows
	static int32_t SwitchGet(Row& row) { return row.GetSwitch(row) ? ON : OFF; }
	static void SwitchSet(Row& row, int32_t Value) { row.SetSwitch(row, Value == ON); }
	static int32_t UnavailableGet(Row& row) { return 0; }
	static void UnavailableSet(Row& row, int32_t Value) {}

	// A live on/off row. Off when its hook isn't in, the description says what to turn on
	static Row& AddLiveRow(Page& page, const wchar_t* Title, const wchar_t* Description, SwitchGetType* Get, SwitchSetType* Set,
		bool bAvailable = true, const wchar_t* Needs = nullptr)
	{
		Row row = { Title, Description };
		row.Choices = OffOn();
		row.bNeedsRestart = false;
		if (!bAvailable)
		{
			// The game has no greyed out rows: one value that can't change, the description says why
			row.Choices.clear();
			row.Choices.push_back({ L"UNAVAILABLE", "" });
			if (Needs)
				row.Description = Own(std::wstring(Description) + L"\n\n" MK_RED(L"Needs ") + Needs + MK_RED(L" in Settings, MK12TTH, then a restart."));
			row.LiveGet = UnavailableGet;
			row.LiveSet = UnavailableSet;
			return AddMenuRow(page, row);
		}
		row.GetSwitch = Get;
		row.SetSwitch = Set;
		row.LiveGet = SwitchGet;
		row.LiveSet = SwitchSet;
		return AddMenuRow(page, row);
	}

	static std::wstring NeedsText(Features::Id Which)
	{ // Green when it's running, red when it isn't
		std::wstring Text;
		const std::vector<Features::Id>& Needs = Features::Get(Which).Needs;
		for (size_t i = 0; i < Needs.size(); i++)
		{
			std::wstring Title = Features::Get(Needs[i]).Title;
			Text += (Text.empty() ? L"\n\n" MK_BOLD(L"Needs: ") : L", ") + (Features::IsInstalled(Needs[i]) ? Green(Title) : Red(Title));
		}
		return Text;
	}

	static bool FeatureGet(Row& row) { return Features::IsActive(row.Feature); }
	static void FeatureSet(Row& row, bool bOn) { Features::SetActive(row.Feature, bOn); }

	// A feature's live switch (MODS). Unavailable unless it's installed, the description says why
	static Row& AddFeatureRow(Page& page, Features::Id Which)
	{
		const Features::Definition& Feature = Features::Get(Which);
		bool bInstalled = Features::IsInstalled(Which);
		const wchar_t* Description = bInstalled ? Feature.Description
			: Own(std::wstring(Feature.Description) + NeedsText(Which) + L"\n\n" MK_RED(L"Not available: ") + Features::Explain(Which));
		Row& row = AddLiveRow(page, Feature.Title, Description, FeatureGet, FeatureSet, bInstalled);
		row.Feature = Which;
		return row;
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
			for (size_t i = 0; i < Feature.Modes.size(); i++)
				Choices.push_back({ Feature.Modes[i].Label, Feature.Modes[i].IniValue });
		}
		Row row = { Feature.Title, Own(Text), Feature.IniSection, Feature.IniKey, Choices, Feature.bIniDefault ? ON : OFF };
		AddSettingsRow(row);
	}

	static void AddSettingsCategory(const wchar_t* Title)
	{ // A header inside the current page
		Row* row = new Row;
		row->Title = Title;
		SettingsRows.push_back(row);
		SettingsPages.back()->Rows.push_back(row);
	}

	static void AddSettingsPage(const wchar_t* Title)
	{ // A new page of the Settings tab, its rows follow
		Page* page = new Page;
		page->TabKey = Title;
		page->Description = PAGE_DESC;
		SettingsPages.push_back(page);
		SettingsGroup.Pages.push_back(page);
	}

	static void AddSettingsRow(const Row& row)
	{
		Row* Added = new Row(row);
		SettingsRows.push_back(Added);
		SettingsPages.back()->Rows.push_back(Added);
	}

	static void AddSettingsRow(const wchar_t* Title, const wchar_t* Description, const char* Section, const char* Key, const std::vector<Choice>& Choices, int32_t Default)
	{
		Row row = { Title, Description, Section, Key, Choices, Default };
		AddSettingsRow(row);
	}

	static void AddSettingsRows()
	{
		AddSettingsPage(L"GENERAL");
		AddFeatureSetting(Features::Id::ModLoader);
		AddFeatureSetting(Features::Id::PakMods);
		AddFeatureSetting(Features::Id::EngineConfig);
		AddFeatureSetting(Features::Id::FastBoot);
		AddFeatureSetting(Features::Id::KeepRunning);
		AddFeatureSetting(Features::Id::AttractMode);
		AddSettingsCategory(L"NAMES AND AUDIO");
		AddFeatureSetting(Features::Id::NameTable);
		AddFeatureSetting(Features::Id::StringSwap);
		AddFeatureSetting(Features::Id::Announcer);

		AddSettingsPage(L"FLOYD");
		AddFeatureSetting(Features::Id::FloydTracking);
		std::vector<Choice> TowerModes;
		TowerModes.push_back({ L"GAME", "Game", L"The game's own schedule, read once you open Living Towers. Shows up once the Floyd Tower is unlocked." });
		TowerModes.push_back({ L"CALCULATED", "Calculated", L"Worked out from your profile, the same way the game does." });
		AddSettingsRow(L"FLOYD TOWER TIMES", L"Where the Floyd Tower times come from.", "Patches", "szFloydTowerMode", TowerModes, 0);
		AddFeatureSetting(Features::Id::FloydTowerAlert);
		AddFeatureSetting(Features::Id::FloydFight);
		AddFeatureSetting(Features::Id::FightMetadata);

		AddSettingsPage(L"ONLINE");
		AddFeatureSetting(Features::Id::ProfileGetter);
		AddFeatureSetting(Features::Id::Curl);
		AddFeatureSetting(Features::Id::PrivateServer);

		AddSettingsPage(L"DEBUG");
		AddSettingsRow(L"CONSOLE WINDOW", L"Opens a console window with the mod's logs.",
			"Settings.Debug", "bEnableConsoleWindow", OffOn(), OFF);
		AddSettingsRow(L"LOG LEVEL", L"How much the console shows when the game starts. Change it while playing in MODS, LOGGING.",
			"Settings", "iLogLevel", LogLevels(), 0);
		AddSettingsRow(L"DEBUG LOGS", L"Shows logs up to the DEBUG level whatever the LOG LEVEL is.",
			"Settings.Debug", "bDebug", OffOn(), OFF);
		AddSettingsRow(L"PAUSE ON START", L"Pauses the game at startup until you press OK, so a debugger can attach.",
			"Settings.Debug", "bPauseOnStart", OffOn(), OFF);
		AddSettingsRow(L"FLOYD DEBUG", L"Shows Floyd dev logs: hash inputs, seeds and extracted IDs." MK_NEEDS(L"FLOYD TRACKING"),
			"Settings.Debug", "bFloydDebug", OffOn(), OFF);
		AddSettingsRow(L"PROFILE DEBUG", L"Shows profile dev logs: platform, platform ID and save key." MK_NEEDS(L"PROFILE GETTER"),
			"Settings.Debug", "bProfileDebug", OffOn(), OFF);
		AddSettingsRow(L"DEV LOGS", L"Shows the developers' investigation logs and checks. Some of them only start with the game.",
			"Settings.Debug", "bDevLogs", OffOn(), OFF);
		AddSettingsRow(L"FNAME FLOOD", L"At LOG LEVEL FLOOD, logs every name lookup.",
			"Settings.Debug.FloodToggles", "bFName", OffOn(), OFF);
		AddSettingsRow(L"CURL FLOOD", L"At LOG LEVEL FLOOD, logs every web request.",
			"Settings.Debug.FloodToggles", "bCurl", OffOn(), OFF);
	}

	static bool FlagGet(Row& row) { return *row.Flag; }
	static void FlagSet(Row& row, bool bOn) { *row.Flag = bOn; }

	static Row& AddFlagRow(Page& page, const wchar_t* Title, const wchar_t* Description, bool& Flag, bool bAvailable = true, const wchar_t* Needs = nullptr)
	{
		Row& row = AddLiveRow(page, Title, Description, FlagGet, FlagSet, bAvailable, Needs);
		row.Flag = &Flag;
		return row;
	}

	static void SetInfo(Row& row, const std::wstring& Label, const std::wstring& Text)
	{
		wcsncpy_s(row.InfoLabel, Label.c_str(), _TRUNCATE);
		row.InfoText = Text;
		row.Description = row.InfoText.c_str();
	}

	static Row& AddInfoRow(Page& page, const wchar_t* Title, RowType* Refresh)
	{
		Row Info = { Title };
		Row& row = AddMenuRow(page, Info);
		row.bNeedsRestart = false;
		row.Refresh = Refresh;
		row.Choices.push_back({ row.InfoLabel, "" }); // The screen reads the label as it opens, after Refresh
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

	static std::wstring FieldValue(Row& row)
	{
		return *row.Field ? std::wstring(*row.Field) : std::wstring();
	}

	static std::wstring ReadAsYouPlay()
	{
		return L"Not read yet, the game reads it as you play.";
	}

	static void RefreshValueRow(Row& row)
	{
		std::wstring Value = FieldValue(row);
		std::wstring Text = row.About;
		if (!Value.empty())
		{
			SetInfo(row, row.bPrivate ? L"SELECT TO SEE" : Short(Value, 20), Text + L"\n\n" MK_BOLD(L"Value: ") + Short(Value, 160));
			return;
		}
		if (!Features::IsInstalled(row.Feature))
			Text += L"\n\n" MK_RED(L"Not available: ") + Features::Explain(row.Feature);
		else if (!Features::IsActive(row.Feature))
			Text += L"\n\n" MK_RED(L"Turned off in LIVE FEATURES.");
		else
			Text += L"\n\n" + row.WhenRead();
		SetInfo(row, NOT_READ, Text);
	}

	// A profile value read by one of our hooks. Long values show in full in the description only, private ones only there
	static void AddValueRow(Page& page, const wchar_t* Title, const wchar_t* About, wchar_t** Field, Features::Id Source,
		bool bPrivate = false, TextType* WhenRead = ReadAsYouPlay)
	{
		Row& row = AddInfoRow(page, Title, RefreshValueRow);
		row.About = About;
		row.Field = Field;
		row.Feature = Source;
		row.bPrivate = bPrivate;
		row.WhenRead = WhenRead;
		row.CopyValue = FieldValue;
	}

	static bool IsDone(const FloydTowerAlert::FloydStats& Stats, uint8_t Challenge)
	{
		return FloydTowerAlert::IsChallengeDone(Stats, Challenge);
	}

	// Floyd's challenges for the player, sorted, with their names. Green when done, red when not, plain when the done
	// ones aren't read yet
	static std::wstring ChallengeList(const std::vector<uint8_t>& Challenges, const FloydTowerAlert::FloydStats& Stats)
	{
		std::vector<uint8_t> Sorted = Challenges;
		std::sort(Sorted.begin(), Sorted.end());
		std::wstring Text;
		for (size_t i = 0; i < Sorted.size(); i++)
		{
			uint8_t Challenge = Sorted[i];
			std::wstring Line = std::to_wstring(Challenge) + L". " + Widen(MK12::EnumMaps::Find(MK12::EnumMaps::FloydChallengeMap, (int)Challenge));
			if (Stats.bRead)
				Line = IsDone(Stats, Challenge) ? Green(Line + L", done") : Red(Line);
			Text += (Text.empty() ? L"" : L"\n") + Line;
		}
		return Text;
	}

	// The player's challenges, or only the done ones when the game didn't pick them yet this session
	static std::vector<uint8_t> KnownChallenges(const FloydTowerAlert::FloydStats& Stats)
	{
		const HookMetadata::FloydCluesInfo& Floyd = HookMetadata::CurrentFloydInfo;
		if (Features::IsActive(Features::Id::FloydTracking) && Floyd.HasClues)
			return Floyd.Clues;
		std::vector<uint8_t> Done;
		for (uint8_t Challenge = 1; Stats.bRead && Challenge <= FloydTower::CHALLENGE_COUNT; Challenge++)
		{
			if (IsDone(Stats, Challenge))
				Done.push_back(Challenge);
		}
		return Done;
	}

	static int DoneCount(const std::vector<uint8_t>& Challenges, const FloydTowerAlert::FloydStats& Stats)
	{
		int Count = 0;
		for (size_t i = 0; i < Challenges.size(); i++)
		{
			if (IsDone(Stats, Challenges[i]))
				Count++;
		}
		return Count;
	}

	static std::wstring FloydTowerText(std::wstring& Label)
	{
		time_t Now = time(nullptr);
		FloydTower::Window window;
		FloydTower::Status status = FloydTower::Find(Now, HookMetadata::CurrentTacoInfo.UserProfileHash, SettingsMgr->bFloydTowerFromGame, window);
		if (status == FloydTower::Status::NotUnlocked)
		{
			// The game drops the tower from the schedule after you had your chance at it, not only before the first win
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

	static std::wstring NotAvailable(const wchar_t* About, Features::Id Which)
	{
		return std::wstring(About) + L"\n\n" MK_RED(L"Not available: ") + Features::Explain(Which);
	}

	static int32_t AttractGet(Row& row)
	{
		std::vector<Choice> Delays = AttractDelays();
		for (int32_t i = 0; i < (int32_t)Delays.size(); i++)
		{
			if (AttractMode::FromIni(Delays[i].IniValue) == AttractMode::Delay())
				return i;
		}
		return 0;
	}

	static void AttractSet(Row& row, int32_t Value)
	{ // Saved for next time too
		const char* Picked = AttractDelays()[Value].IniValue;
		AttractMode::SetDelay(AttractMode::FromIni(Picked));
		CIniReader ini("");
		ini.WriteString((char*)"Patches", (char*)"szAttractModeDelay", (char*)Picked);
	}

	static void AddAttractRow()
	{
		const wchar_t* About = L"How long you idle on the title screen or the main menu before attract mode, the demo, starts.";
		if (!AttractMode::IsReady())
		{
			AddLiveRow(LivePage, L"ATTRACT MODE", Own(NotAvailable(About, Features::Id::AttractMode)), nullptr, nullptr, false);
			return;
		}
		Row row = { L"ATTRACT MODE", About };
		row.Choices = AttractDelays();
		row.bNeedsRestart = false;
		row.LiveGet = AttractGet;
		row.LiveSet = AttractSet;
		AddMenuRow(LivePage, row);
	}

	static int32_t AnnouncerGet(Row& row) { return Announcer::Picked(); }
	static void AnnouncerSet(Row& row, int32_t Value) { Announcer::Pick(Value); }

	static Row& AddAnnouncerRow()
	{
		const wchar_t* About = L"The announcer's voice.\n"
			L"GAME: your own setting.\n"
			L"AUTO: in fights, the announcer of the fighter you play. A fighter without one gets your own setting.\n"
			L"Or any announcer, everywhere.";
		if (!Features::IsInstalled(Features::Id::Announcer) || !Announcer::IsReady())
			return AddLiveRow(LivePage, L"ANNOUNCER", Own(NotAvailable(About, Features::Id::Announcer)), nullptr, nullptr, false);
		Row row = { L"ANNOUNCER", About };
		const std::vector<Announcer::Option>& Options = Announcer::Options();
		for (size_t i = 0; i < Options.size(); i++)
			row.Choices.push_back({ Options[i].Label, Options[i].Value });
		row.bNeedsRestart = false;
		row.LiveGet = AnnouncerGet;
		row.LiveSet = AnnouncerSet;
		return AddMenuRow(LivePage, row);
	}

	// A list row of FLOYD FIGHT, unavailable without its hooks
	static void AddFloydFightRow(const wchar_t* Title, const wchar_t* About, const std::vector<FloydFight::Option>& Options,
		LiveGetType* Get, LiveSetType* Set)
	{
		if (!Features::IsInstalled(Features::Id::FloydFight) || !FloydFight::IsReady())
		{
			AddLiveRow(LivePage, Title, Own(NotAvailable(About, Features::Id::FloydFight)), nullptr, nullptr, false);
			return;
		}
		Row row = { Title, About };
		for (size_t i = 0; i < Options.size(); i++)
			row.Choices.push_back({ Options[i].Label, Options[i].Value });
		row.bNeedsRestart = false;
		row.LiveGet = Get;
		row.LiveSet = Set;
		AddMenuRow(LivePage, row).bInGame = true;
	}

	static int32_t FloydLevelGet(Row& row) { return FloydFight::PickedLevel(); }
	static void FloydLevelSet(Row& row, int32_t Value) { FloydFight::PickLevel(Value); }
	static int32_t FloydRetriesGet(Row& row) { return FloydFight::PickedRetries(); }
	static void FloydRetriesSet(Row& row, int32_t Value) { FloydFight::PickRetries(Value); }

	static void AddFloydFightRows()
	{
		AddFloydFightRow(L"FLOYD DIFFICULTY LEVEL", L"Floyd's AI level, from 1 to 19, higher is harder. Used from his next fight, retries included.\n"
			L"GAME: the game's own, 13 at first and lower after each loss.",
			FloydFight::LevelOptions(), FloydLevelGet, FloydLevelSet);
		AddFloydFightRow(L"FLOYD RETRIES", L"How many times you can retry after losing to Floyd before he leaves.\n"
			L"GAME: the game's own, 2.\n"
			L"NONE: one fight, no retry.",
			FloydFight::RetryOptions(), FloydRetriesGet, FloydRetriesSet);
	}

	static void SpawnTowerNow()
	{
		FloydFight::SpawnTower();
		GameMessageBox::Confirm(L"The Floyd Tower shows up the next time you open Living Towers.", nullptr, L"SPAWN FLOYD TOWER NOW");
	}

	static void AddSpawnTowerRow()
	{
		const wchar_t* TowerNow = L"Makes today's Floyd Tower show up the next time you open Living Towers, for as long as the game's own "
			L"would. Once per press: only that visit, and gone if you restart the game first. Needs a Floyd win, like the game's own tower.\n"
			L"Rewards from it might not count, WB's servers know when the tower is really due.";
		bool bTower = Features::IsInstalled(Features::Id::FloydFight) && Features::IsInstalled(Features::Id::FloydTracking);
		if (!bTower)
		{
			AddLiveRow(LivePage, L"SPAWN FLOYD TOWER NOW", Own(NotAvailable(TowerNow,
				Features::IsInstalled(Features::Id::FloydFight) ? Features::Id::FloydTracking : Features::Id::FloydFight)), nullptr, nullptr, false);
			return;
		}
		AddActionRow(LivePage, L"SPAWN FLOYD TOWER NOW", TowerNow, SpawnTowerNow);
	}

	static void SummonIfYes(bool bYes)
	{
		if (!bYes)
			return;
		FloydTowerAlert::SummonResult Result = FloydTowerAlert::Summon();
		const wchar_t* Text = Result == FloydTowerAlert::SummonResult::Summoned ? L"Done. Floyd shows up at the end of your next match where he can."
			: Result == FloydTowerAlert::SummonResult::AlreadyDone ? L"All of Floyd's challenges were already done."
			: Result == FloydTowerAlert::SummonResult::NoProfile ? L"Your profile isn't loaded yet. Try again from the main menu."
			: L"Not available in this version of the game.";
		GameMessageBox::Confirm(Text, nullptr, L"SUMMON FLOYD");
	}

	static void AskSummon()
	{
		GameMessageBox::Ask(L"Mark all of Floyd's challenges done?", SummonIfYes, L"SUMMON FLOYD");
	}

	static void AddSummonRow()
	{
		if (!FloydTowerAlert::CanSummon())
			return;
		AddActionRow(LivePage, L"SUMMON FLOYD", L"Marks all of Floyd's challenges done in your profile on this PC only, nothing is sent "
			L"to WB's servers. Floyd shows up at the end of your next match where he can, and the game syncs as usual after you fight him.", AskSummon);
	}

	static std::wstring FloydStatText(FloydTowerAlert::Stat Which)
	{
		return std::to_wstring(FloydTowerAlert::LastStats().Values[(size_t)Which]);
	}

	static void RefreshFloydStat(Row& row)
	{ // Floyd profile stats, read when Living Towers opens
		const FloydTowerAlert::FloydStats& Stats = FloydTowerAlert::LastStats();
		if (!Stats.bRead)
		{
			std::wstring Why = Features::IsInstalled(Features::Id::FloydTowerAlert) ? L"Read from your profile when you open Living Towers."
				: std::wstring(MK_RED(L"Not available: ")) + Features::Explain(Features::Id::FloydTowerAlert);
			SetInfo(row, NOT_READ, std::wstring(row.About) + L"\n\n" + Why);
			return;
		}
		row.ShowStat(row, Stats);
	}

	static void AddFloydStatRow(const wchar_t* Title, const wchar_t* About, StatShowType* Show)
	{
		Row& row = AddInfoRow(FloydPage, Title, RefreshFloydStat);
		row.About = About;
		row.ShowStat = Show;
	}

	static void RefreshVersion(Row& row)
	{
		SetInfo(row, ModVersion(), L"The MK12TTH version you're running.");
	}

	static void RefreshGameVersion(Row& row)
	{
		int32_t Changelist = MK12::GetChangelist ? MK12::GetChangelist() : 0;
		std::wstring Build = MK12::GetBuildVersion ? MK12::GetBuildVersion() : L"";
		SetInfo(row, Changelist ? std::to_wstring(Changelist) : NOT_READ,
			L"The game build MK12TTH is running on." + (Build.empty() ? L"" : L"\n\n" MK_BOLD(L"Build: ") + Build));
	}

	static void OpenAuthorPage()
	{
		ShellExecuteW(nullptr, L"open", AUTHOR_URL, nullptr, nullptr, SW_SHOWNORMAL);
	}

	static void ShowWhatsNew()
	{
		GameMessageBox::Confirm(Changelog::Current, nullptr, L"MK12TTH " + ModVersion());
	}

	static void AddAboutRows()
	{
		AddInfoRow(AboutPage, L"VERSION", RefreshVersion);
		AddInfoRow(AboutPage, L"GAME VERSION", RefreshGameVersion);
		AddActionRow(AboutPage, L"MADE BY @THETHINY", L"MK12TTH is made and maintained by @thethiny. Opens @thethiny on X in your browser.", OpenAuthorPage);
		AddActionRow(AboutPage, L"WHAT'S NEW", L"Shows what changed in this version of MK12TTH.", ShowWhatsNew);
	}

	static int32_t LogLevelGet(Row& row)
	{
		std::vector<Choice> Levels = LogLevels();
		int32_t Picked = 0;
		for (int32_t i = 0; i < (int32_t)Levels.size(); i++)
		{
			if (SettingsMgr->iLogLevel >= atoi(Levels[i].IniValue))
				Picked = i;
		}
		return Picked;
	}

	static void LogLevelSet(Row& row, int32_t Value)
	{
		SettingsMgr->iLogLevel = atoi(LogLevels()[Value].IniValue);
	}

	static std::wstring OnlineIdWhenRead()
	{
		if (OnlineIds::HasOnlineCheck() && !OnlineIds::IsOnline())
			return std::wstring(L"Offline: the game isn't signed in to MK1's servers. It shows once it is.");
		return std::wstring(OnlineIds::GaveUp() ? L"The game couldn't sign in online. It shows once it does, back on the main menu."
			: L"Read from the game as it signs in online, on the main menu.");
	}

	static std::wstring PlatformIdWhenRead()
	{
		return std::wstring(L"Read when your save loads, or on the main menu.");
	}

	static void RefreshOnline(Row& row)
	{
		std::wstring About = L"Whether the game is signed in to MK1's servers. Offline, Floyd's challenges come from your platform account alone.";
		if (!OnlineIds::HasOnlineCheck() || !Features::IsActive(Features::Id::ProfileGetter))
		{
			SetInfo(row, NOT_READ, About + L"\n\n" + (Features::IsActive(Features::Id::ProfileGetter) ? L"Can't be read in this version of the game."
				: MK_RED(L"Needs PROFILE GETTER on.")));
			return;
		}
		bool bOnline = OnlineIds::IsOnline();
		SetInfo(row, bOnline ? L"SIGNED IN" : L"OFFLINE", About);
	}

	static std::wstring KeyValue(Row& row)
	{
		return row.ReadValue();
	}

	static void RefreshKeyRow(Row& row)
	{ // Sign in keys, read once signed in to MK1's servers, too long to show in full
		std::wstring Value = row.ReadValue();
		if (!Value.empty())
			SetInfo(row, L"SELECT TO SEE", std::wstring(row.About) + L"\n\n" MK_BOLD(L"Value: ") + Short(Value, 160) + L"\n\nCOPY puts the full value on your clipboard.");
		else if (!Features::IsInstalled(Features::Id::ProfileGetter))
			SetInfo(row, NOT_READ, NotAvailable(row.About, Features::Id::ProfileGetter));
		else if (!Features::IsActive(Features::Id::ProfileGetter))
			SetInfo(row, NOT_READ, std::wstring(row.About) + L"\n\n" MK_RED(L"Needs PROFILE GETTER on."));
		else
			SetInfo(row, NOT_READ, std::wstring(row.About) + L"\n\nNot read yet, it's read once you're signed in to MK1's servers.");
	}

	static void AddKeyRow(const wchar_t* Title, const wchar_t* About, TextType* Read)
	{
		Row& row = AddInfoRow(TokensPage, Title, RefreshKeyRow);
		row.About = About;
		row.ReadValue = Read;
		row.CopyValue = KeyValue;
	}

	static std::wstring AccessToken() { return Widen(HookMetadata::sUserKeys.AccessToken.c_str()); }
	static std::wstring PlatformTicket() { return Widen(HookMetadata::sUserKeys.PlatformTicket.c_str()); }

	static void RefreshFloydTower(Row& row)
	{
		std::wstring About = L"When the Floyd Tower shows up in Living Towers.";
		if (!Features::IsActive(Features::Id::FloydTracking))
		{
			SetInfo(row, NOT_READ, About + L"\n\n" MK_RED(L"Needs FLOYD TRACKING on."));
			return;
		}
		std::wstring Label;
		std::wstring Text = FloydTowerText(Label);
		SetInfo(row, Label, About + L"\n\n" + Text);
	}

	static void RefreshFloydChallenges(Row& row)
	{
		const HookMetadata::FloydCluesInfo& Floyd = HookMetadata::CurrentFloydInfo;
		const FloydTowerAlert::FloydStats& Stats = FloydTowerAlert::LastStats();
		std::wstring About = L"The 10 challenges Floyd's clues point to, numbered like the Floyd tracker.";
		if (!Features::IsActive(Features::Id::FloydTracking) || !Floyd.HasClues)
		{
			SetInfo(row, NOT_READ, About + L"\n\n" + (Features::IsActive(Features::Id::FloydTracking)
				? L"Open Living Towers or start a tower to read them." : MK_RED(L"Needs FLOYD TRACKING on.")));
			return;
		}
		std::wstring Label = Stats.bRead ? std::to_wstring(DoneCount(Floyd.Clues, Stats)) + L" OF 10 DONE" : std::to_wstring(Floyd.Clues.size()) + L" CHALLENGES";
		// The game picks a new list after meeting Floyd, as the next match starts: until then this one is old
		std::wstring Picked = L"\n\n" MK_BOLD(L"Picked at encounter: ") + std::to_wstring(Floyd.FloydEncounters);
		uint64_t TimesMet = Stats.Values[(size_t)FloydTowerAlert::Stat::Encounters];
		if (Stats.bRead && TimesMet != Floyd.FloydEncounters)
		{
			Label = L"OUT OF DATE";
			Picked += L"\n" + Red(L"Open a match to update your stats.");
		}
		SetInfo(row, Label, About + L"\n\n" + ChallengeList(Floyd.Clues, Stats) + Picked);
	}

	static void RefreshProfileHash(Row& row)
	{
		const HookMetadata::FloydCluesInfo& Floyd = HookMetadata::CurrentFloydInfo;
		wchar_t Hash[16];
		swprintf_s(Hash, L"%08X", Floyd.UserProfileHash);
		bool bRead = Features::IsActive(Features::Id::FloydTracking) && Floyd.HasClues;
		SetInfo(row, bRead ? Hash : NOT_READ, L"The hash Floyd's challenges are picked from.");
	}

	static std::wstring ProfileHashValue(Row& row)
	{
		const HookMetadata::FloydCluesInfo& Floyd = HookMetadata::CurrentFloydInfo;
		if (!Features::IsActive(Features::Id::FloydTracking) || !Floyd.HasClues)
			return std::wstring();
		wchar_t Hash[16];
		swprintf_s(Hash, L"%08X", Floyd.UserProfileHash);
		return std::wstring(Hash);
	}

	static void ShowFloydWins(Row& row, const FloydTowerAlert::FloydStats& Stats)
	{
		SetInfo(row, FloydStatText(FloydTowerAlert::Stat::Victories), L"How many times you beat Floyd. The Floyd Tower only shows up once you have.");
	}

	static void ShowTimesMet(Row& row, const FloydTowerAlert::FloydStats& Stats)
	{
		SetInfo(row, FloydStatText(FloydTowerAlert::Stat::Encounters), L"How many times Floyd showed up.");
	}

	static void ShowChallengesDone(Row& row, const FloydTowerAlert::FloydStats& Stats)
	{
		std::vector<uint8_t> Challenges = KnownChallenges(Stats);
		std::wstring Text = L"Floyd's challenges you completed, out of your 10.\n\n" + ChallengeList(Challenges, Stats);
		if (!HookMetadata::CurrentFloydInfo.HasClues)
			Text += L"\n\nThe ones left show once Floyd's challenges are read: open Living Towers or start a tower.";
		SetInfo(row, std::to_wstring(DoneCount(Challenges, Stats)) + L" OF 10", Text);
	}

	static void ShowLastFight(Row& row, const FloydTowerAlert::FloydStats& Stats)
	{
		uint32_t Value = (uint32_t)Stats.Values[(size_t)FloydTowerAlert::Stat::LastBattle];
		const wchar_t* Label = Value == 9 ? L"LOST" : Value == 12 ? L"WON" : Value == 11 ? L"DIDN'T FINISH" : L"NONE YET";
		SetInfo(row, Label, L"How your last fight with Floyd went.");
	}

	static void ShowNextClue(Row& row, const FloydTowerAlert::FloydStats& Stats)
	{
		uint32_t Since = (uint32_t)(Stats.Values[(size_t)FloydTowerAlert::Stat::MatchesSinceSeen] % 501);
		std::wstring Text = Since < 100
			? L"At least " + std::to_wstring(100 - Since) + L" more fights before Floyd may show up to give you a clue."
			: L"Floyd shows up within the next " + std::to_wstring(500 - Since) + L" fights. Even then, it may not be the challenge you need.";
		SetInfo(row, Since < 100 ? L"IN " + std::to_wstring(100 - Since) + L"+ FIGHTS" : L"WITHIN " + std::to_wstring(500 - Since), Text);
	}

	static void AddMenuRows()
	{
		AddAboutRows();

		AddCategory(LivePage, L"FLOYD");
		AddFeatureRow(LivePage, Features::Id::FloydTracking);
		AddFeatureRow(LivePage, Features::Id::FloydTowerAlert);
		AddFloydFightRows();
		AddFeatureRow(LivePage, Features::Id::FightMetadataLog);
		AddSummonRow();
		AddSpawnTowerRow();
		AddCategory(LivePage, L"ONLINE");
		AddFeatureRow(LivePage, Features::Id::ProfileGetter);
		AddFeatureRow(LivePage, Features::Id::Curl);
		AddCategory(LivePage, L"NAMES AND AUDIO");
		AddFeatureRow(LivePage, Features::Id::StringSwap).bInGame = true;
		AddAnnouncerRow().bInGame = true;
		AddCategory(LivePage, L"GAME");
		AddAttractRow();
		AddFeatureRow(LivePage, Features::Id::KeepRunning).bInGame = true;

		bool bFloyd = Features::IsInstalled(Features::Id::FloydTracking);
		bool bProfile = Features::IsInstalled(Features::Id::ProfileGetter);
		AddCategory(LoggingPage, L"CONSOLE");
		Row Level = { L"LOG LEVEL", L"How much the console shows." };
		Level.Choices = LogLevels();
		Level.bNeedsRestart = false;
		Level.LiveGet = LogLevelGet;
		Level.LiveSet = LogLevelSet;
		AddMenuRow(LoggingPage, Level).bInGame = true;
		AddFlagRow(LoggingPage, L"DEBUG LOGS", L"Shows logs up to the DEBUG level whatever the LOG LEVEL is.", SettingsMgr->bDebug).bInGame = true;
		AddFlagRow(LoggingPage, L"FLOYD DEBUG", L"Shows Floyd dev logs: hash inputs, seeds and extracted IDs.", SettingsMgr->bFloydDebug, bFloyd, L"FLOYD TRACKING").bInGame = true;
		AddFlagRow(LoggingPage, L"PROFILE DEBUG", L"Shows profile dev logs: platform, platform ID and save key.", SettingsMgr->bProfileDebug, bProfile, L"PROFILE GETTER");
		AddFlagRow(LoggingPage, L"DEV LOGS", L"Shows the developers' investigation logs and checks. Some of them only start with the game.", SettingsMgr->bDevLogs);
		AddCategory(LoggingPage, L"FLOOD");
		AddFlagRow(LoggingPage, L"FNAME FLOOD", L"At LOG LEVEL FLOOD, logs every name lookup.", SettingsMgr->Floods.bFName).bInGame = true;
		AddFlagRow(LoggingPage, L"CURL FLOOD", L"At LOG LEVEL FLOOD, logs every web request.", SettingsMgr->Floods.bCurl);
		AddFlagRow(LoggingPage, L"ENDPOINT FLOOD", L"At LOG LEVEL FLOOD, logs every server endpoint lookup.", SettingsMgr->Floods.bEndpoint);

		HookMetadata::ProfileInfo& Info = HookMetadata::UserProfileInfo;
		AddValueRow(AccountPage, L"PLATFORM", L"The platform your profile is on.", &Info.Platform, Features::Id::ProfileGetter);
		AddValueRow(AccountPage, L"PLATFORM ID", L"Your account ID on that platform, online or offline.", &Info.PlatformId, Features::Id::ProfileGetter, false,
			PlatformIdWhenRead);
		AddInfoRow(AccountPage, L"ONLINE", RefreshOnline);
		AddValueRow(AccountPage, L"HYDRA ID", L"Your MK1 online account ID.", &Info.HydraId, Features::Id::ProfileGetter, false, OnlineIdWhenRead);
		AddValueRow(AccountPage, L"WB ID", L"Your WB account ID, when it's linked.", &Info.WBId, Features::Id::ProfileGetter, false, OnlineIdWhenRead);

		AddValueRow(TokensPage, L"SAVE KEY", L"The key your save is encrypted with.", &Info.SaveKey, Features::Id::ProfileGetter, true);
		AddKeyRow(L"ACCESS TOKEN", L"The key the game signs in to its server with this session.", AccessToken);
		AddKeyRow(L"PLATFORM TICKET", L"What your platform gave the game to sign you in to MK1's servers.", PlatformTicket);

		AddInfoRow(FloydPage, L"FLOYD TOWER", RefreshFloydTower).bInGame = true;
		AddInfoRow(FloydPage, L"FLOYD CHALLENGES", RefreshFloydChallenges).bInGame = true;
		AddInfoRow(FloydPage, L"PROFILE HASH", RefreshProfileHash).CopyValue = ProfileHashValue;

		AddFloydStatRow(L"FLOYD WINS", L"How many times you beat Floyd. The Floyd Tower only shows up once you have.", ShowFloydWins);
		AddFloydStatRow(L"TIMES MET FLOYD", L"How many times Floyd showed up.", ShowTimesMet);
		AddFloydStatRow(L"CHALLENGES DONE", L"Floyd's challenges you completed, out of your 10.", ShowChallengesDone);
		AddFloydStatRow(L"LAST FLOYD FIGHT", L"How your last fight with Floyd went.", ShowLastFight);
		AddFloydStatRow(L"NEXT CLUE", L"When Floyd may show up again to give you a clue.", ShowNextClue);
	}

	static void RefreshInfoRows()
	{
		if (bInitialized)
			FloydTowerAlert::Refresh(); // Floyd fights change them, read as a screen opens (never at startup)
		for (size_t i = 0; i < AllRows.size(); i++)
		{
			Row* row = AllRows[i];
			if (row->IsInfo())
				row->Refresh(*row);
			if (row->Describe)
				row->Describe(*row);
		}
	}

	static Row* NewHeader(const wchar_t* Title)
	{
		Row* row = new Row;
		row->Title = Title;
		MenuRows.push_back(row);
		return row;
	}

	// The pause menu's page: the rows marked for fights, each under its own header
	static void AddInGamePage()
	{
		Row* Header = nullptr;
		for (size_t i = 0; i < LivePage.Rows.size(); i++)
		{
			Row* row = LivePage.Rows[i];
			if (row->IsCategory())
				Header = row;
			else if (row->bInGame)
			{
				if (Header)
				{
					InGamePage.Rows.push_back(Header);
					InGameDebugPage.Rows.push_back(Header);
				}
				InGamePage.Rows.push_back(row);
				InGameDebugPage.Rows.push_back(row);
				Header = nullptr;
			}
		}
		// Floyd's rows to look at, under a FLOYD header
		Row* FloydHeader = nullptr;
		for (size_t i = 0; i < FloydPage.Rows.size(); i++)
		{
			Row* row = FloydPage.Rows[i];
			if (row->IsCategory() || !row->bInGame)
				continue;
			if (!FloydHeader)
			{
				FloydHeader = NewHeader(L"FLOYD");
				InGamePage.Rows.push_back(FloydHeader);
				InGameDebugPage.Rows.push_back(FloydHeader);
			}
			InGamePage.Rows.push_back(row);
			InGameDebugPage.Rows.push_back(row);
		}
		// Logging rows only while debugging, so they can't be changed by mistake. One LOGGING header for all
		Row* LoggingHeader = nullptr;
		for (size_t i = 0; i < LoggingPage.Rows.size(); i++)
		{
			Row* row = LoggingPage.Rows[i];
			if (row->IsCategory() || !row->bInGame)
				continue;
			if (!LoggingHeader)
			{
				LoggingHeader = NewHeader(L"LOGGING");
				InGameDebugPage.Rows.push_back(LoggingHeader);
			}
			InGameDebugPage.Rows.push_back(row);
		}
	}

	// A SECTION row first on each page of a group with several: its value is the page showing, left / right switch.
	// The switch waits for the next frame, the screen is still handling the press when the value changes
	static Group* PendingGroup = nullptr;
	static size_t PendingSection = 0;

	static int32_t SectionGet(Row& row) { return (int32_t)row.SectionGroup->Current; }
	static void SectionSet(Row& row, int32_t Value)
	{
		PendingGroup = row.SectionGroup;
		PendingSection = (size_t)Value;
	}

	static void AddSectionRows()
	{
		std::vector<Group*> Groups = AllGroups();
		for (size_t g = 0; g < Groups.size(); g++)
		{
			Group* group = Groups[g];
			if (group->Pages.size() < 2)
				continue;
			Row* Section = new Row;
			Section->Title = SECTION_TITLE;
			Section->Description = L"What this page shows. Left and right switch it.";
			Section->bNeedsRestart = false;
			for (size_t p = 0; p < group->Pages.size(); p++)
				Section->Choices.push_back({ group->Pages[p]->TabKey, "" });
			Section->SectionGroup = group;
			Section->LiveGet = SectionGet;
			Section->LiveSet = SectionSet;
			MenuRows.push_back(Section);
			for (size_t p = 0; p < group->Pages.size(); p++)
			{
				Page* page = group->Pages[p];
				// A header under it so the page reads as its own part: SECTION GENERAL, then GENERAL
				bool bOwnHeader = !page->Rows.empty() && page->Rows.front()->IsCategory() && !wcscmp(page->Rows.front()->Title, page->TabKey);
				if (!bOwnHeader)
					page->Rows.insert(page->Rows.begin(), NewHeader(page->TabKey));
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

		std::vector<Page*> Pages = AllPages();
		for (size_t p = 0; p < Pages.size(); p++)
		{
			for (size_t r = 0; r < Pages[p]->Rows.size(); r++)
			{
				Row* row = Pages[p]->Rows[r];
				if (std::find(AllRows.begin(), AllRows.end(), row) == AllRows.end())
					AllRows.push_back(row);
			}
		}
		for (size_t i = 0; i < AllRows.size(); i++)
		{
			if (AllRows[i]->IsIni())
				AllRows[i]->Value = AllRows[i]->StartValue = ReadValue(*AllRows[i]);
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
		OwnVersion.Data = &OwnVersionText[0];
		OwnVersion.Count = (int32_t)OwnVersionText.size() + 1;
		OwnVersion.Max = (int32_t)OwnVersionText.size() + 1;
		return &OwnVersion; // The screen copies it
	}

	// The screen calls these with our Row as the context
	static int32_t __fastcall GetValue(void* Context, int64_t*)
	{
		Row& row = *(Row*)Context;
		return row.IsLive() ? row.LiveGet(row) : row.Value;
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
			row.LiveSet(row, *Value);
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

	// The game's rows are static objects: found without a screen, or on the screen up now
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

	static bool __fastcall ActivateRow(void* Context, void*)
	{ // Confirm on an action row, game thread while the screen ticks
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
		GameRow* game = new GameRow;
		GameRows.push_back(game);
		bool bLink = row.IsAction() && From.Link;
		size_t Size = row.IsCategory() ? MK12::FSettingsOption::Size : bLink ? MK12::FSettingsLinkOption::Size : MK12::FSettingsListOption::Size;
		game->Memory = new char[Size];
		memcpy(game->Memory, row.IsCategory() ? (void*)From.Category : bLink ? (void*)From.Link : (void*)From.List, Size);

		MK12::FSettingsOption* Option = (MK12::FSettingsOption*)game->Memory;
		Option->Table = TEXT_TABLE;
		Option->Key = row.Title;
		Option->bLocalizedTitle = false;
		Option->Id = Id;
		memset(Option->Callbacks, 0, sizeof(Option->Callbacks));

		if (bLink)
		{
			MK12::FSettingsLinkOption* Link = (MK12::FSettingsLinkOption*)Option;
			Link->Activate = ActivateRow;
			Link->Context = &row;
			return Option;
		}
		if (row.IsAction())
		{
			row.Choices.clear();
			row.Choices.push_back({ L"UNAVAILABLE", "", L"This version of the game has no buttons to copy." });
		}

		if (!row.IsCategory())
		{
			for (int32_t v = 0; v < (int32_t)row.Choices.size(); v++)
			{
				// The game puts the picked choice's text right after the row's, a blank line keeps them apart
				const wchar_t* Text = row.Choices[v].Description;
				if (Text && *Text && row.Description && *row.Description)
					Text = Own(L"\n\n" + std::wstring(Text));
				MK12::FSettingsListValue Value = { nullptr, row.Choices[v].Label, Text, v, false };
				game->Values.push_back(Value);
			}

			MK12::FSettingsListOption* List = (MK12::FSettingsListOption*)Option;
			List->Values.Data = game->Values.data();
			List->Values.ElementsCount = (int32_t)game->Values.size();
			List->Values.Size = (int32_t)game->Values.size();
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
		for (size_t i = 0; i < _countof(LINK_TEMPLATE_KEYS) && !From.Link; i++)
			From.Link = FindGameRow(Screen, MK12::SettingsOptionType::Link, LINK_TEMPLATE_KEYS[i]);
		if (!From.Link)
			printfWarning("Mod Settings: Couldn't find the game's buttons to copy, MODS has no buttons");

		int32_t Id = FIRST_ROW_ID;
		std::vector<Page*> Pages = AllPages();
		for (size_t p = 0; p < Pages.size(); p++)
		{
			for (size_t r = 0; r < Pages[p]->Rows.size(); r++)
				Pages[p]->GameRows.push_back(BuildGameRow(*Pages[p]->Rows[r], Id++, From));
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
		MK12::FSettingsPage Out = { (MK12::FSettingsOption**)page.GameRows.data(), (int32_t)page.GameRows.size(), 0 };
		return Out;
	}

	static void AddPage(MK12::USettingsScreen* Screen, Group& group)
	{
		if (ShownGroups.empty())
			FirstOwnPage = Screen->Pages.ElementsCount;
		ActiveScreen = Screen;
		// The game frees page descriptions with the screen, the key goes in the game's memory
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
			std::vector<Group*> Groups = Entries(MenuGroups());
			for (size_t i = 0; i < Groups.size(); i++)
			{
				if (!Groups[i]->Shown().Rows.empty())
					AddPage(Screen, *Groups[i]);
			}
			return;
		}
		std::vector<Group*> Groups = Entries(SettingsGroups());
		for (size_t i = 0; i < Groups.size(); i++)
			AddPage(Screen, *Groups[i]);
	}

	static void ToGameText(MK12::FText* Out, const wchar_t* Text)
	{
		int32_t Count = (int32_t)wcslen(Text) + 1;
		MK12::FString String = { (wchar_t*)Text, Count, Count };
		MK12::TextFromString(Out, &String); // Copies it
	}

	static bool TitleMatches(const Row* row, const wchar_t* Title, size_t Length)
	{
		return wcslen(row->Title) == Length && !wcsncmp(row->Title, Title, Length);
	}

	// The screen only gives us the title. Settings and our menu share some titles, the pages showing now go first
	static const Row* FindRow(const wchar_t* Title, size_t Length)
	{
		for (size_t g = 0; g < ShownGroups.size(); g++)
		{
			const Group* group = ShownGroups[g];
			for (size_t p = 0; p < group->Pages.size(); p++)
			{
				const Page* page = group->Pages[p];
				for (size_t r = 0; r < page->Rows.size(); r++)
				{
					if (TitleMatches(page->Rows[r], Title, Length))
						return page->Rows[r];
				}
			}
		}
		for (size_t i = 0; i < AllRows.size(); i++)
		{
			if (TitleMatches(AllRows[i], Title, Length))
				return AllRows[i];
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
			std::vector<Group*> All = AllGroups();
			Groups.insert(Groups.end(), All.begin(), All.end());
			Groups.insert(Groups.end(), SplitGroups.begin(), SplitGroups.end());
			for (size_t i = 0; i < Groups.size(); i++)
			{
				if (!Text && Key[0] == L'M' && !wcscmp(Key, Groups[i]->DescKey))
					Text = Groups[i]->Shown().Description;
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

		// The game copies the tabs, ours only live on this copy. A tab shows the page with the same index, so in our
		// menu only our tabs are passed
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

			MK12::FSettingsTab Tab = {};
			WithOurs.push_back(Tab);
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

	// The steps of the game's own page switch: the page showing gets the section's rows
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
	constexpr uint32_t	COPY_INPUT		= 0x40;
	constexpr uint8_t	COPY_INPUT_BIT	= 6;

	static Row* FocusedOwnRow(MK12::USettingsScreen* Screen)
	{ // Null on the game's pages
		Group* group = Screen && Screen == ScreenUp() ? GroupOnScreen(Screen) : nullptr;
		if (!group)
			return nullptr;
		const Page& page = group->Shown();
		int32_t Index = Screen->FocusedRow;
		return Index >= 0 && Index < (int32_t)page.Rows.size() ? page.Rows[Index] : nullptr;
	}

	static std::wstring CopyText(Row* row)
	{
		return row && row->CopyValue ? row->CopyValue(*row) : std::wstring();
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
		// The panel isn't a settings screen run of its own, nothing says when it closed: it never becomes the active one
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
		for (size_t i = 0; i < AllRows.size(); i++)
		{
			if (AllRows[i]->IsIni() && AllRows[i]->bNeedsRestart)
				Values.push_back(AllRows[i]->Value);
		}
		return Values;
	}

	static bool NeedsRestart()
	{
		for (size_t i = 0; i < AllRows.size(); i++)
		{
			const Row* row = AllRows[i];
			if (row->IsIni() && row->bNeedsRestart && row->Value != row->StartValue)
				return true;
		}
		return false;
	}

	static std::wstring RelaunchUrl()
	{ // The store the game was started from, so it comes back the same way
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

	// The restart helper's script: waits for this process to end, runs BeforeStart, starts the game again. A file, so any
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
		if (Size > 0)
			WideCharToMultiByte(CP_UTF8, 0, Script.c_str(), (int)Script.size(), &Bytes[3], Size, nullptr, nullptr);
		HANDLE File = CreateFileW(Path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
		if (File == INVALID_HANDLE_VALUE)
			return L"";
		DWORD Written = 0;
		bool bOk = WriteFile(File, Bytes.data(), (DWORD)Bytes.size(), &Written, nullptr) && Written == Bytes.size();
		CloseHandle(File);
		return bOk ? Path : L"";
	}

	static void Restart(const std::wstring& BeforeStart)
	{ // Asks the game to exit, a hidden helper starts it again once this process is gone
		std::wstring ScriptPath = MK12::lpEngineExitRequested ? WriteRestartScript(BeforeStart) : L"";
		if (ScriptPath.empty())
		{
			GameMessageBox::Confirm(L"Couldn't restart the game. Close it and start it again to apply the changes.", nullptr, L"MK12TTH");
			return;
		}

		std::wstring Command = L"powershell.exe -NoProfile -ExecutionPolicy Bypass -WindowStyle Hidden -File \"" + ScriptPath + L"\"";
		STARTUPINFOW Startup = { sizeof(Startup) };
		PROCESS_INFORMATION Process = {};
		if (!CreateProcessW(nullptr, &Command[0], nullptr, nullptr, FALSE, CREATE_NO_WINDOW | DETACHED_PROCESS, nullptr, nullptr, &Startup, &Process))
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

	// Overlay
	constexpr ULONGLONG OVERLAY_REFRESH_MS	= 500; // While it draws
	constexpr ULONGLONG OVERLAY_IDLE_MS		= 1000; // It stopped asking: closed

	struct OverlayChange {
		const void*	Id;
		int32_t		Value; // Choice, or -1 to press an action row
	};

	// The copy and the changes are shared with the overlay's thread
	static SRWLOCK						OverlayLock		= SRWLOCK_INIT;
	static std::vector<OverlayPage>		OverlayCopy;
	static std::vector<OverlayChange>	OverlayChanges;
	static ULONGLONG					OverlayAskedAt	= 0;

	static std::string OverlayText(const std::wstring& Text)
	{ // Markup tags dropped, our escapes undone, UTF-8
		static const wchar_t* Escapes[][2] = { { L"&lt;", L"<" }, { L"&gt;", L">" }, { L"&amp;", L"&" } };
		std::wstring Out;
		bool bTag = false;
		for (size_t i = 0; i < Text.size(); i++)
		{
			wchar_t c = Text[i];
			if (c == L'<')
				bTag = true;
			else if (c == L'>')
				bTag = false;
			else if (!bTag)
				Out += c;
		}
		for (size_t e = 0; e < _countof(Escapes); e++)
		{
			const wchar_t* From = Escapes[e][0];
			for (size_t At = Out.find(From); At != std::wstring::npos; At = Out.find(From, At + 1))
				Out.replace(At, wcslen(From), Escapes[e][1]);
		}
		int Size = WideCharToMultiByte(CP_UTF8, 0, Out.c_str(), (int)Out.size(), nullptr, 0, nullptr, nullptr);
		std::string Narrow(Size, '\0');
		if (Size > 0)
			WideCharToMultiByte(CP_UTF8, 0, Out.c_str(), (int)Out.size(), &Narrow[0], Size, nullptr, nullptr);
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
			Out.Picked = row.LiveGet(row);
			for (size_t i = 0; i < row.Choices.size(); i++)
				Out.Choices.push_back(OverlayText(row.Choices[i].Label));
		}
		else if (row.IsIni() && row.Value >= 0 && row.Value < (int32_t)row.Choices.size())
			Out.Value = OverlayText(row.Choices[row.Value].Label) + (row.bNeedsRestart ? " (change it in game)" : "");
		return Out;
	}

	static void TickOverlay()
	{
		std::vector<OverlayChange> Changes;
		AcquireSRWLockExclusive(&OverlayLock);
		Changes.swap(OverlayChanges);
		ULONGLONG AskedAt = OverlayAskedAt;
		ReleaseSRWLockExclusive(&OverlayLock);
		for (size_t i = 0; i < Changes.size(); i++)
		{
			std::vector<Row*>::iterator Found = std::find(AllRows.begin(), AllRows.end(), (Row*)Changes[i].Id);
			if (Found == AllRows.end())
				continue;
			Row& row = **Found;
			int32_t Value = Changes[i].Value;
			if (Value < 0 && row.Action)
				row.Action();
			else if (Value >= 0 && row.LiveSet && Value < (int32_t)row.Choices.size())
				row.LiveSet(row, Value);
		}

		static ULONGLONG LastCopy = 0;
		ULONGLONG Now = GetTickCount64();
		if (!bInitialized || Now - AskedAt > OVERLAY_IDLE_MS || (Changes.empty() && Now - LastCopy < OVERLAY_REFRESH_MS))
			return;
		LastCopy = Now;
		RefreshInfoRows();
		std::vector<OverlayPage> Pages;
		std::vector<Group*> Groups = MenuGroups();
		for (size_t g = 0; g < Groups.size(); g++)
		{
			Group* group = Groups[g];
			for (size_t p = 0; p < group->Pages.size(); p++)
			{
				Page* page = group->Pages[p];
				OverlayPage Out;
				Out.Title = OverlayText(group->Pages.size() > 1 ? std::wstring(group->TabKey) + L": " + page->TabKey : std::wstring(group->TabKey));
				for (size_t r = 0; r < page->Rows.size(); r++)
				{
					Row* row = page->Rows[r];
					if (row->Title && !wcscmp(row->Title, SECTION_TITLE))
						continue; // The overlay lists every page, no switch needed
					Out.Rows.push_back(ToOverlay(*row));
				}
				Pages.push_back(Out);
			}
		}
		AcquireSRWLockExclusive(&OverlayLock);
		OverlayCopy.swap(Pages);
		ReleaseSRWLockExclusive(&OverlayLock);
	}

	std::vector<OverlayPage> OverlayPages()
	{
		AcquireSRWLockExclusive(&OverlayLock);
		OverlayAskedAt = GetTickCount64();
		std::vector<OverlayPage> Copy = OverlayCopy;
		ReleaseSRWLockExclusive(&OverlayLock);
		return Copy;
	}

	void OverlayPick(const void* Id, int32_t Choice)
	{
		OverlayChange Change = { Id, Choice };
		AcquireSRWLockExclusive(&OverlayLock);
		OverlayChanges.push_back(Change);
		ReleaseSRWLockExclusive(&OverlayLock);
	}

	void OverlayPress(const void* Id)
	{
		OverlayChange Change = { Id, -1 };
		AcquireSRWLockExclusive(&OverlayLock);
		OverlayChanges.push_back(Change);
		ReleaseSRWLockExclusive(&OverlayLock);
	}

	void Tick()
	{
		TickOverlay();

		// The game's static rows are there from startup: build ours once the main menu is up, so the pause menu has
		// them even when Settings was never opened
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
		GameMessageBox::Ask(L"Some changes apply after a restart.\nRestart the game now?", RestartIfYes, L"MK12TTH");
	}
}
