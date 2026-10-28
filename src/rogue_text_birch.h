// PROF. BIRCH and run memory (the Birch domain). Included at the end of
// src/rogue_gen_text.h.
//
// What he says about a region on his map in the speech, from its land (only
// what the map shows) and its facts (how it began, its festival, its famous
// figure), with the old rumor about the first GYM LEADER; his lines in the
// lab; his "welcome back" on the speech platform, which remembers the last
// region, how far the player got and, now and then, who was lost; the
// epilogue on the run summary's first page after a win or a loss; and what
// the CHAMPION (the wanderer) says walking the player into the HALL OF FAME.
//
// The game calls the functions below (src/rogue.c, src/main_menu.c,
// src/rogue_ui.c) and converts the result; gen_host --script prints all of
// it (TextDump_Birch). Picks are spread by the run count, so a returning
// player hears new words each time.
//
// Voice (notes/v013-style.md section 6): excited, kind, a field researcher
// who would rather be outside. He is the one person who may talk about the
// region as a land he found and studies. Fallen POK@MON: rarely (about one
// time in three when the run lost any), gently, never blame (section 5).

#define BIRCH_SALT 42100

// ---------------------------------------------------------------------------
// The land, as his map shows it: a line for its strongest trait, and, the
// first time, a little more about the second.

TEXT_BANK(sBirchLandLakes,
    "This is {REGION}. Rivers and lakes wind all through it!",
    "This is {REGION}, a land of lakes and lazy rivers.",
    "Here's {REGION}! So many lakes, so many rivers!",
    "This is {REGION}. Half of it is water, I swear!");
TEXT_BANK(sBirchLandSea,
    "This is {REGION}. It sits right beside the open sea!",
    "Here's {REGION}! Its coasts go on and on.",
    "This is {REGION}, where the land runs down to the sea.",
    "This is {REGION}. Bring a towel. Trust me on that one.");
TEXT_BANK(sBirchLandWoods,
    "This is {REGION}. Deep, old forests cover much of it.",
    "Here's {REGION}! It's more trees than roads, really.",
    "This is {REGION}. The sun barely gets into its woods!",
    "This is {REGION}. I could live in its forests!");
TEXT_BANK(sBirchLandRock,
    "This is {REGION}. Steep, rocky hills run across it.",
    "Here's {REGION}! Lots of rock, lots of climbing.",
    "This is {REGION}. Its hills are rough, but oh, the views!",
    "This is {REGION}. Good boots, that's what this land wants.");
TEXT_BANK(sBirchLandAsh,
    "This is {REGION}. A volcano smokes over the land!",
    "Here's {REGION}! See that crater? That's a real volcano!",
    "This is {REGION}. One of its towns lives under a volcano!",
    "This is {REGION}. The ground there is warm. I checked!");
TEXT_BANK(sBirchLandFields,
    "This is {REGION}. Wide, grassy meadows as far as you can see!",
    "Here's {REGION}! Tall grass and open sky!",
    "This is {REGION}, a gentle land of fields and meadows.",
    "This is {REGION}. The grass there smells like summer.");
static const struct RogueTextBank *const sBirchLand[RTRAIT_COUNT] =
{
    [RTRAIT_LAKES] = &sBirchLandLakes, [RTRAIT_SEA] = &sBirchLandSea, [RTRAIT_WOODS] = &sBirchLandWoods,
    [RTRAIT_ROCK] = &sBirchLandRock, [RTRAIT_ASH] = &sBirchLandAsh, [RTRAIT_FIELDS] = &sBirchLandFields,
};

TEXT_BANK(sBirchMoreLakes,
    "Bring a FISHING ROD. Half its POK@MON live in the water!",
    "It rains a lot by the lakes. WATER POK@MON love it!",
    "I fell in a lake there once. My notes never dried!");
TEXT_BANK(sBirchMoreSea,
    "Sailors swear its sea hides rare POK@MON!",
    "Some of it you can only reach across the water!",
    "I watched its tide all afternoon. For research!");
TEXT_BANK(sBirchMoreWoods,
    "BUG POK@MON love those woods. So do I, mostly!",
    "TRAINERS hide among the trees there. Keep your eyes open!",
    "I got lost in those woods for two days. Best days ever!");
TEXT_BANK(sBirchMoreRock,
    "The sun bakes those rocks. Only tough POK@MON live there.",
    "Watch for ledges in the hills. My knees remember them!",
    "I found a fossil in those hills! It was a rock. Still!");
TEXT_BANK(sBirchMoreAsh,
    "Ash falls like snow near the volcano. FIRE POK@MON love it.",
    "Even the grass is gray near the volcano!",
    "I took the volcano's heat once. My thermometer melted!");
TEXT_BANK(sBirchMoreFields,
    "POK@MON hide in all that tall grass. Choose well!",
    "Its grass grows taller than you. Taller than me, even!",
    "I tried to count the POK@MON in its fields. I lost count!");
static const struct RogueTextBank *const sBirchMore[RTRAIT_COUNT] =
{
    [RTRAIT_LAKES] = &sBirchMoreLakes, [RTRAIT_SEA] = &sBirchMoreSea, [RTRAIT_WOODS] = &sBirchMoreWoods,
    [RTRAIT_ROCK] = &sBirchMoreRock, [RTRAIT_ASH] = &sBirchMoreAsh, [RTRAIT_FIELDS] = &sBirchMoreFields,
};

