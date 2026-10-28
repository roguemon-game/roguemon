@ Dungeon content (src/rogue_dungeon.c): bosses, masters, the choice room
@ and the camp.

@ ---------------------------------------------------------------------------
@ A guardian is a strong wild Pokemon standing in the way; a legend is one
@ of the region's legendary Pokemon. Their battles skip the one-catch rule: the
@ ball is always free and the area stays open. Once beaten or caught, the
@ boss is gone for the run.

Rogue_EventScript_DungeonBoss::
	lock
	faceplayer
	special Rogue_PrepareBoss
	@ A legend asleep: too few badges, or the villain leader who waits
	@ before it (ROBJ_VILLAIN_LEADER, Rogue_LegendAsleep) not beaten yet.
	goto_if_eq VAR_RESULT, 2, Rogue_EventScript_LegendAsleep
	@ The line shows as the cry sounds, so a press of A answers at once
	@ (with the cry first and the text after, a short press seemed to do
	@ nothing for a second).
	waitse
	playmoncry VAR_0x8004, CRY_MODE_ENCOUNTER
	msgbox gStringVar4, MSGBOX_DEFAULT
	waitmoncry
	closemessage
	special Rogue_CreateBossMon
	setflag FLAG_SYS_CTRL_OBJ_DELETE
	special Rogue_StartBossBattle
	clearflag FLAG_SYS_CTRL_OBJ_DELETE
	specialvar VAR_0x8008, GetBattleOutcome
	goto_if_eq VAR_0x8008, B_OUTCOME_RAN, Rogue_EventScript_BossStays
	goto_if_eq VAR_0x8008, B_OUTCOME_PLAYER_TELEPORTED, Rogue_EventScript_BossStays
	special Rogue_BossBeaten
	removeobject VAR_LAST_TALKED
	goto_if_eq VAR_0x8008, B_OUTCOME_CAUGHT, Rogue_EventScript_BossEnd
	special Rogue_PrepareBoss
	special Rogue_BufferBossGone
	msgbox gStringVar4, MSGBOX_DEFAULT
Rogue_EventScript_BossEnd::
	call Rogue_EventScript_ShowUnlocks
	closemessage
	release
	end

Rogue_EventScript_BossStays::
	release
	end

Rogue_EventScript_LegendAsleep::
	msgbox gStringVar4, MSGBOX_DEFAULT
	release
	end

@ What keeps a legend's chamber shut (ROBJ_SEAL, Rogue_CheckSeal): a
@ cracked rock or a boulder, an old statue, a keeper or a villain lookout.
@ Opened, it is gone for the run, and where the seal stood on the
@ legend's own spot the legend appears.
Rogue_EventScript_LegendSeal::
	lock
	faceplayer
	special Rogue_CheckSeal
	msgbox gStringVar4, MSGBOX_DEFAULT
	goto_if_eq VAR_RESULT, FALSE, Rogue_EventScript_LegendSealEnd
	closemessage
	call_if_eq VAR_0x8007, TRUE, Rogue_EventScript_SealSound
	fadescreen FADE_TO_BLACK
	removeobject VAR_LAST_TALKED
	special Rogue_OpenSeal
	fadescreen FADE_FROM_BLACK
	waitse
	goto_if_eq VAR_0x8005, FALSE, Rogue_EventScript_LegendSealEnd
	special Rogue_RevealLegend
	addobject VAR_0x8006
	playmoncry VAR_0x8004, CRY_MODE_ENCOUNTER
	msgbox Rogue_Text_LegendAppeared, MSGBOX_DEFAULT
	waitmoncry
Rogue_EventScript_LegendSealEnd::
	release
	end

Rogue_EventScript_SealSound::
	playse SE_M_ROCK_THROW
	return

