@ Scripts for the generated region. The outdoor events that use them are built
@ at runtime in src/rogue.c. The interiors are in data/maps/Rogue*.

@ ---------------------------------------------------------------------------
@ Arrival, the professor and the starter

@ The run starts in the professor's lab in the first town, facing him
@ (WarpToStart in src/rogue.c). He greets the player at once, until a
@ starter is chosen, as a real game's first partner is picked in the lab.
@ The region was picked on his map in the speech (Rogue_OpenRegionPreview).
@ Every house shares the house map, but no other can be entered before
@ the starter.
Rogue_Lab_OnFrame::
	map_script_2 VAR_ROGUE_PROGRESS, ROGUE_PROGRESS_NO_STARTER, Rogue_EventScript_Arrival
	.2byte 0

@ Only if the first town has no lab, which check_seeds.py rules out: the
@ greeting comes in the street.
Rogue_Town1_OnFrame::
	map_script_2 VAR_ROGUE_PROGRESS, ROGUE_PROGRESS_NO_STARTER, Rogue_EventScript_Arrival
	.2byte 0

@ (v1.1 towns2) A raft village: its logs sink a little under each step, as
@ Pacifidlog's do (src/rogue_gen_towns2.h). Set on every map that loads it,
@ walked into across a seam or out of a door.
Rogue_RaftVillage_MapScripts::
	map_script MAP_SCRIPT_ON_RESUME, Rogue_RaftVillage_OnResume
	.byte 0

Rogue_RaftVillage_OnResume:
	setstepcallback STEP_CB_PACIFIDLOG_BRIDGE
	end

Rogue_EventScript_Arrival::
	lockall
	goto_if_eq VAR_ROGUE_RUN_KIND, ROGUE_RUN_KIND_RUN0, Rogue_EventScript_Run0Arrival
	@ ENDLESS after the first region keeps its party: no starter to give.
	goto_if_eq VAR_ROGUE_RUN_KIND, ROGUE_RUN_KIND_ENDLESS, Rogue_EventScript_Run0EndlessArrival
	applymovement LOCALID_PLAYER, Common_Movement_WalkInPlaceFasterUp
	waitmovement 0
	goto Rogue_EventScript_ProfessorStart

@ RUN 0 (v1.0): the run starts in the same lab, with the party from the
@ HALL OF FAME (src/rogue_run0.c). Its arrival and Birch facing the wall are
@ the story's (Rogue_EventScript_Run0Arrival in data/scripts/rogue_run0.inc).

Rogue_EventScript_Professor::
	lock
	faceplayer
	goto_if_set FLAG_SYS_POKEMON_GET, Rogue_EventScript_ProfessorAfter
	goto Rogue_EventScript_ProfessorStart

Rogue_EventScript_ProfessorStart::
	setvar VAR_0x8006, BIRCH_LINE_WELCOME
	call Rogue_EventScript_ProfessorSays
	@ What he would have said on his map, if the region was not shown there.
	special Rogue_BufferRegionHints
	call_if_eq VAR_RESULT, TRUE, Rogue_EventScript_RegionHints
	@ What was unlocked since the last run, told once.
	special Rogue_HasUnlockNews
	call_if_eq VAR_RESULT, TRUE, Rogue_EventScript_UnlockNews
	special Rogue_BufferHardcoreRules
	call_if_eq VAR_RESULT, TRUE, Rogue_EventScript_HardcoreRules
	special Rogue_BufferRareFindHint
	call_if_eq VAR_RESULT, TRUE, Rogue_EventScript_RegionHints
	special Rogue_BufferLegendHint
	call_if_eq VAR_RESULT, TRUE, Rogue_EventScript_RegionHints
	goto Rogue_EventScript_ChooseStarter

Rogue_EventScript_RegionHints::
	msgbox Rogue_Text_Str1, MSGBOX_DEFAULT
	return

Rogue_EventScript_UnlockNews::
	setvar VAR_0x8006, BIRCH_LINE_NEWS
	call Rogue_EventScript_ProfessorSays
	setvar VAR_0x8005, 0
Rogue_EventScript_UnlockNewsNext::
	special Rogue_BufferUnlockNews
	goto_if_eq VAR_RESULT, FALSE, Rogue_EventScript_UnlockNewsDone
	msgbox gStringVar4, MSGBOX_DEFAULT
	goto Rogue_EventScript_UnlockNewsNext

Rogue_EventScript_UnlockNewsDone::
	return

Rogue_EventScript_HardcoreRules::
	msgbox Rogue_Text_HardcoreRules, MSGBOX_DEFAULT
	return

Rogue_Text_HardcoreRules:
	.string "You chose the HARDCORE RULES.\p"
	.string "No POKéMON will grow past the next\n"
	.string "LEADER's ace. The first is level\l"
	.string "{STR_VAR_1}.\p"
	.string "In battle, only BALLS may be used,\n"
	.string "and no switching when a foe faints.\l"
	.string "Good luck!$"

Rogue_EventScript_ChooseStarter::
	setvar VAR_0x8006, BIRCH_LINE_CHOOSE
	call Rogue_EventScript_ProfessorSays
	setvar VAR_0x8008, 0xFFFF
Rogue_EventScript_ChooseStarterMenu::
	@ The rows, their pictures and left and right: src/rogue_starter_menu.c.
	special Rogue_ChooseStarter
	waitstate
	@ "So you want X, the Y POKeMON?" as in the real games; NO goes back
	@ to the menu on the same row.
	special Rogue_BufferStarterConfirm
	msgbox gStringVar4, MSGBOX_YESNO
	goto_if_eq VAR_RESULT, NO, Rogue_EventScript_ChooseStarterMenu
	copyvar VAR_RESULT, VAR_0x8008
	special Rogue_GiveStarter
	playfanfare MUS_OBTAIN_ITEM
	message Rogue_Text_ReceivedStarter
	waitmessage
	waitfanfare
	msgbox Rogue_Text_StarterKit, MSGBOX_DEFAULT
	call_if_eq VAR_0x8005, TRUE, Rogue_EventScript_StarterExtras
	@ And, as Emerald's professor, a nickname.
	special Rogue_BufferStarterNickname
	call_if_eq VAR_RESULT, TRUE, Rogue_EventScript_StarterNickname
	setflag FLAG_SYS_POKEMON_GET
	setvar VAR_ROGUE_PROGRESS, ROGUE_PROGRESS_STARTER
	setvar VAR_0x8006, BIRCH_LINE_GOOD_LUCK
	call Rogue_EventScript_ProfessorSays
	releaseall
	end

@ One of the professor's lines (BIRCH_LINE_* in VAR_0x8006), from
@ src/rogue_text_birch.h.
Rogue_EventScript_ProfessorSays::
	special Rogue_BufferProfessor
	goto_if_eq VAR_RESULT, FALSE, Rogue_EventScript_ProfessorSaysNothing
	msgbox gStringVar4, MSGBOX_DEFAULT
Rogue_EventScript_ProfessorSaysNothing::
	return

Rogue_EventScript_StarterExtras::
	msgbox Rogue_Text_Str3, MSGBOX_DEFAULT
	return

Rogue_EventScript_StarterNickname::
	msgbox Rogue_Text_StarterNickname, MSGBOX_YESNO
	goto_if_eq VAR_RESULT, NO, Rogue_EventScript_StarterNicknameDone
	setvar VAR_0x8004, 0
	call Common_EventScript_NameReceivedPartyMon
Rogue_EventScript_StarterNicknameDone::
	return

Rogue_Text_StarterNickname:
	.string "PROF. BIRCH: While you're at it, why\n"
	.string "not give a nickname to {STR_VAR_1}?$"

@ Talked to again in his lab: where the first GYM is and his kid, the
@ rival, until a BADGE is won, then how the run goes; the rare find (with
@ the unlock), and the region's name to share.
Rogue_EventScript_ProfessorAfter::
	setvar VAR_0x8006, BIRCH_LINE_AFTER
	call Rogue_EventScript_ProfessorSays
	special Rogue_BufferRareFindHint
	call_if_eq VAR_RESULT, TRUE, Rogue_EventScript_RegionHints
	special Rogue_BufferSeed
	msgbox Rogue_Text_ProfessorAbout, MSGBOX_DEFAULT
	release
	end

Rogue_Text_StarterKit:
	.string "{STR_VAR_2}$"

Rogue_Text_Str3:
	.string "{STR_VAR_3}$"

Rogue_Text_Str2:
	.string "{STR_VAR_2}$"

Rogue_Text_ReceivedStarter:
	.string "{PLAYER} received {STR_VAR_1}\n"
	.string "and a starting kit!$"

Rogue_Text_ProfessorAbout:
	.string "I've named this land {STR_VAR_1}.\n"
	.string "Tell a friend! They can pick\l"
	.string "TYPE NAME on my map to visit.{STR_VAR_3}$"

@ After a battle in which Pokemon fainted. The text is built in
@ Rogue_CheckFallenMessage.
Rogue_EventScript_Fallen::
	lockall
	msgbox Rogue_Text_Fallen, MSGBOX_DEFAULT
	releaseall
	end

Rogue_Text_Fallen:
	.string "{STR_VAR_1}$"

@ ---------------------------------------------------------------------------
@ Permanent unlocks. Call after any milestone.

Rogue_EventScript_ShowUnlocks::
	setvar VAR_0x8009, FALSE
Rogue_EventScript_ShowUnlocksNext::
	special Rogue_CheckUnlocks
	goto_if_eq VAR_RESULT, FALSE, Rogue_EventScript_ShowUnlocksDone
	goto_if_eq VAR_RESULT, 2, Rogue_EventScript_ShowFeat
	playfanfare MUS_OBTAIN_ITEM
	message Rogue_Text_Unlocked
	waitmessage
	waitfanfare
	goto Rogue_EventScript_ShowUnlocksNext

@ A legend for the LEGEND LOG or a feat, told in gStringVar4. The first of
@ them gets the jingle of a Frontier symbol; the rest follow as plain
@ messages, so a caught legend (the log, then FIRST LEGEND) does not hold
@ the player through two five-second jingles in which A does nothing.
Rogue_EventScript_ShowFeat::
	goto_if_eq VAR_0x8009, TRUE, Rogue_EventScript_ShowFeatQuiet
	setvar VAR_0x8009, TRUE
	playfanfare MUS_OBTAIN_SYMBOL
	message gStringVar4
	waitmessage
	waitfanfare
	waitbuttonpress
	goto Rogue_EventScript_ShowUnlocksNext

