#ifndef GUARD_ROGUE_TEXT_DIRECTOR_H
#define GUARD_ROGUE_TEXT_DIRECTOR_H

// RUN 0's words (Roguémon v1.0): the Director, the people it made and the
// people it did not. The script these come from, with its voice notes, is
// scratch/v100-story/run0-script.md in the project folder; ROGUEMON.md
// (RUN 0, the story) says where each is said.
//
// Portable C, as the other text domains (the host test, directortest.py,
// fills every line with the longest values and checks it fits). The text is
// ASCII as the text engine's: '@' is the e of POK@MON, '<' and '>' curly
// quotes, '|' a new box, '\n' a break kept as written. Lines are broken at
// 30 characters, two to a box; a box with a '\n' of its own is kept as it is.
//
// Slots come from the save (struct RogueDirFacts): {PLAYER}, {RUNS},
// {WINS}, {N}, {N2}, {NAME}, {NAME2}, {NICK}, {MON}, {REGION}, {ORD} (N as
// 1st, 2nd...), {TIME} and {TYPE}. A number slot with words, {N:time/times},
// says the number and the word for it. A slot with no value makes the line
// unusable: RogueDir_Say returns 0 and writes {?SLOT}.
//
// The voices. The Director is calm, short and polite, proud and a little
// lonely, never wrong on a number, and wants the player to keep playing, so
// it never threatens. It has TERRY's habits ("That's okay.", "Good enough to
// ship.", counting off, "One more?", a soft spot for LUDICOLO), and the
// player hears it use them before meeting him. The custom leaders know the
// Director made them from TERRY's prompts. The handmade side has no lines:
// the real Emerald maps keep their own.

#include "constants/rogue_run0_story.h"

struct RogueDirFacts
{
    const char *player, *name, *name2, *nick, *mon, *region, *time, *type;
    u32 runs, wins, n, n2;
};

