RoguePokemonCenter_MapScripts::
	map_script MAP_SCRIPT_ON_RESUME, CableClub_OnResume
	map_script MAP_SCRIPT_ON_LOAD, Rogue_SealInterior_OnLoad
	map_script MAP_SCRIPT_ON_TRANSITION, RoguePokemonCenter_OnTransition
	.byte 0

RoguePokemonCenter_OnTransition:
	special Rogue_SetCenterRespawn
	end