@ Whoever stands in a tunnel's narrow way (ROBJ_TUNNEL_GUARD,
@ Rogue_CheckTunnelGuard): a worker who lets those with a gym leader's badge
@ by, or a villain grunt until the hideout is cleared. Once its key is held
@ they are gone (UpdateVillainObjects).
Rogue_EventScript_TunnelGuard::
	lock
	faceplayer
	special Rogue_CheckTunnelGuard
	msgbox gStringVar4, MSGBOX_DEFAULT
	goto_if_eq VAR_RESULT, FALSE, Rogue_EventScript_TunnelGuardEnd
	closemessage
	fadescreen FADE_TO_BLACK
	removeobject VAR_LAST_TALKED
	fadescreen FADE_FROM_BLACK
Rogue_EventScript_TunnelGuardEnd::
	release
	end

Rogue_Text_LegendAppeared:
	.string "The legendary {STR_VAR_1}\n"
	.string "appeared!$"

@ ---------------------------------------------------------------------------
@ A dungeon master waits to be spoken to. The stand-in trainer is the
@ master of the dungeon the player is in (Rogue_ResolveTrainerId). Beaten,
@ they hand over a reward.

Rogue_EventScript_DungeonMaster::
	trainerbattle_single ROGUE_TRAINER(ROGUE_SLOT_DUNGEON_MASTER), Rogue_Text_MasterLine, Rogue_Text_MasterLine, Rogue_EventScript_MasterReward
	special Rogue_BufferMasterAfter
	msgbox gStringVar4, MSGBOX_AUTOCLOSE
	end

Rogue_EventScript_MasterReward::
	msgbox Rogue_Text_MasterReward, MSGBOX_DEFAULT
	special Rogue_PrepareMasterReward
	giveitem VAR_0x8000
	special Rogue_BufferMasterAfter
	msgbox gStringVar4, MSGBOX_DEFAULT
	release
	end

@ Stand-in text: the real lines are swapped in by the battle setup.
Rogue_Text_MasterLine:
	.string "...!$"

Rogue_Text_MasterReward:
	.string "You've earned this. Take it!$"

@ ---------------------------------------------------------------------------
@ A choice room's item balls: taking one, the others vanish.

@ First it asks, naming the item, as Kanto's starter balls and Mt. Moon's
@ fossils do, so nobody takes one not knowing it is a choice.
Rogue_EventScript_ChoiceItem::
	lock
	special Rogue_BufferChoiceName
	msgbox Rogue_Text_ChoiceAsk, MSGBOX_YESNO
	goto_if_eq VAR_RESULT, NO, Rogue_EventScript_ChoiceEnd
	special Rogue_PrepareItem
	callstd STD_FIND_ITEM
	special Rogue_ChoiceTaken
	goto_if_eq VAR_RESULT, FALSE, Rogue_EventScript_ChoiceEnd
	msgbox gStringVar4, MSGBOX_DEFAULT
Rogue_EventScript_ChoiceEnd::
	release
	end

Rogue_Text_ChoiceAsk:
	.string "Only one of these may be taken.\n"
	.string "Take the {STR_VAR_1}?$"

@ The person beside the balls says so too (Rogue_BufferChoiceGuide).
Rogue_EventScript_ChoiceGuide::
	lock
	faceplayer
	special Rogue_BufferChoiceGuide
	msgbox gStringVar4, MSGBOX_DEFAULT
	release
	end

@ ---------------------------------------------------------------------------
@ A camp halfway down a deep dungeon: the camper heals the team once.

@ The lines are the camper's own for the kind of dungeon (Rogue_BufferCampText).
Rogue_EventScript_Camp::
	lock
	faceplayer
	special Rogue_CheckCamp
	goto_if_eq VAR_RESULT, FALSE, Rogue_EventScript_CampUsed
	setvar VAR_0x8004, 0
	special Rogue_BufferCampText
	msgbox gStringVar4, MSGBOX_YESNO
	goto_if_eq VAR_RESULT, NO, Rogue_EventScript_CampDeclined
	closemessage
	call Common_EventScript_OutOfCenterPartyHeal
	special Rogue_UseCamp
	setvar VAR_0x8004, 1
	special Rogue_BufferCampText
	msgbox gStringVar4, MSGBOX_DEFAULT
	release
	end

