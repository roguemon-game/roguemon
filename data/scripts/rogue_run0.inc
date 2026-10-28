@ ---------------------------------------------------------------------------
@ RUN 0's story (Roguémon v1.0): the scenes. The code is src/rogue_run0_story.c,
@ every word is in src/rogue_text_director.h (the DIR_* numbers in
@ include/constants/rogue_run0_story.h). The Director never has a sprite: it
@ speaks in a plain message box.

@ A line by number (VAR_0x8004), shown.
Rogue_EventScript_Run0Line::
	special Rogue_Run0Say
	msgbox gStringVar4, MSGBOX_DEFAULT
	return

@ The lab. The run starts in front of Birch, who faces the wall.
Rogue_EventScript_Run0Arrival::
	applymovement LOCALID_PLAYER, Common_Movement_WalkInPlaceFasterUp
	waitmovement 0
	special Rogue_Run0Arrived
	@ TERRY's team, in the party or the PC: the Director says whose.
	special Rogue_Run0TestTeamLine
	call_if_eq VAR_RESULT, TRUE, Rogue_EventScript_Run0SayVar4
	goto_if_ge VAR_ROGUE_RUN0_ACT, 2, Rogue_EventScript_Run0Checkpoint
	releaseall
	end

Rogue_EventScript_Run0SayVar4:
	msgbox gStringVar4, MSGBOX_DEFAULT
	closemessage
	return

@ An attempt started at a later act: back where the last one got to.
Rogue_EventScript_Run0Checkpoint::
	special Rogue_Run0BirchTalk
	msgbox gStringVar4, MSGBOX_DEFAULT
	closemessage
	special Rogue_Run0PlaceCheckpoint
	waitstate
	releaseall
	end

@ Birch, talked to: his first welcome word for word, and how often he gave it.
Rogue_EventScript_Run0Birch::
	lock
	special Rogue_Run0BirchTalk
	msgbox gStringVar4, MSGBOX_DEFAULT
	goto_if_eq VAR_RESULT, TRUE, Rogue_EventScript_Run0BirchWarp
	release
	end

Rogue_EventScript_Run0BirchWarp:
	closemessage
	special Rogue_Run0PlaceCheckpoint
	waitstate
	release
	end

@ A scene that waited for the field (Rogue_Run0StoryFrame): VAR_0x8004.
Rogue_EventScript_Run0Scene::
	lockall
	special Rogue_Run0SceneText
	goto_if_eq VAR_RESULT, FALSE, Rogue_EventScript_Run0SceneEnd
	msgbox gStringVar4, MSGBOX_DEFAULT
	closemessage
Rogue_EventScript_Run0SceneEnd:
	releaseall
	end

@ ---------------------------------------------------------------------------
@ The badge's PATCH box is the challenge's (Rogue_EventScript_Run0PatchNotes
@ in data/scripts/rogue.inc): the story plays the unlock jingle slow and low
@ before it (special Rogue_Run0PatchJingle).

@ ---------------------------------------------------------------------------
@ The story's things on the map (the region's hooks): the gift box, CHAMPION
@ TERRY, and the PROMPT LOG signs.

Rogue_EventScript_Run0Thing::
	lock
	special Rogue_Run0ThingHook
	special Rogue_Run0HookTalk
	goto_if_eq VAR_RESULT, 2, Rogue_EventScript_Run0ChampionTerry
	copyvar VAR_0x8006, VAR_RESULT
	msgbox gStringVar4, MSGBOX_DEFAULT
	call_if_eq VAR_0x8006, 1, Rogue_EventScript_Run0GiftCandy
	release
	end

Rogue_EventScript_Run0GiftCandy:
	giveitem ITEM_RARE_CANDY
	return

Rogue_EventScript_Run0ChampionTerry:
	faceplayer
	playbgm MUS_ENCOUNTER_CHAMPION, FALSE
	setvar VAR_0x8004, DIR_CT_INTRO
	call Rogue_EventScript_Run0Line
	trainerbattle_no_intro ROGUE_TRAINER(ROGUE_SLOT_RUN0_TERRY), Rogue_Text_TrainerLine
	special Rogue_Run0ChampionTerryAfter
	msgbox gStringVar4, MSGBOX_DEFAULT
	release
	end

Rogue_EventScript_Run0Hook::
	special Rogue_Run0HookTalk
	msgbox gStringVar4, MSGBOX_SIGN
	end

Rogue_EventScript_Run0Prompt0::
	setvar VAR_0x8004, 7 @ RUN0_HOOK_PROMPT_0
	goto Rogue_EventScript_Run0Hook
Rogue_EventScript_Run0Prompt1::
	setvar VAR_0x8004, 8 @ RUN0_HOOK_PROMPT_1
	goto Rogue_EventScript_Run0Hook
