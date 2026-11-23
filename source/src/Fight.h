#pragma once
#include <cstdint>

// The fight's fighters, read from the fight's own structures. Game thread, in a fight or as one starts
namespace Fight {
	// A fighter definition (FMainCharacterDefinition)
	constexpr uint32_t FIGHTER_NAME					= 0xE0;		// The fighter's name
	constexpr uint32_t FIGHTER_AI					= 0x118;	// Its AI script, 0 is a human player

	// A team's setup: one composition per kind, the team's definitions point at the one in use
	constexpr uint32_t TEAM_KAMEO_COMPOSITION		= 0x90;		// Main fighter first, the Kameo second
	constexpr uint32_t TEAM_TAG_COMPOSITION			= 0x410;	// First fighter, then the partner
	constexpr uint32_t TEAM_ENDURANCE_COMPOSITION	= 0x8B8;	// A list of fighters at its first spot
	constexpr uint32_t TEAM_SINGLE_COMPOSITION		= 0x8D0;	// The fighter first
	constexpr uint32_t COMPOSITION_FIRST			= 0x08;

	typedef bool (FighterVisitor)(int32_t Team, char* Fighter, void* Context); // False stops

	// Each team's main fighter definition, team 0 first, until Visit returns false. False when the fight can't be read
	bool ForEachFighter(FighterVisitor* Visit, void* Context);
}