Rogue_EventScript_CampDeclined::
	setvar VAR_0x8004, 2
	special Rogue_BufferCampText
	msgbox gStringVar4, MSGBOX_DEFAULT
	release
	end

Rogue_EventScript_CampUsed::
	setvar VAR_0x8004, 3
	special Rogue_BufferCampText
	msgbox gStringVar4, MSGBOX_DEFAULT
	release
	end

@ ---------------------------------------------------------------------------
@ Shipwrecks and power plants

@ A shipwreck's floors: its cabin doors locked until their keys are picked
@ up (Rogue_ApplyFloorState). A power plant's floor: New Mauville's own
@ barriers, after a blue or a green switch, as its own OnResume sets them
@ again after a battle.
Rogue_ShipFloor_MapScripts::
	map_script MAP_SCRIPT_ON_LOAD, Rogue_ShipFloor_OnLoad
	.byte 0

Rogue_ShipFloor_OnLoad:
	special Rogue_ApplyFloorState
	end

@ The region's houses, POKeMON CENTERS and League: the doors and stairs of
@ their vanilla rooms that lead nowhere here are sealed (Rogue_SealInterior).
Rogue_SealInterior_OnLoad::
	special Rogue_SealInterior
	end

Rogue_PlantFloor_MapScripts::
	map_script MAP_SCRIPT_ON_LOAD, Rogue_PlantFloor_OnLoad
	.byte 0

Rogue_PlantFloor_OnLoad:
	call_if_eq VAR_TEMP_1, 1, NewMauville_Inside_EventScript_SetBarrierStateBlueButton
	call_if_eq VAR_TEMP_2, 1, NewMauville_Inside_EventScript_SetBarrierStateGreenButton
	end

@ A locked cabin door, as the Abandoned Ship's: its key opens it.
Rogue_EventScript_ShipDoor::
	lockall
	special Rogue_CheckShipDoor
	goto_if_eq VAR_RESULT, 1, Rogue_EventScript_ShipDoorOpens
	goto_if_eq VAR_RESULT, 2, Rogue_EventScript_ShipDoorOpen
	msgbox Rogue_Text_ShipDoorLocked, MSGBOX_DEFAULT
	releaseall
	end

Rogue_EventScript_ShipDoorOpens::
	msgbox Rogue_Text_ShipDoorKey, MSGBOX_DEFAULT
	playse SE_PIN
	special DrawWholeMapView
	releaseall
	end

Rogue_EventScript_ShipDoorOpen::
	msgbox Rogue_Text_ShipDoorOpen, MSGBOX_DEFAULT
	releaseall
	end

Rogue_Text_ShipDoorLocked:
	.string "The door is locked.\p"
	.string "The key must be somewhere\n"
	.string "on this deck...$"

Rogue_Text_ShipDoorKey:
	.string "{PLAYER} inserted and turned the\n"
	.string "{STR_VAR_1}.\p"
	.string "The inserted key stuck fast,\n"
	.string "but the door opened.$"

Rogue_Text_ShipDoorOpen:
	.string "The door is open.$"

@ A power plant's item ball that is a VOLTORB, as New Mauville's.
Rogue_EventScript_Decoy::
	lock
	faceplayer
	special Rogue_PrepareDecoy
	waitse
	playmoncry VAR_0x8004, CRY_MODE_ENCOUNTER
	delay 40
	waitmoncry
	setflag FLAG_SYS_CTRL_OBJ_DELETE
	dowildbattle
	clearflag FLAG_SYS_CTRL_OBJ_DELETE
	removeobject VAR_LAST_TALKED
	release
	end

@ ---------------------------------------------------------------------------
@ Ruins (src/rogue_gen_ruins.h). Their floors are the Mirage Tower's, whose
@ cracked floor gives way under a step, as Emerald's does without the
@ Mach Bike (Rogue_SetFallWarp sends the fall to the floor below). The
@ vault's inner door is shut, as the Sealed Chamber's is, until its three
@ tablets are read in the riddle's order.