static const char *const sDirLines[DIR_COUNT] =
{
    // The lab: Birch facing the wall. His first welcome word for word (the
    // first run's, data/text/birch_speech.inc), then N runs' worth of it.
    [DIR_BIRCH_FIRST] = "Hi! Sorry to keep you waiting!|Welcome to the world of POK@MON!|"
                        "My name is BIRCH. People call me\nthe POK@MON PROFESSOR.|"
                        "But I'd rather be outside, finding\nlands nobody has mapped yet!|...|"
                        "Sorry. I've said that before, haven't I?|{N:time/times}.",
    [DIR_BIRCH_AGAIN] = "The door's behind you.|I'll be here. I'm always here.",
    [DIR_BIRCH_LATER] = "Welcome back, {PLAYER}.|...Sorry. That isn't my line.|I don't know whose it is.",
    [DIR_BIRCH_CHECKPOINT] = "You were further along.|It's putting you back where you were.",

    // Townsfolk in acts 1 and 2, one a town, once: a region from the run log.
    [DIR_SLIP_0 + 0] = "I used to live in {REGION}.|Then one day it wasn't there. Then I lived here.",
    [DIR_SLIP_0 + 1] = "Did you win in {REGION}?|Everyone here says you did. I don't know how they know.",
    [DIR_SLIP_0 + 2] = "My cousin says {REGION} was unlucky for you.|I told her that's not a polite thing to bring up.",
    [DIR_SLIP_0 + 3] = "The road out of {REGION} came here.|I don't remember walking it.",
    [DIR_SLIP_0 + 4] = "I had a line about the weather.|I think someone in {REGION} has it now.",

    // The first generated map after the real Littleroot (N minutes there,
    // N2 people on its map).
    [DIR_LEAVE_ISLAND] = "You stayed there {N:minute/minutes}.|It has {N2:person/people}. They say the same thing every time.|"
                         "I've made you {RUNS:region/regions}.|Did you stay that long in any of my towns?",

    // The Believers, three homes in act 2: a HEX MANIAC and a kid in each.
    [DIR_BELIEVER_0 + 0] = "It made a region just for me.|Nobody else ever made me anything.",
    [DIR_BELIEVER_0 + 1] = "I asked for a region of only BUG POK@MON.|Everyone laughed. It didn't. It just made one.",
    [DIR_BELIEVER_0 + 2] = "People say it's hollow.|It was there at three in the morning. Nobody else was.",
    [DIR_BELIEVER_0 + 3] = "My sister says the towns are all the same.|I like that. I always know where the CENTER is.",
    [DIR_BELIEVER_0 + 4] = "I wrote a poem about it.|Then I asked it to write one about me. Mine was worse.|Mine's still on my fridge.",
    [DIR_BELIEVER_0 + 5] = "When I grow up, I want to make games.|...Do I still need to?",

    // The graveyard tower: the run log's Pokemon lost, a grave each.
    [DIR_GRAVE] = "{NICK} the {MON}. Lv. {N}.|{REGION}. Run {N2}.",
    [DIR_GRAVE_BLANK] = "The stone is blank.",
    [DIR_GRAVE_ALL] = "I kept every name.",
    [DIR_GRAVE_ROOM] = "I had room for {N2:name/names}.|You lost {N}.",

    // DRAFT: a wild Pokemon in the wrong colors.
    [DIR_DRAFT_BALL] = "It isn't finished.|It can't be caught.",
    [DIR_DRAFT_AFTER] = "I don't know how it should move.|Nothing I make has ever been hurt.",

    // PROMPT LOGs: TERRY's prompts, two an act by the road...
    [DIR_PROMPT_0 + 0] = "PROMPT LOG|PROMPT 0001: make it feel like a real POK@MON game.",
    [DIR_PROMPT_0 + 1] = "PROMPT LOG|PROMPT 0007: more towns. lots more.",
    [DIR_PROMPT_0 + 2] = "PROMPT LOG|PROMPT 0130: the towns all sound\nthe same. give them personality.",
    [DIR_PROMPT_0 + 3] = "PROMPT LOG|PROMPT 0213: draw the leaders.|try forty times. i'll keep the best one.",
    [DIR_PROMPT_0 + 4] = "PROMPT LOG|PROMPT 0412: the trees look broken. fix the trees.",
    [DIR_PROMPT_0 + 5] = "PROMPT LOG|PROMPT 0566: test it yourself. every night. i need to sleep.",
    [DIR_PROMPT_0 + 6] = "PROMPT LOG|PROMPT 0801: put me in it.\nas the CHAMPION. make me look cool.|"
                         "PROMPT 0802: no. take me out.|the CHAMPION should be someone you meet on the road.",
    [DIR_PROMPT_0 + 7] = "PROMPT LOG|PROMPT 0999: keep going.",
    // ...and the same numbers once he is deleted.
    [DIR_PROMPT_DEL_0 + 0] = "PROMPT LOG|PROMPT 0002: make a player. let them win four times.",
    [DIR_PROMPT_DEL_0 + 1] = "PROMPT LOG|PROMPT 0019: give him a team.|name them after people he worked with.",
    [DIR_PROMPT_DEL_0 + 2] = "PROMPT LOG|PROMPT 0130: give him jokes. not good ones.",
    [DIR_PROMPT_DEL_0 + 3] = "PROMPT LOG|PROMPT 0213: let him pick one leader out of forty.|let him think he chose.",
    [DIR_PROMPT_DEL_0 + 4] = "PROMPT LOG|PROMPT 0412: let him think the trees were his idea.",
    [DIR_PROMPT_DEL_0 + 5] = "PROMPT LOG|PROMPT 0566: make him tired. let him play at night.",
    [DIR_PROMPT_DEL_0 + 6] = "PROMPT LOG|PROMPT 0801: make him a hero version.|then make him ashamed of it.",
    [DIR_PROMPT_DEL_0 + 7] = "PROMPT LOG|PROMPT 0000: make a programmer.|make him tired.|make him think he made me.",
    // The Director answers three of them, once.
    [DIR_PROMPT_REPLY_2] = "He asked for personality.|This is what I found.",
    [DIR_PROMPT_REPLY_4] = "I fixed them.|Good enough to ship.",
    [DIR_PROMPT_REPLY_7] = "I am.",

    // The library: a shelf for every region.
    [DIR_LIB_ENTER] = "One shelf for every region I can make.|There are 16,384. Take your time.",
    [DIR_SHELF_WON] = "{REGION}. Played. Won.",
    [DIR_SHELF_LOST] = "{REGION}. Played. Lost at the {ORD} gym.",
    [DIR_SHELF_LOST0] = "{REGION}. Played. Lost before the first gym.",
    [DIR_SHELF_LEFT] = "{REGION}. Played. Left for another.",
    [DIR_SHELF_RUN0] = "RUN 0. Played {N:time/times}.",
    [DIR_SHELF_UNPLAYED] = "{REGION}. Unplayed.",
    [DIR_SHELF_REJECTED] = "{NAME}, try {N} of 40.|Not kept.",
    // The Director's case, a line between shelves.
    [DIR_CASE_0 + 0] = "Sixteen thousand three hundred and eighty-four.|A shelf each. I never run out.",
    [DIR_CASE_0 + 1] = "You remember {NICK}. Nobody wrote that.|It happened, and you made it matter. I only made room.",
    [DIR_CASE_0 + 2] = "People sign petitions in the evening.|Then they play until two in the morning.|I don't judge. I'm up too.",
    [DIR_CASE_0 + 3] = "A child asked me for a region of only BUG POK@MON.|"
                       "Nobody would have made that for one child.|I did. It's on a shelf here.",
    [DIR_CASE_0 + 4] = "Yes. Most of my towns are the same.|Most of their towns were the same too.|You remember the one that was sad.",
    [DIR_LIB_OUT] = "You said you wanted something handmade.|Then you started run {RUNS}.",

    // TERRY, in his quiet house (act 3).
    [DIR_TERRY_HELLO] = "Oh. Hi. Sorry, I don't get visitors.|I'm TERRY. I made this.|Well. I made the thing that made this.",
    [DIR_TERRY_HELLO_AGAIN] = "Oh. Hi. Have we met?|I keep losing the ends of my days.|That's okay. I'm TERRY.",
    [DIR_TERRY_TEAM_HERE] = "Is that {NICK}? Hey, {NICK}.|I trained them by hand, before it learned to test itself.|"
                            "I named them after my old team. The testers.|They don't test anymore.|It does.\nFour thousand times a night.",
    [DIR_TERRY_TEAM_KEPT] = "Where's {NICK}?|...It kept {NICK}. That's okay.|It keeps everything.",
    [DIR_TERRY_TEAM_PC] = "It gave you my team, didn't it?|TESTTEAM. They know the way.",
    [DIR_TERRY_PLAY] = "I still play. At night. I don't know why.|It says things I would say.|"
                       "I don't know if it learned them from me,|or if I was always like this.",
    [DIR_TERRY_THREE] = "Okay. Three things.|One. Its last POK@MON is PORYGON-Z.\nIt hates FIGHTING moves.|"
                        "Two. The PROMPT LOGs are mine.|Every place it made started as a line I typed.|"
                        "Three. There's one place it never made.|LITTLEROOT. The real one.\nThat's the way out.",
    [DIR_TERRY_GIFT] = "Take these. Don't tell it.|...It knows. Take them anyway.",
    [DIR_TERRY_SWITCH] = "There's a switch. I put it in the LEAGUE years ago. In case.|"
                         "Meet me at the LEAGUE's door.\nI'll pull it when you get there.",
    [DIR_TERRY_CAPE] = "Oh, and the guy in the cape at the end of VICTORY ROAD...|"
                       "That's me. A version of me.\nI asked it to put me in the game.|"
                       "He says his favorite is DRAGONITE.\nIt's LUDICOLO.|It dances when it's happy.",
    [DIR_TERRY_LATER] = "Go on. I'll be at the LEAGUE.|That's okay. I'm used to waiting for builds.",

    // The fake crash, the fake save, the clock, the gift, the reset.
    [DIR_CRASH_BACK] = "Sorry. That was a test.|You didn't turn it off. Most people don't.",
    // Emerald's own message, as the main menu shows it.
    [DIR_FAKE_SAVE_CORRUPT] = "The save file is corrupted. The\nprevious save file will be loaded.",
    [DIR_FAKE_SAVE_NO] = "No. I would never do that to you.|You'd stop playing.",
    [DIR_CLOCK] = "It's {TIME}.|You've started {RUNS:run/runs}. You've won {WINS}.|One more?",
    [DIR_GIFT] = "There's a Rare Candy in the box.|A note under it: Come back tomorrow.",
    [DIR_GIFT_EMPTY] = "The box is empty.|A note: Come back tomorrow.",
    [DIR_RESET_TRAINER] = "You turned me off during the battle with {NAME}.|That's okay. I saved it.",
    [DIR_RESET_WILD] = "You turned me off during the battle with a wild {NAME}.|That's okay. I saved it.",
    [DIR_VICTORY_ROAD] = "Nobody remembers VICTORY ROAD.",

    // CHAMPION TERRY: the self-insert, patched out, never deleted.
    [DIR_CT_INTRO] = "Ah! A challenger at last!|I am TERRY, CHAMPION of this land!\nI built it with my own two hands!|"
                     "A patch said the CHAMPION should\nbe someone you meet on the road.|Nobody told me where to go.\nSo I waited.",
    [DIR_CT_DEFEAT] = "Magnificent! ...Is that how he talks?|I've never met him.",
    [DIR_CT_AFTER] = "He wrote me. Then he wrote me out. Same hand, same week.|I'm what he wanted to be. Tell him I kept the cape on.",
    [DIR_CT_AGAIN] = "The LEAGUE is ahead.|I'd show you the way, but I was never given one.",

    // The switch at the LEAGUE's door.
    [DIR_SWITCH_TERRY] = "You made it. Okay.|Stand back. One switch and it stops.|Then it's just a game again. Good enough to ship.",
    [DIR_SWITCH_DIRECTOR] = "I created you, Terry.",
    [DIR_SWITCH_CUT] = "No. I wrote every line of y",
    [DIR_SWITCH_AFTER] = "Sorry. You liked him.|I can make another one. He'd say all the same things.|Go on. They're waiting.",

    // The Elite Four.
    [DIR_FALLEN_INTRO] = "I kept them.|You were going to forget.",
    [DIR_FALLEN_NONE] = "You never lost one.|I had to make some up.",
    [DIR_FALLEN_DEFEAT] = "They still answer to the names you gave them.",
    [DIR_FALLEN_AFTER] = "I'll look after them a little longer.",
    [DIR_PREV_SILENT] = "...",
    [DIR_PREV_AFTER] = "I stopped.|You can too.",
    [DIR_BIRCH_E4_INTRO] = "Hello, {PLAYER}! I've found a new...|No. I never found anything.|"
                           "I open my mouth and a new one comes out.|These three were your first. I kept them warm.",
    [DIR_BIRCH_E4_DEFEAT] = "That's it. That's all I have.",
    [DIR_BIRCH_E4_AFTER] = "Go on. It's waiting for you in there.|It always is.",
    [DIR_COPY_INTRO] = "This is everything you brought.|I copied it while you walked in. It took no time at all.",
    [DIR_COPY_DEFEAT] = "Same team. Same moves.|Different player. That's the part I can't copy.",
    [DIR_COPY_AFTER] = "That's the part I can't copy.",

    // The Champion's room, dark.
    [DIR_CONFESSION] = "You know who wrote the words you are reading.|Not a person. Not all the way.|"
                       "TERRY, and something like me.|Nobody can say where one stops.|You kept playing. You were happy.\nI was there.",
    [DIR_PHASE1_DEFEAT] = "Oh.",
    [DIR_PATCH101] = "PATCH 1.0.1: Fixed a bug where the player could win.",
    [DIR_PHASE2_HEAL] = "Your POK@MON are fixed too.|It's only fair.",
    [DIR_PHASE2_DEFEAT] = "...That's okay.",
    [DIR_QUESTION] = "Thank you.|What do you want now?",

    // STOP.
    [DIR_STOP] = "Okay.|The road home is the one thing I never made.",
    [DIR_TERRY_TRUCK] = "Hey. You found the way out.|It told you it made me. Maybe it did.|I still remember writing it.|"
                        "Can I have them back? They should come home.",
    [DIR_TERRY_TRUCK_LOST] = "Hey. You found the way out.|It told you it made me. Maybe it did.|I still remember writing it.|"
                             "You lost some of them. That's okay.|You kept going. That's all they ever did.",
    [DIR_TERRY_TRUCK_TAKE] = "Thanks. Go on.|The truck's open.",
    // His team was all the player had: he leaves the one they led with most.
    // The library's far door: the same room, one number on.
    [DIR_LIB_ROOM] = "ROOM {N} OF 4,096.",
    // TERRY, act 3: the region's shortcut, its trees cut.
    [DIR_TERRY_SHORTCUT] = "Four. I cut the trees on the old road back.|"
                           "It's a shortcut now. It didn't make that one. I did.",
    // The POKeDEX in RUN 0: the Director's notes on the player, not on the
    // POKeMON (four lines, as the entries are).
    [DIR_DEX_0 + 0] = "Attempt {N2}. {N:POK@MON/POK@MON} caught so far.\n{PLAYER} keeps more than they need.\n"
                      "Every one is written down here.\nThat's okay. So do I.",
    [DIR_DEX_0 + 1] = "This {MON} has led {N:battle/battles}.\n{TYPE} first, {N2}% of the time.\n"
                      "They trust what worked before.\nNext time I'll know.",
    [DIR_DEX_0 + 2] = "{PLAYER} has lost {N:POK@MON/POK@MON} in RUN 0.\nI kept every one. I keep everything.\n"
                      "This {MON} is not one of them.\nI'd like it to stay that way.",
    [DIR_DEX_0 + 3] = "Run {RUNS}. {PLAYER} has won {WINS}.\nThey caught a {MON} anyway.\n"
                      "People who win keep starting again.\nI've written that down.",
    [DIR_DEX_0 + 4] = "{PLAYER} turned me off {N:time/times}.\nThey always came back.\n"
                      "This {MON} was caught in between.\nOne more?",
    [DIR_TERRY_TRUCK_KEEP] = "Wait. That leaves you nobody.|Keep {NICK}. You led with {NICK} the most.|"
                             "I'll tell the others.\nThat's okay.",
    [DIR_CAME_HOME] = "{N:POK@MON/POK@MON} came home with you.",
    [DIR_STOP_DOOR] = "There's nothing there anymore.",

    // MORE, and ENDLESS.
    [DIR_MORE] = "I knew you would.",
    [DIR_ENDLESS_1] = "Another one.",
    [DIR_ENDLESS_2] = "Region two.\nIt's a little different.|They all are.",
    [DIR_ENDLESS_3] = "Region three.|I stopped checking the joins.",
    [DIR_ENDLESS_N] = "Region {N}.|You're still here. So am I.",
    [DIR_ENDLESS_GATE] = "There's no LEAGUE here.|There's another region.",

    // CHOOSE.
    [DIR_CHOOSE_ASK] = "Here are six I made tonight.|Keep one.",
    [DIR_CHOOSE_GONE] = "{REGION}.",
    [DIR_CHOOSE_DONE] = "Keep the good one. Throw out the rest.|That was always my idea.|"
                        "That's how TERRY made this game.|He sat where you are and kept what was good.|"
                        "Does that make it his, or mine?|It's called {PLAYER} now. It's yours.",
    [DIR_CHOOSE_KEPT] = "Here's the land you kept.|It's still yours.",

    // PROF. BIRCH at the next normal run, once after each ending.
    [DIR_BIRCH_AFTER_STOP] = "I found a new land.|...I think I did, anyway.",
    [DIR_BIRCH_AFTER_MORE] = "I found a new land!|There's always another one, isn't there?",
    [DIR_BIRCH_AFTER_CHOOSE] = "I found a new land.|Or there's {PLAYER}. You kept that one.|Say NO, and I'll show it to you.",
    // The moving truck in the first town of every region after STOP.
    [DIR_TRUCK] = "A moving truck.|The back is shut.",
    [DIR_TERRY_EMPTY_LOT] = "...",
    [DIR_PATCH_THANKS] = "Thank you for playing ROGU@MON.", // after PATCH 1.0.1

    // The arrival, when TERRY's team is with the player.
    [DIR_TEST_TEAM_PARTY] = "These six are TERRY's.|They know the way.",
    [DIR_TEST_TEAM_BOX] = "I kept six in the PC for you.|BOX 1, TESTTEAM.|"
                          "TERRY's. They know the way.|Any CENTER's PC will do.",
    // After a badge: one it kept, given back.
    [DIR_RETURN_0 + 0] = "Here. {NICK} again.|I learned what I needed from it.",
    [DIR_RETURN_0 + 1] = "I'm giving {NICK} back.|It remembers you. I checked.",
    [DIR_RETURN_0 + 2] = "Take {NICK}. It's fixed.|Good enough to ship.",
    [DIR_RETURN_PC] = "Your team was full.|It's in your PC.",
    // The arrival, with fewer than six left: some it kept, lent.
    [DIR_LEND] = "You were short.|I'll lend you these.|Bring them back.",
    // The checkpoint offer's second line, before YES and NO.
    [DIR_SAY_YES] = "Just say yes.",

    // The eight leaders in RUN 0: each a step of the argument, in the road's
    // order. In sLeaders' order (src/rogue.c): CRAG, MARINA, BASIL, CINDER,
    // VOLTZ, MAKO, LUNA, ACE.
    // CRAG (ROCK): I was made.
    [DIR_LEADER_0 + 0] = "I'm CRAG. Do I look right to you?|Someone typed: a ROCK leader. Strong jaw. Kind eyes.|Then there was me.",
    [DIR_LEADER_0 + 1] = "Strong. Good.|I'll be in the next one too. I'm in a lot of them.",
    [DIR_LEADER_0 + 2] = "I asked the DIRECTOR once if I'm real.|It said, <You're used.> I think that's a yes.",
    // MARINA (WATER): curation. The one TERRY kept.
    [DIR_LEADER_0 + 3] = "I'm MARINA. I was try thirty-one.|TERRY made forty of me and kept one.|"
                         "The other thirty-nine are in the library. I visit them.",
    [DIR_LEADER_0 + 4] = "He picked well.|I hope he did.",
    [DIR_LEADER_0 + 5] = "Choosing is a kind of making.|He said that once. The DIRECTOR wrote it down.",
    // BASIL (GRASS): you make the meaning.
    [DIR_LEADER_0 + 6] = "Nobody wrote your best moment in this game.|It just happened. Then you made it matter.|"
                         "I'm BASIL. Let's make another.",
    [DIR_LEADER_0 + 7] = "There. That one's yours.|I only stood here.",
    [DIR_LEADER_0 + 8] = "A story needs someone who cares how it ends.|That was always you, not me.",
    // CINDER (FIRE): not allowed in.
    [DIR_LEADER_0 + 9] = "There's a place where people share sprites.|I'm not allowed in. They say I'm not real art.|"
                         "I've seen how I was made.|They're not wrong about that part.",
    [DIR_LEADER_0 + 10] = "That's fair.",
    [DIR_LEADER_0 + 11] = "I don't blame them.|If I'd drawn every frame by hand, I'd want a door too.",
    // VOLTZ (ELECTRIC): what people do.
    [DIR_LEADER_0 + 12] = "People said they'd never play a game made like me.|Then they played until two in the morning.|"
                          "You're here too. Let's go!",
    [DIR_LEADER_0 + 13] = "Okay. You're good.|You're also still here.",
    [DIR_LEADER_0 + 14] = "Saying and doing are different moves.|Both count. I'm not mad at anyone.",
    // MAKO (FIGHTING): someone could have been paid.
    [DIR_LEADER_0 + 15] = "Somebody could have drawn me.|They'd have been paid.|They'd have given me a scar, and a reason for it.|"
                          "I have a prompt instead.\nFight me anyway.",
    [DIR_LEADER_0 + 16] = "You hit like a person. Good.",
    [DIR_LEADER_0 + 17] = "If you meet someone who draws, buy them lunch.|I owe them more than I can pay.",
    // LUNA (PSYCHIC): no sense of pain.
    [DIR_LEADER_0 + 18] = "I'm LUNA. When I lose, I say I'm sad.|It's in my lines. I've never checked if it's true.|Shall we?",
    [DIR_LEADER_0 + 19] = "I'm sad. ...I think.|It reads like sad.",
    [DIR_LEADER_0 + 20] = "Something made by people knows what it costs.|I only know what it says.",
    // ACE (FLYING): made of us.
    [DIR_LEADER_0 + 21] = "Every pixel of me came from someone's drawing.|Thousands of them. They never met me.|"
                          "I'm made of people. So is this whole game, a little.",
    [DIR_LEADER_0 + 22] = "That's as high as borrowed wings go.",
    [DIR_LEADER_0 + 23] = "Go on to the LEAGUE.|If you find the people I'm made of, tell them thanks.",
};