Rogue_EventScript_Run0Prompt2::
	setvar VAR_0x8004, 9 @ RUN0_HOOK_PROMPT_2
	goto Rogue_EventScript_Run0Hook
Rogue_EventScript_Run0Prompt3::
	setvar VAR_0x8004, 10 @ RUN0_HOOK_PROMPT_3
	goto Rogue_EventScript_Run0Hook
Rogue_EventScript_Run0Prompt4::
	setvar VAR_0x8004, 11 @ RUN0_HOOK_PROMPT_4
	goto Rogue_EventScript_Run0Hook
Rogue_EventScript_Run0Prompt5::
	setvar VAR_0x8004, 12 @ RUN0_HOOK_PROMPT_5
	goto Rogue_EventScript_Run0Hook
Rogue_EventScript_Run0Prompt6::
	setvar VAR_0x8004, 13 @ RUN0_HOOK_PROMPT_6
	goto Rogue_EventScript_Run0Hook
Rogue_EventScript_Run0Prompt7::
	setvar VAR_0x8004, 14 @ RUN0_HOOK_PROMPT_7
	goto Rogue_EventScript_Run0Hook

@ A grave or a shelf (Rogue_Run0ThingScript): its words are ready.
Rogue_EventScript_Run0ReadThing::
	msgbox gStringVar4, MSGBOX_SIGN
	end

@ ---------------------------------------------------------------------------
@ The people of the story's homes

Rogue_EventScript_Run0Believer::
	lock
	faceplayer
	special Rogue_Run0BelieverTalk
	msgbox gStringVar4, MSGBOX_DEFAULT
	release
	end

Rogue_EventScript_Run0Terry::
	lock
	faceplayer
	goto_if_set RUN0_FLAG_TERRY_MET, Rogue_EventScript_Run0TerryAgain
	setvar VAR_0x8004, 0
	special Rogue_Run0TerryTalk
	msgbox gStringVar4, MSGBOX_DEFAULT
	setvar VAR_0x8004, 3
	special Rogue_Run0TerryTalk
	msgbox gStringVar4, MSGBOX_DEFAULT
	giveitem ITEM_MAX_REVIVE, 2
	setvar VAR_0x8004, 1
	special Rogue_Run0TerryTalk
	msgbox gStringVar4, MSGBOX_DEFAULT
	release
	end

Rogue_EventScript_Run0TerryAgain:
	setvar VAR_0x8004, 2
	special Rogue_Run0TerryTalk
	msgbox gStringVar4, MSGBOX_DEFAULT
	release
	end

@ ---------------------------------------------------------------------------
@ The fake crash: the game in memory goes on after it.

Rogue_EventScript_Run0Crash::
	lockall
	special Rogue_Run0FakeCrash
	waitstate
	delay 30
	setvar VAR_0x8004, DIR_CRASH_BACK
	call Rogue_EventScript_Run0Line
	releaseall
	end

@ The fake corrupted save, after a save that finished (src/start_menu.c).
@ The screen goes black and the game seems to boot again: the main menu
@ with Emerald's own message in its own box (src/main_menu.c), then back.
Rogue_EventScript_Run0FakeSave::
	lockall
	delay 20
	fadescreenspeed FADE_TO_BLACK, 0
	special Rogue_Run0FakeSaveBoot
	waitstate
	delay 60
	setvar VAR_0x8004, DIR_FAKE_SAVE_NO
	call Rogue_EventScript_Run0Line
	releaseall
	end

@ ---------------------------------------------------------------------------
@ TERRY's switch, at the LEAGUE's door (act 4, CHAMPION TERRY beaten).

Rogue_League_OnTransition::
	special Rogue_Run0LeagueTransition
	end