// Its past and its people, as he heard them: one a region.
TEXT_BANK(sBirchRegionFacts,
    "The folks there tell an old tale.|Long ago, {FOUNDING}.",
    "Long ago, {FOUNDING}.|That's what they tell the kids in {OLDTOWN}, anyway!",
    "I've read their oldest books.|Long ago, {FOUNDING}.",
    "{FEST_TOWN} holds the {FESTIVAL} there.|{^FEST_ACT}. I can't wait!",
    "They love their {FESTIVAL} there.|{^FEST_ACT}. What fun!",
    "Everyone there is proud of {FIGURE}.|{FIGURE} {FIGURE_DEED}!",
    "{FIGURE_TOWN} was home to {FIGURE}.|{FIGURE} {FIGURE_DEED}. Imagine that!",
    "I'd love to have met {FIGURE}.|{FIGURE} {FIGURE_DEED}. What a life!");

// The old rumor about the first GYM LEADER, two ways for each type (each
// fits the box's second line).
static const char *const sBirchRumorStart[] = { "They say the first GYM LEADER ", "Word is, the first GYM LEADER " };
static const char *const sBirchRumors[RTYPE_COUNT][2] =
{
    [RTYPE_NONE]     = { "is tough to beat.", "is tough to beat." },
    [RTYPE_NORMAL]   = { "keeps it simple, and tough.", "never does anything fancy." },
    [RTYPE_FIGHTING] = { "trains with fists, not words.", "does push-ups between battles." },
    [RTYPE_FLYING]   = { "is always watching the sky.", "would rather fly than walk." },
    [RTYPE_POISON]   = { "mixes potions nobody drinks.", "has a garden nobody touches." },
    [RTYPE_GROUND]   = { "has dirt under every nail.", "digs holes just for fun." },
    [RTYPE_ROCK]     = { "is as hard as stone.", "collects rocks. Big ones." },
    [RTYPE_BUG]      = { "knows every bug in the woods.", "keeps bugs in jars at home." },
    [RTYPE_GHOST]    = { "sees things nobody else can.", "laughs at things nobody sees." },
    [RTYPE_STEEL]    = { "builds machines for fun.", "wears armor to breakfast." },
    [RTYPE_MYSTERY]  = { "is tough to beat.", "is tough to beat." },
    [RTYPE_FIRE]     = { "has a fiery temper.", "keeps the GYM hot as an oven." },
    [RTYPE_WATER]    = { "would rather swim than walk.", "swims laps before breakfast." },
    [RTYPE_GRASS]    = { "talks to the plants.", "grows flowers in the GYM." },
    [RTYPE_ELECTRIC] = { "makes your hair stand on end.", "never needs a light switch." },
    [RTYPE_PSYCHIC]  = { "knows your moves ahead of you.", "can bend a spoon by looking." },
    [RTYPE_ICE]      = { "keeps the GYM freezing cold.", "eats ice cream in a blizzard." },
    [RTYPE_DRAGON]   = { "was raised by dragon tamers.", "fears nothing. Nothing at all." },
    [RTYPE_DARK]     = { "only battles after sundown.", "never says where they live." },
    [RTYPE_FAIRY]    = { "is sweet. Don't be fooled.", "has a smile that hides a lot." },
};

// ---------------------------------------------------------------------------
// The lab

// The first time the player walks in.
TEXT_BANK(sBirchLabFirst,
    "There you are, {PLAYER}! Welcome to {TOWN}!|Mind the boxes. I only moved my lab here last week!",
    "{PLAYER}! You found the lab! Welcome to {REGION}.|{TOWN} is small, but the people here have been so kind to me.",
    "Welcome, {PLAYER}! This is {TOWN}, where every road in {REGION} starts.|I've been up since dawn. The POK@MON here wake up early!",
    "There you are, {PLAYER}! Welcome to {TOWN}.|The {TRADE_FOLK} here helped me carry all these books in. Such nice people!");
// Later journeys: the same professor, a new town outside the window.
TEXT_BANK(sBirchLabBack,
    "There you are, {PLAYER}! Welcome to {REGION}!|New land, same old lab. My boxes got here before I did!",
    "{PLAYER}! Welcome to {TOWN}.|The {TRADE_FOLK} here already know your name. I may have talked about you a little.",
    "Welcome to the lab, {PLAYER}! It's {TOWN} outside the window this time.|Different view, same mess!",
    "Ah, {PLAYER}! Welcome to {REGION}!|I've been out on the road all morning. My boots are full of grass!",
    "{PLAYER}! Welcome to {TOWN}.|Folks here say it's known for {PRIDE}. I'm already a fan!",
    "There you are! Welcome to {REGION}, {PLAYER}.|I haven't even unpacked. Who needs chairs when there's a whole land out there?");

TEXT_BANK(sBirchNews,
    "Oh, and there's news since your last journey!",
    "Oh! I almost forgot. There's news since your last journey!",
    "Before you go, some news from your last journey!");

TEXT_BANK(sBirchChoose,
    "Now, your first partner! Choose well. You'll go through a lot together.",
    "These POK@MON have been waiting all morning. Go on, pick one!",
    "Every TRAINER needs a partner. Which one will see {REGION} with you?",
    "Here they are! I caught them myself. Well, my aide helped. A lot.",
    "Pick a partner, {PLAYER}. Take your time. ...But not too much time!");