Rogue_EventScript_ShowFeatQuiet::
	msgbox gStringVar4, MSGBOX_DEFAULT
	goto Rogue_EventScript_ShowUnlocksNext

Rogue_EventScript_ShowUnlocksDone::
	return

@ An unlock earned out on the field, such as by beating a dungeon's boss
@ trainer: told once the field is free (Rogue_CheckFallenMessage).
Rogue_EventScript_FieldUnlocks::
	lockall
	call Rogue_EventScript_ShowUnlocks
	closemessage
	releaseall
	end

Rogue_Text_Unlocked:
	.string "Unlocked for every future run:\n"
	.string "{STR_VAR_1}!\p"
	.string "{STR_VAR_2}$"

@ ---------------------------------------------------------------------------
@ Gates. They turn the player back until the town's goal is met.

Rogue_EventScript_GateNorth::
	lockall
	call Rogue_EventScript_GateMessage
	applymovement LOCALID_PLAYER, Rogue_Movement_StepDown
	waitmovement 0
	releaseall
	end

Rogue_EventScript_GateEast::
	lockall
	call Rogue_EventScript_GateMessage
	applymovement LOCALID_PLAYER, Rogue_Movement_StepLeft
	waitmovement 0
	releaseall
	end

Rogue_EventScript_GateSouth::
	lockall
	call Rogue_EventScript_GateMessage
	applymovement LOCALID_PLAYER, Rogue_Movement_StepUp
	waitmovement 0
	releaseall
	end

Rogue_EventScript_GateWest::
	lockall
	call Rogue_EventScript_GateMessage
	applymovement LOCALID_PLAYER, Rogue_Movement_StepRight
	waitmovement 0
	releaseall
	end

Rogue_EventScript_GateMessage::
	msgbox Rogue_Text_GateNeedStarter, MSGBOX_DEFAULT
	closemessage
	return

@ The way to VICTORY ROAD, out of the last gym town: as Emerald's guard at
@ the foot of VICTORY ROAD checks for eight BADGES. The coord event runs
@ while VAR_ROGUE_GATE_OPEN is 0, so with every BADGE won it only opens the
@ gate for the rest of the visit.
	.macro rogue_badge_gate name:req, movement:req
	.global Rogue_EventScript_BadgeGate\name
Rogue_EventScript_BadgeGate\name:
	goto_if_ge VAR_ROGUE_PROGRESS, ROGUE_PROGRESS_BADGE_8, Rogue_EventScript_BadgeGateOpen
	lockall
	msgbox Rogue_Text_GateNeedBadges, MSGBOX_DEFAULT
	closemessage
	applymovement LOCALID_PLAYER, \movement
	waitmovement 0
	releaseall
	end
	.endm

	rogue_badge_gate North, Rogue_Movement_StepDown
	rogue_badge_gate East, Rogue_Movement_StepLeft
	rogue_badge_gate South, Rogue_Movement_StepUp
	rogue_badge_gate West, Rogue_Movement_StepRight

Rogue_EventScript_BadgeGateOpen::
	setvar VAR_ROGUE_GATE_OPEN, 1
	end

Rogue_Movement_StepDown:
	walk_down
	step_end

Rogue_Movement_StepLeft:
	walk_left
	step_end

Rogue_Movement_StepUp:
	walk_up
	step_end

Rogue_Movement_StepRight:
	walk_right
	step_end

Rogue_Text_GateNeedStarter:
	.string "It's dangerous to go out without\n"
	.string "a POKéMON! Talk to the PROFESSOR.$"

Rogue_Text_GateNeedBadges:
	.string "VICTORY ROAD lies ahead. Only\n"
	.string "TRAINERS with all eight BADGES\l"
	.string "may pass. Come back when you have\l"
	.string "them all!$"

@ ---------------------------------------------------------------------------
@ Signs

Rogue_EventScript_TownSign::
	special Rogue_BufferTownSign
	msgbox Rogue_Text_TownSign, MSGBOX_SIGN
	end

Rogue_EventScript_GameCornerSign::
	msgbox Rogue_Text_GameCornerSign, MSGBOX_SIGN
	end

Rogue_EventScript_BattleTentSign::
	msgbox Rogue_Text_BattleTentSign, MSGBOX_SIGN
	end

Rogue_EventScript_MotelSign::
	msgbox Rogue_Text_MotelSign, MSGBOX_SIGN
	end

Rogue_EventScript_DungeonSign::
	special Rogue_BufferDungeonSign
	goto_if_eq VAR_RESULT, TRUE, Rogue_EventScript_HideoutSign
	goto_if_eq VAR_RESULT, 2, Rogue_EventScript_LegendSign
	goto_if_eq VAR_RESULT, 3, Rogue_EventScript_LegendShrine
	msgbox Rogue_Text_TowerSign, MSGBOX_SIGN
	end

Rogue_EventScript_ForestSign::
	special Rogue_BufferForestSign
	goto_if_eq VAR_RESULT, TRUE, Rogue_EventScript_ForestRoadSign
	msgbox Rogue_Text_ForestSign, MSGBOX_SIGN
	end

Rogue_EventScript_ForestRoadSign::
	msgbox Rogue_Text_ForestRoadSign, MSGBOX_SIGN
	end

@ Old words on a chamber's wall name the legend that rests there and
@ what opens its way (Rogue_BufferLegendCarving).
Rogue_EventScript_LegendCarving::
	special Rogue_BufferLegendCarving
	msgbox gStringVar4, MSGBOX_SIGN
	end

Rogue_EventScript_LegendSign::
	msgbox Rogue_Text_LegendSign, MSGBOX_SIGN
	end

@ A lair's sign, with what people notice there written under its name
@ (LORE_WAY_SHRINE: no shrine is built, so the sign says it).
Rogue_EventScript_LegendShrine::
	msgbox gStringVar4, MSGBOX_SIGN
	end

Rogue_EventScript_HideoutSign::
	msgbox Rogue_Text_HideoutSign, MSGBOX_SIGN
	end

Rogue_EventScript_RouteSign::
	special Rogue_BufferRouteSign
	msgbox Rogue_Text_RouteSign, MSGBOX_SIGN
	end

Rogue_EventScript_GymSign::
	special Rogue_BufferGymInfo
	goto_if_eq VAR_RESULT, FALSE, Rogue_EventScript_LeagueSign
	msgbox Rogue_Text_GymSign, MSGBOX_SIGN
	end

Rogue_EventScript_GymStatue::
	special Rogue_BufferGymInfo
	msgbox Rogue_Text_GymSign, MSGBOX_SIGN
	end

Rogue_EventScript_LeagueSign::
	msgbox Rogue_Text_LeagueSign, MSGBOX_SIGN
	end

Rogue_EventScript_LockedDoor::
	msgbox Rogue_Text_LockedDoor, MSGBOX_SIGN
	end

@ The plate beside a home: its family (Rogue_BufferHouseSign).
Rogue_EventScript_HouseSign::
	special Rogue_BufferHouseSign
	msgbox Rogue_Text_Str1, MSGBOX_SIGN
	end

Rogue_EventScript_LabSign::
	special Rogue_BufferSeed
	msgbox Rogue_Text_LabSign, MSGBOX_SIGN
	end

@ Signs beside special houses.
Rogue_EventScript_SchoolSign::
	msgbox Rogue_Text_SchoolSign, MSGBOX_SIGN
	end

Rogue_EventScript_FanClubSign::
	msgbox Rogue_Text_FanClubSign, MSGBOX_SIGN
	end

Rogue_EventScript_BikeShopSign::
	msgbox Rogue_Text_BikeShopSign, MSGBOX_SIGN
	end

Rogue_EventScript_NameRaterSign::
	msgbox Rogue_Text_NameRaterSign, MSGBOX_SIGN
	end

Rogue_EventScript_MoveDeleterSign::
	msgbox Rogue_Text_MoveDeleterSign, MSGBOX_SIGN
	end

Rogue_EventScript_TutorSign::
	msgbox Rogue_Text_TutorSign, MSGBOX_SIGN
	end

@ The gift house's plate: its family's name and a welcome (src/rogue_town.c),
@ and where the fossil scientist visits, word of it.
Rogue_EventScript_GiftHouseSign::
	special Rogue_BufferHouseSign
	special Rogue_CheckFossilTown
	goto_if_eq VAR_RESULT, TRUE, Rogue_EventScript_FossilHouseSign
	msgbox Rogue_Text_Str1, MSGBOX_SIGN
	end

Rogue_EventScript_FossilHouseSign::
	msgbox Rogue_Text_FossilHouseSign, MSGBOX_SIGN
	end

Rogue_Text_TownSign:
	.string "{STR_VAR_1}\n"
	.string "“{STR_VAR_2}”$"

Rogue_Text_TowerSign:
	.string "{STR_VAR_1}\n"
	.string "{STR_VAR_2}$"

Rogue_Text_ForestSign:
	.string "{STR_VAR_1}\n"
	.string "{STR_VAR_2}$"

Rogue_Text_ForestRoadSign:
	.string "{STR_VAR_1}\n"
	.string "{STR_VAR_2}$"

Rogue_Text_LegendSign:
	.string "{STR_VAR_1}\p"
	.string "Old words are carved in the rock:\n"
	.string "“An ancient POKéMON rests\l"
	.string "deep within.”{STR_VAR_3}$"

Rogue_Text_HideoutSign:
	.string "PRIVATE PROPERTY\n"
	.string "No trespassing. Keep out!$"

Rogue_Text_GameCornerSign:
	.string "GAME CORNER\n"
	.string "Closed for renovations. Sorry!$"

Rogue_Text_BattleTentSign:
	.string "BATTLE TENT\n"
	.string "Opening next season!$"

Rogue_Text_MotelSign:
	.string "COVE LILY MOTEL\n"
	.string "No vacancy.$"

Rogue_Text_RouteSign:
	.string "{STR_VAR_1}\n"
	.string "Ahead: {STR_VAR_2}$"

Rogue_Text_GymSign:
	.string "{STR_VAR_1} POKéMON GYM\n"
	.string "LEADER: {STR_VAR_2}\p"
	.string "The {STR_VAR_3}-type expert!$"

Rogue_Text_LeagueSign:
	.string "POKéMON LEAGUE\n"
	.string "CHAMPION: {STR_VAR_2}\p"
	.string "There is no turning back\n"
	.string "once you step inside.$"