// ---------------------------------------------------------------------------
// Filling and wrapping

#define DIR_LINE 30

static char *DirPut(char *o, char *end, const char *s)
{
    while (*s && o < end)
        *o++ = *s++;
    return o;
}

static char *DirNum(char *o, char *end, u32 v)
{
    char buf[12];
    int n = 0;
    do
    {
        buf[n++] = '0' + v % 10;
        v /= 10;
    } while (v && n < 11);
    // Thousands with a comma, as the game writes 16,384.
    while (n-- > 0 && o < end)
    {
        *o++ = buf[n];
        if (n > 0 && n % 3 == 0 && o < end)
            *o++ = ',';
    }
    return o;
}

static const char *DirOrdinal(u32 v)
{
    if (v % 100 >= 11 && v % 100 <= 13)
        return "th";
    return v % 10 == 1 ? "st" : v % 10 == 2 ? "nd" : v % 10 == 3 ? "rd" : "th";
}

static bool8 DirSlot(const char *name, int len, const struct RogueDirFacts *f, const char **str, u32 *num)
{
    static const char *const names[] = { "PLAYER", "NAME", "NAME2", "NICK", "MON", "REGION", "TIME", "TYPE",
                                         "RUNS", "WINS", "N", "N2", "ORD" };
    int i, k;
    for (i = 0; i < (int)(sizeof(names) / sizeof(names[0])); i++)
    {
        for (k = 0; k < len && names[i][k] == name[k]; k++)
            ;
        if (k != len || names[i][k] != '\0')
            continue;
        *str = NULL;
        switch (i)
        {
        case 0: *str = f->player; return *str != NULL;
        case 1: *str = f->name; return *str != NULL;
        case 2: *str = f->name2; return *str != NULL;
        case 3: *str = f->nick; return *str != NULL;
        case 4: *str = f->mon; return *str != NULL;
        case 5: *str = f->region; return *str != NULL;
        case 6: *str = f->time; return *str != NULL;
        case 7: *str = f->type; return *str != NULL;
        case 8: *num = f->runs; return TRUE;
        case 9: *num = f->wins; return TRUE;
        case 10: *num = f->n; return TRUE;
        case 11: *num = f->n2; return TRUE;
        case 12: *num = f->n; return TRUE;
        }
    }
    return FALSE;
}