Rogue_RuinsFloor_MapScripts::
	map_script MAP_SCRIPT_ON_FRAME_TABLE, CaveHole_CheckFallDownHole
	map_script MAP_SCRIPT_ON_TRANSITION, Rogue_RuinsFloor_OnTransition
	map_script MAP_SCRIPT_ON_RESUME, Rogue_RuinsFloor_OnResume
	map_script MAP_SCRIPT_ON_LOAD, Rogue_RuinsFloor_OnLoad
	.byte 0

Rogue_RuinsFloor_OnTransition:
	setvar VAR_ICE_STEP_COUNT, 1
	end

Rogue_RuinsFloor_OnResume:
	setstepcallback STEP_CB_CRACKED_FLOOR
	end

Rogue_RuinsFloor_OnLoad:
	special Rogue_ShutRuinsDoor
	end

@ A tablet by the vault's door (Rogue_ReadRuinsTablet): its word, and once
@ the third is read in the right order, the door opens.
Rogue_EventScript_RuinsTablet::
	lockall
	special Rogue_ReadRuinsTablet
	msgbox gStringVar4, MSGBOX_DEFAULT
	goto_if_eq VAR_RESULT, TRUE, Rogue_EventScript_RuinsDoorOpens
	releaseall
	end

Rogue_EventScript_RuinsDoorOpens::
	closemessage
	delay 20
	special DoSealedChamberShakingEffect_Long
	special Rogue_OpenRuinsDoor
	playse SE_BANG
	msgbox Rogue_Text_RuinsDoorOpened, MSGBOX_DEFAULT
	releaseall
	end

Rogue_Text_RuinsDoorOpened:
	.string "With a deep rumble, the old\n"
	.string "door opened!$"

@ ---------------------------------------------------------------------------
@ Someone lost deep in a dungeon (ROBJ_EXPLORER, Rogue_BufferExplorerText):
@ they ask to be led out. Agreed, they hand over a reward and take the
@ player out with them by their ESCAPE ROPE, to the way in.

Rogue_EventScript_Explorer::
	lock
	faceplayer
	setvar VAR_0x8004, 0
	special Rogue_BufferExplorerText
	msgbox gStringVar4, MSGBOX_YESNO
	goto_if_eq VAR_RESULT, NO, Rogue_EventScript_ExplorerStays
	setvar VAR_0x8004, 1
	special Rogue_BufferExplorerText
	msgbox gStringVar4, MSGBOX_DEFAULT
	special Rogue_PrepareItem
	giveitem VAR_0x8000, VAR_0x8001
	goto_if_eq VAR_RESULT, FALSE, Rogue_EventScript_ExplorerBagFull
	setvar VAR_0x8004, 2
	special Rogue_BufferExplorerText
	msgbox gStringVar4, MSGBOX_DEFAULT
	closemessage
	playse SE_WARP_IN
	special Rogue_ExplorerEscape
	release
	end

Rogue_EventScript_ExplorerStays::
	setvar VAR_0x8004, 3
	special Rogue_BufferExplorerText
	msgbox gStringVar4, MSGBOX_DEFAULT
	release
	end

Rogue_EventScript_ExplorerBagFull::
	setvar VAR_0x8004, 4
	special Rogue_BufferExplorerText
	msgbox gStringVar4, MSGBOX_DEFAULT
	release
	end

@ ---------------------------------------------------------------------------
@ Words to read (src/rogue_text_dungeons.h)

@ A dungeon's prize ball: a line before it is picked up, by the kind of
@ dungeon, and a diary page under it where the lore puts one
@ (LORE_WAY_DIARY, Rogue_BufferPrizeText).
Rogue_EventScript_PrizeItem::
	lock
	special Rogue_BufferPrizeText
	goto_if_eq VAR_RESULT, FALSE, Rogue_EventScript_PrizeFind
	msgbox gStringVar4, MSGBOX_DEFAULT