// Out the door: the first two journeys get the MAP tip.
TEXT_BANK(sBirchGoodLuckTip,
    "Press START and open the MAP to see the whole region. Good luck!",
    "Lost? Press START and open the MAP. It shows all of {REGION}. Good luck!");
TEXT_BANK(sBirchGoodLuck,
    "My kid {RIVAL} set off a little while ago. You'll catch up, I'm sure. Good luck!",
    "The road out of {TOWN} is full of {LOCALMON}. Say hello for me! Good luck!",
    "Go on, {PLAYER}. {REGION} is waiting. I'll be right here, buried in notes!",
    "If you see any odd POK@MON out there, tell me about it! Good luck!",
    "Off you go! And {PLAYER}... come back with stories, all right?");

// Talked to again: the first GYM and his kid, the rival.
TEXT_BANK(sBirchFirstGym,
    "The first GYM is in {GYM_TOWN}. {RIVAL} headed there ages ago!",
    "{RIVAL} took {RIVAL_MON} and ran for {GYM_TOWN}. Didn't even finish breakfast!",
    "Off to {GYM_TOWN}? That's where the first GYM is. Good luck!",
    "{RIVAL} has been training {RIVAL_MON} since dawn. Don't let my kid get too far ahead!");
TEXT_BANK(sBirchAfterBadge,
    "A BADGE already? {RIVAL} called to say you're catching up fast!",
    "Back to visit? I've filled seven notebooks about {REGION} so far!",
    "You look stronger every time I see you. So does {LEAD}!",
    "{RIVAL} writes home about you, you know. Mostly good things!");

// ---------------------------------------------------------------------------
// Welcome back: on the speech platform, said in the last region.

TEXT_BANK(sBirchBackWon,
    "Welcome back, CHAMPION of {REGION}!",
    "{PLAYER}! I still tell everyone about {REGION}!",
    "There's my CHAMPION! {REGION} was something!",
    "{PLAYER}! Your picture from {REGION} is on my lab wall!");
TEXT_BANK(sBirchBackBadges,
    "{^BADGES} BADGES in {REGION}, {PLAYER}! Not bad at all.",
    "{PLAYER}! {^BADGES} BADGES! {REGION} won't forget you.",
    "Welcome back. {^BADGES} BADGES in {REGION}! I'm proud of you.");
TEXT_BANK(sBirchBackLost,
    "Welcome back, {PLAYER}. {REGION} was a tough land.",
    "{PLAYER}! {REGION} didn't go your way. It happens!",
    "Rested up, {PLAYER}? {REGION} was a hard start.",
    "Welcome back. Every TRAINER has a land like {REGION}.");
TEXT_BANK(sBirchBackLeft,
    "{PLAYER}! You left {REGION} behind? There's more to see!",
    "Back so soon? {REGION} will be there if you miss it.",
    "Done with {REGION}? Some lands just aren't for you.");
TEXT_BANK(sBirchBackFallen,
    "Not everyone came home from {REGION}. Keep going for them.",
    "Every TRAINER remembers the ones who didn't come home.",
    "I lit a candle for your POK@MON from {REGION}.");
TEXT_BANK(sBirchBackFriend,
    "{NAME} was a good friend to you. I won't forget.",
    "I still think about {NAME}, {PLAYER}.");
// When the next land is this one again (a land the player chose to play
// again).
TEXT_BANK(sBirchBackSame,
    "Back to {REGION} again? I've redrawn my map. Take a look!",
    "{REGION} once more! I'll dig out my old notes.");
TEXT_BANK(sBirchBackNew,
    "I've found another land for you. Take a look!",
    "And I've found a new land! Come and see.",
    "I've been mapping again. Look what I found!",
    "There's a land I want to show you. Here!");

// ---------------------------------------------------------------------------
// The epilogue: the run summary's first page.

TEXT_BANK(sBirchEndWon,
    "{PLAYER}! I heard the cheering all the way from {TOWN}!",
    "{REGION} has a new CHAMPION, and it's you! I knew it all along.",
    "You and {LEAD} did it! It's going in my notes in big letters.",
    "The CHAMPION of {REGION}! And you still look like the kid from my lab.");
TEXT_BANK(sBirchEndWonMore,
    "My kid {RIVAL} called. Grumpy, but proud of you.",
    "I'll need a whole new notebook for {REGION} now.",
    "{LEAD} looked so happy up there!");
// After a win that earned a POK@DEX upgrade (the NATIONAL DEX with the
// first, the GLOBAL DEX with the third), as Emerald's after the HALL OF FAME.
TEXT_BANK(sBirchEndDexNational,
    "Let me see your POK@DEX... There! It's a NATIONAL DEX now. Pick it in UNLOCKS.",
    "And your POK@DEX gets the NATIONAL upgrade! Pick it in UNLOCKS next time.");
TEXT_BANK(sBirchEndDexGlobal,
    "Three times! Your POK@DEX is a GLOBAL DEX now. Pick it in UNLOCKS.",
    "Let me upgrade your POK@DEX again... A GLOBAL DEX! Pick it in UNLOCKS.");