Rogue_Text_LockedDoor:
	.string "It's locked.$"

Rogue_Text_LabSign:
	.string "PROF. BIRCH's FIELD LAB\n"
	.string "Studying the {STR_VAR_1} region.$"

Rogue_Text_SchoolSign:
	.string "POKéMON TRAINER'S SCHOOL\p"
	.string "“We'll teach you anything about\n"
	.string "POKéMON!”$"

Rogue_Text_FanClubSign:
	.string "POKéMON FAN CLUB\n"
	.string "“Calling all fans of POKéMON!”$"

Rogue_Text_BikeShopSign:
	.string "CYCLE SHOP\n"
	.string "“Every TRAINER needs a BIKE!”$"

Rogue_Text_NameRaterSign:
	.string "NAME RATER'S HOUSE\n"
	.string "“POKéMON nicknames rated.”$"

Rogue_Text_MoveDeleterSign:
	.string "MOVE DELETER'S HOUSE\n"
	.string "“Unwanted moves forgotten.”$"

Rogue_Text_TutorSign:
	.string "MOVE TUTOR'S HOUSE\n"
	.string "“Forgotten moves remembered.”$"

Rogue_Text_FossilHouseSign:
	.string "{STR_VAR_1}\p"
	.string "“Visiting now: a scientist who\n"
	.string "brings fossils back to life.”$"

@ Dungeon bosses, masters and rewards: data/scripts/rogue_dungeon.inc

@ ---------------------------------------------------------------------------
@ The villain team. Its grunts stand in the way out of the hideout's town
@ until the admin at the bottom of the hideout is beaten; then he leads them
@ out and the way is open for good (FLAG_ROGUE_HIDEOUT_CLEARED). The leader
@ waits later, before the legend.

	.macro rogue_villain_gate name:req, movement:req
	.global Rogue_EventScript_VillainGate\name
Rogue_EventScript_VillainGate\name:
	goto_if_set FLAG_ROGUE_HIDEOUT_CLEARED, Rogue_EventScript_BadgeGateOpen
	lockall
	special Rogue_BufferVillainBlock
	msgbox Rogue_Text_Str1, MSGBOX_DEFAULT
	closemessage
	applymovement LOCALID_PLAYER, \movement
	waitmovement 0
	releaseall
	end
	.endm

	rogue_villain_gate North, Rogue_Movement_StepDown
	rogue_villain_gate East, Rogue_Movement_StepLeft
	rogue_villain_gate South, Rogue_Movement_StepUp
	rogue_villain_gate West, Rogue_Movement_StepRight

Rogue_EventScript_VillainGrunt::
	lock
	faceplayer
	special Rogue_BufferVillainBlock
	msgbox Rogue_Text_Str1, MSGBOX_DEFAULT
	release
	end

Rogue_EventScript_VillainAdmin::
	trainerbattle_single ROGUE_TRAINER(ROGUE_SLOT_VILLAIN_ADMIN), Rogue_Text_TrainerLine, Rogue_Text_TrainerLine, Rogue_EventScript_VillainAdminBeaten
	goto Rogue_EventScript_VillainAdminBeaten

@ The boss's parting words, then he leads the grunts out and the field says
@ the team has left.
Rogue_EventScript_VillainAdminBeaten::
	special Rogue_VillainAdminBeaten
	msgbox gStringVar4, MSGBOX_DEFAULT
	closemessage
	fadescreen FADE_TO_BLACK
	removeobject VAR_0x8004
	fadescreen FADE_FROM_BLACK
	msgbox Rogue_Text_Str1, MSGBOX_DEFAULT
	release
	end

Rogue_EventScript_VillainLeader::
	trainerbattle_single ROGUE_TRAINER(ROGUE_SLOT_VILLAIN_LEADER), Rogue_Text_TrainerLine, Rogue_Text_TrainerLine, Rogue_EventScript_VillainLeaderBeaten
	goto Rogue_EventScript_VillainLeaderBeaten

Rogue_EventScript_VillainLeaderBeaten::
	special Rogue_VillainLeaderBeaten
	msgbox gStringVar4, MSGBOX_DEFAULT
	closemessage
	fadescreen FADE_TO_BLACK
	removeobject VAR_0x8004
	fadescreen FADE_FROM_BLACK
	release
	end

@ ---------------------------------------------------------------------------
@ Items

Rogue_EventScript_FindItem::
	special Rogue_PrepareItem
	callstd STD_FIND_ITEM
	end

@ ---------------------------------------------------------------------------
@ Pokemon Centers, Marts and houses

Rogue_EventScript_Nurse::
	setvar VAR_0x800B, 1
	@ RUN 0's patch: three heals per CENTER (src/rogue_run0_challenge.c).
	special Rogue_Run0NurseBefore
	goto_if_eq VAR_RESULT, FALSE, Rogue_EventScript_Run0NurseDone
	@ Under the CENTERS patch she says how many heals are left.
	call_if_ne VAR_0x8006, 0xFFFF, Rogue_EventScript_Run0NurseLeft
	call Common_EventScript_PkmnCenterNurse
	special Rogue_Run0NurseAfter
	waitmessage
	waitbuttonpress
	release
	end

Rogue_EventScript_Run0NurseDone::
	lock
	faceplayer
	msgbox Rogue_Text_Run0NurseDone, MSGBOX_DEFAULT
	release
	end

Rogue_EventScript_Run0NurseLeft:
	lock
	faceplayer
	buffernumberstring STR_VAR_1, VAR_0x8006
	goto_if_eq VAR_0x8006, 1, Rogue_EventScript_Run0NurseLastHeal
	msgbox Rogue_Text_Run0NurseLeft, MSGBOX_DEFAULT
	return

Rogue_EventScript_Run0NurseLastHeal:
	msgbox Rogue_Text_Run0NurseLast, MSGBOX_DEFAULT
	return

Rogue_Text_Run0NurseLeft:
	.string "The new rule: this CENTER has\n"
	.string "{STR_VAR_1} heals left.$"

Rogue_Text_Run0NurseLast:
	.string "The new rule: this CENTER has\n"
	.string "one heal left.$"

Rogue_Text_Run0NurseDone:
	.string "I'm sorry. This CENTER has given\n"
	.string "its three heals.\p"
	.string "That's the rule now.$"

@ One clerk for every Mart. What it sells grows with the badges won: the
@ basics, as Emerald's Marts sell them, with Timer Balls from the fifth
@ badge (Rustboro's Mart sells them after Devon's) and the plain evolution
@ stones, as Celadon's store sells them, so a stone evolution can help in
@ the last three gyms, and from the seventh Dusk and Quick Balls for the
@ legends. Themed
@ towns have a second clerk with a specialty counter
@ (Rogue_EventScript_SpecialtyClerk).
Rogue_EventScript_MartClerk::
	lock
	faceplayer
	message gText_HowMayIServeYou
	waitmessage
	@ RUN 0's Marts: ETHERS, MAX ETHERS from the sixth badge, and the
	@ SHOPS patch's list with no medicine (src/rogue_run0_challenge.c).
	special Rogue_Run0PrepareMart
	goto_if_eq VAR_RESULT, TRUE, Rogue_EventScript_Run0Mart
	specialvar VAR_RESULT, Rogue_GetMartTier
	switch VAR_RESULT
	case 0, Rogue_EventScript_Mart0
	case 1, Rogue_EventScript_Mart1
	case 2, Rogue_EventScript_Mart2
	case 3, Rogue_EventScript_Mart3
	case 4, Rogue_EventScript_Mart4
	goto Rogue_EventScript_Mart5

	.macro rogue_mart n:req
Rogue_EventScript_Mart\n\():
	pokemart Rogue_Mart\n\()_Items
	msgbox gText_PleaseComeAgain, MSGBOX_DEFAULT
	release
	end
	.endm

@ The specialty counter of a themed town's Mart (src/rogue_town.c).
Rogue_EventScript_SpecialtyClerk::
	lock
	faceplayer
	special Rogue_PrepareSpecialtyShop
	message Rogue_Text_Str1
	waitmessage
	special Rogue_OpenSpecialtyShop
	waitstate
	msgbox gText_PleaseComeAgain, MSGBOX_DEFAULT
	release
	end

Rogue_EventScript_Run0Mart:
	special Rogue_Run0OpenMart
	waitstate
	msgbox gText_PleaseComeAgain, MSGBOX_DEFAULT
	release
	end

	@ The SHOPS patch's list. RUN 0's Marts sell it, or the fourth or fifth
	@ list, with ETHERS after it (Rogue_Run0PrepareMart).
	.align 2
Rogue_Run0Mart_Items::
	.2byte ITEM_ULTRA_BALL
	.2byte ITEM_GREAT_BALL
	.2byte ITEM_TIMER_BALL
	.2byte ITEM_DUSK_BALL
	.2byte ITEM_QUICK_BALL
	.2byte ITEM_MAX_REPEL
	.2byte ITEM_ESCAPE_ROPE
	.2byte ITEM_X_ATTACK
	.2byte ITEM_X_DEFENSE
	.2byte ITEM_X_SPEED
	pokemartlistend

	rogue_mart 0
	rogue_mart 1
	rogue_mart 2
	rogue_mart 3
	rogue_mart 4
	rogue_mart 5

	.align 2
Rogue_Mart0_Items:
	.2byte ITEM_POKE_BALL
	.2byte ITEM_POTION
	.2byte ITEM_ANTIDOTE
	.2byte ITEM_PARALYZE_HEAL
	.2byte ITEM_AWAKENING
	.2byte ITEM_REPEL
	.2byte ITEM_ESCAPE_ROPE
	pokemartlistend

	.align 2
Rogue_Mart1_Items:
	.2byte ITEM_POKE_BALL
	.2byte ITEM_GREAT_BALL
	.2byte ITEM_POTION
	.2byte ITEM_SUPER_POTION
	.2byte ITEM_ANTIDOTE
	.2byte ITEM_PARALYZE_HEAL
	.2byte ITEM_BURN_HEAL
	.2byte ITEM_ICE_HEAL
	.2byte ITEM_AWAKENING
	.2byte ITEM_REPEL
	.2byte ITEM_ESCAPE_ROPE
	pokemartlistend

	.align 2
