#ifndef GUARD_CONSTANTS_ROGUE_RUN0_STORY_H
#define GUARD_CONSTANTS_ROGUE_RUN0_STORY_H

// RUN 0's story (Roguémon v1.0): the Director's lines by number, so the
// scripts (data/scripts/rogue_run0.inc) can ask for one (VAR_0x8004, special
// Rogue_Run0Say). The words are in src/rogue_text_director.h.

#define DIR_BIRCH_FIRST        0
#define DIR_BIRCH_AGAIN        1
#define DIR_BIRCH_LATER        2
#define DIR_BIRCH_CHECKPOINT   3
#define DIR_SLIP_0             4  // 4 to 8: townsfolk who slip
#define DIR_SLIPS              5
#define DIR_LEAVE_ISLAND       9
#define DIR_BELIEVER_0         10 // 10 to 15: home h, its HEX MANIAC (2h) and kid (2h + 1)
#define DIR_GRAVE              16
#define DIR_GRAVE_BLANK        17
#define DIR_GRAVE_ALL          18
#define DIR_GRAVE_ROOM         19
#define DIR_DRAFT_BALL         20
#define DIR_DRAFT_AFTER        21
#define DIR_PROMPT_0           22 // 22 to 29: the PROMPT LOGs
#define DIR_PROMPT_DEL_0       30 // 30 to 37: the PROMPT LOGs once Terry is deleted
#define DIR_PROMPT_REPLY_2     38
#define DIR_PROMPT_REPLY_4     39
#define DIR_PROMPT_REPLY_7     40
#define DIR_LIB_ENTER          41
#define DIR_SHELF_WON          42
#define DIR_SHELF_LOST         43
#define DIR_SHELF_LOST0        44
#define DIR_SHELF_LEFT         45
#define DIR_SHELF_RUN0         46
#define DIR_SHELF_UNPLAYED     47
#define DIR_SHELF_REJECTED     48
#define DIR_CASE_0             49 // 49 to 53: the Director's case in the library
#define DIR_CASES              5
#define DIR_LIB_OUT            54
#define DIR_TERRY_HELLO        55
#define DIR_TERRY_HELLO_AGAIN  56
#define DIR_TERRY_TEAM_HERE    57
#define DIR_TERRY_TEAM_KEPT    58
#define DIR_TERRY_TEAM_PC      59
#define DIR_TERRY_PLAY         60
#define DIR_TERRY_THREE        61
#define DIR_TERRY_GIFT         62
#define DIR_TERRY_SWITCH       63
#define DIR_TERRY_CAPE         64
#define DIR_TERRY_LATER        65
#define DIR_CRASH_BACK         66
#define DIR_FAKE_SAVE_CORRUPT  67
#define DIR_FAKE_SAVE_NO       68
#define DIR_CLOCK              69
#define DIR_GIFT               70
#define DIR_GIFT_EMPTY         71
#define DIR_RESET_TRAINER      72
#define DIR_RESET_WILD         73
#define DIR_VICTORY_ROAD       74
#define DIR_CT_INTRO           75 // CHAMPION TERRY
#define DIR_CT_DEFEAT          76
#define DIR_CT_AFTER           77
#define DIR_CT_AGAIN           78
#define DIR_SWITCH_TERRY       79
#define DIR_SWITCH_DIRECTOR    80
#define DIR_SWITCH_CUT         81
#define DIR_SWITCH_AFTER       82
#define DIR_FALLEN_INTRO       83
#define DIR_FALLEN_NONE        84
#define DIR_FALLEN_DEFEAT      85
#define DIR_FALLEN_AFTER       86
#define DIR_PREV_SILENT        87
#define DIR_PREV_AFTER         88
#define DIR_BIRCH_E4_INTRO     89
#define DIR_BIRCH_E4_DEFEAT    90
#define DIR_BIRCH_E4_AFTER     91
#define DIR_COPY_INTRO         92
#define DIR_COPY_DEFEAT        93
#define DIR_COPY_AFTER         94
#define DIR_CONFESSION         95
#define DIR_PHASE1_DEFEAT      96
#define DIR_PATCH101           97
#define DIR_PHASE2_HEAL        98
#define DIR_PHASE2_DEFEAT      99
#define DIR_QUESTION           100
#define DIR_STOP               101
#define DIR_TERRY_TRUCK        102
#define DIR_TERRY_TRUCK_LOST   103
#define DIR_TERRY_TRUCK_TAKE   104
#define DIR_CAME_HOME          105
#define DIR_MORE               106
#define DIR_ENDLESS_1          107
#define DIR_ENDLESS_2          108
#define DIR_ENDLESS_3          109
#define DIR_ENDLESS_N          110
#define DIR_ENDLESS_GATE       111
#define DIR_CHOOSE_ASK         112
#define DIR_CHOOSE_DONE        113
#define DIR_BIRCH_AFTER_STOP   114
#define DIR_BIRCH_AFTER_MORE   115
#define DIR_BIRCH_AFTER_CHOOSE 116
#define DIR_TRUCK              117
#define DIR_LEADER_0           118 // 118 to 141: leader l's intro (3l), defeat (3l + 1) and after (3l + 2)
#define DIR_STOP_DOOR          142
#define DIR_PATCH_THANKS       143
#define DIR_CHOOSE_KEPT        144
#define DIR_CHOOSE_GONE        145
#define DIR_TERRY_EMPTY_LOT    146
#define DIR_TERRY_TRUCK_KEEP   147
#define DIR_LIB_ROOM           148 // the sign by the library's far door
#define DIR_TERRY_SHORTCUT     149
#define DIR_DEX_0              150 // 150 to 154: the POKeDEX's notes on what RUN 0 caught
#define DIR_DEX_NOTES          5
#define DIR_TEST_TEAM_PARTY    155 // the arrival: TEST TEAM is the party
#define DIR_TEST_TEAM_BOX      156 // the arrival: TEST TEAM is in the PC
#define DIR_RETURN_0           157 // 157 to 159: a kept POKeMON given back after a badge
#define DIR_RETURNS            3
#define DIR_SAY_YES            160 // the checkpoint offer
#define DIR_RETURN_PC          161 // ...and the party was full
#define DIR_LEND               162 // the arrival: fewer than six in all, some it kept lent
#define DIR_COUNT              163