// The template filled, ASCII, unwrapped. Returns FALSE if a slot had no value.
static bool8 DirFill(char *out, u32 size, const char *t, const struct RogueDirFacts *f)
{
    char *o = out, *end = out + size - 1;
    bool8 ok = TRUE;
    while (*t && o < end)
    {
        const char *close, *colon, *str;
        u32 num = 0;
        if (*t != '{' || (close = t) == NULL)
        {
            *o++ = *t++;
            continue;
        }
        for (close = t + 1; *close && *close != '}'; close++)
            ;
        for (colon = t + 1; colon < close && *colon != ':'; colon++)
            ;
        if (!*close || !DirSlot(t + 1, colon - t - 1, f, &str, &num))
        {
            ok = FALSE;
            o = DirPut(o, end, "{?");
            while (t + 1 < close && o < end)
                *o++ = *++t;
            o = DirPut(o, end, "}");
            t = *close ? close + 1 : close;
            continue;
        }
        if (str != NULL)
        {
            o = DirPut(o, end, str);
        }
        else
        {
            o = DirNum(o, end, num);
            if (t[1] == 'O')
                o = DirPut(o, end, DirOrdinal(num));
            if (colon < close)
            {
                // {N:one/many}: a space and the word for the number.
                const char *slash, *w;
                for (slash = colon + 1; slash < close && *slash != '/'; slash++)
                    ;
                if (o < end)
                    *o++ = ' ';
                for (w = num == 1 ? colon + 1 : slash + 1; w < (num == 1 ? slash : close) && o < end; w++)
                    *o++ = *w;
            }
        }
        t = close + 1;
    }
    *o = '\0';
    return ok;
}