Rogue_Mart2_Items:
	.2byte ITEM_POKE_BALL
	.2byte ITEM_GREAT_BALL
	.2byte ITEM_POTION
	.2byte ITEM_SUPER_POTION
	.2byte ITEM_ANTIDOTE
	.2byte ITEM_PARALYZE_HEAL
	.2byte ITEM_BURN_HEAL
	.2byte ITEM_ICE_HEAL
	.2byte ITEM_AWAKENING
	.2byte ITEM_SUPER_REPEL
	.2byte ITEM_ESCAPE_ROPE
	pokemartlistend

	.align 2
Rogue_Mart3_Items:
	.2byte ITEM_POKE_BALL
	.2byte ITEM_GREAT_BALL
	.2byte ITEM_ULTRA_BALL
	.2byte ITEM_TIMER_BALL
	.2byte ITEM_SUPER_POTION
	.2byte ITEM_HYPER_POTION
	.2byte ITEM_FULL_HEAL
	.2byte ITEM_SUPER_REPEL
	.2byte ITEM_ESCAPE_ROPE
	.2byte ITEM_X_ATTACK
	.2byte ITEM_X_DEFENSE
	.2byte ITEM_FIRE_STONE
	.2byte ITEM_WATER_STONE
	.2byte ITEM_THUNDER_STONE
	.2byte ITEM_LEAF_STONE
	.2byte ITEM_MOON_STONE
	.2byte ITEM_SUN_STONE
	pokemartlistend

	.align 2
Rogue_Mart4_Items::
	.2byte ITEM_ULTRA_BALL
	.2byte ITEM_TIMER_BALL
	.2byte ITEM_DUSK_BALL
	.2byte ITEM_QUICK_BALL
	.2byte ITEM_HYPER_POTION
	.2byte ITEM_MAX_POTION
	.2byte ITEM_FULL_HEAL
	.2byte ITEM_MAX_REPEL
	.2byte ITEM_ESCAPE_ROPE
	.2byte ITEM_X_ATTACK
	.2byte ITEM_X_DEFENSE
	.2byte ITEM_X_SPEED
	.2byte ITEM_FIRE_STONE
	.2byte ITEM_WATER_STONE
	.2byte ITEM_THUNDER_STONE
	.2byte ITEM_LEAF_STONE
	.2byte ITEM_MOON_STONE
	.2byte ITEM_SUN_STONE
	pokemartlistend

	.align 2
Rogue_Mart5_Items::
	.2byte ITEM_ULTRA_BALL
	.2byte ITEM_TIMER_BALL
	.2byte ITEM_DUSK_BALL
	.2byte ITEM_QUICK_BALL
	.2byte ITEM_HYPER_POTION
	.2byte ITEM_MAX_POTION
	.2byte ITEM_FULL_RESTORE
	.2byte ITEM_FULL_HEAL
	.2byte ITEM_MAX_REPEL
	.2byte ITEM_ESCAPE_ROPE
	.2byte ITEM_X_ATTACK
	.2byte ITEM_X_DEFENSE
	.2byte ITEM_X_SPEED
	.2byte ITEM_X_SP_ATK
	.2byte ITEM_FIRE_STONE
	.2byte ITEM_WATER_STONE
	.2byte ITEM_THUNDER_STONE
	.2byte ITEM_LEAF_STONE
	.2byte ITEM_MOON_STONE
	.2byte ITEM_SUN_STONE
	pokemartlistend

@ People in houses. Who they are and what they say is set in src/rogue_town.c.

Rogue_EventScript_HouseChat::
	lock
	faceplayer
	special Rogue_BufferHouseTalk
	msgbox Rogue_Text_Str1, MSGBOX_DEFAULT
	release
	end

@ An old one in the POKéMON CENTER who knows a legend's tale
@ (Rogue_BufferLoreElder, src/rogue_town.c).
Rogue_EventScript_LoreElder::
	lock
	faceplayer
	special Rogue_BufferLoreElder
	msgbox Rogue_Text_Str1, MSGBOX_DEFAULT
	release
	end

@ The ruin maniac in the POKéMON CENTER of the town ruins are named after:
@ the riddle of their vault (Rogue_BufferRuinsScholar).
Rogue_EventScript_RuinsScholar::
	lock
	faceplayer
	special Rogue_BufferRuinsScholar
	msgbox Rogue_Text_Str1, MSGBOX_DEFAULT
	release
	end

Rogue_EventScript_HouseGift::
	lock
	faceplayer
	special Rogue_PrepareGift
	delay 1
	goto_if_eq VAR_RESULT, FALSE, Rogue_EventScript_HouseGiftDone
	special Rogue_BufferGiftOffer
	msgbox Rogue_Text_Str1, MSGBOX_DEFAULT
	giveitem VAR_0x8000, VAR_0x8001
	goto_if_eq VAR_RESULT, FALSE, Rogue_EventScript_HouseGiftBagFull
	special Rogue_MarkGiftGiven
	special Rogue_BufferGiftAfter
	msgbox Rogue_Text_Str1, MSGBOX_DEFAULT
	release
	end

Rogue_EventScript_HouseGiftDone::
	special Rogue_BufferGiftAfter
	msgbox Rogue_Text_Str1, MSGBOX_DEFAULT
	release
	end

Rogue_EventScript_HouseGiftBagFull::
	msgbox gText_TooBadBagIsFull, MSGBOX_DEFAULT
	release
	end

@ A free move tutor: relearns forgotten level-up moves.
Rogue_EventScript_MoveRelearner::
	lock
	faceplayer
	msgbox Rogue_Text_RelearnerIntro, MSGBOX_YESNO
	goto_if_eq VAR_RESULT, NO, Rogue_EventScript_RelearnerBye
Rogue_EventScript_RelearnerChooseMon::
	msgbox Rogue_Text_RelearnerWhich, MSGBOX_DEFAULT
	setmoverelearnerstate MOVE_RELEARNER_LEVEL_UP_MOVES
	chooseboxmon SELECT_PC_MON_MOVE_RELEARNER
	waitstate
	goto_if_eq VAR_0x8004, PARTY_NOTHING_CHOSEN, Rogue_EventScript_RelearnerBye
	special IsSelectedMonEgg
	goto_if_eq VAR_RESULT, TRUE, Rogue_EventScript_RelearnerBye
	special Special_HasMoveToRelearn
	goto_if_eq VAR_RESULT, FALSE, Rogue_EventScript_RelearnerNothing
	msgbox Rogue_Text_RelearnerTeach, MSGBOX_DEFAULT
	special TeachMoveRelearnerMove
	goto_if_eq VAR_RESULT, FALSE, Rogue_EventScript_RelearnerChooseMon
	goto Rogue_EventScript_RelearnerBye
	end

Rogue_EventScript_RelearnerNothing::
	msgbox Rogue_Text_RelearnerNothing, MSGBOX_DEFAULT
	goto Rogue_EventScript_RelearnerChooseMon
	end

Rogue_EventScript_RelearnerBye::
	msgbox Rogue_Text_RelearnerBye, MSGBOX_DEFAULT
	release
	end

Rogue_Text_RelearnerIntro:
	.string "I'm the MOVE TUTOR. I can remind\n"
	.string "a POKéMON of a move it forgot.\p"
	.string "No charge for new trainers.\n"
	.string "Shall I?$"

Rogue_Text_RelearnerWhich:
	.string "Which POKéMON needs reminding?\n"
	.string "Pick one from your party.$"

Rogue_Text_RelearnerTeach:
	.string "Which move should it remember?$"

Rogue_Text_RelearnerNothing:
	.string "Hmm, that POKéMON has nothing\n"
	.string "to remember.$"

Rogue_Text_RelearnerBye:
	.string "Come back any time.$"

@ PROF. BIRCH's aide in the first and fourth gym towns' POKéMON CENTERS,
@ with a starter the player did not pick (SPARE STARTERS). A full party sends
@ it to the PC.
Rogue_EventScript_SpareAide::
	lock
	faceplayer
	special Rogue_PrepareSpareStarter
	goto_if_eq VAR_RESULT, FALSE, Rogue_EventScript_SpareAideAfter
	msgbox Rogue_Text_SpareOffer, MSGBOX_DEFAULT
	givemon VAR_0x8000, VAR_0x8001
	goto_if_eq VAR_RESULT, MON_CANT_GIVE, Rogue_EventScript_SpareAideNoRoom
	copyvar VAR_0x8002, VAR_RESULT
	special Rogue_MarkSpareStarterGiven
	playfanfare MUS_OBTAIN_ITEM
	message Rogue_Text_SpareReceived
	waitmessage
	waitfanfare
	call_if_eq VAR_0x8002, MON_GIVEN_TO_PC, Rogue_EventScript_SpareToPC
	msgbox Rogue_Text_SpareAfter, MSGBOX_DEFAULT
	release
	end

Rogue_EventScript_SpareToPC::
	msgbox Rogue_Text_SpareToPC, MSGBOX_DEFAULT
	return

Rogue_EventScript_SpareAideAfter::
	msgbox Rogue_Text_SpareAfter, MSGBOX_DEFAULT
	release
	end

Rogue_EventScript_SpareAideNoRoom::
	msgbox Rogue_Text_SpareNoRoom, MSGBOX_DEFAULT
	release
	end

Rogue_Text_SpareOffer:
	.string "Hi! I'm one of PROF. BIRCH's AIDES.\p"
	.string "The PROF. asked me to bring you the\n"
	.string "{STR_VAR_1} you didn't pick.\p"
	.string "A spare partner might help on the\n"
	.string "road ahead. Please take it!$"

Rogue_Text_SpareReceived:
	.string "{PLAYER} received {STR_VAR_1}!$"

Rogue_Text_SpareToPC:
	.string "Your party is full, so {STR_VAR_1}\n"
	.string "went to a BOX in the PC.$"

Rogue_Text_SpareAfter:
	.string "The PROF. is cheering for you,\n"
	.string "{PLAYER}. Good luck out there!$"

Rogue_Text_SpareNoRoom:
	.string "Oh, there's no room anywhere, not\n"
	.string "even in the PC. I'll wait here.$"

@ The League lobby at the summit. Her heals are never counted, in RUN 0 too:
@ the CENTERS patch counts the towns' CENTERS only, and in RUN 0 she says so.
Rogue_EventScript_LeagueNurse::
	setvar VAR_0x800B, LOCALID_ROGUE_LEAGUE_NURSE
	special Rogue_Run0LeagueNurse
	call_if_eq VAR_RESULT, TRUE, Rogue_EventScript_Run0LeagueNurse
	call Common_EventScript_PkmnCenterNurse
	waitmessage
	waitbuttonpress
	release
	end