// The custom leaders in RUN 0 (the order of sLeaders in src/rogue.c).
#define DIR_LEADERS            8

// This attempt's moments, FLAG_ROGUE_RUN0_BEAT(n) (include/constants/rogue.h;
// cleared with the run). Written out for the scripts' assembler.
#define RUN0_FLAG_BIRCH_TALKED  (FLAG_ROGUE_RUN0_BEAT_0 + 0)
#define RUN0_FLAG_CHECKPOINT    (FLAG_ROGUE_RUN0_BEAT_0 + 1) // put back at the act's town
#define RUN0_FLAG_TERRY_MET     (FLAG_ROGUE_RUN0_BEAT_0 + 2)
#define RUN0_FLAG_ISLAND_ON     (FLAG_ROGUE_RUN0_BEAT_0 + 3) // on the real Littleroot's island
#define RUN0_FLAG_IN_LIBRARY    (FLAG_ROGUE_RUN0_BEAT_0 + 4)
#define RUN0_FLAG_STOP_WALK     (FLAG_ROGUE_RUN0_BEAT_0 + 6) // STOP: walking home
#define RUN0_FLAG_TRUCK_TERRY   (FLAG_ROGUE_RUN0_BEAT_0 + 7) // STOP: Terry stands by the truck
#define RUN0_FLAG_GIFT_TAKEN    (FLAG_ROGUE_RUN0_BEAT_0 + 8) // the daily gift, with no clock
#define RUN0_FLAG_ENDLESS_TOLD  (FLAG_ROGUE_RUN0_BEAT_9 + 0) // ENDLESS: this region's line
#define RUN0_FLAG_DRAFT_NOW     (FLAG_ROGUE_RUN0_BEAT_9 + 1) // this wild battle is a DRAFT
#define RUN0_FLAG_CT_TALKED     (FLAG_ROGUE_RUN0_BEAT_9 + 2) // CHAMPION TERRY talked to after
#define RUN0_FLAG_DRAFT_SEEN    (FLAG_ROGUE_RUN0_BEAT_9 + 3) // a DRAFT battle just ended
#define RUN0_FLAG_SWITCH_READY  (FLAG_ROGUE_RUN0_BEAT_9 + 4) // Terry waits at the LEAGUE's door
#define RUN0_FLAG_TRUCK_PARKED  (FLAG_ROGUE_RUN0_BEAT_9 + 5) // a normal run after STOP: the truck in the first town
#define RUN0_FLAG_SHORTCUT      (FLAG_ROGUE_RUN0_BEAT_9 + 6) // act 3: TERRY cleared the shortcut's obstacles
// The library's room (Route 114's Fossil Maniac's house, its back doorway
// open): the far door, and the wall beside it where its sign is read.
#define RUN0_LIBRARY_DOOR_X     4
#define RUN0_LIBRARY_DOOR_Y     1
// The LEAGUE's TERRY stands while this temporary flag is clear; the
// CHAMPION's room is empty while the next one is set.
#define FLAG_ROGUE_RUN0_HIDE_TERRY    FLAG_TEMP_1E
#define FLAG_ROGUE_RUN0_HIDE_CHAMPION FLAG_TEMP_1F
// CHAMPION TERRY's battle (a slot no RUN 0 region uses: the dojo's spare).
#define ROGUE_SLOT_RUN0_TERRY 238

// Scenes the scripts and the tests start (VAR_0x8004 for Rogue_Run0Scene,
// gRogueTestStory for the tests).
#define RUN0_SCENE_NONE        0
#define RUN0_SCENE_RESET       1  // the reset line
#define RUN0_SCENE_CLOCK       2
#define RUN0_SCENE_ISLAND      3  // leaving the real Littleroot
#define RUN0_SCENE_VICTORY     4  // Victory Road in act 4
#define RUN0_SCENE_CRASH       5  // the fake crash
#define RUN0_SCENE_SWITCH      6  // Terry's switch at the LEAGUE's door
#define RUN0_SCENE_ENDLESS     7  // an ENDLESS region's first line
#define RUN0_SCENE_LIB_OUT     8  // leaving the library
#define RUN0_SCENE_STOP_WALK   9  // the STOP ending: the walk home
#define RUN0_SCENE_ENDING      10 // the Director's question (tests)
#define RUN0_SCENE_DRAFT       11 // after the first DRAFT
#define RUN0_SCENE_ENDLESS_GATE 12 // ENDLESS: where the LEAGUE would be
#define RUN0_SCENE_LIB_IN      13 // into the library, the first time ever
#define RUN0_SCENE_COUNT       14
// Test hooks only (gRogueTestStory): the next wild Pokemon is a DRAFT; to
// the real Littleroot by the truck.
#define RUN0_TEST_DRAFT        20
#define RUN0_TEST_LITTLEROOT   21
// The POKeDEX note of the species in VAR_0x8004 into gStringVar4, and the
// species marked caught (to see the entry).
#define RUN0_TEST_DEX          22

// The endings' answers (VAR_RESULT of the question).
#define RUN0_ANSWER_STOP       0
#define RUN0_ANSWER_MORE       1
#define RUN0_ANSWER_CHOOSE     2

#endif // GUARD_CONSTANTS_ROGUE_RUN0_STORY_H