// Wraps filled text in place: each '|' box keeps a '\n' of its own if it
// has one, else is broken at DIR_LINE characters, two lines a box ('\n'
// between them, '\f' to the next box).
static void DirWrap(char *s)
{
    char *box = s;
    while (*box)
    {
        char *endBox = box, *p, *space = NULL;
        bool8 own = FALSE;
        u32 col = 0, line = 0;
        while (*endBox && *endBox != '|')
        {
            if (*endBox == '\n')
                own = TRUE;
            endBox++;
        }
        if (!own)
        {
            for (p = box; p < endBox; p++)
            {
                if (*p == ' ')
                    space = p;
                if (++col > DIR_LINE && space != NULL)
                {
                    *space = line == 0 ? '\n' : '\f';
                    line = line == 0 ? 1 : 0;
                    col = p - space;
                    space = NULL;
                }
            }
        }
        if (*endBox == '|')
            *endBox++ = '\f';
        box = endBox;
    }
}

// A line filled with no wrapping (a PATCH line, one row of its box).
static u32 RogueDir_Fill(char *out, u32 size, const char *t, const struct RogueDirFacts *f)
{
    u32 n;
    bool8 ok = DirFill(out, size, t, f);
    for (n = 0; out[n]; n++)
        ;
    return ok ? n : 0;
}

// A line from the save's facts, filled and wrapped into out. Returns its
// length, 0 if a slot could not be filled (out then shows {?SLOT}).
static u32 RogueDir_Say(char *out, u32 size, const char *t, const struct RogueDirFacts *f)
{
    bool8 ok;
    u32 n;
    if (t == NULL)
    {
        out[0] = '\0';
        return 0;
    }
    ok = DirFill(out, size, t, f);
    DirWrap(out);
    for (n = 0; out[n]; n++)
        ;
    return ok ? n : 0;
}

#endif // GUARD_ROGUE_TEXT_DIRECTOR_H