Rogue_EventScript_PrizeFind::
	special Rogue_PrepareItem
	callstd STD_FIND_ITEM
	release
	end

@ A thing read where it stands on the map (Rogue_GetThingScript fills
@ gStringVar4 first): a tower's grave, a shipwreck's bin, a power plant's
@ terminal or generator, a painting at home or on a POKéMON CENTER's wall.
Rogue_EventScript_ReadThing::
	lockall
	msgbox gStringVar4, MSGBOX_DEFAULT
	releaseall
	end

@ The child in a POKéMON CENTER with the town's skipping rhyme
@ (LORE_WAY_RHYME).
Rogue_EventScript_RhymeChild::
	lock
	faceplayer
	special Rogue_BufferRhyme
	msgbox gStringVar4, MSGBOX_DEFAULT
	release
	end

@ The explorer, led out of a dungeon, resting in the POKéMON CENTER of the
@ town it is named after.
Rogue_EventScript_ExplorerThanks::
	lock
	faceplayer
	special Rogue_BufferExplorerThanks
	msgbox gStringVar4, MSGBOX_DEFAULT
	release
	end

@ For a museum (the town signatures): set VAR_0x8004 to the exhibit's
@ number, 0 for the first, and goto here. The first exhibit is the legend's
@ where LORE_WAY_MUSEUM tells one in this town; VAR_RESULT is FALSE past
@ the last (nothing is shown).
Rogue_EventScript_MuseumExhibit::
	lockall
	special Rogue_BufferMuseumExhibit
	goto_if_eq VAR_RESULT, FALSE, Rogue_EventScript_MuseumExhibitEnd
	msgbox gStringVar4, MSGBOX_DEFAULT
Rogue_EventScript_MuseumExhibitEnd::
	releaseall
	end

@ (v1.1 peaks) A route in a sandstorm (RogueGen_RouteWeather's RWEATHER_SAND;
@ the desert tower's most of all): in Emerald the desert is entered only with
@ GO-GOGGLES. Here the first step into a sandstorm without them brings a
@ hiker over with a spare pair, so no way is ever shut: the bag holds them
@ for the rest of the run, so nothing more is saved. VAR_TEMP_9 is set as
@ the map loads when the bag has none.
Rogue_Sandstorm_MapScripts::
	map_script MAP_SCRIPT_ON_TRANSITION, Rogue_Sandstorm_OnTransition
	map_script MAP_SCRIPT_ON_FRAME_TABLE, Rogue_Sandstorm_OnFrame
	.byte 0

Rogue_Sandstorm_OnTransition:
	setvar VAR_TEMP_9, 0
	checkitem ITEM_GO_GOGGLES
	goto_if_eq VAR_RESULT, TRUE, Rogue_Sandstorm_OnTransitionEnd
	setvar VAR_TEMP_9, 1
Rogue_Sandstorm_OnTransitionEnd:
	end

Rogue_Sandstorm_OnFrame:
	map_script_2 VAR_ROGUE_PROGRESS, ROGUE_PROGRESS_NO_STARTER, Rogue_EventScript_Arrival
	map_script_2 VAR_TEMP_9, 1, Rogue_EventScript_GoGoggles
	.2byte 0

Rogue_EventScript_GoGoggles::
	lockall
	setvar VAR_TEMP_9, 0
	msgbox Rogue_Text_SandstormBlind, MSGBOX_DEFAULT
	giveitem ITEM_GO_GOGGLES
	msgbox Rogue_Text_GogglesOn, MSGBOX_DEFAULT
	releaseall
	end

Rogue_Text_SandstormBlind:
	.string "The sandstorm is fierce! Sand\n"
	.string "whips into your eyes…\p"
	.string "A HIKER hurries over.\p"
	.string "“Whoa there! You'll be blind out\n"
	.string "here. Take these GO-GOGGLES.\p"
	.string "Desert folk always carry a\n"
	.string "spare pair!”$"

Rogue_Text_GogglesOn:
	.string "With the GO-GOGGLES on, you can\n"
	.string "see through the sandstorm!$"