Rogue_EventScript_Run0Switch::
	lockall
	applymovement LOCALID_ROGUE_LEAGUE_TERRY, Common_Movement_FacePlayer
	waitmovement 0
	setvar VAR_0x8004, DIR_SWITCH_TERRY
	call Rogue_EventScript_Run0Line
	closemessage
	applymovement LOCALID_ROGUE_LEAGUE_TERRY, Common_Movement_WalkInPlaceFasterUp
	waitmovement 0
	delay 20
	playse SE_SWITCH
	fadescreenspeed FADE_TO_WHITE, 0
	fadeoutbgm 0
	delay 30
	fadescreenspeed FADE_FROM_WHITE, 0
	delay 40
	setvar VAR_0x8004, DIR_SWITCH_DIRECTOR
	call Rogue_EventScript_Run0Line
	applymovement LOCALID_ROGUE_LEAGUE_TERRY, Common_Movement_FacePlayer
	waitmovement 0
	setvar VAR_0x8004, DIR_SWITCH_CUT
	special Rogue_Run0Say
	message gStringVar4
	waitmessage
	@ (Held long enough to read the half word, then cut.)
	delay 50
	removeobject LOCALID_ROGUE_LEAGUE_TERRY
	closemessage
	special Rogue_Run0DeleteTerry
	delay 90
	setvar VAR_0x8004, DIR_SWITCH_AFTER
	call Rogue_EventScript_Run0Line
	@ fadeoutbgm only paused the music: a paused song never finishes the
	@ fade a warp waits for, so every door faded to black for good.
	@ fadeinbgm lets it play again (fadedefaultbgm did nothing: the song
	@ was already the map's).
	fadeinbgm 2
	releaseall
	end

@ ---------------------------------------------------------------------------
@ The CHAMPION's room, dark and empty: the Director.

Rogue_EventScript_Run0ChampionRoom::
	applymovement LOCALID_PLAYER, Common_Movement_WalkUp4
	waitmovement 0
	delay 60
	applymovement LOCALID_PLAYER, Rogue_Movement_WalkUp2
	waitmovement 0
	setvar VAR_TEMP_1, 1
	delay 60
	setvar VAR_0x8004, DIR_CONFESSION
	call Rogue_EventScript_Run0Line
	playbgm MUS_ENCOUNTER_CHAMPION, FALSE
	trainerbattle_no_intro ROGUE_TRAINER(ROGUE_SLOT_CHAMPION), Rogue_Text_TrainerLine
	special Rogue_Run0DirectorPatch
	msgbox gStringVar4, MSGBOX_DEFAULT
	trainerbattle_no_intro ROGUE_TRAINER(ROGUE_SLOT_CHAMPION), Rogue_Text_TrainerLine
Rogue_EventScript_Run0Question::
	setvar VAR_0x8004, DIR_QUESTION
	special Rogue_Run0Say
	message gStringVar4
	waitmessage
	special Rogue_Run0AskEnding
	waitstate
	switch VAR_RESULT
	case RUN0_ANSWER_MORE, Rogue_EventScript_Run0More
	case RUN0_ANSWER_CHOOSE, Rogue_EventScript_Run0Choose
	goto Rogue_EventScript_Run0Stop

@ The tests' way to the question (gRogueTestStory = RUN0_SCENE_ENDING).
Rogue_EventScript_Run0TestQuestion::
	lockall
	goto Rogue_EventScript_Run0Question

Rogue_EventScript_Run0Stop:
	setvar VAR_0x8004, DIR_STOP
	call Rogue_EventScript_Run0Line
	closemessage
	fadescreenspeed FADE_TO_BLACK, 24
	special Rogue_Run0StartStop
	waitstate
	releaseall
	end

Rogue_EventScript_Run0More:
	setvar VAR_0x8004, DIR_MORE
	call Rogue_EventScript_Run0Line
	closemessage
	fadescreenspeed FADE_TO_BLACK, 8
	special Rogue_Run0StartMore
	waitstate
	releaseall
	end

Rogue_EventScript_Run0Choose:
	setvar VAR_0x8004, DIR_CHOOSE_ASK
	call Rogue_EventScript_Run0Line
	special Rogue_Run0Choose
	waitstate
	special Rogue_Run0ChooseDone
	msgbox gStringVar4, MSGBOX_DEFAULT
	closemessage
	fadescreenspeed FADE_TO_BLACK, 24
	special Rogue_GameClear
	waitstate
	releaseall
	end

@ ---------------------------------------------------------------------------
@ STOP: TERRY by the truck in the real Littleroot, then the credits.

Rogue_EventScript_Run0TruckTerry::
	lock
	faceplayer
	special Rogue_Run0TruckTerry
	msgbox gStringVar4, MSGBOX_DEFAULT
	closemessage
	applymovement VAR_LAST_TALKED, Common_Movement_WalkInPlaceFasterDown
	waitmovement 0
	delay 30
	fadescreenspeed FADE_TO_BLACK, 16
	playse SE_TRUCK_DOOR
	waitse
	delay 60
	special Rogue_Run0StopCredits
	waitstate
	release
	end

@ ---------------------------------------------------------------------------
@ ENDLESS: no LEAGUE, another region.

Rogue_EventScript_Run0EndlessGate::
	lockall
	setvar VAR_0x8004, DIR_ENDLESS_GATE
	call Rogue_EventScript_Run0Line
	closemessage
	fadescreenspeed FADE_TO_BLACK, 16
	special Rogue_Run0NextEndless
	waitstate
	releaseall
	end

@ An ENDLESS region's lab: with the party carried in, nothing to give.
Rogue_EventScript_Run0EndlessArrival::
	applymovement LOCALID_PLAYER, Common_Movement_WalkInPlaceFasterUp
	waitmovement 0
	goto_if_unset FLAG_SYS_POKEMON_GET, Rogue_EventScript_ProfessorStart
	special Rogue_Run0Arrived
	releaseall
	end