Rogue_EventScript_Run0LeagueNurse:
	lock
	faceplayer
	msgbox Rogue_Text_Run0LeagueNurse, MSGBOX_DEFAULT
	return

Rogue_Text_Run0LeagueNurse:
	.string "No rule reaches this counter.\n"
	.string "I heal as often as you need.$"

Rogue_EventScript_LeagueGuard::
	lock
	faceplayer
	special Rogue_BufferSeed
	msgbox Rogue_Text_LeagueGuard, MSGBOX_DEFAULT
	release
	end

Rogue_Text_LeagueGuard:
	.string "Welcome to the POKéMON LEAGUE.\n"
	.string "Eight BADGES... Yes, you may pass.\p"
	.string "The ELITE FOUR wait through that\n"
	.string "door, then the CHAMPION. There is\l"
	.string "no coming back out until it's decided.$"

@ ---------------------------------------------------------------------------
@ Route and cave trainers

	@ The intro and defeat lines come from Rogue_GetTrainerText, by class.
	.macro rogue_trainer n:req, trainer:req
	.global Rogue_EventScript_Trainer\n\()
Rogue_EventScript_Trainer\n\():
	trainerbattle_single \trainer, Rogue_Text_TrainerLine, Rogue_Text_TrainerLine
	setvar VAR_0x8004, \n
	special Rogue_BufferTrainerAfter
	msgbox Rogue_Text_Str1, MSGBOX_AUTOCLOSE
	end
	.endm

@ Stand-in text: the real lines are swapped in by the battle setup.
Rogue_Text_TrainerLine:
	.string "...!$"

Rogue_Text_Str1:
	.string "{STR_VAR_1}$"

@ ---------------------------------------------------------------------------
@ The rival. Waits on the second route, before the cave on the way to the
@ fourth gym, after the sixth gym and at the foot of Victory Road, and leaves
@ after each battle.

	.macro rogue_rival n:req
	.global Rogue_EventScript_Rival\n
Rogue_EventScript_Rival\n\():
	trainerbattle_single ROGUE_TRAINER(ROGUE_SLOT_RIVAL_0 + \n - 1), Rogue_Text_TrainerLine, Rogue_Text_TrainerLine, Rogue_EventScript_Rival\n\()Leaves
	end

Rogue_EventScript_Rival\n\()Leaves:
	special Rogue_RecordRivalWin
	setvar VAR_0x8004, \n
	special Rogue_BufferRivalAfter
	msgbox gStringVar4, MSGBOX_DEFAULT
	.if \n == 2
	call Rogue_EventScript_RivalItemfinder
	.endif
	.if \n == 4
	call Rogue_EventScript_RivalGift
	.endif
	closemessage
	setflag FLAG_ROGUE_RIVAL_1 + \n - 1
	@ On ahead, or home at the foot of Victory Road. VAR_LAST_TALKED is not
	@ the rival when she walked up to the player, so the special finds her.
	.if \n == 4
	setvar VAR_0x8005, TRUE
	.else
	setvar VAR_0x8005, FALSE
	.endif
	special Rogue_PrepareRivalExit
	goto_if_eq VAR_RESULT, FALSE, Rogue_EventScript_RivalFadesOut
	applymovement VAR_0x8004, gRogueRivalExitMovement
	waitmovement 0
	removeobject VAR_0x8004
	release
	end
	.endm

@ With no clear way off, the screen fades and the rival is gone.
Rogue_EventScript_RivalFadesOut:
	fadescreen FADE_TO_BLACK
	removeobject VAR_0x8004
	fadescreen FADE_FROM_BLACK
	release
	end


	rogue_rival 1
	rogue_rival 2
	rogue_rival 3
	rogue_rival 4

	.align 2
gRogueRivalScripts::
	.4byte Rogue_EventScript_Rival1
	.4byte Rogue_EventScript_Rival2
	.4byte Rogue_EventScript_Rival3
	.4byte Rogue_EventScript_Rival4

@ RIVAL'S GIFT: at the foot of Victory Road, once per run.
Rogue_EventScript_RivalGift::
	special Rogue_PrepareRivalGift
	goto_if_eq VAR_RESULT, FALSE, Rogue_EventScript_RivalGiftDone
	msgbox Rogue_Text_RivalGift, MSGBOX_DEFAULT
	giveitem VAR_0x8000
	goto_if_eq VAR_RESULT, FALSE, Rogue_EventScript_RivalGiftDone
	setflag FLAG_ROGUE_RIVAL_GIFT
	msgbox Rogue_Text_RivalGiftAfter, MSGBOX_DEFAULT
Rogue_EventScript_RivalGiftDone::
	return

@ The second time, the rival hands over an Itemfinder, as on Emerald's
@ Route 110: hidden items lie all over the region.
Rogue_EventScript_RivalItemfinder::
	@ RUN 0's Previous Player only says "...".
	goto_if_eq VAR_ROGUE_RUN_KIND, ROGUE_RUN_KIND_RUN0, Rogue_EventScript_RivalItemfinderDone
	checkitem ITEM_DOWSING_MACHINE
	goto_if_eq VAR_RESULT, TRUE, Rogue_EventScript_RivalItemfinderDone
	msgbox Rogue_Text_RivalItemfinder, MSGBOX_DEFAULT
	giveitem ITEM_DOWSING_MACHINE
	msgbox Rogue_Text_RivalItemfinderAfter, MSGBOX_DEFAULT
Rogue_EventScript_RivalItemfinderDone::
	return

Rogue_Text_RivalItemfinder:
	.string "Oh, Dad asked me to give you\n"
	.string "this. Here!$"

Rogue_Text_RivalItemfinderAfter:
	.string "It beeps when something's hidden\n"
	.string "nearby. Use it on the routes!$"

Rogue_Text_RivalGift:
	.string "Oh, and take this. I found it on\n"
	.string "the way, but you'll need it more.$"

Rogue_Text_RivalGiftAfter:
	.string "Give it to a POKéMON to hold.\n"
	.string "It's tough up there!$"

@ ---------------------------------------------------------------------------
@ Townsfolk and their Pokemon. What they say is built in src/rogue_town.c.

Rogue_EventScript_Townsperson::
	lock
	faceplayer
	special Rogue_PrepareFolkGift
	goto_if_eq VAR_RESULT, TRUE, Rogue_EventScript_FolkGift
	special Rogue_BufferTownTalk
	msgbox Rogue_Text_Str1, MSGBOX_DEFAULT
	release
	end

@ A visitor resting in a POKéMON CENTER (src/rogue_town.c).
Rogue_EventScript_CenterFolk::
	lock
	faceplayer
	special Rogue_BufferCenterTalk
	msgbox Rogue_Text_Str1, MSGBOX_DEFAULT
	release
	end

@ A townsperson's small gift, once per run (src/rogue_town.c).
Rogue_EventScript_FolkGift::
	delay 1
	special Rogue_BufferFolkGift
	msgbox Rogue_Text_Str1, MSGBOX_DEFAULT
	giveitem VAR_0x8000, VAR_0x8001
	goto_if_eq VAR_RESULT, FALSE, Rogue_EventScript_HouseGiftBagFull
	special Rogue_MarkFolkGiftGiven
	special Rogue_BufferFolkGiftAfter
	msgbox Rogue_Text_Str1, MSGBOX_DEFAULT
	release
	end

@ The wanderer, met on the road (src/rogue.c, UpdateCastObjects): a talk
@ that hints, a gift at the second meeting.
Rogue_EventScript_Wanderer::
	lock
	faceplayer
	special Rogue_BufferWanderer
	msgbox gStringVar4, MSGBOX_DEFAULT
	goto_if_eq VAR_0x8000, ITEM_NONE, Rogue_EventScript_WandererDone
	giveitem VAR_0x8000
	special Rogue_BufferWandererBye
	msgbox gStringVar4, MSGBOX_DEFAULT
Rogue_EventScript_WandererDone::
	release
	end

Rogue_Text_FolkGiftAfter:
	.string "Good luck out there!$"

Rogue_EventScript_TownMon::
	lock
	faceplayer
	special Rogue_BufferTownMon
	playmoncry VAR_0x8004, CRY_MODE_NORMAL
	msgbox Rogue_Text_Str1, MSGBOX_DEFAULT
	waitmoncry
	release
	end

@ ---------------------------------------------------------------------------
@ Gyms

Rogue_Gym_OnTransition:
	special Rogue_SetGymGraphics
	end

Rogue_EventScript_GymGuide::
	lock
	faceplayer
	special Rogue_BufferGymGuide
	msgbox gStringVar4, MSGBOX_DEFAULT
	special Rogue_BufferGymScouting
	goto_if_eq VAR_RESULT, FALSE, Rogue_EventScript_GymGuideDone
	msgbox gStringVar4, MSGBOX_DEFAULT
Rogue_EventScript_GymGuideDone::
	release
	end

	.macro rogue_gym_trainer gym:req, n:req, trainer:req
	.global Rogue_EventScript_GymTrainer\gym\()_\n
Rogue_EventScript_GymTrainer\gym\()_\n:
	trainerbattle_single \trainer, Rogue_Text_TrainerLine, Rogue_Text_TrainerLine
	@ Their own after line, from their set (src/rogue_text_trainers.h).
	setvar VAR_0x8004, ROGUE_SLOT_GYM_0 + (\gym - 1) * 3 + \n - 1
	special Rogue_BufferTrainerAfter
	msgbox Rogue_Text_Str1, MSGBOX_AUTOCLOSE
	end
	.endm

	@ The badge flags record which gyms are won; VAR_ROGUE_PROGRESS counts
	@ them (Rogue_UpdateProgress).
	.macro rogue_leader gym:req, trainer:req, badge:req
	.global Rogue_EventScript_Leader\gym\()
Rogue_EventScript_Leader\gym\():
	special Rogue_BufferGymInfo
	trainerbattle_single \trainer, Rogue_Text_TrainerLine, Rogue_Text_TrainerLine, Rogue_EventScript_LeaderDefeated\gym\(), NO_MUSIC
	@ Talked to again: their other after line (the first closed the badge).
	setvar VAR_0x8004, TRUE
	special Rogue_BufferLeaderAfter
	msgbox Rogue_Text_Str1, MSGBOX_DEFAULT
	release
	end

	.global Rogue_EventScript_LeaderDefeated\gym\()