TEXT_BANK(sBirchEndLost,
    "{PLAYER}... I'm just glad you're safe. Come on home.",
    "{REGION} was a hard road. You walked it with your head up.",
    "It's all right, {PLAYER}. Everyone has days like this.");
TEXT_BANK(sBirchEndLostWhere,
    "You made it to {PLACE}. That's further than you know.",
    "I heard about {DUNGEON}. You didn't face it alone.",
    "{^BADGES} BADGES, {PLAYER}! Most people never earn one.");
// The same with one POK@MON lost.
// A loss in the LEAGUE: how far it got.
TEXT_BANK(sBirchEndElite,
    "The ELITE FOUR, {PLAYER}! Most TRAINERS never see the inside of that hall.",
    "You walked into the LEAGUE itself. Not many ever do.");
TEXT_BANK(sBirchEndChampion,
    "You stood in front of the CHAMPION, {PLAYER}! Hardly anyone ever gets that far.",
    "All four of the ELITE FOUR, and then the CHAMPION! I'm so proud I could burst.");
TEXT_BANK(sBirchEndWonFallen1,
    "One friend didn't make it to the top. They're part of this, too.",
    "One friend didn't make it this far. They'd be so proud.");
TEXT_BANK(sBirchBackFallen1,
    "Not every POK@MON came home from {REGION}. Keep going for them.",
    "I lit a candle for the POK@MON you lost in {REGION}.");
TEXT_BANK(sBirchEndWonFallen,
    "Not every friend made it to the top. They're part of this, too.",
    "Some friends didn't make it this far. They'd be so proud.");
TEXT_BANK(sBirchEndFallen,
    "Your POK@MON gave everything for you. Remember them.",
    "Your team stood by you to the end. That's what friends do.");
TEXT_BANK(sBirchEndFriend,
    "{NAME} was with you to the very end. That means something.");
TEXT_BANK(sBirchEndClose,
    "Rest up. I'll find us another land soon!",
    "Come by the lab when you're ready. I'll have a new map.",
    "There's always another land. I'll start packing.");
TEXT_BANK(sBirchEndCloseShort,
    "Rest up, all right?",
    "Come home soon.",
    "I'll be at the lab.");

// ---------------------------------------------------------------------------
// The HALL OF FAME: the CHAMPION walks the player in, in the wanderer's
// voice (their look, RLOOK_*).

TEXT_BANK(sHallSteven,
    "This room is older than any stone in {REGION}. Well, almost.|Let's record you and the POK@MON who climbed with you.",
    "I've walked every cave in {REGION}. Nothing in them shines like this room.|Come. Your team belongs on these walls.");
TEXT_BANK(sHallWallace,
    "Look at them, {PLAYER}. Every one of your POK@MON shines here.|This room remembers every team that reached the top. Now it remembers yours.",
    "Such a team! I could watch them for hours.|Come, let's give them the place they've earned.");
TEXT_BANK(sHallAce,
    "I've trained my whole life to stand in this room. I never thought I'd share it.|Come on. Let's put your team on the wall!",
    "You know what I like about you? You never stopped training.|This room is for teams like yours. Let's go.");
TEXT_BANK(sHallVeteran,
    "When I first came up here, the paint was still wet.|Every team that ever won is remembered here. Yours is next.",
    "I've walked every road in {REGION}, and every one led here in the end.|Come, young one. Let's remember your team.");
static const struct RogueTextBank *const sHallBanks[RLOOK_COUNT] =
{
    [RLOOK_STEVEN] = &sHallSteven, [RLOOK_WALLACE] = &sHallWallace, [RLOOK_ACE] = &sHallAce, [RLOOK_VETERAN] = &sHallVeteran,
};

// ---------------------------------------------------------------------------
// Saying it

