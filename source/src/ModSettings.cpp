#include "mkutils.h"
#include "ModSettings.h"
#include "GameMessageBox.h"
#include "PakMods.h"
#include "StateFile.h"
#include "Features.h"
#include "FloydTowerAlert.h"
#include "Announcer.h"
#include "EngineConfig.h"
#include "AttractMode.h"
#include "FloydTower.h"
#include "OnlineIds.h"
#include "mk12.h"
#include "mk12hook.h"
#include "mk12_enums.h"
#include "eSettingsManager.h"
#include "../IniReader.h"
#include <algorithm>
#include <cwchar>
#include <cwctype>
#include <string>
#include <vector>

namespace ModSettings {
	constexpr int32_t			FIRST_ROW_ID			= 0x7000; // Far from the game's own ids
	constexpr ULONGLONG			RESTART_ASK_DELAY		= 1500; // Without the screen hook: waits for the player to stop changing values
	constexpr const wchar_t*	LIST_TEMPLATE_KEY		= L"RoundTimeTitle"; // Game rows we copy, both on the Gameplay page
	constexpr const wchar_t*	CATEGORY_TEMPLATE_KEY	= L"FightSettingsCategory";
	constexpr const wchar_t*	STEAM_RELAUNCH_URL		= L"steam://rungameid/1971870";

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
	typedef int32_t			(LiveGetType)(Row& row);
	typedef void			(LiveSetType)(Row& row, int32_t Value);
	typedef bool			(SwitchGetType)(Row& row);
	typedef void			(SwitchSetType)(Row& row, bool bOn);
	typedef void			(RowType)(Row& row);
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

		// What the callbacks work on
		SwitchGetType*			GetSwitch		= nullptr;
		SwitchSetType*			SetSwitch		= nullptr;
		Features::Id			Feature			= Features::Id::Count;
		bool*					Flag			= nullptr;
		const wchar_t*			About			= nullptr;
		wchar_t**				Field			= nullptr;
		TextType*				WhenRead		= nullptr;
		bool					bPrivate		= false;
		StatShowType*			ShowStat		= nullptr;