Rogue_EventScript_LeaderDefeated\gym\():
	special Rogue_BufferGymInfo
	special Rogue_BufferBadge
	message Rogue_Text_ReceivedBadge
	waitmessage
	call Common_EventScript_PlayGymBadgeFanfare
	setflag \badge
	special Rogue_UpdateProgress
	special Rogue_RecordBadge
	call Rogue_EventScript_ShowUnlocks
	special Rogue_BufferBadgeInfo
	msgbox gStringVar4, MSGBOX_DEFAULT
	setvar VAR_0x8004, FALSE
	special Rogue_BufferLeaderTM
	msgbox gStringVar4, MSGBOX_DEFAULT
	special Rogue_PrepareLeaderTM
	giveitem VAR_0x8000
	call Rogue_EventScript_LeaderKey
	special Rogue_CheckTrainerCandy
	call_if_eq VAR_RESULT, TRUE, Rogue_EventScript_TrainerCandy
	@ Then the leader's own words: their town, another leader ahead.
	setvar VAR_0x8004, FALSE
	special Rogue_BufferLeaderAfter
	msgbox Rogue_Text_Str1, MSGBOX_DEFAULT
	@ RUN 0: the Director patches the game.
	call Rogue_EventScript_Run0PatchNotes
	release
	end
	.endm

@ RUN 0: after a badge the Director patches the game (src/rogue_run0_challenge.c).
@ Its box, written like our release notes, with a gift if the patch is one.
@ The story may dress it (the unlock jingle backwards); the text is
@ Rogue_Run0BufferPatch's.
Rogue_EventScript_Run0PatchNotes::
	@ First the Director's line on the ones lost to the leader (it keeps
	@ them), so a Pokemon given back below is never one just lost.
	special Rogue_Run0FallenNow
	call_if_eq VAR_RESULT, TRUE, Rogue_EventScript_Run0FallenNow
	call_if_set FLAG_ROGUE_RUN0_PATCH_NEW, Rogue_EventScript_Run0PatchBox
	@ Then one of the Pokemon it kept, given back (Rogue_Run0ReturnKept).
	special Rogue_Run0ReturnKept
	goto_if_eq VAR_RESULT, FALSE, Rogue_EventScript_Run0PatchEnd
	msgbox gStringVar4, MSGBOX_DEFAULT
Rogue_EventScript_Run0PatchEnd::
	return

Rogue_EventScript_Run0FallenNow:
	msgbox Rogue_Text_Fallen, MSGBOX_DEFAULT
	return

Rogue_EventScript_Run0PatchBox:
	special Rogue_Run0BufferPatch
	@ The story's dressing: the unlock jingle, slow and low (rogue_run0.inc).
	closemessage
	special Rogue_Run0PatchJingle
	waitfanfare
	msgbox gStringVar4, MSGBOX_DEFAULT
	goto_if_eq VAR_0x8000, ITEM_NONE, Rogue_EventScript_Run0PatchEnd
	giveitem VAR_0x8000, VAR_0x8001
	return

@ A leader who keeps a key to a legend's way hands it over too, as the
@ Eon Ticket came to Emerald's players.
Rogue_EventScript_LeaderKey::
	special Rogue_PrepareLeaderKey
	goto_if_eq VAR_RESULT, FALSE, Rogue_EventScript_LeaderKeyEnd
	msgbox Rogue_Text_LeaderKey, MSGBOX_DEFAULT
	giveitem VAR_0x8000
Rogue_EventScript_LeaderKeyEnd::
	return

Rogue_Text_LeaderKey:
	.string "Oh, and one more thing.\p"
	.string "A traveler left this {STR_VAR_2}\n"
	.string "with me. You should have it.\p"
	.string "They say it opens the way to\n"
	.string "{STR_VAR_1}.\p"
	.string "A legendary POKéMON rests there...{STR_VAR_3}$"

@ TRAINER'S CANDY: the 4th to 6th leaders add Rare Candies to the badge.
Rogue_EventScript_TrainerCandy::
	msgbox Rogue_Text_TrainerCandy, MSGBOX_DEFAULT
	giveitem ITEM_RARE_CANDY, 3
	return

Rogue_Text_TrainerCandy:
	.string "The road gets harder from here.\n"
	.string "Your whole team must keep up.$"

Rogue_Text_ReceivedBadge:
	.string "{PLAYER} received the {STR_VAR_3}\n"
	.string "from {STR_VAR_2}!$"


@ ---------------------------------------------------------------------------
@ The Champion and the Hall of Fame

Rogue_ChampionRoom_OnWarp:
	map_script_2 VAR_TEMP_1, 0, Rogue_ChampionRoom_EventScript_TurnNorth
	.2byte 0

Rogue_ChampionRoom_EventScript_TurnNorth::
	turnobject LOCALID_PLAYER, DIR_NORTH
	end

Rogue_ChampionRoom_OnFrame:
	map_script_2 VAR_TEMP_1, 0, Rogue_ChampionRoom_EventScript_Enter
	.2byte 0

Rogue_ChampionRoom_EventScript_Enter::
	lockall
	goto_if_eq VAR_ROGUE_RUN_KIND, ROGUE_RUN_KIND_RUN0, Rogue_EventScript_Run0ChampionRoom
	applymovement LOCALID_PLAYER, Common_Movement_WalkUp4
	waitmovement 0
	delay 40
	applymovement LOCALID_PLAYER, Rogue_Movement_WalkUp2
	waitmovement 0
	setvar VAR_TEMP_1, 1
	playbgm MUS_ENCOUNTER_CHAMPION, FALSE
	setvar VAR_0x8004, 0 @ RCHAMP_INTRO
	special Rogue_BufferChampion
	msgbox gStringVar4, MSGBOX_DEFAULT
	trainerbattle_no_intro ROGUE_TRAINER(ROGUE_SLOT_CHAMPION), Rogue_Text_ChampionDefeat
	playse SE_DOOR
	setmetatile 6, 1, METATILE_EliteFour_OpenDoorChampion_Frame, FALSE
	setmetatile 6, 2, METATILE_EliteFour_OpenDoorChampion_Opening, FALSE
	special DrawWholeMapView
	setvar VAR_0x8004, 2 @ RCHAMP_AFTER
	special Rogue_BufferChampion
	msgbox gStringVar4, MSGBOX_DEFAULT
	closemessage
	applymovement LOCALID_ROGUE_CHAMPION, Rogue_Movement_ChampionExitStart
	applymovement LOCALID_PLAYER, Rogue_Movement_WalkUp2
	waitmovement 0
	delay 20
	applymovement LOCALID_ROGUE_CHAMPION, Rogue_Movement_ChampionExit
	applymovement LOCALID_PLAYER, Rogue_Movement_PlayerExit
	waitmovement 0
	warp MAP_ROGUE_HALL_OF_FAME, 7, 16
	waitstate
	releaseall
	end

Rogue_Movement_WalkUp2:
	walk_up
	walk_up
	step_end

Rogue_Movement_ChampionExitStart:
	walk_up
	walk_up
	walk_in_place_faster_down
	step_end

Rogue_Movement_ChampionExit:
	walk_up
	delay_8
	set_invisible
	step_end

Rogue_Movement_PlayerExit:
	walk_up
	delay_16
	walk_up
	delay_8
	set_invisible
	step_end

@ Stand-in: the CHAMPION's words are the wanderer's (Rogue_BufferChampion,
@ and the defeat line from Rogue_GetTrainerText).
Rogue_Text_ChampionDefeat:
	.string "...!$"

Rogue_ChampionRoom_OnTransition:
	@ RUN 0: nobody stands in the CHAMPION's room.
	call_if_eq VAR_ROGUE_RUN_KIND, ROGUE_RUN_KIND_RUN0, Rogue_EventScript_Run0EmptyRoom
Rogue_HallOfFame_OnTransition:
	special Rogue_SetChampionGraphics
	end

Rogue_EventScript_Run0EmptyRoom:
	setflag FLAG_ROGUE_RUN0_HIDE_CHAMPION
	return

Rogue_HallOfFame_OnWarp:
	map_script_2 VAR_TEMP_1, 0, Rogue_HallOfFame_EventScript_TurnNorth
	.2byte 0

Rogue_HallOfFame_EventScript_TurnNorth::
	turnobject LOCALID_PLAYER, DIR_NORTH
	end

Rogue_HallOfFame_OnFrame:
	map_script_2 VAR_TEMP_1, 0, Rogue_HallOfFame_EventScript_Enter
	.2byte 0

Rogue_HallOfFame_EventScript_Enter::
	lockall
	applymovement LOCALID_ROGUE_HOF_CHAMPION, Rogue_Movement_WalkIntoHall1
	applymovement LOCALID_PLAYER, Rogue_Movement_WalkIntoHall1
	waitmovement 0
	applymovement LOCALID_ROGUE_HOF_CHAMPION, Common_Movement_WalkInPlaceFasterRight
	applymovement LOCALID_PLAYER, Common_Movement_WalkInPlaceFasterLeft
	waitmovement 0
	setvar VAR_0x8004, 3 @ RCHAMP_HOF
	special Rogue_BufferChampion
	msgbox gStringVar4, MSGBOX_DEFAULT
	closemessage
	applymovement LOCALID_ROGUE_HOF_CHAMPION, Rogue_Movement_WalkIntoHall2
	applymovement LOCALID_PLAYER, Rogue_Movement_WalkIntoHall2
	waitmovement 0
	delay 20
	applymovement LOCALID_ROGUE_HOF_CHAMPION, Common_Movement_WalkInPlaceFasterUp
	applymovement LOCALID_PLAYER, Common_Movement_WalkInPlaceFasterUp
	waitmovement 0
	delay 20
	dofieldeffect FLDEFF_HALL_OF_FAME_RECORD
	waitfieldeffect FLDEFF_HALL_OF_FAME_RECORD
	delay 40
	setvar VAR_TEMP_1, 1
	fadescreenspeed FADE_TO_BLACK, 24
	special Rogue_GameClear
	waitstate
	releaseall
	end

Rogue_Movement_WalkIntoHall1:
	walk_up
	walk_up
	walk_up
	walk_up
	walk_up
	walk_up
	step_end

Rogue_Movement_WalkIntoHall2:
	walk_up
	walk_up
	walk_up
	walk_up
	walk_up
	step_end

@ ---------------------------------------------------------------------------
@ Obstacles a badge clears. The badge alone is enough (see
@ Rogue_HasFieldMoveBadge): the lead Pokemon does the work.

Rogue_EventScript_CutTree::
	goto EventScript_CutTree