// A letter of a name: names in caps (ROUTE 15, POK@ BALL, STONE DAY) are
// kept on one line where the line can be broken elsewhere.
static bool8 BirchNameChar(char ch)
{
    return (ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9') || ch == '@';
}

// RogueText_Wrap, but a break inside a name in caps is the last resort.
// Returns FALSE if a word is too long.
static bool8 BirchWrap(char *s, u32 width)
{
    u32 col = 0, line = 0;
    char *space = NULL, *soft = NULL;
    bool8 ok = TRUE;
    for (; *s; s++)
    {
        if (*s == '|' || *s == '\f')
        {
            *s = '\f';
            col = line = 0;
            space = soft = NULL;
            continue;
        }
        if (*s == '\n' || *s == '\v')
        {
            col = 0;
            line = 1;
            space = soft = NULL;
            continue;
        }
        if (*s == ' ')
        {
            space = s;
            if (col == 0 || !BirchNameChar(s[-1]) || !BirchNameChar(s[1]))
                soft = s;
        }
        if (++col > width)
        {
            char *at = soft ? soft : space;
            if (at == NULL)
            {
                ok = FALSE;
                continue;
            }
            *at = line == 0 ? '\n' : '\v';
            line = 1;
            col = s - at;
            space = soft = NULL;
            // A space passed since the break can break the next line.
            {
                char *t;
                for (t = at + 1; t < s; t++)
                    if (*t == ' ')
                    {
                        space = t;
                        if (!BirchNameChar(t[-1]) || !BirchNameChar(t[1]))
                            soft = t;
                    }
            }
        }
    }
    return ok;
}

// Fills and wraps t at out (size bytes) with BirchWrap. Returns the length,
// 0 if a slot could not be filled.
static u32 BirchSay(char *out, u32 size, const char *t, struct RogueTextCtx *c, u32 width)
{
    u8 missing = c->missing;
    u32 n = RogueText_Fill(out, size, t, c);
    BirchWrap(out, width);
    if (c->missing != missing)
    {
        c->missing = missing;
        return 0;
    }
    return n;
}

// Says t as a new box after the n characters already in out. Returns the
// new length (the same if t is NULL or cannot be said).
static u32 BirchBox(char *out, u32 size, u32 n, const char *t, struct RogueTextCtx *c)
{
    u32 len;
    if (t == NULL || n + 4 >= size)
        return n;
    if (n > 0)
        out[n++] = '\f';
    len = BirchSay(out + n, size - n, t, c, TEXT_LINE);
    if (len == 0)
    {
        if (n > 0)
            n--;
        out[n] = 0;
        return n;
    }
    return n + len;
}

// Whether he mentions the POK@MON lost on a journey: only if it lost any,
// about one time in three, as the run count turns.
static bool8 BirchSpeaksOfFallen(const struct RogueRegion *r, const struct RogueBirchMemory *m, u32 salt)
{
    return m->fallen > 0 && RogueRng_Hash(r->seed, BIRCH_SALT + salt + m->runs * 7) % 3 == 0;
}

// A context for his lines about a journey: where it ended, its badges, its
// fallen friend. st holds the state the slots read.
static void BirchMemoryCtx(struct RogueTextCtx *c, struct RogueTextState *st, const struct RogueBirchMemory *m)
{
    const struct RogueRegion *r = c->r;
    u8 place = m->place < r->nPlaces ? m->place : r->start;
    memset(st, 0, sizeof(*st));
    st->badges = m->badges;
    st->fallen = m->fallen;
    st->runs = m->runs;
    st->flags = RTSTATE_STARTER;
    RogueText_Init(c, r, place, RSPK_BIRCH);
    c->state = st;
    c->dungeon = m->dungeon < r->nDungeons ? m->dungeon : 0xFF;
    c->name = m->friendName;
}

// Lines of a wrapped text.
static u32 BirchLines(const char *s)
{
    u32 n = *s ? 1 : 0;
    for (; *s; s++)
        n += *s == '\n' || *s == '\v' || *s == '\f';
    return n;
}

// The map's strip shows two lines at a time and does not scroll: a box of
// three lines is wrapped again, narrower, into four, and every two lines
// make a box; wider, the strip may take a line a few letters longer than a
// message box. The welcome back goes on by itself, so it does not scroll
// either.
static void BirchStrip(char *out, bool8 wider)
{
    char *box = out, *end, *s;
    while (*box)
    {
        u32 lines = 1, w, k = 0;
        for (end = box; *end && *end != '\f'; end++)
            lines += *end == '\n' || *end == '\v';
        if (lines == 3)
        {
            // A little wider first (the strip is wider than a message box),
            // else narrower, into four.
            static const u8 widths[] = { 31, 32, 33, 29, 28, 27, 26, 25, 24, 23, 22, 21, 20, 19, 18 };
            u32 from = wider ? 0 : 3;
            char save = *end;
            *end = 0;
            for (w = from; w < LORE_COUNT_OF(widths); w++)
            {
                for (s = box; *s; s++)
                    if (*s == '\n' || *s == '\v')
                        *s = ' ';
                BirchWrap(box, widths[w]);
                lines = BirchLines(box);
                if (lines == (widths[w] > TEXT_LINE ? 2 : 4))
                    break;
            }
            *end = save;
        }
        for (s = box; s < end; s++)
            if (*s == '\n' || *s == '\v')
                *s = (++k % 2) ? '\n' : '\f';
        box = *end ? end + 1 : end;
    }
}

u32 RogueText_BirchRegion(struct RogueTextCtx *c, u8 mode, char *out, u32 size)
{
    u8 first, second, type;
    u32 n = 0, pick = RogueRng_Hash(c->r->seed, BIRCH_SALT);
    char rumor[80];
    char *s;

    out[0] = 0;
    RogueGen_RegionTraits(c->r, &first, &second);
    if (mode != RBIRCH_DESC_LAB)
        n = BirchBox(out, size, n, RogueText_PickRegion(sBirchLand[first % RTRAIT_COUNT], c, 0), c);
    if (mode == RBIRCH_DESC_MAP_FIRST)
        n = BirchBox(out, size, n, RogueText_PickRegion(sBirchMore[second % RTRAIT_COUNT], c, 0), c);
    n = BirchBox(out, size, n, RogueText_PickRegion(&sBirchRegionFacts, c, 0), c);
    type = RogueGen_GymLeader(c->r, 0)->type % RTYPE_COUNT;
    s = LoreCopy(rumor, sBirchRumorStart[pick & 1]);
    LoreCopy(s, sBirchRumors[type][(pick >> 1) & 1]);
    n = BirchBox(out, size, n, rumor, c);
    if (mode != RBIRCH_DESC_LAB)
        BirchStrip(out, TRUE);
    return n;
}

u32 RogueText_BirchWelcomeBack(struct RogueTextCtx *c, const struct RogueBirchMemory *m, char *out, u32 size)
{
    const struct RogueRegion *r = c->r;
    const struct RogueTextBank *banks[2];
    struct RogueTextState st;
    u32 n = 0, nb = 0, k = m->runs;

    out[0] = 0;
    BirchMemoryCtx(c, &st, m);
    switch (m->outcome)
    {
    case RBIRCH_WON:
        banks[nb++] = &sBirchBackWon;
        break;
    case RBIRCH_LOST:
        // How far, by its BADGES: where it ended would need the old region
        // planned again, seconds of black screen.
        if (m->badges >= 2)
            banks[nb++] = &sBirchBackBadges;
        banks[nb++] = &sBirchBackLost;
        break;
    case RBIRCH_LEFT:
        banks[nb++] = &sBirchBackLeft;
        break;
    }
    if (nb)
    {
        u32 i = RogueRng_Hash(r->seed, BIRCH_SALT + 1 + k) % nb, j;
        for (j = 0; j < nb && n == 0; j++)
            n = BirchBox(out, size, n, RogueText_PickRegion(banks[(i + j) % nb], c, k), c);
    }
    if (BirchSpeaksOfFallen(r, m, 2))
        n = BirchBox(out, size, n, RogueText_PickRegion(m->friendName ? &sBirchBackFriend : m->fallen == 1 ? &sBirchBackFallen1 : &sBirchBackFallen,
                                                        c, k), c);
    // Once, after the third win, the word slips (RUN 0's teaser).
    if (m->endingLine != NULL && !m->same)
        n = BirchBox(out, size, n, m->endingLine, c);
    else if (m->given && !m->same)
        n = BirchBox(out, size, n, "I've been given a new land. Take a look!", c);
    else
        n = BirchBox(out, size, n, RogueText_PickRegion(m->same ? &sBirchBackSame : &sBirchBackNew, c, k), c);
    BirchStrip(out, FALSE);
    return n;
}

u32 RogueText_BirchLab(struct RogueTextCtx *c, u8 what, char *out, u32 size)
{
    const struct RogueTextBank *b = NULL;
    u32 runs = c->state ? c->state->runs : 1;
    u32 badges = c->state ? c->state->badges : 0;

    out[0] = 0;
    switch (what)
    {
    case RBIRCH_LAB_WELCOME:  b = runs <= 1 ? &sBirchLabFirst : &sBirchLabBack; break;
    case RBIRCH_LAB_CHOOSE:   b = &sBirchChoose; break;
    case RBIRCH_LAB_GOODLUCK: b = runs <= 2 ? &sBirchGoodLuckTip : &sBirchGoodLuck; break;
    case RBIRCH_LAB_NEWS:     b = &sBirchNews; break;
    case RBIRCH_LAB_AFTER:
        c->gym = 0;
        b = badges == 0 ? &sBirchFirstGym : &sBirchAfterBadge;
        runs += badges;
        break;
    default:
        return 0;
    }
    return BirchBox(out, size, 0, RogueText_PickRegion(b, c, runs), c);
}

// Fills t at out, after a space if out is past start. Returns the end.
static char *BirchPart(char *start, char *out, char *end, const char *t, struct RogueTextCtx *c)
{
    u8 missing = c->missing;
    char *e;
    if (t == NULL || out + 2 >= end)
        return out;
    if (out > start)
        *out++ = ' ';
    e = out + RogueText_Fill(out, end - out, t, c);
    if (c->missing != missing)
    {
        c->missing = missing;
        if (out > start)
            out--;
        *out = 0;
        return out;
    }
    return e;
}

#define BIRCH_EPILOGUE_LINES 6

// The length of a wrapped text's last line.
static u32 BirchLastLine(const char *s)
{
    u32 n = 0;
    for (; *s; s++)
        n = (*s == '\n' || *s == '\v' || *s == '\f') ? 0 : n + 1;
    return n;
}

// A wrapped text back to one run of words.
static void BirchUnwrap(char *s)
{
    for (; *s; s++)
        if (*s == '\n' || *s == '\v' || *s == '\f')
            *s = ' ';
}

u32 RogueText_BirchEpilogue(struct RogueTextCtx *c, const struct RogueBirchMemory *m, u32 width, char *out, u32 size)
{
    const struct RogueRegion *r = c->r;
    struct RogueTextState st;
    const char *open, *middle = NULL, *close;
    u32 k = m->runs, tries;
    char *e, *s;

    BirchMemoryCtx(c, &st, m);
    // {TOWN} is the first town, where the lab is.
    c->town = r->start;
    if (m->outcome == RBIRCH_WON)
    {
        open = RogueText_PickRegion(&sBirchEndWon, c, k);
        if (m->dex)
            middle = RogueText_PickRegion(m->dex == 2 ? &sBirchEndDexGlobal : &sBirchEndDexNational, c, k);
        else if (BirchSpeaksOfFallen(r, m, 3))
            middle = RogueText_PickRegion(m->fallen == 1 ? &sBirchEndWonFallen1 : &sBirchEndWonFallen, c, k);
        else
            middle = RogueText_PickRegion(&sBirchEndWonMore, c, k);
    }
    else
    {
        open = RogueText_PickRegion(&sBirchEndLost, c, k);
        if (m->league)
            middle = RogueText_PickRegion(m->league == 5 ? &sBirchEndChampion : &sBirchEndElite, c, k);
        else if (BirchSpeaksOfFallen(r, m, 3))
            middle = RogueText_PickRegion(m->friendName ? &sBirchEndFriend : &sBirchEndFallen, c, k);
        else if (c->dungeon != 0xFF)
            middle = sBirchEndLostWhere_Lines[1];
        else if (m->badges >= 2 && (k & 1))
            middle = sBirchEndLostWhere_Lines[2];
        else if (m->place < r->nPlaces && r->places[m->place].kind != RPLACE_TOWN)
            middle = sBirchEndLostWhere_Lines[0];
        else if (m->badges >= 2)
            middle = sBirchEndLostWhere_Lines[2];
    }
    // If the whole does not fit the page: a short goodbye, then none, then
    // without the middle.
    for (tries = 0; tries < 4; tries++)
    {
        close = tries == 0 ? RogueText_PickRegion(&sBirchEndClose, c, k) : tries == 1 ? sBirchEndCloseShort_Lines[k % LORE_COUNT_OF(sBirchEndCloseShort_Lines)] : NULL;
        // (The POK@DEX upgrade is never left out.)
        if (tries == 3)
            middle = m->dex ? middle : NULL, close = m->dex ? NULL : RogueText_PickRegion(&sBirchEndClose, c, k);
        e = BirchPart(out, out, out + size - 1, open, c);
        e = BirchPart(out, e, out + size - 1, middle, c);
        e = BirchPart(out, e, out + size - 1, close, c);
        *e = 0;
        BirchWrap(out, width);
        if (BirchLines(out) <= BIRCH_EPILOGUE_LINES)
            break;
    }
    // No word left alone on the last line: a little narrower, if it still
    // fits the page, carries more down to it.
    if (BirchLastLine(out) < 8)
    {
        u32 w, best = width, bestLen = BirchLastLine(out);
        for (w = width - 1; w + 6 >= width; w--)
        {
            BirchUnwrap(out);
            BirchWrap(out, w);
            if (BirchLines(out) <= BIRCH_EPILOGUE_LINES && BirchLastLine(out) > bestLen)
                best = w, bestLen = BirchLastLine(out);
        }
        BirchUnwrap(out);
        BirchWrap(out, best);
    }
    for (s = out; *s; s++)
        if (*s == '\v' || *s == '\f')
            *s = '\n';
    return e - out;
}

u32 RogueText_HallOfFame(struct RogueTextCtx *c, char *out, u32 size)
{
    struct RogueCast cast;
    out[0] = 0;
    RogueGen_Cast(c->r, &cast);
    return BirchBox(out, size, 0, RogueText_PickRegion(sHallBanks[cast.wandererLook % RLOOK_COUNT], c, 0), c);
}

// ---------------------------------------------------------------------------
// The dump: every line, for each journey it changes with. The welcome back
// is dumped as if the last journey had been in this very region (a player
// can go back to it by its name).

#ifdef ROGUE_HOST
static void BirchDump(struct TextDump *d, const char *who, const char *bank, const char *text)
{
    TextDump_Line(d, d->r->start, RSPK_BIRCH, who, bank, -1, text);
}

// The epilogue as the summary page shows it, two lines to a box for the checks.
static void BirchDumpEpilogue(struct TextDump *d, const char *who, const struct RogueBirchMemory *m)
{
    struct RogueTextCtx c;
    char out[600], *s;
    u32 line = 0;
    RogueText_Init(&c, d->r, d->r->start, RSPK_BIRCH);
    RogueText_BirchEpilogue(&c, m, BIRCH_EPILOGUE_WIDTH, out, sizeof(out));
    for (s = out; *s; s++)
        if (*s == '\n' && ++line % 2 == 0)
            *s = '\f';
    BirchDump(d, who, "sBirchEnd", out);
}

// A place the journey could have ended on: the route before the gym town
// of the badges won (or the first route), and a dungeon with a boss.
static u8 BirchDumpRoute(const struct RogueRegion *r, u8 badges)
{
    u8 gym = r->gymTown[badges < ROGUE_NUM_GYMS ? badges : ROGUE_NUM_GYMS - 1], i;
    for (i = gym; i > 0; i--)
        if (r->places[i].kind == RPLACE_ROUTE && !r->places[i].side)
            return i;
    return r->start;
}

static void TextDump_Birch(struct TextDump *d)
{
    const struct RogueRegion *r = d->r;
    struct RogueTextCtx c;
    struct RogueTextState st;
    struct RogueBirchMemory m;
    char out[600], who[96], friendName[24];
    u32 i;
    s32 boss = -1;

    for (i = 0; i < r->nDungeons; i++)
        if (r->dungeons[i].boss != RBOSS_NONE && r->dungeons[i].kind != RDUN_VICTORY && boss < 0)
            boss = i;
    if (RogueText_Runtime(TS_LEAD, 0, 0, friendName) == NULL)
        LoreCopy(friendName, "MARSHTOMP");

    if (d->state == NULL)
    {
        // His map in the speech, and the lab when the map did not show it.
        static const char *const modes[] = { "lab region hints", "map", "map, first journey" };
        for (i = 0; i < 3; i++)
        {
            RogueText_Init(&c, r, r->start, RSPK_BIRCH);
            RogueText_BirchRegion(&c, i, out, sizeof(out));
            BirchDump(d, modes[i], "sBirchRegion", out);
        }
        // The welcome back, after each way a journey ends, over a few runs.
        for (i = 2; i <= 6; i++)
        {
            static const char *const ends[] = { "-", "lost, two BADGES", "won", "left", "lost early", "lost, fallen friend" };
            u32 e = i - 1;
            memset(&m, 0, sizeof(m));
            m.runs = i;
            m.place = 0xFF;
            m.dungeon = 0xFF;
            m.fallen = 2 + i % 3;
            switch (e)
            {
            case 1: m.outcome = RBIRCH_LOST; m.badges = 2; break;
            case 2: m.outcome = RBIRCH_WON; m.badges = 8; m.fallen = 1; break;
            case 3: m.outcome = RBIRCH_LEFT; m.badges = 1; m.fallen = 0; break;
            case 4: m.outcome = RBIRCH_LOST; m.badges = 0; break;
            case 5: m.outcome = RBIRCH_LOST; m.badges = 5; m.friendName = friendName; break;
            }
            for (m.runs = i; m.runs < i + 3; m.runs++)
            {
                RogueText_Init(&c, r, r->start, RSPK_BIRCH);
                RogueText_BirchWelcomeBack(&c, &m, out, sizeof(out));
                snprintf(who, sizeof(who), "welcome back, %s (run %u, %u fallen)", ends[e], m.runs, m.fallen);
                BirchDump(d, who, "sBirchBack", out);
            }
        }
        // The lab on arrival, the first journey and a later one.
        for (i = 1; i <= 5; i += 4)
        {
            u32 w;
            memset(&st, 0, sizeof(st));
            st.runs = i;
            for (w = RBIRCH_LAB_WELCOME; w <= RBIRCH_LAB_NEWS; w++)
            {
                static const char *const whats[] = { "lab welcome", "lab choose", "lab good luck", "-", "lab news" };
                if (w == RBIRCH_LAB_AFTER || (w == RBIRCH_LAB_NEWS && i == 1))
                    continue;
                RogueText_Init(&c, r, r->start, RSPK_BIRCH);
                st.flags = w == RBIRCH_LAB_GOODLUCK ? RTSTATE_STARTER : 0;
                c.state = &st;
                RogueText_BirchLab(&c, w, out, sizeof(out));
                snprintf(who, sizeof(who), "%s (run %u)", whats[w], i);
                BirchDump(d, who, "sBirchLab", out);
            }
        }
        return;
    }

    // Talked to again in the lab, as the run goes.
    st = *d->state;
    st.runs = 3;
    RogueText_Init(&c, r, r->start, RSPK_BIRCH);
    c.state = &st;
    RogueText_BirchLab(&c, RBIRCH_LAB_AFTER, out, sizeof(out));
    BirchDump(d, "lab, talked to again", "sBirchLab", out);

    // The epilogue after a loss here (and a win, late).
    memset(&m, 0, sizeof(m));
    m.dungeon = 0xFF;
    m.outcome = RBIRCH_LOST;
    m.badges = d->state->badges;
    m.place = BirchDumpRoute(r, m.badges);
    for (m.runs = 1; m.runs <= 3; m.runs++)
    {
        m.fallen = m.runs;
        m.friendName = m.badges >= 4 ? friendName : NULL;
        snprintf(who, sizeof(who), "epilogue, lost on a route (run %u)", m.runs);
        BirchDumpEpilogue(d, who, &m);
    }
    if (d->state->badges >= ROGUE_NUM_GYMS)
    {
        if (boss >= 0)
        {
            m.place = 0xFF;
            m.dungeon = boss;
            m.runs = 4;
            BirchDumpEpilogue(d, "epilogue, lost in a dungeon", &m);
        }
        // Lost in the LEAGUE: to an ELITE FOUR member, to the CHAMPION.
        m.place = r->summit;
        m.dungeon = 0xFF;
        m.runs = 5;
        m.league = 3;
        BirchDumpEpilogue(d, "epilogue, lost to the ELITE FOUR", &m);
        m.league = 5;
        BirchDumpEpilogue(d, "epilogue, lost to the CHAMPION", &m);
        memset(&m, 0, sizeof(m));
        m.outcome = RBIRCH_WON;
        m.badges = 8;
        m.place = r->nPlaces - 1;
        m.dungeon = 0xFF;
        for (m.runs = 1; m.runs <= 3; m.runs++)
        {
            m.fallen = m.runs - 1;
            snprintf(who, sizeof(who), "epilogue, won (run %u, %u fallen)", m.runs, m.fallen);
            BirchDumpEpilogue(d, who, &m);
        }
        // A win that upgrades the POK@DEX.
        for (m.dex = 1; m.dex <= 2; m.dex++)
        {
            for (m.runs = 1; m.runs <= 2; m.runs++)
            {
                snprintf(who, sizeof(who), "epilogue, won, %s DEX (run %u)", m.dex == 2 ? "GLOBAL" : "NATIONAL", m.runs);
                BirchDumpEpilogue(d, who, &m);
            }
        }
        m.dex = 0;
        RogueText_Init(&c, r, r->nPlaces - 1, RSPK_CHAMPION);
        c.state = d->state;
        RogueText_HallOfFame(&c, out, sizeof(out));
        TextDump_Line(d, r->nPlaces - 1, RSPK_CHAMPION, "hall of fame", "sHall", -1, out);
    }
}
#endif