		bool IsIni() const { return Section != nullptr; }
		bool IsLive() const { return LiveSet != nullptr; }
		bool IsInfo() const { return Refresh != nullptr; }
		bool IsCategory() const { return !IsIni() && !IsLive() && !IsInfo(); }
	};

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

	#define MK_LIVE_NOTE		MK_BOLD(L"Changes apply right away.") L" Settings, MK12TTH decides which features are set up at startup."
	constexpr const wchar_t*	MODS_DESC			= L"Mods found in Content\\Mods and Content\\Paks, saved to MK12TTH_State.ini. Mods in Content\\Mods load over the game's files and over Content\\Paks, MK12TTH sets their order.\n\n" MK_BOLD(L"Turning a mod on or off applies after a restart.") L"\n\n" MK_RED(L"Turn mods off before playing online.");
	constexpr const wchar_t*	LIVE_DESC			= L"Turn MK12TTH features off and on while the game runs. Saved for next time.\n\n" MK_LIVE_NOTE;
	constexpr const wchar_t*	LOGGING_DESC		= L"What the console shows, changed right away. Only for this session, Settings, MK12TTH keeps the startup values.";
	constexpr const wchar_t*	PROFILE_DESC		= L"Read from your profile while you play. Select a row to see its value.\n\n" MK_RED(L"Your save key and access token are private, don't share them.");
	constexpr const wchar_t*	OWN_MENU_TITLE		= L"MODS";
	constexpr const wchar_t*	MENU_TITLE_KEY		= L"SettingsSubMenuTitle"; // The settings screen's title, ours says MODS
	constexpr int32_t			MOD_ROW_DEFAULT		= ON; // A mod missing from the ini is on

	// A tab of ours: its rows and the game's rows built for them
	struct Page {
		const wchar_t*						TabKey;
		const wchar_t*						DescKey;
		const wchar_t*						Description;
		std::vector<Row*>					Rows;
		std::vector<MK12::FSettingsOption*>	GameRows;
	};

	// Rows are never freed, the screen keeps pointers to them
	static std::vector<Row*>	ModRows; // One per pak mod
	static std::vector<Row*>	MenuRows; // Live and info rows of our menu
	static Page SettingsPage	= { TabKey, PAGE_DESC_KEY, PAGE_DESC };
	static Page ModsPage		= { L"PAK MODS", L"MK12TTH_ModsDesc", MODS_DESC };
	static Page LivePage		= { L"LIVE FEATURES", L"MK12TTH_LiveDesc", LIVE_DESC };
	static Page LoggingPage		= { L"LOGGING", L"MK12TTH_LoggingDesc", LOGGING_DESC };
	static Page ProfilePage		= { L"PROFILE", L"MK12TTH_ProfileDesc", PROFILE_DESC };
	static Page* const AllPages[]	= { &SettingsPage, &ModsPage, &LivePage, &LoggingPage, &ProfilePage };
	static Page* const MenuPages[]	= { &ModsPage, &LivePage, &LoggingPage, &ProfilePage }; // Our main menu entry's screen
	static std::vector<Row*> AllRows;
	static std::vector<Page*> AddedPages; // Pages added for the tab bar being built now

	// The game's rows for ours, copies of the game's own rows. Never freed
	struct GameRow {
		char*									Memory;
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
			return L"\n\n" MK_BOLD(L"Load order: ") L"the game's own, by path. Content\\Mods loads over it, move it there and MK12TTH orders it.";
		if (mod.Priority >= 0)
			return L"\n\n" MK_BOLD(L"Load order: ") L"priority " + std::to_wstring(mod.Priority) + L" from its name (_P" + std::to_wstring(mod.Priority)
				+ L"), over the mods without one. Higher wins.";
		return L"\n\n" MK_BOLD(L"Load order: ") L"automatic. Mods with only a .pak load over mods with .utoc and .ucas, then by name, "
			L"the last one wins. End its name with _P and a number to set it.";
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

	static int ModsNamed(const std::wstring& Name)
	{
		const std::vector<PakMods::Mod>& Mods = PakMods::List();
		int Count = 0;
		for (size_t i = 0; i < Mods.size(); i++)
		{
			if (!_wcsicmp(Mods[i].Name.c_str(), Name.c_str()))
				Count++;
		}
		return Count;
	}

	// A row per pak mod under a category per place, titled with its name, with its folder when two share one
	static void AddModRows()
	{
		const std::vector<PakMods::Mod>& Mods = PakMods::List();
		bool bCategory[2] = {};
		for (size_t i = 0; i < Mods.size(); i++)
		{
			const PakMods::Mod& mod = Mods[i];
			bool& bAdded = bCategory[mod.From == PakMods::Source::ModsFolder ? 0 : 1];
			if (!bAdded)
			{
				bAdded = true;
				Row* Category = new Row;
				Category->Title = mod.From == PakMods::Source::ModsFolder ? L"CONTENT\\MODS" : L"CONTENT\\PAKS";
				ModRows.push_back(Category);
				ModsPage.Rows.push_back(Category);
			}
			bool bSharedName = ModsNamed(mod.Name) > 1;
			std::wstring Place = mod.Folder.empty() ? (mod.From == PakMods::Source::ModsFolder ? L"Mods" : L"Paks") : mod.Folder;
			std::wstring Title = Upper(bSharedName ? mod.Name + L" (" + Place + L")" : mod.Name);
			std::wstring Where = WhereIs(mod.From, mod.Folder);

			std::wstring Files = PakFiles(mod);
			std::wstring Engine;
			std::vector<const EngineConfig::LooseFile*> EngineFiles = EngineConfig::FilesFor(mod);
			for (size_t f = 0; f < EngineFiles.size(); f++)
				Engine += EngineValues(*EngineFiles[f]);
			if (!Engine.empty())
				Engine = L"\n\n" MK_BOLD(L"Engine settings: ") + Engine;
			Row* row = new Row;
			row->Title = Own(Title);
			row->Description = Own(L"In " + Where + L".\n\n" MK_BOLD(L"Files: ") + Files + Engine + OrderText(mod) + LoadText(mod)
				+ L"\n\nTurning it off keeps the game from mounting it after a restart.");
			row->Section = StateFile::MODS;
			row->Key = mod.Key.c_str();
			row->Choices = OffOn();
			row->Default = MOD_ROW_DEFAULT;
			row->IniFile = StateFile::FILE_NAME;
			std::wstring Summary = LoadSummary(mod);
			if (!Summary.empty())
				row->Choices[ON].Description = Own(Summary);
			ModRows.push_back(row);
			ModsPage.Rows.push_back(row);
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
	static void AddFeatureRow(Page& page, Features::Id Which)
	{
		const Features::Definition& Feature = Features::Get(Which);
		bool bInstalled = Features::IsInstalled(Which);
		const wchar_t* Description = bInstalled ? Feature.Description
			: Own(std::wstring(Feature.Description) + NeedsText(Which) + L"\n\n" MK_RED(L"Not available: ") + Features::Explain(Which));
		AddLiveRow(page, Feature.Title, Description, FeatureGet, FeatureSet, bInstalled).Feature = Which;
	}

	static void AddSettingsRow(const wchar_t* Title, const wchar_t* Description, const char* Section, const char* Key, const std::vector<Choice>& Choices, int32_t Default)
	{
		Row* row = new Row;
		row->Title = Title;
		row->Description = Description;
		row->Section = Section;
		row->Key = Key;
		row->Choices = Choices;
		row->Default = Default;
		SettingsRows.push_back(row);
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
		AddSettingsRow(Feature.Title, Own(Text), Feature.IniSection, Feature.IniKey, OffOn(), Feature.bIniDefault ? ON : OFF);
	}

	static void AddSettingsCategory(const wchar_t* Title)
	{
		Row* row = new Row;
		row->Title = Title;
		SettingsRows.push_back(row);
	}

	static void AddSettingsRows()
	{
		AddSettingsCategory(L"GENERAL");
		AddFeatureSetting(Features::Id::ModLoader);
		AddFeatureSetting(Features::Id::PakMods);
		AddFeatureSetting(Features::Id::EngineConfig);
		AddFeatureSetting(Features::Id::FastBoot);
		AddFeatureSetting(Features::Id::KeepRunning);
		AddFeatureSetting(Features::Id::AttractMode);

		AddSettingsCategory(L"FLOYD");
		AddFeatureSetting(Features::Id::FloydTracking);
		std::vector<Choice> TowerModes;
		TowerModes.push_back({ L"GAME", "Game", L"The game's own schedule, read once you open Living Towers. Shows up once the Floyd Tower is unlocked." });
		TowerModes.push_back({ L"CALCULATED", "Calculated", L"Worked out from your profile, the same way the game does." });
		AddSettingsRow(L"FLOYD TOWER TIMES", L"Where the Floyd Tower times come from.", "Patches", "szFloydTowerMode", TowerModes, 0);
		AddFeatureSetting(Features::Id::FloydTowerAlert);
		AddFeatureSetting(Features::Id::FightMetadata);

		AddSettingsCategory(L"ONLINE");
		AddFeatureSetting(Features::Id::ProfileGetter);
		AddFeatureSetting(Features::Id::Curl);
		AddFeatureSetting(Features::Id::PrivateServer);

		AddSettingsCategory(L"NAMES AND AUDIO");
		AddFeatureSetting(Features::Id::NameTable);
		AddFeatureSetting(Features::Id::StringSwap);
		AddFeatureSetting(Features::Id::Announcer);

		AddSettingsCategory(L"DEBUG");
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
		AddSettingsRow(L"FNAME FLOOD", L"At LOG LEVEL FLOOD, logs every name lookup.",
			"Settings.Debug.FloodToggles", "bFName", OffOn(), OFF);
		AddSettingsRow(L"CURL FLOOD", L"At LOG LEVEL FLOOD, logs every web request.",
			"Settings.Debug.FloodToggles", "bCurl", OffOn(), OFF);
	}

	static bool FlagGet(Row& row) { return *row.Flag; }
	static void FlagSet(Row& row, bool bOn) { *row.Flag = bOn; }

	static void AddFlagRow(Page& page, const wchar_t* Title, const wchar_t* Description, bool& Flag, bool bAvailable = true, const wchar_t* Needs = nullptr)
	{
		AddLiveRow(page, Title, Description, FlagGet, FlagSet, bAvailable, Needs).Flag = &Flag;
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

	static std::wstring ReadAsYouPlay()
	{
		return L"Not read yet, the game reads it as you play.";
	}

	static void RefreshValueRow(Row& row)
	{
		std::wstring Value = *row.Field ? std::wstring(*row.Field) : std::wstring();
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
		const wchar_t* About = L"How long you idle on the main menu before attract mode, the demo, starts.";
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

	static void AddAnnouncerRow()
	{
		const wchar_t* About = L"The announcer's voice. GAME uses your own setting, AUTO the announcer of the fighter you play when they have one.";
		if (!Features::IsInstalled(Features::Id::Announcer) || !Announcer::IsReady())
		{
			AddLiveRow(LivePage, L"ANNOUNCER", Own(NotAvailable(About, Features::Id::Announcer)), nullptr, nullptr, false);
			return;
		}
		Row row = { L"ANNOUNCER", About };
		const std::vector<Announcer::Option>& Options = Announcer::Options();
		for (size_t i = 0; i < Options.size(); i++)
			row.Choices.push_back({ Options[i].Label, Options[i].Value });
		row.bNeedsRestart = false;
		row.LiveGet = AnnouncerGet;
		row.LiveSet = AnnouncerSet;
		AddMenuRow(LivePage, row);
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
		Row& row = AddInfoRow(ProfilePage, Title, RefreshFloydStat);
		row.About = About;
		row.ShowStat = Show;
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

	static void RefreshAccessToken(Row& row)
	{
		std::wstring Token = Widen(HookMetadata::sUserKeys.AccessToken.c_str());
		std::wstring About = L"The key the game signs in to its server with this session.";
		if (!Token.empty())
			SetInfo(row, L"SELECT TO SEE", About + L"\n\n" MK_BOLD(L"Value: ") + Short(Token, 160) + L"\n\nThe full token is in the MK12TTH overlay menu.");
		else if (!Features::IsInstalled(Features::Id::Curl))
			SetInfo(row, NOT_READ, About + L"\n\n" MK_RED(L"Not available: ") + Features::Explain(Features::Id::Curl));
		else if (!Features::IsActive(Features::Id::Curl))
			SetInfo(row, NOT_READ, About + L"\n\nTurn on WEB CAPTURE in LIVE FEATURES, it's read the next time the game signs in.");
		else
			SetInfo(row, NOT_READ, About + L"\n\nNot read yet, it's read when the game signs in.");
	}

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
		SetInfo(row, Label, About + L"\n\n" + ChallengeList(Floyd.Clues, Stats)
			+ L"\n\n" MK_BOLD(L"Encounters: ") + std::to_wstring(Floyd.FloydEncounters));
	}

	static void RefreshProfileHash(Row& row)
	{
		const HookMetadata::FloydCluesInfo& Floyd = HookMetadata::CurrentFloydInfo;
		wchar_t Hash[16];
		swprintf_s(Hash, L"%08X", Floyd.UserProfileHash);
		bool bRead = Features::IsActive(Features::Id::FloydTracking) && Floyd.HasClues;
		SetInfo(row, bRead ? Hash : NOT_READ, L"The hash Floyd's challenges are picked from.");
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
		AddCategory(LivePage, L"FLOYD");
		AddFeatureRow(LivePage, Features::Id::FloydTracking);
		AddFeatureRow(LivePage, Features::Id::FloydTowerAlert);
		AddFeatureRow(LivePage, Features::Id::FightMetadataLog);
		AddCategory(LivePage, L"ONLINE");
		AddFeatureRow(LivePage, Features::Id::ProfileGetter);
		AddFeatureRow(LivePage, Features::Id::Curl);
		AddCategory(LivePage, L"NAMES AND AUDIO");
		AddFeatureRow(LivePage, Features::Id::StringSwap);
		AddAnnouncerRow();
		AddCategory(LivePage, L"GAME");
		AddAttractRow();
		AddFeatureRow(LivePage, Features::Id::KeepRunning);

		bool bFloyd = Features::IsInstalled(Features::Id::FloydTracking);
		bool bProfile = Features::IsInstalled(Features::Id::ProfileGetter);
		AddCategory(LoggingPage, L"CONSOLE");
		Row Level = { L"LOG LEVEL", L"How much the console shows." };
		Level.Choices = LogLevels();
		Level.bNeedsRestart = false;
		Level.LiveGet = LogLevelGet;
		Level.LiveSet = LogLevelSet;
		AddMenuRow(LoggingPage, Level);
		AddFlagRow(LoggingPage, L"DEBUG LOGS", L"Shows logs up to the DEBUG level whatever the LOG LEVEL is.", SettingsMgr->bDebug);
		AddFlagRow(LoggingPage, L"FLOYD DEBUG", L"Shows Floyd dev logs: hash inputs, seeds and extracted IDs.", SettingsMgr->bFloydDebug, bFloyd, L"FLOYD TRACKING");
		AddFlagRow(LoggingPage, L"PROFILE DEBUG", L"Shows profile dev logs: platform, platform ID and save key.", SettingsMgr->bProfileDebug, bProfile, L"PROFILE GETTER");
		AddCategory(LoggingPage, L"FLOOD");
		AddFlagRow(LoggingPage, L"FNAME FLOOD", L"At LOG LEVEL FLOOD, logs every name lookup.", SettingsMgr->Floods.bFName);
		AddFlagRow(LoggingPage, L"CURL FLOOD", L"At LOG LEVEL FLOOD, logs every web request.", SettingsMgr->Floods.bCurl);
		AddFlagRow(LoggingPage, L"ENDPOINT FLOOD", L"At LOG LEVEL FLOOD, logs every server endpoint lookup.", SettingsMgr->Floods.bEndpoint);

		HookMetadata::ProfileInfo& Info = HookMetadata::UserProfileInfo;
		AddCategory(ProfilePage, L"ACCOUNT");
		AddValueRow(ProfilePage, L"PLATFORM", L"The platform your profile is on.", &Info.Platform, Features::Id::ProfileGetter);
		AddValueRow(ProfilePage, L"PLATFORM ID", L"Your account ID on that platform.", &Info.PlatformId, Features::Id::ProfileGetter);
		AddInfoRow(ProfilePage, L"ONLINE", RefreshOnline);
		AddValueRow(ProfilePage, L"HYDRA ID", L"Your MK1 online account ID.", &Info.HydraId, Features::Id::ProfileGetter, false, OnlineIdWhenRead);
		AddValueRow(ProfilePage, L"WB ID", L"Your WB account ID, when it's linked.", &Info.WBId, Features::Id::ProfileGetter, false, OnlineIdWhenRead);
		AddValueRow(ProfilePage, L"OFFLINE PROFILE ID", L"The ID of the profile the game uses offline. Only some accounts have one.",
			&Info.OfflineProfileId, Features::Id::ProfileGetter, false, OnlineIdWhenRead);
		AddValueRow(ProfilePage, L"SAVE KEY", L"The key your save is encrypted with.", &Info.SaveKey, Features::Id::ProfileGetter, true);
		AddInfoRow(ProfilePage, L"ACCESS TOKEN", RefreshAccessToken);

		AddCategory(ProfilePage, L"FLOYD");
		AddInfoRow(ProfilePage, L"FLOYD TOWER", RefreshFloydTower);
		AddInfoRow(ProfilePage, L"FLOYD CHALLENGES", RefreshFloydChallenges);
		AddInfoRow(ProfilePage, L"PROFILE HASH", RefreshProfileHash);

		AddFloydStatRow(L"FLOYD WINS", L"How many times you beat Floyd. The Floyd Tower only shows up once you have.", ShowFloydWins);
		AddFloydStatRow(L"TIMES MET FLOYD", L"How many times Floyd showed up.", ShowTimesMet);
		AddFloydStatRow(L"CHALLENGES DONE", L"Floyd's challenges you completed, out of your 10.", ShowChallengesDone);
		AddFloydStatRow(L"LAST FLOYD FIGHT", L"How your last fight with Floyd went.", ShowLastFight);
		AddFloydStatRow(L"NEXT CLUE", L"When Floyd may show up again to give you a clue.", ShowNextClue);
	}

	static void RefreshInfoRows()
	{
		for (size_t i = 0; i < AllRows.size(); i++)
		{
			if (AllRows[i]->IsInfo())
				AllRows[i]->Refresh(*AllRows[i]);
		}
	}

	void Init(bool bWithMods)
	{
		AddSettingsRows();
		for (size_t i = 0; i < SettingsRows.size(); i++)
			SettingsPage.Rows.push_back(SettingsRows[i]);
		if (bWithMods)
			AddModRows();
		AddMenuRows();

		for (size_t p = 0; p < _countof(AllPages); p++)
			AllRows.insert(AllRows.end(), AllPages[p]->Rows.begin(), AllPages[p]->Rows.end());
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

	static MK12::FSettingsOption* FindGameRow(MK12::USettingsScreen* Screen, int32_t Type, const wchar_t* Key)
	{
		for (int32_t p = 0; p < Screen->Pages.ElementsCount; p++)
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

	static MK12::FSettingsOption* BuildGameRow(Row& row, int32_t Id, MK12::FSettingsOption* ListTemplate, MK12::FSettingsOption* CategoryTemplate)
	{
		GameRow* game = new GameRow;
		GameRows.push_back(game);
		size_t Size = row.IsCategory() ? MK12::FSettingsOption::Size : MK12::FSettingsListOption::Size;
		game->Memory = new char[Size];
		memcpy(game->Memory, row.IsCategory() ? (void*)CategoryTemplate : (void*)ListTemplate, Size);

		MK12::FSettingsOption* Option = (MK12::FSettingsOption*)game->Memory;
		Option->Table = TEXT_TABLE;
		Option->Key = row.Title;
		Option->bLocalizedTitle = false;
		Option->Id = Id;
		memset(Option->Callbacks, 0, sizeof(Option->Callbacks));

		if (!row.IsCategory())
		{
			for (int32_t v = 0; v < (int32_t)row.Choices.size(); v++)
			{
				MK12::FSettingsListValue Value = { nullptr, row.Choices[v].Label, row.Choices[v].Description, v, false };
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
		MK12::FSettingsOption* ListTemplate = FindGameRow(Screen, MK12::SettingsOptionType::List, LIST_TEMPLATE_KEY);
		MK12::FSettingsOption* CategoryTemplate = FindGameRow(Screen, MK12::SettingsOptionType::Category, CATEGORY_TEMPLATE_KEY);
		if (!ListTemplate || !CategoryTemplate)
		{
			printfError("Mod Settings: Couldn't find the game's rows to copy, no MK12TTH tab");
			return false;
		}

		int32_t Id = FIRST_ROW_ID;
		for (size_t p = 0; p < _countof(AllPages); p++)
		{
			Page* page = AllPages[p];
			for (size_t r = 0; r < page->Rows.size(); r++)
				page->GameRows.push_back(BuildGameRow(*page->Rows[r], Id++, ListTemplate, CategoryTemplate));
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

	static void AddPage(MK12::USettingsScreen* Screen, Page& page)
	{
		if (AddedPages.empty())
			FirstOwnPage = Screen->Pages.ElementsCount;
		ActiveScreen = Screen;
		// The game frees page descriptions with the screen, the key goes in the game's memory
		int32_t KeyCount = (int32_t)wcslen(page.DescKey) + 1;
		MK12::FString Key = { nullptr, 0, 0 };
		MK12::StringReserve(&Key, KeyCount);
		memcpy(Key.Data, page.DescKey, KeyCount * sizeof(wchar_t));
		Key.Count = KeyCount;

		MK12::FSettingsPage GamePage = { page.GameRows.data(), (int32_t)page.GameRows.size(), 0 };
		AddSlot(Screen->Pages) = GamePage;
		AddSlot(Screen->PageDescriptions) = Key;
		AddedPages.push_back(&page);
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

	void OnPagesBuilt(MK12::USettingsScreen* Screen)
	{
		AddedPages.clear();
		if (!bInitialized || bBuildFailed || !Screen || !MK12::GrowArray)
			return;
		if (!bBuilt)
		{
			bBuilt = Build(Screen);
			bBuildFailed = !bBuilt;
			if (!bBuilt)
				return;
		}

		if (bOwnMenu && MK12::MemoryFree)
		{
			DropGamePages(Screen);
			RefreshInfoRows();
			for (size_t p = 0; p < _countof(MenuPages); p++)
			{
				if (!MenuPages[p]->Rows.empty())
					AddPage(Screen, *MenuPages[p]);
			}
			return;
		}
		AddPage(Screen, SettingsPage);
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
		std::vector<const Page*> Showing;
		if (bOwnMenu)
			Showing.assign(MenuPages, MenuPages + _countof(MenuPages));
		else
			Showing.push_back(&SettingsPage);
		for (size_t p = 0; p < Showing.size(); p++)
		{
			for (size_t r = 0; r < Showing[p]->Rows.size(); r++)
			{
				if (TitleMatches(Showing[p]->Rows[r], Title, Length))
					return Showing[p]->Rows[r];
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
			for (size_t p = 0; p < _countof(AllPages); p++)
			{
				if (Key[0] == L'M' && !wcscmp(Key, AllPages[p]->DescKey))
					Text = AllPages[p]->Description;
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
		if (AddedPages.empty())
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
		for (size_t i = 0; i < AddedPages.size(); i++)
		{
			MK12::FMenuEntry Entry = {};
			MK12::FMenuEntryCallback NoCallback;
			MK12::MenuEntryConstruct(&Entry, AddedPages[i]->TabKey, &NoCallback);
			Entry.bLocalized = false; // Shows the key as the label

			MK12::FSettingsTab Tab = {};
			WithOurs.push_back(Tab);
			MK12::SettingsTabConstruct(&WithOurs.back(), &Entry, TabOrder + (int32_t)i);
		}
		AddedPages.clear();

		MK12::SettingsCopyTabs(Out, WithOurs.data(), (int32_t)WithOurs.size(), ExtraA, ExtraB);
		for (size_t i = FirstOurs; i < WithOurs.size(); i++)
			MK12::SettingsTabDestruct(&WithOurs[i]);
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

	static void Restart()
	{ // Asks the game to exit, a hidden helper starts it again once this process is gone
		if (!MK12::lpEngineExitRequested)
		{
			GameMessageBox::Confirm(L"Couldn't restart the game. Close it and start it again to apply the changes.", nullptr, L"MK12TTH");
			return;
		}

		std::wstring Command = L"powershell.exe -NoProfile -WindowStyle Hidden -Command \"Wait-Process -Id "
			+ std::to_wstring(GetCurrentProcessId()) + L" -ErrorAction SilentlyContinue; Start-Process '" + RelaunchUrl() + L"'\"";
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

	static void RestartIfYes(bool bYes)
	{
		if (bYes)
			Restart();
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
			ActiveScreen = nullptr;
	}

	void OnInputUpdate(MK12::FPlayerInput* Input)
	{
		if (!Input || !ActiveScreen || !LastChangeTick || ActiveScreen->CurrentPage < FirstOwnPage)
			return;
		if ((Input->Down & ~Input->Previous) & MK12::MessageBoxInput::Confirm)
			bAskNow = true;
	}

	void Tick()
	{
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