Rogue_EventScript_RockSmash::
	goto EventScript_RockSmash

Rogue_EventScript_Boulder::
	goto EventScript_StrengthBoulder

@ ---------------------------------------------------------------------------
@ The Elite Four. Four rooms in a row; each member's door opens once they
@ are beaten, and the last leads to the Champion.

Rogue_E4_OnWarp:
	map_script_2 VAR_TEMP_1, 0, Rogue_E4_EventScript_PlayerTurnNorth
	.2byte 0

Rogue_E4_EventScript_PlayerTurnNorth::
	turnobject LOCALID_PLAYER, DIR_NORTH
	end

Rogue_E4_OnFrame:
	map_script_2 VAR_TEMP_1, 0, Rogue_E4_EventScript_WalkIn
	.2byte 0

Rogue_E4_EventScript_WalkIn::
	lockall
	call PokemonLeague_EliteFour_EventScript_WalkInCloseDoor
	setvar VAR_TEMP_1, 1
	releaseall
	end

	.macro rogue_e4 n:req
Rogue_E4_\n\()_OnTransition:
	setvar VAR_0x8004, \n - 1
	special Rogue_SetE4Graphics
	end

Rogue_E4_\n\()_OnLoad:
	call_if_set TRAINER_FLAGS_START + ROGUE_TRAINER(ROGUE_SLOT_E4_0 + \n - 1), PokemonLeague_EliteFour_EventScript_ResetAdvanceToNextRoom
	end

	.global Rogue_EventScript_E4_\n
Rogue_EventScript_E4_\n\():
	lock
	faceplayer
	setvar VAR_0x8004, \n - 1
	goto_if_set TRAINER_FLAGS_START + ROGUE_TRAINER(ROGUE_SLOT_E4_0 + \n - 1), Rogue_EventScript_E4After
	playbgm MUS_ENCOUNTER_ELITE_FOUR, FALSE
	special Rogue_BufferE4
	msgbox gStringVar4, MSGBOX_DEFAULT
	trainerbattle_no_intro ROGUE_TRAINER(ROGUE_SLOT_E4_0 + \n - 1), Rogue_Text_TrainerLine
	setvar VAR_ROGUE_E4, \n
	call PokemonLeague_EliteFour_SetAdvanceToNextRoomMetatiles
	setvar VAR_0x8004, \n - 1
	goto Rogue_EventScript_E4After
	end
	.endm

	rogue_e4 1
	rogue_e4 2
	rogue_e4 3
	rogue_e4 4

Rogue_EventScript_E4After::
	special Rogue_BufferE4After
	msgbox gStringVar4, MSGBOX_DEFAULT
	release
	end

	.include "data/scripts/rogue_trainers.inc"

@ ---------------------------------------------------------------------------
@ The cycle shop gives the player a BIKE, once.

Rogue_EventScript_BikeShop::
	lock
	faceplayer
	goto_if_set FLAG_RECEIVED_BIKE, Rogue_EventScript_BikeShopAfter
	msgbox Rogue_Text_BikeShopGive, MSGBOX_DEFAULT
	message Rogue_Text_BikeShopWhich
	waitmessage
	multichoice 21, 8, MULTI_BIKE, TRUE
	switch VAR_RESULT
	case 0, Rogue_EventScript_BikeShopMach
	case 1, Rogue_EventScript_BikeShopAcro
	end

Rogue_EventScript_BikeShopMach::
	giveitem ITEM_MACH_BIKE
	goto Rogue_EventScript_BikeShopGot

Rogue_EventScript_BikeShopAcro::
	giveitem ITEM_ACRO_BIKE
	goto Rogue_EventScript_BikeShopGot

Rogue_EventScript_BikeShopGot::
	setflag FLAG_RECEIVED_BIKE
	msgbox Rogue_Text_BikeShopEnjoy, MSGBOX_DEFAULT
	release
	end

Rogue_EventScript_BikeShopAfter::
	msgbox Rogue_Text_BikeShopAfter, MSGBOX_DEFAULT
	release
	end

Rogue_Text_BikeShopGive:
	.string "Welcome to the cycle shop!\p"
	.string "Your shoes are worn right through.\n"
	.string "You walked all this way?\p"
	.string "A region this big needs a BIKE.\n"
	.string "Here, take one. On the house!$"

Rogue_Text_BikeShopWhich:
	.string "The MACH BIKE is fast. The ACRO BIKE\n"
	.string "does tricks. Which one?$"

Rogue_Text_BikeShopEnjoy:
	.string "Press SELECT to ride it once you\n"
	.string "register it in the BAG. Enjoy!$"

Rogue_Text_BikeShopAfter:
	.string "How's the BIKE treating you?\n"
	.string "Keep it oiled!$"

@ ---------------------------------------------------------------------------
@ FOSSILS: a scientist in the fourth gym town brings the region's fossil back
@ to life, once per run.

Rogue_EventScript_FossilScientist::
	lock
	faceplayer
	special Rogue_PrepareFossil
	switch VAR_RESULT
	case 2, Rogue_EventScript_FossilRevive
	case 3, Rogue_EventScript_FossilDone
	msgbox Rogue_Text_FossilLooking, MSGBOX_DEFAULT
	release
	end

Rogue_EventScript_FossilRevive::
	msgbox Rogue_Text_FossilOffer, MSGBOX_YESNO
	goto_if_eq VAR_RESULT, NO, Rogue_EventScript_FossilDeclined
	givemon VAR_0x8001, VAR_0x8002
	goto_if_eq VAR_RESULT, MON_CANT_GIVE, Rogue_EventScript_FossilNoRoom
	special Rogue_FossilRevived
	playfanfare MUS_OBTAIN_ITEM
	message Rogue_Text_FossilRevived
	waitmessage
	waitfanfare
	call_if_eq VAR_RESULT, MON_GIVEN_TO_PC, Rogue_EventScript_FossilToPC
	msgbox Rogue_Text_FossilAfter, MSGBOX_DEFAULT
	release
	end

Rogue_EventScript_FossilToPC::
	msgbox Rogue_Text_FossilToPC, MSGBOX_DEFAULT
	return

Rogue_EventScript_FossilDeclined::
	msgbox Rogue_Text_FossilDeclined, MSGBOX_DEFAULT
	release
	end

Rogue_EventScript_FossilNoRoom::
	msgbox Rogue_Text_FossilNoRoom, MSGBOX_DEFAULT
	release
	end

Rogue_EventScript_FossilDone::
	msgbox Rogue_Text_FossilAfter, MSGBOX_DEFAULT
	release
	end

Rogue_Text_FossilLooking:
	.string "I study POKéMON from long ago.\p"
	.string "They say a fossil lies in the cave\n"
	.string "on the road here. If you find one,\l"
	.string "bring it to me!$"

Rogue_Text_FossilOffer:
	.string "Is that a {STR_VAR_1}?\n"
	.string "May I bring it back to life?$"

Rogue_Text_FossilRevived:
	.string "The {STR_VAR_1} came back to\n"
	.string "life as {STR_VAR_2}!$"

Rogue_Text_FossilToPC:
	.string "Your party is full, so I sent it\n"
	.string "to your PC.$"

Rogue_Text_FossilDeclined:
	.string "Oh... If you change your mind,\n"
	.string "I'll be right here.$"

Rogue_Text_FossilNoRoom:
	.string "You have no room for it! Come back\n"
	.string "once you've made some space.$"

Rogue_Text_FossilAfter:
	.string "A POKéMON from long ago, alive\n"
	.string "again! Take good care of it.$"

@ ---------------------------------------------------------------------------
@ A rest house on a route: the team can rest and heal.

Rogue_EventScript_RestSign::
	special Rogue_BufferRestSign
	msgbox Rogue_Text_Str1, MSGBOX_SIGN
	end

@ The keeper's words are each route's own (src/rogue_town.c), the offer by
@ whether the team is hurt.
Rogue_EventScript_RestHouse::
	lock
	faceplayer
	special Rogue_BufferRestHouse
	msgbox Rogue_Text_Str1, MSGBOX_YESNO
	goto_if_eq VAR_RESULT, NO, Rogue_EventScript_RestDeclined
	closemessage
	call Common_EventScript_OutOfCenterPartyHeal
	msgbox Rogue_Text_Str2, MSGBOX_DEFAULT
	release
	end

Rogue_EventScript_RestDeclined::
	msgbox Rogue_Text_Str3, MSGBOX_DEFAULT
	release
	end

Rogue_EventScript_RestTraveler::
	lock
	faceplayer
	special Rogue_BufferRestTraveler
	msgbox Rogue_Text_Str1, MSGBOX_DEFAULT
	release
	end





@ ---------------------------------------------------------------------------
@ Town signatures (src/rogue_town.c, src/rogue_gen_signatures.h): the town's
@ people, their gift Pokemon (which uses no catch), the hot spring's soak,
@ the ranch's milk, the market's stall, the landmark's sign, a museum's
@ exhibits and a dojo's disciples and master.

Rogue_EventScript_SigFolk::
	lock
	faceplayer
Rogue_EventScript_SigFolkTalk::
	special Rogue_PrepareSigFolk
	goto_if_eq VAR_RESULT, 1, Rogue_EventScript_SigGiftMon
	goto_if_eq VAR_RESULT, 2, Rogue_EventScript_SigGiftItem
	goto_if_eq VAR_RESULT, 3, Rogue_EventScript_SigStall
	goto_if_eq VAR_RESULT, 4, Rogue_EventScript_SigSoak
	msgbox Rogue_Text_Str1, MSGBOX_DEFAULT
	release
	end

Rogue_EventScript_SigGiftMon::
	msgbox Rogue_Text_Str1, MSGBOX_YESNO
	goto_if_eq VAR_RESULT, NO, Rogue_EventScript_SigGiftDeclined
	givemon VAR_0x8000, VAR_0x8001
	goto_if_eq VAR_RESULT, MON_CANT_GIVE, Rogue_EventScript_SigGiftNoRoom
	copyvar VAR_0x8002, VAR_RESULT
	special Rogue_MarkSigMonGiven
	playfanfare MUS_OBTAIN_ITEM
	message Rogue_Text_SigMonReceived
	waitmessage
	waitfanfare
	call_if_eq VAR_0x8002, MON_GIVEN_TO_PC, Rogue_EventScript_SigToPC
	special Rogue_BufferSigGiftAfter
	msgbox Rogue_Text_Str1, MSGBOX_DEFAULT
	release
	end

Rogue_EventScript_SigToPC::
	msgbox Rogue_Text_SigToPC, MSGBOX_DEFAULT
	return

Rogue_EventScript_SigGiftDeclined::
	msgbox Rogue_Text_SigGiftDeclined, MSGBOX_DEFAULT
	release
	end

Rogue_EventScript_SigGiftNoRoom::
	msgbox Rogue_Text_SigGiftNoRoom, MSGBOX_DEFAULT
	release
	end

Rogue_EventScript_SigGiftItem::
	msgbox Rogue_Text_Str1, MSGBOX_DEFAULT
	giveitem VAR_0x8000, VAR_0x8001
	goto_if_eq VAR_RESULT, FALSE, Rogue_EventScript_HouseGiftBagFull
	special Rogue_MarkSigItemGiven
	release
	end

Rogue_EventScript_SigStall::
	message Rogue_Text_Str1
	waitmessage
	special Rogue_PrepareMarketStall
	special Rogue_OpenSpecialtyShop
	waitstate
	msgbox gText_PleaseComeAgain, MSGBOX_DEFAULT
	release
	end

Rogue_EventScript_SigSoak::
	msgbox Rogue_Text_Str1, MSGBOX_DEFAULT
	closemessage
	fadescreen FADE_TO_BLACK
	special Rogue_HotSpringSoak
	copyvar VAR_0x8002, VAR_RESULT
	playfanfare MUS_HEAL
	waitfanfare
	fadescreen FADE_FROM_BLACK
	msgbox Rogue_Text_SoakDone, MSGBOX_DEFAULT
	goto_if_eq VAR_0x8002, FALSE, Rogue_EventScript_SigSoakEnd
	msgbox Rogue_Text_Str1, MSGBOX_DEFAULT
	giveitem VAR_0x8000, VAR_0x8001
	goto_if_eq VAR_RESULT, FALSE, Rogue_EventScript_HouseGiftBagFull
	special Rogue_MarkSigItemGiven
Rogue_EventScript_SigSoakEnd::
	release
	end

Rogue_EventScript_LandmarkSign::
	special Rogue_BufferLandmarkSign
	msgbox Rogue_Text_Str1, MSGBOX_SIGN
	end

Rogue_EventScript_Exhibit0::
	setvar VAR_0x8004, 0
	special Rogue_BufferExhibit
	msgbox Rogue_Text_Str1, MSGBOX_SIGN
	end

Rogue_EventScript_Exhibit1::
	setvar VAR_0x8004, 1
	special Rogue_BufferExhibit
	msgbox Rogue_Text_Str1, MSGBOX_SIGN
	end

Rogue_EventScript_Dojo0::
	trainerbattle_single ROGUE_TRAINER(ROGUE_SLOT_DOJO_0), Rogue_Text_TrainerLine, Rogue_Text_TrainerLine
	setvar VAR_0x8004, ROGUE_SLOT_DOJO_0
	special Rogue_BufferTrainerAfter
	msgbox Rogue_Text_Str1, MSGBOX_AUTOCLOSE
	end

Rogue_EventScript_Dojo1::
	trainerbattle_single ROGUE_TRAINER(ROGUE_SLOT_DOJO_0 + 1), Rogue_Text_TrainerLine, Rogue_Text_TrainerLine
	setvar VAR_0x8004, ROGUE_SLOT_DOJO_0 + 1
	special Rogue_BufferTrainerAfter
	msgbox Rogue_Text_Str1, MSGBOX_AUTOCLOSE
	end

@ The master fights once; after that (and straight after the battle) the
@ master gives the dojo's gift Pokemon, if the region has one there.
Rogue_EventScript_DojoMaster::
	trainerbattle_single ROGUE_TRAINER(ROGUE_SLOT_DOJO_0 + 2), Rogue_Text_TrainerLine, Rogue_Text_TrainerLine, Rogue_EventScript_SigFolkTalk
	lock
	faceplayer
	goto Rogue_EventScript_SigFolkTalk

@ ---------------------------------------------------------------------------
@ Route features (src/rogue_town.c, src/rogue_gen_features.h): the people of
@ the Day Care, the BERRY MASTER's house, the glass workshop, the Seashore
@ House, the battling family, the gauntlet, the fishing hut and the old
@ sailor's cottage; the sleeping SNORLAX and the child with the POKe FLUTE;
@ the double battle pairs. Gifts use no catch; SNORLAX is a one-off battle,
@ as a legend's.

Rogue_EventScript_FeatFolk::
	lock
	faceplayer
Rogue_EventScript_FeatFolkTalk::
	special Rogue_PrepareFeatFolk
	goto_if_eq VAR_RESULT, 1, Rogue_EventScript_FeatGiftMon
	goto_if_eq VAR_RESULT, 2, Rogue_EventScript_FeatGiftItem
	goto_if_eq VAR_RESULT, 3, Rogue_EventScript_FeatFlute
	msgbox Rogue_Text_Str1, MSGBOX_DEFAULT
	release
	end

Rogue_EventScript_FeatGiftMon::
	msgbox Rogue_Text_Str1, MSGBOX_YESNO
	goto_if_eq VAR_RESULT, NO, Rogue_EventScript_SigGiftDeclined
	givemon VAR_0x8000, VAR_0x8001
	goto_if_eq VAR_RESULT, MON_CANT_GIVE, Rogue_EventScript_SigGiftNoRoom
	copyvar VAR_0x8002, VAR_RESULT
	special Rogue_MarkFeatDone
	playfanfare MUS_OBTAIN_ITEM
	message Rogue_Text_SigMonReceived
	waitmessage
	waitfanfare
	call_if_eq VAR_0x8002, MON_GIVEN_TO_PC, Rogue_EventScript_SigToPC
	special Rogue_BufferFeatAfter
	msgbox Rogue_Text_Str1, MSGBOX_DEFAULT
	release
	end

Rogue_EventScript_FeatGiftItem::
	msgbox Rogue_Text_Str1, MSGBOX_DEFAULT
	giveitem VAR_0x8000, VAR_0x8001
	goto_if_eq VAR_RESULT, FALSE, Rogue_EventScript_HouseGiftBagFull
	special Rogue_MarkFeatDone
	release
	end

@ The child plays the POKe FLUTE for the player, SNORLAX wakes and
@ battles; then it is gone for the run, caught or not.
Rogue_EventScript_FeatFlute::
	msgbox Rogue_Text_Str1, MSGBOX_YESNO
	goto_if_eq VAR_RESULT, NO, Rogue_EventScript_FeatFluteNo
	closemessage
	playfanfare MUS_RG_POKE_FLUTE
	message Rogue_Text_PlayedFlute
	waitmessage
	waitfanfare
	@ (No waitse: a route's rain plays as a sound effect that never ends.)
	playmoncry VAR_0x8004, CRY_MODE_ENCOUNTER
	msgbox Rogue_Text_SnorlaxWoke, MSGBOX_DEFAULT
	waitmoncry
	closemessage
	special Rogue_CreateSnorlax
	setflag FLAG_SYS_CTRL_OBJ_DELETE
	dowildbattle
	clearflag FLAG_SYS_CTRL_OBJ_DELETE
	special Rogue_MarkFeatDone
	removeobject VAR_0x8006
	special Rogue_BufferFeatAfter
	msgbox Rogue_Text_Str1, MSGBOX_DEFAULT
	release
	end

Rogue_EventScript_FeatFluteNo::
	msgbox Rogue_Text_FluteNo, MSGBOX_DEFAULT
	release
	end

Rogue_EventScript_Snorlax::
	lock
	special Rogue_BufferSnorlax
	msgbox Rogue_Text_Str1, MSGBOX_DEFAULT
	release
	end

Rogue_EventScript_FeatureSign::
	special Rogue_BufferFeatureSign
	msgbox Rogue_Text_Str1, MSGBOX_SIGN
	end

@ The family's grandparent battles last, then gives the reward once all
@ four are beaten.
Rogue_EventScript_FamilyHead::
	trainerbattle_single ROGUE_TRAINER(ROGUE_SLOT_FAMILY_0 + 3), Rogue_Text_TrainerLine, Rogue_Text_TrainerLine, Rogue_EventScript_FeatFolkTalk
	lock
	faceplayer
	goto Rogue_EventScript_FeatFolkTalk

@ A double battle pair: both of the two say this. A team of one can't
@ take them on.
	.macro rogue_pair k:req
	.global Rogue_EventScript_Pair\k\()
Rogue_EventScript_Pair\k\():
	trainerbattle_double ROGUE_TRAINER(ROGUE_SLOT_PAIR_0 + \k), Rogue_Text_TrainerLine, Rogue_Text_TrainerLine, Rogue_Text_PairNeedTwo
	setvar VAR_0x8004, ROGUE_SLOT_PAIR_0 + \k
	special Rogue_BufferTrainerAfter
	msgbox Rogue_Text_Str1, MSGBOX_AUTOCLOSE
	end
	.endm

	rogue_pair 0
	rogue_pair 1
	rogue_pair 2

	.align 2
gRoguePairScripts::
	.4byte Rogue_EventScript_Pair0
	.4byte Rogue_EventScript_Pair1
	.4byte Rogue_EventScript_Pair2

Rogue_Text_PairNeedTwo:
	.string "We battle as a pair! Come back\n"
	.string "with two POKéMON who can fight.$"

Rogue_Text_PlayedFlute:
	.string "{PLAYER} played the POKé FLUTE.$"

Rogue_Text_SnorlaxWoke:
	.string "The sleeping POKéMON woke up!\n"
	.string "It's in a bad mood!$"

Rogue_Text_FluteNo:
	.string "Oh... It might sleep all year.$"

Rogue_Text_SigMonReceived:
	.string "{PLAYER} received {STR_VAR_1}!$"

Rogue_Text_SigToPC:
	.string "Your party is full, so {STR_VAR_1}\n"
	.string "was sent to the PC.$"

Rogue_Text_SigGiftDeclined:
	.string "Oh... Come back if you change\n"
	.string "your mind.$"

Rogue_Text_SigGiftNoRoom:
	.string "Your party and your PC are full.\n"
	.string "Make some room and come back!$"

Rogue_Text_SoakDone:
	.string "Your POKéMON look refreshed!\n"
	.string "Their troubles melted away.$"


@ RUN 0's story hooks (the gift box, CHAMPION TERRY, the PROMPT LOGs): in
@ data/scripts/rogue_run0.inc.
