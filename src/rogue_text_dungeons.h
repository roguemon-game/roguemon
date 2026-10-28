// The dungeons domain: what is said and read in and about the region's
// dungeons, and the new legend lore ways' homes. Included at the end of
// src/rogue_gen_text.h. The game asks here (src/rogue_dungeon.c,
// src/rogue_town.c) and converts the result; gen_host --script prints it all
// (TextDump_Dungeons).
//
// - Signs by a dungeon's door in town and by the woods (RogueText_DungeonSign).
// - Guardians showing themselves and beaten, by the dungeon's kind
//   (RogueText_GuardianAppears, RogueText_BossGone).
// - Masters: a set of intro, defeat and after lines each, by kind
//   (RogueText_MasterLine).
// - The choice room's keeper and the balls that roll away
//   (RogueText_GuideLine, RogueText_ChoiceGone).
// - Camps: each camper keeps one voice, offer to goodbye, and the region's
//   campers each have their own (RogueText_CampLine).
// - The lost explorer ({EXPLORER}, the cast's), asking by the kind of place,
//   and thanking the player later in the POKéMON CENTER of the town the
//   dungeon is named after (RogueText_ExplorerLine).
// - The prize ball's line, by kind, with a diary page under it where the
//   lore puts one (RogueText_PrizeLine, LORE_WAY_DIARY).
// - Things to read on a floor (RogueGen_FloorThingAt, RogueText_FloorThing):
//   a tower's graves (Mt. Pyre's) with Pokémon epitaphs and, on one floor,
//   the old keeper's grave (LORE_WAY_GRAVE); a shipwreck's waste bins with
//   pages of the ship's log; a power plant's terminals and its generator.
// - Rumors of the dungeons, spread over the three nearest a town, not only
//   the nearest (RogueText_RumorDungeon, RogueText_DungeonRumor).
// - Paintings: in homes whose room has one, and the picture on a POKéMON
//   CENTER's wall (RogueText_Painting); a child's skipping rhyme in the
//   POKéMON CENTER (LORE_WAY_RHYME); museum exhibits, for the town
//   signatures' museum to call (RogueText_MuseumExhibit), with the legend's
//   exhibit where LORE_WAY_MUSEUM puts it, a plaque under the POKéMON
//   CENTER's picture until the town has a museum.
//
// Lines follow the game's style: one idea a box, names from slots, no
// mechanics talk but the odd in-character tip.

// ---------------------------------------------------------------------------
// Helpers

// How many dungeons of its kind come before a dungeon: each one of a kind
// takes the next line of its bank.
static u32 DunOrdinal(const struct RogueRegion *r, u8 dungeon)
{
    u32 i, n = 0;
    for (i = 0; i < dungeon && i < r->nDungeons; i++)
        n += r->dungeons[i].kind == r->dungeons[dungeon].kind;
    return n;
}

static u8 DunKind(const struct RogueTextCtx *c)
{
    return c->dungeon < c->r->nDungeons ? c->r->dungeons[c->dungeon].kind : RDUN_CAVE;
}

// A bank by kind, or the cave's where a kind has none of its own.
static const struct RogueTextBank *DunBank(const struct RogueTextBank *const *banks, u8 kind)
{
    return kind < RDUN_COUNT && banks[kind] != NULL ? banks[kind] : banks[RDUN_CAVE];
}

// A line of a bank for this dungeon: each dungeon of a kind takes its own.
static u32 DunSay(struct RogueTextCtx *c, const struct RogueTextBank *b, u32 k, char *out, u32 size)
{
    const char *t = RogueText_PickRegion(b, c, k);
    if (t == NULL)
    {
        out[0] = 0;
        return 0;
    }
    return RogueText_Say(out, size, t, c);
}

// Sets of lines said by one person (intro, defeat, after; offer, done...):
// rows of a table, a row a person, every line of the row fillable here.
struct DunSets
{
    const char *name;
    const char *const *cells;
    u8 rows, cols;
};
#define DUN_SETS(id, cols, ...) \
    static const char *const id##_Cells[][cols] = { __VA_ARGS__ }; \
    static const struct DunSets id = { #id, &id##_Cells[0][0], sizeof(id##_Cells) / sizeof(id##_Cells[0]), cols }

static u32 DunSetsKey(const struct DunSets *s)
{
    const char *n = s->name;
    u32 h = 43000;
    while (*n)
        h = h * 31 + (u8)*n++;
    return h;
}

// The row the k-th person of this kind in the region speaks from.
static u32 DunRow(struct RogueTextCtx *c, const struct DunSets *s, u32 k)
{
    u32 i, j, row = 0;
    for (i = 0; i < s->rows; i++)
    {
        row = RogueText_Deal(c->r->seed, DunSetsKey(s), k + i, s->rows);
        for (j = 0; j < s->cols; j++)
            if (!RogueText_CanFill(s->cells[row * s->cols + j], c))
                break;
        if (j == s->cols)
            return row;
    }
    return row;
}

static u32 DunSaySet(struct RogueTextCtx *c, const struct DunSets *s, u32 k, u8 col, char *out, u32 size)
{
    u32 row = DunRow(c, s, k);
    return RogueText_Say(out, size, s->cells[row * s->cols + (col < s->cols ? col : 0)], c);
}

static const struct DunSets *DunSetsOf(const struct DunSets *const *sets, u8 kind)
{
    return kind < RDUN_COUNT && sets[kind] != NULL ? sets[kind] : sets[RDUN_CAVE];
}

// ---------------------------------------------------------------------------
// Signs

// Under the name on the sign by a dungeon's door in town (a tower, the
// volcano, an ice cave), and by a route's way into the woods: Emerald's
// sign voice, short and official. The game adds the curly quotes.
TEXT_BANK(sSignTower,
    "A master waits at the top.",
    "Quiet, please. Spirits rest.",
    "Climb gently. Remember them.",
    "Open from dawn until dusk.",
    "Please mind the candles.");
TEXT_BANK(sSignVolcano,
    "Danger! Hot ash and lava!",
    "Keep out! The rocks are hot.",
    "Turn back if boots smoke!",
    "Hikers, please carry water.");
TEXT_BANK(sSignIce,
    "Caution! Ice floors inside.",
    "Slippery floors! Take care.",
    "Mind the ice. It's slippery.",
    "Dress warmly, travelers.");
TEXT_BANK(sSignOther,
    "Enter at your own risk.",
    "Please take an ESCAPE ROPE.",
    "Only the brave go in.");
TEXT_BANK(sSignForest,
    "Keep to the trail!",
    "Watch for BUG POK@MON!",
    "Mind the roots. They trip.",
    "Thick woods. Stay together.",
    "Lost? Follow the light.");
TEXT_BANK(sSignForestRoad,
    "The trail leads to the road.",
    "Through the woods, then on!",
    "Follow the trail across.",
    "This way to the far side.");

static const struct RogueTextBank *const sSignBanks[RDUN_COUNT] =
{
    [RDUN_CAVE] = &sSignOther, [RDUN_TOWER] = &sSignTower, [RDUN_VOLCANO] = &sSignVolcano, [RDUN_ICE] = &sSignIce,
    [RDUN_FOREST] = &sSignForest,
};

// The motto under a dungeon sign's name (c->dungeon; c->number 1 for woods
// the road goes through), one line. Returns its length.
u32 RogueText_DungeonSign(struct RogueTextCtx *c, char *out, u32 size)
{
    u8 kind = DunKind(c);
    const struct RogueTextBank *b = kind == RDUN_FOREST && c->number ? &sSignForestRoad : DunBank(sSignBanks, kind);
    const char *t = RogueText_PickRegion(b, c, c->dungeon < c->r->nDungeons ? DunOrdinal(c->r, c->dungeon) : 0);
    if (t == NULL)
        t = b->lines[0];
    return RogueText_Fill(out, size, t, c);
}

// ---------------------------------------------------------------------------
// Guardians ({NAME} is the species)

TEXT_BANK(sGuardianCave,
    "The {NAME} glares down at you!",
    "A low growl echoes off the rock... The {NAME} blocks the way!",
    "Pebbles rattle down the wall. The {NAME} was waiting in the dark!",
    "This cave belongs to the {NAME}. It won't let you pass!",
    "Something big shifts in the dark... It's the {NAME}!");
TEXT_BANK(sGuardianVictory,
    "The {NAME} blocks the way!",
    "The {NAME} stands guard. Only the strong go on!",
    "The {NAME} won't let just anyone reach the LEAGUE!");
TEXT_BANK(sGuardianForest,
    "The {NAME} bursts out of the undergrowth!",
    "The leaves part... The {NAME} has been watching you!",
    "Birds scatter from the trees. The {NAME} is here!",
    "The {NAME} rises from its bed of moss, ready to fight!");
TEXT_BANK(sGuardianTower,
    "The {NAME} looms out of the gloom!",
    "The candles flicker... The {NAME} drifts out of the mist!",
    "A cold touch on your shoulder... It's the {NAME}!",
    "The {NAME} guards the ones who rest here!");
TEXT_BANK(sGuardianHideout,
    "The {NAME} lunges at you!");
TEXT_BANK(sGuardianLair,
    "The {NAME} glares down at you!",
    "The {NAME} guards the way deeper in!",
    "The {NAME} bares its teeth. Nobody goes further!");
TEXT_BANK(sGuardianRuins,
    "The ancient {NAME} stirs from its long sleep!",
    "Dust falls from the ceiling. The ancient {NAME} opens its eyes!",
    "The {NAME} has kept this vault for ages. It wakes!");
TEXT_BANK(sGuardianShip,
    "A chill... The {NAME} rises out of the dark!",
    "The cabin door creaks shut behind you. The {NAME} was waiting!",
    "Something moans in the hold... The {NAME} appears!");
TEXT_BANK(sGuardianPlant,
    "The {NAME} crackles by the generator!",
    "The lights flicker and die... The {NAME} sparks to life!",
    "Bzzt! The {NAME} has been feeding on the generator!");
TEXT_BANK(sGuardianIce,
    "The {NAME} looms out of the freezing mist!",
    "The ice groans... The {NAME} slides out to meet you!",
    "A blast of cold air! The {NAME} won't let you pass!");
TEXT_BANK(sGuardianMirage,
    "Sand pours down the walls... The {NAME} rises out of it!",
    "The {NAME} has made the top of the tower its den!",
    "The wind howls outside. The {NAME} stands over the fossils!");
TEXT_BANK(sGuardianVolcano,
    "The {NAME} rises from the heat by the lava!",
    "The rock glows red... The {NAME} climbs out of the heat!",
    "Ash swirls in the hot air. The {NAME} is waiting!");

static const struct RogueTextBank *const sGuardianBanks[RDUN_COUNT] =
{
    [RDUN_CAVE] = &sGuardianCave, [RDUN_VICTORY] = &sGuardianVictory, [RDUN_FOREST] = &sGuardianForest,
    [RDUN_TOWER] = &sGuardianTower, [RDUN_HIDEOUT] = &sGuardianHideout, [RDUN_LAIR] = &sGuardianLair,
    [RDUN_RUINS] = &sGuardianRuins, [RDUN_SHIP] = &sGuardianShip, [RDUN_PLANT] = &sGuardianPlant,
    [RDUN_ICE] = &sGuardianIce, [RDUN_VOLCANO] = &sGuardianVolcano, [RDUN_MIRAGE] = &sGuardianMirage,
};

// The guardian of c->dungeon shows itself (c->name its species).
u32 RogueText_GuardianAppears(struct RogueTextCtx *c, char *out, u32 size)
{
    return DunSay(c, DunBank(sGuardianBanks, DunKind(c)), DunOrdinal(c->r, c->dungeon), out, size);
}

TEXT_BANK(sGoneCave,
    "The {NAME} is beaten. The way is clear.",
    "The {NAME} retreats into the dark. The way is clear.",
    "The {NAME} lumbers off into a side tunnel.");
TEXT_BANK(sGoneForest,
    "The {NAME} limps off into the trees. The way is clear.",
    "The {NAME} disappears into the undergrowth.");
TEXT_BANK(sGoneTower,
    "The {NAME} fades back into the mist...",
    "The {NAME} drifts away. The candles burn steady again.");
TEXT_BANK(sGoneRuins,
    "The {NAME} settles back into stillness. The vault is quiet.",
    "The {NAME} crumbles to dust... no, it just went back to sleep.");
TEXT_BANK(sGoneShip,
    "The {NAME} sinks through the floor and is gone.",
    "The {NAME} slips out through a porthole. The cabin is still.");
TEXT_BANK(sGonePlant,
    "The {NAME} fizzles out. The generator hums quietly.",
    "The {NAME} zips off into the cables. The lights steady.");
TEXT_BANK(sGoneIce,
    "The {NAME} slides away across the ice.",
    "The {NAME} vanishes into the freezing mist.");
TEXT_BANK(sGoneVolcano,
    "The {NAME} sinks back into the heat.",
    "The {NAME} slinks off toward the lava. The way is clear.");
TEXT_BANK(sGoneMirage,
    "The {NAME} burrows into the sand and is gone.",
    "The {NAME} slips out into the sandstorm.");
TEXT_BANK(sGoneLegend,
    "The {NAME} vanished into thin air...",
    "In a blink, the {NAME} was gone...",
    "The {NAME} faded like a dream...",
    "The {NAME} looked at you a long moment... Then it was gone.",
    "A rush of wind, and the {NAME} was gone...");

static const struct RogueTextBank *const sGoneBanks[RDUN_COUNT] =
{
    [RDUN_CAVE] = &sGoneCave, [RDUN_FOREST] = &sGoneForest, [RDUN_TOWER] = &sGoneTower, [RDUN_RUINS] = &sGoneRuins,
    [RDUN_SHIP] = &sGoneShip, [RDUN_PLANT] = &sGonePlant, [RDUN_ICE] = &sGoneIce, [RDUN_VOLCANO] = &sGoneVolcano,
    [RDUN_MIRAGE] = &sGoneMirage,
};

// The boss of c->dungeon beaten, not caught (legend: TRUE for a legend).
u32 RogueText_BossGone(struct RogueTextCtx *c, bool8 legend, char *out, u32 size)
{
    return DunSay(c, legend ? &sGoneLegend : DunBank(sGoneBanks, DunKind(c)), c->dungeon, out, size);
}

// ---------------------------------------------------------------------------
// Masters: intro, defeat (said in battle: short), after

DUN_SETS(sMasterCave, 3,
    { "Fifty years I've lived down here. Nobody gets past without a battle!",
      "Ho! You've got grit, youngster!",
      "The deep rock taught me patience. You taught me something new." },
    { "Hah! You came all the way down {DUNGEON} just to see me? Then let's make it worth the climb!",
      "Hah! Flattened!",
      "When you get back up top, look at the sky for me. I haven't seen it in a month." },
    { "I chip at this rock a little every day. One day I'll reach the other side. Today I chip at you!",
      "My pick broke!",
      "There's a seam of shiny stone behind me. I'll show you when you're older. Hah!" },
    { "These boots have climbed every hill near {TOWN}. Now they stand in your way!",
      "Out-walked by a kid!",
      "Go on, then. The rock here likes you. I can tell." },
    { "Shh! Hear that? That's the mountain breathing. It says you want a battle!",
      "The mountain laughed...",
      "Come back when the mountain's quiet. I'll show you the deep tunnels." },
    { "Hah! You found the deepest spot in {DUNGEON}! Your prize is a battle with me!",
      "Hah! What a prize!",
      "I've walked these tunnels so long, I talk to the walls. They say you're tough." },
    { "Rock doesn't rush. I don't rush. But I do battle! Ready?",
      "Ho ho! Rocked!",
      "Slow and steady got me here. Fast and steady got you here. Hah!" },
    { "Every TRAINER who reaches me gets a battle and a story. Battle first!",
      "Ho! That's the story!",
      "The story? I got lost down here once, and liked it so much I stayed." });
DUN_SETS(sMasterVictory, 3,
    { "Only the best make it this deep. Prove you belong here!",
      "You belong, all right.",
      "The LEAGUE is waiting for you. Don't keep them waiting." },
    { "I've beaten everyone who came down this road this year. You're next.",
      "...Not this year, then.",
      "Go. The ELITE FOUR won't wait, and neither should you." },
    { "Eight BADGES. Same as me. Only one of us goes on from here.",
      "It's you. Fine.",
      "I'll train another year. Maybe I'll see you at the top." },
    { "This is the last place to turn back. I'm the last reason to.",
      "No turning back for you.",
      "Hold on to that look in your eyes. You'll need it up there." },
    { "I trained on every route in {REGION}. Let's see if it was enough.",
      "It wasn't enough.",
      "Every route, every rival... and I still lost. Good. There's more to learn." },
    { "You made it this far. That's luck, or skill. Let's find out which.",
      "Skill. Definitely skill.",
      "Show the LEAGUE what you showed me." });
DUN_SETS(sMasterForest, 3,
    { "Shh! The rarest bugs live in the heart of these woods. Battle me!",
      "My precious bugs!",
      "Bugs are tougher than they look. So are you." },
    { "Don't move! There's a rare one on your shoulder! ...Kidding. Battle me!",
      "Squashed!",
      "I've counted three hundred bugs in these woods today. You were more fun than all of them." },
    { "These woods are my bug garden. I tend it. I guard it. I battle in it!",
      "My garden!",
      "Step softly on your way out. Some of my friends are sleeping." },
    { "I've lived in {DUNGEON} so long, the bugs think I'm a tree. Battle!",
      "Timber...",
      "A beetle has lived in my hat for a week. I don't have the heart to move it." },
    { "Bug POK@MON shed their shells to grow. I'll shed my nerves and battle!",
      "Molted!",
      "Next time I'll have new shells. Er, new POK@MON." },
    { "The deepest part of the woods is where the toughest bugs live. Me too!",
      "Not tough enough!",
      "Out there they call me a bug maniac. In here they call me... Well, they don't talk." });
DUN_SETS(sMasterTower, 3,
    { "The spirits whispered your name... They want to see you battle!",
      "The spirits... are pleased...",
      "The spirits will watch over you from now on. Hehehe..." },
    { "Hee hee... The spirits at the top get lonely. Stay a while... and battle!",
      "The spirits sigh...",
      "They liked you, you know. They don't like many." },
    { "I've climbed these stairs every night for years. Tonight, I have company!",
      "Hee... company...",
      "Light a candle on your way down. For the ones who rest here." },
    { "The wind up here sings. It sang your name. Hee hee!",
      "The song is over...",
      "The wind will sing about this battle for a long time." },
    { "Shh... Every grave in {DUNGEON} is listening. Make it a good battle.",
      "They clapped...",
      "The ones who rest here loved their TRAINERS once. Yours would be proud of you." },
    { "You came to the top of the tower. Few do. Fewer leave without a battle!",
      "You leave, then...",
      "Come back on a foggy night. The view is better. Hee hee!" });
DUN_SETS(sMasterIce, 3,
    { "Brr! I came to see the ice and stayed for the battles. Ready?",
      "Brr... A cold defeat!",
      "My mustache froze an hour ago. Worth it!" },
    { "Hah! Slid all the way here, did you? Let's see you battle on solid ground!",
      "Slipped up!",
      "The ice teaches balance. You've got plenty." },
    { "I've camped in {DUNGEON} every winter for twenty years. You're my first visitor!",
      "Iced!",
      "Next winter, bring a scarf. I'll be right here." },
    { "The cold keeps my POK@MON sharp. Is yours sharp too?",
      "Sharper than me!",
      "Off you go. Mind the slippery bits. All the bits are slippery." });
DUN_SETS(sMasterVolcano, 3,
    { "Hah! It's hot down here, but my spirit's hotter! Battle!",
      "Hah! I'm toast!",
      "I come down here to sweat out my worries. You took them all at once." },
    { "Every step nearer the lava makes my POK@MON stronger. This is near enough!",
      "Scorched!",
      "The rocks here are still warm from the last eruption. So am I." },
    { "I've climbed {VOLCANO} a hundred times. Never met anyone this deep. Battle!",
      "Hah! A hundred and one!",
      "Watch the ash on your way out. It gets in your boots." },
    { "You walked through all that heat to reach me? Hah! I like you already!",
      "Melted!",
      "Drink some water up top. Trust an old hiker." });
DUN_SETS(sMasterRuins, 3,
    { "Forty years I've dug these ruins. Found three tablets and a spoon. Battle!",
      "My spoon!",
      "The old ones who built this place must have battled too. I just know it." },
    { "Careful! These walls are older than {REGION} itself. So am I, nearly!",
      "Ancient history!",
      "Dig slowly, read carefully, battle hard. That's the job." },
    { "The words on these walls say only the worthy may pass. I say battle!",
      "Worthy, all right.",
      "I'll carve your name on the wall. Small. Next to mine." });

static const struct DunSets *const sMasterSets[RDUN_COUNT] =
{
    [RDUN_CAVE] = &sMasterCave, [RDUN_VICTORY] = &sMasterVictory, [RDUN_FOREST] = &sMasterForest,
    [RDUN_TOWER] = &sMasterTower, [RDUN_ICE] = &sMasterIce, [RDUN_VOLCANO] = &sMasterVolcano, [RDUN_RUINS] = &sMasterRuins,
};

// A master's line (RMASTER_INTRO, _DEFEAT, _AFTER) for c->dungeon.
u32 RogueText_MasterLine(struct RogueTextCtx *c, u8 field, char *out, u32 size)
{
    return DunSaySet(c, DunSetsOf(sMasterSets, DunKind(c)), DunOrdinal(c->r, c->dungeon), field, out, size);
}

// ---------------------------------------------------------------------------
// The choice room: the keeper beside the balls (before and after one is
// taken), and the balls that roll away

DUN_SETS(sGuideCave, 2,
    { "I dug these out of the rock! You may take only one of them.",
      "Well chosen! The rest are mine. Finders keepers, as they say." },
    { "I found these while digging in {DUNGEON}! I can spare one. Just one!",
      "A good pick! I'll catalog the others tonight." },
    { "Science says share. My heart says keep. So, you may take one.",
      "There. Science is happy. My heart is a little sad." },
    { "These finds were buried in the rock for ages. One may go with you.",
      "Off it goes! Take good care of it." },
    { "So many finds, and only one TRAINER! Choose well. I'm taking notes.",
      "Interesting choice! I've written it down." });
DUN_SETS(sGuideForest, 2,
    { "Someone left these in the woods. Take only one, and leave the rest!",
      "Only one, as I said. The woods will keep the others safe." },
    { "Travelers leave things here for those who come after. Take one, leave the rest.",
      "Good. The woods remember who is greedy." },
    { "Ah, a young one. Pick one, only one. The trees are watching.",
      "The trees nod. You chose fairly." },
    { "I've sat by these for years. Only one may leave. Which will it be?",
      "That one, eh? A fine choice. I'd have taken it too." });
DUN_SETS(sGuideTower, 2,
    { "These are offerings, child. You may take only one of them.",
      "You chose. The spirits will keep the others now." },
    { "Families left these for the spirits. The spirits say a TRAINER may take one.",
      "The spirits are content. Go gently, child." },
    { "Choose with your heart, child. Only one.",
      "Your heart chose well. The others stay with the spirits." },
    { "My grandmother kept these offerings, and hers before her. Take one, child.",
      "There. The rest will keep for the next child who climbs this far." });
DUN_SETS(sGuideRuins, 2,
    { "The old ones sealed these stones in here. Take only one of them.",
      "A fine choice. The rest belong to the ruins." },
    { "Stones of the old ones. Each was meant for one TRAINER. Choose yours.",
      "So the old ones would have chosen." },
    { "I've guarded these stones half my life. One may leave. Only one.",
      "Go. The rest will wait another thousand years." });
DUN_SETS(sGuideIce, 2,
    { "I thawed these out of the ice myself! You may take only one.",
      "Good choice! The rest go back in the cold. For science." },
    { "These were frozen solid until yesterday. One is yours, if you like.",
      "Keep it warm! Or cold. I'm not sure, actually." });
DUN_SETS(sGuideVolcano, 2,
    { "I dug these out of cooled lava! Only one, please. They were hard to get.",
      "Mind your fingers. It may still be warm." },
    { "The heat makes stones strong. One of these may go with you.",
      "A fine choice! I'll study the rest back at camp." });

static const struct DunSets *const sGuideSets[RDUN_COUNT] =
{
    [RDUN_CAVE] = &sGuideCave, [RDUN_FOREST] = &sGuideForest, [RDUN_TOWER] = &sGuideTower, [RDUN_RUINS] = &sGuideRuins,
    [RDUN_ICE] = &sGuideIce, [RDUN_VOLCANO] = &sGuideVolcano,
};

// The keeper of c->dungeon's choice room, before or after a ball is taken.
u32 RogueText_GuideLine(struct RogueTextCtx *c, bool8 taken, char *out, u32 size)
{
    return DunSaySet(c, DunSetsOf(sGuideSets, DunKind(c)), DunOrdinal(c->r, c->dungeon), taken ? 1 : 0, out, size);
}

// The others roll away: one ball, or several (as {N}... is not English,
// two lines a kind).
DUN_SETS(sChoiceGone, 2,
    { "The other POK@ BALL rolled away into the dark...", "The other POK@ BALLS rolled away into the dark..." },
    { "The other POK@ BALL rolled away into the tall grass...", "The other POK@ BALLS rolled away into the tall grass..." },
    { "The other POK@ BALL rolled away between the graves...", "The other POK@ BALLS rolled away between the graves..." },
    { "The other POK@ BALL rolled away into the sand...", "The other POK@ BALLS rolled away into the sand..." },
    { "The other POK@ BALL slid away across the ice...", "The other POK@ BALLS slid away across the ice..." },
    { "The other POK@ BALL rolled down a crack in the rock...", "The other POK@ BALLS rolled down a crack in the rock..." });

u32 RogueText_ChoiceGone(struct RogueTextCtx *c, bool8 several, char *out, u32 size)
{
    static const u8 rows[RDUN_COUNT] = { [RDUN_FOREST] = 1, [RDUN_TOWER] = 2, [RDUN_RUINS] = 3, [RDUN_ICE] = 4, [RDUN_VOLCANO] = 5,
                                         [RDUN_MIRAGE] = 3 };
    u8 kind = DunKind(c);
    return RogueText_Say(out, size, sChoiceGone.cells[(kind < RDUN_COUNT ? rows[kind] : 0) * 2 + (several ? 1 : 0)], c);
}

// ---------------------------------------------------------------------------
// Camps: offer, done, declined, used up. A camper keeps one voice.

DUN_SETS(sCampCave, 4,
    { "I've made camp down here. You look worn out. Want to rest a while?",
      "There! Good as new. The deep floors are rough, so watch your step.",
      "Suit yourself. The fire's warm if you change your mind.",
      "That was all my supplies. Good luck down there!" },
    { "Hey! Sit by the lantern a minute. Your POK@MON look tired. Want to rest?",
      "All better! A little lantern light does wonders.",
      "No? Brave. Or stubborn. Same thing down here.",
      "I'm out of everything but stories. Want one? ...No? OK!" },
    { "Welcome to camp! Population: me. Want to rest your team?",
      "There you go. Population: me, and one happy team.",
      "OK! The camp isn't going anywhere. Neither am I.",
      "Camp's closed! Well, the snacks are. I'm still here." },
    { "I got tired halfway down {DUNGEON}, so I made camp. Want to rest too?",
      "Rested? Good! Off you go, then.",
      "Fair enough. I'll keep the lantern lit.",
      "My supplies are gone. Sorry! The way down is that way." },
    { "Shh, my POK@MON are napping. Yours could too. Rest a while?",
      "Look at them, all perked up. Off you go!",
      "That's OK. I'll whisper good luck instead.",
      "Mine are still asleep. Yours are rested. Go on, quietly!" },
    { "Tent, blanket, POTIONS. I came prepared! Want a rest?",
      "Ta-da! The POTIONS were the important part.",
      "No? More POTIONS for me, then. Ha!",
      "I came prepared, but not THAT prepared. All gone!" });
DUN_SETS(sCampForest, 4,
    { "I've made camp in the woods. You look tired. Rest a while?",
      "There! Nothing beats fresh forest air. Mind the bugs out there!",
      "Off you go, then! The bugs say hello.",
      "That was the last of my supplies. Take care in the deep woods!" },
    { "The pond here is cool and clear. Let your POK@MON rest by it a while?",
      "There. They drank their fill and dozed in the shade. Good as new!",
      "OK! The pond will still be here.",
      "The pond's still here, but my snacks aren't. Take care!" },
    { "I camp by this pond every summer. Want to rest with me a bit?",
      "Feels good, doesn't it? Mind the roots on your way!",
      "Suit yourself! I'll save you a spot in the shade.",
      "I've nothing left but a fishing rod. Good luck out there!" },
    { "Lunch by the pond? I made extra. Rest a while!",
      "Full and rested! Your POK@MON loved the sandwiches.",
      "No? More for me, then!",
      "Lunch is all gone. Not a crumb left!" },
    { "Phew, these woods are thick! I made camp to catch my breath. Join me?",
      "All better! I could sit in this shade all day.",
      "No? OK! The shade's free if you change your mind.",
      "My bag's empty, but the shade's free. Take care!" });
DUN_SETS(sCampTower, 4,
    { "I look after this tower. You look weary, child. Rest here a while?",
      "There, there. Now go on up, and be kind to the spirits above.",
      "As you like, child. I'll keep the candles lit.",
      "I've nothing more to give, child. Take care on the floors above." },
    { "Climbing is hard on old legs and young ones. Sit, child. Rest a while?",
      "There. The spirits would want you to go on rested.",
      "Go on, then. The stairs won't climb themselves.",
      "I've given all I have, child. The stairs are that way." },
    { "I keep these floors swept. You look as tired as my broom. Rest?",
      "There. Now you look ready for the stairs.",
      "As you like. The broom and I will wait.",
      "Nothing more to give, child. Go up gently." },
    { "My family has kept this tower for generations. We always offer a rest. Will you take it?",
      "There. Go on up, and greet the spirits for me.",
      "Very well, child. The offer stays open, for now.",
      "The rest has been given. Be kind to the spirits above." });
DUN_SETS(sCampRoad, 4,
    { "Long road, huh? I've made camp here. Want to rest a while?",
      "There! Good as new. The next GYM isn't far now. You can do it!",
      "No worries! Fire's always lit.",
      "That was all my supplies. Good luck out there!" },
    { "Long day? Sit by my fire a bit. Your POK@MON look beat!",
      "Back on your feet! The road's still long, but you're ready.",
      "OK! Try not to run the whole way.",
      "Fire's out, snacks are gone. Good luck on the road!" },
    { "I've walked this road for a week. You look like you did it in a day! Rest?",
      "All set! Walk a little slower, maybe.",
      "Suit yourself! I'll be here all week.",
      "I'm all out. The road's that way, friend!" },
    { "Hey! Camp's open. One rest per TRAINER, house rules. Want it?",
      "House rules say you're good to go!",
      "House rules say you can come back later.",
      "House rules: one rest. Sorry, friend!" });
DUN_SETS(sCampVictory, 4,
    { "Quite a battle! I've made camp here. Want to rest a while?",
      "There! Good as new. The LEAGUE is past the cave. Go get 'em!",
      "Feeling lucky? OK! I'll be here.",
      "That was all my supplies. Good luck at the POK@MON LEAGUE!" },
    { "I didn't make it through VICTORY ROAD last year. Maybe I can help you make it. Rest?",
      "There. Do it for both of us, OK?",
      "That's OK! I'll cheer for you either way.",
      "I gave you all I had. Now go win it!" },
    { "Every TRAINER who stops here goes on to the LEAGUE. Well, most. Rest?",
      "Now you'll be one of the ones who make it!",
      "Brave! Let me know how it goes.",
      "No more rests. Just good luck!" },
    { "Eight BADGES, huh? Mine too, once. Sit down and rest before the last climb.",
      "There. Now go show them what eight BADGES look like.",
      "OK! Stubborn ones make the best CHAMPIONS, they say.",
      "I'm all out, champ. Well, almost-champ. Go!" });
DUN_SETS(sCampIce, 4,
    { "Brr! Come warm up by the stove. Your POK@MON must be freezing!",
      "Toasty! Now back out into the cold with you.",
      "Suit yourself! Don't lick the ice.",
      "The stove's out of fuel. Stay warm down there!" },
    { "I'm watching the ice grow. It's slow work. Want to rest while I watch?",
      "There! The ice grew a whole inch while you rested.",
      "OK! Watch your step on the slippery parts.",
      "That's all I had. Slide carefully!" },
    { "Hot soup! Well, it was hot. Rest a while?",
      "Warm all the way through! Good luck out there.",
      "More soup for me!",
      "The soup is gone. Even the cold soup." });
DUN_SETS(sCampVolcano, 4,
    { "Phew, it's hot! I set up camp where the air's cooler. Rest a while?",
      "There! Cool and rested. Watch out for the lava down there.",
      "OK! Drink some water, at least.",
      "My water's all gone! Stay cool down there." },
    { "Ash in my hair, ash in my boots. Want to rest a while?",
      "All better! Shake out your boots before you go.",
      "Suit yourself! I'll sit here and watch the ash fall.",
      "I'm out of everything. Good luck!" },
    { "I come to {VOLCANO} to watch the lava glow. Rest with me a while?",
      "Isn't it pretty? Now go, before you get sleepy again.",
      "OK! The lava will still be glowing later.",
      "I've nothing left to share but the view. Take care!" });

enum { CAMP_SETS_CAVE, CAMP_SETS_FOREST, CAMP_SETS_TOWER, CAMP_SETS_ROAD, CAMP_SETS_VICTORY, CAMP_SETS_ICE, CAMP_SETS_VOLCANO,
       CAMP_SETS_COUNT };
static const struct DunSets *const sCampSets[CAMP_SETS_COUNT] =
{
    &sCampCave, &sCampForest, &sCampTower, &sCampRoad, &sCampVictory, &sCampIce, &sCampVolcano,
};

// Which camper: a dungeon's by its kind, or (c->dungeon 0xFF) the one by
// the road at c->place (Victory Road's, LATE CAMP's).
static u8 CampWho(const struct RogueTextCtx *c)
{
    if (c->dungeon >= c->r->nDungeons)
        return c->place == c->r->victoryRoad ? CAMP_SETS_VICTORY : CAMP_SETS_ROAD;
    switch (DunKind(c))
    {
    case RDUN_FOREST:  return CAMP_SETS_FOREST;
    case RDUN_TOWER:   return CAMP_SETS_TOWER;
    case RDUN_VICTORY: return CAMP_SETS_VICTORY;
    case RDUN_ICE:     return CAMP_SETS_ICE;
    case RDUN_VOLCANO: return CAMP_SETS_VOLCANO;
    default:           return CAMP_SETS_CAVE;
    }
}

// A camper's line (RCAMP_OFFER, _DONE, _DECLINED, _USED).
u32 RogueText_CampLine(struct RogueTextCtx *c, u8 line, char *out, u32 size)
{
    u8 who = CampWho(c);
    u32 k = c->dungeon < c->r->nDungeons ? c->dungeon : 16 + c->place, i;
    // Victory Road's two campers (on its floor and on its own map past the
    // rival) never share a voice.
    if (who == CAMP_SETS_VICTORY && c->dungeon >= c->r->nDungeons)
        for (i = 0; i < c->r->nDungeons; i++)
            if (c->r->dungeons[i].kind == RDUN_VICTORY)
                k = i + 1;
    return DunSaySet(c, sCampSets[who], k, line, out, size);
}

// ---------------------------------------------------------------------------
// The lost explorer ({EXPLORER}): excited, a little embarrassed, generous

TEXT_BANK(sExplorerAskCave,
    "You found me! I'm {EXPLORER}. I was mapping this floor, and the map ran out before the floor did! Will you lead me back out?",
    "Oh, thank goodness, a person! I'm {EXPLORER}. I've been lost down here for days. Can you get me out?");
TEXT_BANK(sExplorerAskForest,
    "Hello? Oh, a TRAINER! I'm {EXPLORER}. These woods all look the same! Could you lead me out?",
    "I'm {EXPLORER}, and I'm a little bit lost. OK, very lost. Will you lead me out?");
TEXT_BANK(sExplorerAskTower,
    "Oh! I'm {EXPLORER}. I came to read the old graves, and now I can't find the stairs! Lead me out?");
TEXT_BANK(sExplorerAskRuins,
    "These floors keep moving! I'm {EXPLORER}, and I've drawn this map six times. Can you lead me out?");
TEXT_BANK(sExplorerAskIce,
    "Brr... I'm {EXPLORER}. I slid in here and I can't slide out! Will you lead me out?");
TEXT_BANK(sExplorerAskVolcano,
    "Phew! I'm {EXPLORER}. I came to study the lava and lost my way in the heat. Will you lead me out?");
TEXT_BANK(sExplorerAskShip,
    "Ahoy? I'm {EXPLORER}. I came aboard to explore, and the doors keep locking! Will you lead me out?");
TEXT_BANK(sExplorerAskPlant,
    "Oh! Who turned off the lights? I'm {EXPLORER}, and I'm lost. Will you lead me out?");

static const struct RogueTextBank *const sExplorerAsks[RDUN_COUNT] =
{
    [RDUN_CAVE] = &sExplorerAskCave, [RDUN_FOREST] = &sExplorerAskForest, [RDUN_TOWER] = &sExplorerAskTower,
    [RDUN_RUINS] = &sExplorerAskRuins, [RDUN_ICE] = &sExplorerAskIce, [RDUN_VOLCANO] = &sExplorerAskVolcano,
    [RDUN_SHIP] = &sExplorerAskShip, [RDUN_PLANT] = &sExplorerAskPlant,
};

DUN_SETS(sExplorerReply, 4,
    { "You will? Thank you! Here, I found this down here. You should have it.",
      "I've one ESCAPE ROPE left, but I was too scared to go alone. Hold on tight. Here we go!",
      "Oh... OK! I'll wait right here. Don't be long!",
      "Your BAG is full! Make room, then come back for me, OK?" },
    { "Really? Oh, thank you! Take this. It's the least I can do.",
      "Now, I have an ESCAPE ROPE. I just didn't want to use it alone. Ready? Hold on!",
      "That's OK! Finish what you came for. I'll draw another map while I wait.",
      "Oh, your BAG is stuffed! Make some room and come back, OK?" });

TEXT_BANK(sExplorerTown,
    "{PLAYER}! It's me, {EXPLORER}! You led me out of {DUNGEON}. I'm drawing a new map now. With more exits.",
    "Everyone in {TOWN} heard how you found me in {DUNGEON}. I told them all myself!",
    "I'm resting up before my next trip. Somewhere flat this time. Thanks again, {PLAYER}!",
    "I still have the map from {DUNGEON}. It's all arrows pointing at other arrows.");

// The explorer's line (REXPLORER_ASK... in the dungeon; REXPLORER_TOWN in
// the POKéMON CENTER later, c->number turning the lines over).
u32 RogueText_ExplorerLine(struct RogueTextCtx *c, u8 line, char *out, u32 size)
{
    struct RogueCast cast;
    RogueGen_Cast(c->r, &cast);
    if (c->dungeon >= c->r->nDungeons)
        c->dungeon = cast.explorerDungeon;
    switch (line)
    {
    case REXPLORER_ASK:
        return DunSay(c, DunBank(sExplorerAsks, DunKind(c)), cast.explorer, out, size);
    case REXPLORER_TOWN:
        return DunSay(c, &sExplorerTown, cast.explorer + c->number, out, size);
    default:
        return DunSaySet(c, &sExplorerReply, cast.explorer, line - REXPLORER_THANKS, out, size);
    }
}

// ---------------------------------------------------------------------------
// The prize ball: a line before it is picked up, and a diary page under it
// where the lore puts one

TEXT_BANK(sPrizeCave,
    "Behind the rocks, half buried in the gravel, is a POK@ BALL.",
    "A POK@ BALL sits in a hollow in the rock, as if someone left it here on purpose.",
    "Something glints in a crack in the wall. It's a POK@ BALL!");
TEXT_BANK(sPrizeForest,
    "Tangled in the roots of an old tree is a POK@ BALL.",
    "A POK@ BALL is nestled in the moss, warm from the sun.");
TEXT_BANK(sPrizeTower,
    "A POK@ BALL rests among the offerings at the top of the tower.",
    "Someone left a POK@ BALL by the shrine, with a note: FOR WHOEVER CLIMBS THIS FAR.");
TEXT_BANK(sPrizeLegend,
    "A POK@ BALL lies near where the legend rested. It hums faintly.",
    "Where the legend stood, the ground is warm. A POK@ BALL lies there.",
    "Something glitters where the legend slept. It's a POK@ BALL!",
    "A POK@ BALL rests in a hollow worn smooth, as if something great once curled up here.",
    "An old offering lies by the wall: a POK@ BALL wrapped in faded cloth.",
    "A POK@ BALL lies in the quiet. Even the air seems to hold its breath.");
TEXT_BANK(sPrizeRuins,
    "A POK@ BALL rests on a stone shelf. The dust around it is undisturbed.",
    "Between two old tablets lies a POK@ BALL. Someone hid it well.");
TEXT_BANK(sPrizeShip,
    "In the corner of the cabin, under a soggy blanket, is a POK@ BALL.",
    "The captain's strongbox lies open. A POK@ BALL rolls around inside.");
TEXT_BANK(sPrizePlant,
    "A POK@ BALL sits on the generator's housing. It's warm to the touch.",
    "A POK@ BALL is wedged between two cables. Is it humming? No, that's the generator.");
TEXT_BANK(sPrizeIce,
    "A POK@ BALL is frozen into the ice. With a tug, it comes free.",
    "A POK@ BALL sits on a shelf of ice, glittering with frost.");
TEXT_BANK(sPrizeVolcano,
    "A POK@ BALL lies on a ledge of cooled rock. It's still a little warm.",
    "A POK@ BALL sits in the ash, as if someone set it down and ran.");
TEXT_BANK(sPrizeVictory,
    "A POK@ BALL waits at the end of the road. A note says: FOR THE NEXT ONE.");
TEXT_BANK(sPrizeHideout,
    "The team's stolen goods! A POK@ BALL lies among the crates.",
    "Crates of stolen goods... and a POK@ BALL on top of them!");

static const struct RogueTextBank *const sPrizeBanks[RDUN_COUNT] =
{
    [RDUN_CAVE] = &sPrizeCave, [RDUN_VICTORY] = &sPrizeVictory, [RDUN_FOREST] = &sPrizeForest,
    [RDUN_TOWER] = &sPrizeTower, [RDUN_HIDEOUT] = &sPrizeHideout, [RDUN_LAIR] = &sPrizeLegend,
    [RDUN_RUINS] = &sPrizeRuins, [RDUN_SHIP] = &sPrizeShip, [RDUN_PLANT] = &sPrizePlant,
    [RDUN_ICE] = &sPrizeIce, [RDUN_VOLCANO] = &sPrizeVolcano,
};

// The line before c->dungeon's prize ball is taken, and the diary page
// under it where the lore puts one (LORE_WAY_DIARY). Returns the length.
u32 RogueText_PrizeLine(struct RogueTextCtx *c, char *out, u32 size)
{
    const struct RogueRegion *r = c->r;
    u8 kind = DunKind(c);
    const struct RogueTextBank *b = c->dungeon < r->nDungeons && r->dungeons[c->dungeon].boss == RBOSS_LEGEND
                                  ? &sPrizeLegend : DunBank(sPrizeBanks, kind);
    u32 k = 0, i, n;
    s32 d = c->dungeon < r->nDungeons ? RogueGen_LoreAt(r, LORE_WAY_DIARY, c->dungeon) : -1;
    // Each legend's chamber, or each dungeon of a kind, its own line.
    if (b == &sPrizeLegend)
        for (i = 0; i < c->dungeon && i < r->nDungeons; i++)
            k += r->dungeons[i].boss == RBOSS_LEGEND;
    else
        k = DunOrdinal(r, c->dungeon);
    n = DunSay(c, b, k, out, size);
    if (d >= 0 && n + 2 < size)
    {
        out[n++] = '\f';
        n += RogueGen_LoreText(r, d, LORE_STORY, LORE_WAY_DIARY, out + n, size - n);
    }
    return n;
}

// ---------------------------------------------------------------------------
// Things to read on a floor

// The metatiles that are things to read: Mt. Pyre's graves (Facility; every
// one says something, a name and a line on one in three),
// the Abandoned Ship's waste bins (InsideShip), New Mauville's terminals
// and generator (BikeShop).
static const u16 sThingBins[] = { 0x235, 0x262, 0x268, 0x277, 0x27F, 0x285 };
static const u16 sThingTerminals[] = { 0x2D6, 0x2D7, 0x2E4, 0x2E5 };

static u8 ThingOfMetatile(u8 kind, u16 mt)
{
    u32 i;
    switch (kind)
    {
    case RDUN_TOWER:
        // Mt. Pyre's graves indoors, and out on its summit (the white and
        // the brown stones).
        return mt == LORE_GRAVE_METATILE || mt == 0x324 || mt == 0x325 ? RTHING_GRAVE : RTHING_NONE;
    case RDUN_SHIP:
        for (i = 0; i < LORE_COUNT_OF(sThingBins); i++)
            if (mt == sThingBins[i])
                return RTHING_BIN;
        return RTHING_NONE;
    case RDUN_PLANT:
        for (i = 0; i < LORE_COUNT_OF(sThingTerminals); i++)
            if (mt == sThingTerminals[i])
                return RTHING_TERMINAL;
        if ((mt >= 0x2D8 && mt <= 0x2DB) || (mt >= 0x2E0 && mt <= 0x2E3) || (mt >= 0x2E8 && mt <= 0x2EB))
            return RTHING_GENERATOR;
        return RTHING_NONE;
    }
    return RTHING_NONE;
}

// TRUE if the block at (i, j) of a room is a thing of its kind to read
// there: a terminal's left block (its right one is the same terminal); a
// grave with open floor before it, one in three of them (Mt. Pyre's are
// rows and rows, and none of Emerald's can be read), and always the one
// the keeper's might be.
static bool8 ThingHere(const struct RogueRegion *r, u8 dungeon, u8 floor, const struct RogueRoom *room, u8 kind, u32 i, u32 j, u8 thing)
{
    if (ThingOfMetatile(kind, room->blocks[j * room->w + i] & 0x3FF) != thing)
        return FALSE;
    if (thing == RTHING_TERMINAL && i > 0 && ThingOfMetatile(kind, room->blocks[j * room->w + i - 1] & 0x3FF) == thing)
        return FALSE;
    if (thing == RTHING_GRAVE)
    {
        if (j + 1 >= room->h || ((room->blocks[(j + 1) * room->w + i] >> 10) & 3) != 0)
            return FALSE;
        return RogueRng_Hash(r->seed, 17300 + dungeon * 64 + floor * 8 + i * 3 + j * 131) % 3 == 0;
    }
    return TRUE;
}

// The keeper's grave on a tower's grave floor: the grave before open floor
// nearest the way in, as (x, y); FALSE if none.
static bool8 KeeperGrave(const struct RogueRoom *room, u32 *gx, u32 *gy)
{
    u32 i, j, best = 0xFFFF;
    for (j = 0; j + 1 < room->h; j++)
        for (i = 0; i < room->w; i++)
            if ((room->blocks[j * room->w + i] & 0x3FF) == LORE_GRAVE_METATILE
             && ((room->blocks[(j + 1) * room->w + i] >> 10) & 3) == 0 && room->nWarps)
            {
                u32 dist = abs((s32)i - room->warps[0].x) + abs((s32)j - room->warps[0].y);
                if (dist < best)
                    best = dist, *gx = i, *gy = j;
            }
    return best != 0xFFFF;
}

// How many things of a kind a dungeon's floors before this one hold, so
// each floor's take the next lines (a ship's log goes on page by page).
static u32 ThingsBefore(const struct RogueRegion *r, u8 dungeon, u8 floor, u8 thing)
{
    u32 f, i, j, n = 0;
    for (f = 1; f < floor; f++)
    {
        u8 id = RogueGen_FloorRoom(r, dungeon, f);
        if (id == 0xFF)
            continue;
        for (j = 0; j < sRogueRooms[id].h; j++)
            for (i = 0; i < sRogueRooms[id].w; i++)
                n += ThingHere(r, dungeon, f, &sRogueRooms[id], r->dungeons[dungeon].kind, i, j, thing);
    }
    return n;
}

// What there is to read at a block (x, y) of a dungeon's floor (map
// coordinates, without the border): RTHING_*, with *k its order among the
// dungeon's things of that kind (floor by floor, row by row) and *lore TRUE
// for the old keeper's grave (LORE_WAY_GRAVE: on the grave floor, the grave
// nearest the way in).
u8 RogueGen_FloorThingAt(const struct RogueRegion *r, u8 dungeon, u8 floor, u32 x, u32 y, u8 *k, bool8 *lore)
{
    const struct RogueRoom *room;
    u8 id, thing, kind;
    u32 i, j, n;
    *k = 0;
    *lore = FALSE;
    if (dungeon >= r->nDungeons)
        return RTHING_NONE;
    kind = r->dungeons[dungeon].kind;
    if ((kind != RDUN_TOWER && kind != RDUN_SHIP && kind != RDUN_PLANT) || (id = RogueGen_FloorRoom(r, dungeon, floor)) == 0xFF)
        return RTHING_NONE;
    room = &sRogueRooms[id];
    if (x >= room->w || y >= room->h)
        return RTHING_NONE;
    thing = ThingOfMetatile(kind, room->blocks[y * room->w + x] & 0x3FF);
    if (thing == RTHING_NONE || thing == RTHING_GENERATOR)
        return thing;
    if (thing == RTHING_GRAVE && RogueGen_LoreAt(r, LORE_WAY_GRAVE, dungeon) >= 0 && floor == RogueGen_GraveFloor(r, dungeon))
    {
        u32 gx, gy;
        if (KeeperGrave(room, &gx, &gy) && gx == x && gy == y)
        {
            *lore = TRUE;
            return thing;
        }
    }
    // A terminal's right block reads as its left.
    if (thing == RTHING_TERMINAL && x > 0 && ThingOfMetatile(kind, room->blocks[y * room->w + x - 1] & 0x3FF) == thing)
        x--;
    if (!ThingHere(r, dungeon, floor, room, kind, x, y, thing))
    {
        // The other graves one can stand before say something plainer, so
        // no grave is silent (RTHING_PLAIN_GRAVE, *k a roll for its line).
        if (thing == RTHING_GRAVE && y + 1 < room->h && ((room->blocks[(y + 1) * room->w + x] >> 10) & 3) == 0)
        {
            // Those on a floor take the bank's lines in turn.
            n = RogueRng_Hash(r->seed, 17350 + dungeon * 8 + floor);
            for (j = 0; j <= y; j++)
                for (i = 0; i < room->w && (j < y || i < x); i++)
                    n += ThingOfMetatile(kind, room->blocks[j * room->w + i] & 0x3FF) == RTHING_GRAVE
                      && j + 1 < room->h && ((room->blocks[(j + 1) * room->w + i] >> 10) & 3) == 0
                      && !ThingHere(r, dungeon, floor, room, kind, i, j, thing);
            *k = n & 0xFF;
            return RTHING_PLAIN_GRAVE;
        }
        return RTHING_NONE;
    }
    // A ship's bins: the Abandoned Ship's floors hold 6, 2, 2 or 3, and 3
    // bins (no more), so the log runs on floor by floor from these starts,
    // never repeating a page, without counting the floors above at talk
    // time (a frame's work on the GBA).
    if (thing == RTHING_BIN)
    {
        static const u8 sBinStarts[] = { 0, 6, 8, 11, 14 };
        n = sBinStarts[floor - 1 < (s32)LORE_COUNT_OF(sBinStarts) ? floor - 1 : LORE_COUNT_OF(sBinStarts) - 1];
    }
    else
        n = ThingsBefore(r, dungeon, floor, thing);
    for (j = 0; j <= y; j++)
        for (i = 0; i < room->w && (j < y || i < x); i++)
            n += ThingHere(r, dungeon, floor, room, kind, i, j, thing);
    *k = n;
    return thing;
}

// Pokémon graves, as Mt. Pyre's: a friend's name and a line about them.
TEXT_BANK(sGraves,
    "HERE RESTS BISCUIT. Best friend. Worst at fetch.",
    "In memory of PIP, who sang every morning.",
    "HERE LIES SOOTY. Always found the warm spot.",
    "To our brave TUMBLE. You always came home.",
    "HERE RESTS OLD BRAMBLE, who guarded the garden for thirty years.",
    "For MOCHI. We still leave a light on.",
    "HERE RESTS DUSTY, twice winner of the {TOWN} pie contest.",
    "In loving memory of PEBBLE. Small, round and very loved.",
    "HERE LIES ZIPPY, fastest on the whole road. Rest now.",
    "For CAPTAIN. You always found the way home.",
    "HERE RESTS HONEY, who never once lost a staring contest.",
    "To MIDGE, who hated baths and loved us.",
    "HERE LIES a friend with no name, found on the road and loved for fifteen years.",
    "In memory of NIBBLES, who tried one of every BERRY in {REGION}.",
    "HERE RESTS SPARK. The lights still flicker when we miss you.",
    "HERE LIES FERN. The flowers on this grave grow by themselves.",
    "To WALDO. Lost so many times. Found every time.",
    "HERE RESTS BUTTONS. Small POK@MON, big heart.",
    "In memory of SNOWBALL, who slept through every storm.",
    "For TOFFEE, who met every visitor at the door.",
    "HERE RESTS ROCKY. We never found out what it would have evolved into.",
    "In memory of LULU. Thank you for every evening walk.",
    "The name on this grave has worn away. Fresh flowers lie in front of it.",
    "HERE RESTS MARMALADE, who came along on every journey from {TOWN}.",
    "HERE RESTS GUS. Snored like a thunderstorm. We miss the noise.",
    "In memory of CLOVER, who found every lost thing but the ball.",
    "For BEANS, who sat on every map we ever tried to read.",
    "HERE LIES PUDDING. Never once ran from a battle. Often ran from baths.",
    "To SUNNY, who waited by the gate at six o'clock.",
    "HERE RESTS OLD GRUMBLE. Grumpy to strangers. Never to us.",
    "In memory of TWIG, who carried the same stick for nine years.",
    "For PEPPER, who won one battle in her life, and never let us forget it.",
    "HERE LIES COBBLE, who could sleep on any rock in {REGION}.",
    "To BRAMBLE JR., who was nothing like his father, and just as loved.",
    "HERE RESTS WISP. Light as a feather, brave as anyone.",
    "For JUNIPER, who walked every road with us, and then walked home.");

// The other graves: no name to read, but never nothing.
TEXT_BANK(sPlainGraves,
    "It's a POK@MON's grave. Someone has swept the stone clean.",
    "A small grave. Its name has worn away with the years.",
    "A POK@MON's grave. A faded ribbon is tied around the stone.",
    "Someone wrote a name here in chalk. It has nearly rubbed off.",
    "A quiet grave. The candles beside it have burned down low.",
    "It's a POK@MON's grave. A child's drawing is tucked under the stone.",
    "The stone is old and mossy. Nobody remembers the name, but someone still visits.",
    "A paw print is carved where the name should be.",
    "It's a POK@MON's grave. It's peaceful here.",
    "A tiny grave, hardly bigger than a POK@ BALL.",
    "The grave of a POK@MON who was loved. You can tell.",
    "Someone has left a chewed old toy beside this grave.",
    "It's a POK@MON's grave. The flowers on it are fresh.",
    "A plain stone. Somebody has polished it until it shines.");

// The ship's name in its log ({NAME}): one a region.
static const char *const sShipNames[] = { "S.S. MARIGOLD", "S.S. HALCYON", "S.S. WAYFARER", "S.S. CORMORANT", "S.S. LADY ROSE",
                                          "S.S. BLUEBELL", "S.S. KINGFISHER", "S.S. STARLING" };

// Pages of the ship's log in its bins, in the order a reader may find them.
TEXT_BANK(sShipLog,
    "A damp page of the ship's log: {NAME}, DAY 1. Fair winds. The crew is cheerful. The cook is not.",
    "A page of the ship's log: DAY 2. A passenger asked if the ship was haunted. Ha! Ha. Ha...",
    "A page of the ship's log: DAY 4. A passenger's WINGULL stole the captain's hat. It has not given it back.",
    "A page of the ship's log: DAY 9. Fog all day. Footsteps in the hold. Nobody is in the hold.",
    "A page of the ship's log: DAY 12. Locked the storage room. Someone has been eating the cargo.",
    "A postcard of the {NAME} in her best days. Someone has drawn a mustache on the captain.",
    "A page of the ship's log: DAY 15. Storm! The cabin keys went flying. Note: find the keys.",
    "A page of the ship's log: DAY 16. We struck the shoal. All passengers rowed safely ashore. The {NAME} stays.",
    "A page of the ship's log: DAY 17. Came back for the cargo. The doors had locked themselves. We left in a hurry.",
    "A page of the ship's log: DAY 18. The harbor master says the {NAME} is a wreck now. Rude, but true.",
    "A note in a shaky hand: The moaning in the hold is NOT the pipes. Do NOT open the last cabin.",
    "A crumpled drawing of the deck, with an X near the stern. Under it, in big letters: NOT HERE.",
    "A torn menu from the galley: THURSDAY: FISH. FRIDAY: FISH. SATURDAY: SURPRISE. (It's fish.)",
    "A list in the cook's hand: FLOUR, SALT, MORE FLOUR. DO NOT LET THE WINGULL NEAR THE FLOUR.",
    "A letter, never sent: Dear MOM, the sea is lovely. I miss your cooking. Please send BERRIES.",
    "An old ticket: ONE PASSAGE ON THE {NAME}. NO REFUNDS. NO POK@MON ON THE DINING TABLES.",
    "A crossword, half done. Seven letters, ABANDONED: the answer has been scribbled out.",
    "A note: To whoever finds this, the captain's spare boots are under the bunk. You're welcome.");

// What a terminal says, a screen a terminal.
TEXT_BANK(sTerminals,
    "The screen flickers: POWER OUTPUT 3 PERCENT. PLEASE CALL THE OPERATOR.",
    "SYSTEM LOG: Unit 4 reports VOLTORB in the cable room again. Unit 4 asks for a raise.",
    "The terminal beeps: LAST LOGIN 9,999 DAYS AGO. WELCOME BACK!",
    "MAINTENANCE NOTE: Do not pick up the POK@ BALLS on the floor. They are not POK@ BALLS.",
    "SHIFT SCHEDULE: MONDAY, nobody. TUESDAY, nobody. WEDNESDAY, also nobody.",
    "The screen shows a map of {REGION}'s power lines. Most of them are dark.",
    "ALERT: Something is eating the power near the generator. ALERT: It is still hungry.",
    "A note is taped to the screen: The blue switch does something. The green switch does something else. Good luck!",
    "The keyboard is covered in dust and tiny paw prints.",
    "Numbers scroll across the screen. One of them says HELLO.");
TEXT_BANK(sGeneratorOn,
    "The generator crackles and spits sparks. Something has been feeding on it...",
    "The generator groans. The lights around it flicker in time with something breathing.");
TEXT_BANK(sGeneratorOff,
    "The generator hums softly. Whatever was feeding on it is gone.",
    "The generator purrs along. The lights are steady at last.");

// What is read at a floor's thing (RTHING_*; k its order on the floor,
// lore the keeper's grave), for c->dungeon on floor.
u32 RogueText_FloorThing(struct RogueTextCtx *c, u8 floor, u8 thing, u8 k, bool8 lore, char *out, u32 size)
{
    const struct RogueRegion *r = c->r;
    u32 n = c->dungeon * 64 + k;
    out[0] = 0;
    switch (thing)
    {
    case RTHING_GRAVE:
        if (lore && RogueGen_LoreAt(r, LORE_WAY_GRAVE, c->dungeon) >= 0)
            return RogueGen_LoreText(r, RogueGen_LoreAt(r, LORE_WAY_GRAVE, c->dungeon), LORE_STORY, LORE_WAY_GRAVE, out, size);
        return DunSay(c, &sGraves, n, out, size);
    case RTHING_PLAIN_GRAVE:
        return RogueText_Say(out, size, sPlainGraves.lines[k % sPlainGraves.count], c);
    case RTHING_BIN:
        c->name = sShipNames[RogueRng_Hash(r->seed, 17400) % LORE_COUNT_OF(sShipNames)];
        // The bins hold the log's pages in order, the first on the deck.
        return RogueText_Say(out, size, sShipLog.lines[k % sShipLog.count], c);
    case RTHING_TERMINAL:
        return DunSay(c, &sTerminals, n, out, size);
    case RTHING_GENERATOR:
        return DunSay(c, c->state && ((c->state->bosses >> c->dungeon) & 1) ? &sGeneratorOff : &sGeneratorOn, c->dungeon, out, size);
    }
    return 0;
}

// ---------------------------------------------------------------------------
// Rumors: what townsfolk have heard of the dungeons round them

TEXT_BANK(sRumorGuardian,
    "Something big lives in {DUNGEON}. I heard it roar from out here!",
    "Hikers say a guardian waits at the end of {DUNGEON}. Nobody gets past it!",
    "My cousin went into {DUNGEON} with three POTIONS. Came out with none, and a story.",
    "There's a POK@MON deep in {DUNGEON} bigger than my house. Well, bigger than my shed.",
    "I dared my brother to go to the bottom of {DUNGEON}. He still won't talk about it.",
    "Something at the bottom of {DUNGEON} doesn't like visitors. Bring a strong team.",
    "A hiker told me something guards the deepest part of {DUNGEON}. Something with big teeth.",
    "Strong TRAINERS go into {DUNGEON} to test themselves against what lives at the bottom.",
    "Don't go down {DUNGEON} at night. Well, it's always night down there. Don't go, then.");
TEXT_BANK(sRumorMaster,
    "A tough TRAINER waits deep in {DUNGEON}. Nobody has beaten them yet!",
    "There's an old TRAINER at the bottom of {DUNGEON} who battles anyone who gets that far.",
    "Someone has been training in {DUNGEON} for years. Years! Imagine the muscles.",
    "Folks say the TRAINER deep in {DUNGEON} hasn't lost in ten years. Hasn't battled much, either.",
    "Beat the TRAINER at the bottom of {DUNGEON}, and you get a prize. A good one, I hear.",
    "I made it to the bottom of {DUNGEON} once. A TRAINER lives down there. They beat me flat.");
TEXT_BANK(sRumorAdmin,
    "Odd people in uniforms keep going into {DUNGEON}. What are they up to?",
    "I saw boxes going into {DUNGEON} at night. Lots of boxes.",
    "Nobody who works at {DUNGEON} will say what they do there. That's not normal, right?",
    "Someone in a uniform asked me the way to {DUNGEON}. Then asked me to forget it.",
    "{TEAM}? In {DUNGEON}? I'd stay away, if I were you.",
    "The lights in {DUNGEON} are on all night. What could anyone be doing in there?");
TEXT_BANK(sRumorLegend,
    "They say a legendary POK@MON rests in {DUNGEON}.",
    "My uncle says he saw {LEGEND} in {DUNGEON} once! I believe him.",
    "Something ancient rests deep in {DUNGEON}. The way to it is sealed, they say.",
    "If you ever see {LEGEND}, come and tell me everything!",
    "I've read every book about {LEGEND}. None of them say how to get into {DUNGEON}.",
    "Old folks leave BERRIES at the way into {DUNGEON}. For {LEGEND}, they say.",
    "Some nights the air near {DUNGEON} feels different. Like something big is dreaming.",
    "A scientist came looking for {LEGEND}. They got as far as {DUNGEON}, then came back for more POTIONS.");
TEXT_BANK(sRumorTreasure,
    "There's treasure deep in {DUNGEON}, they say. Take an ESCAPE ROPE with you!",
    "Someone found something rare in {DUNGEON} last spring. Now everyone's looking.",
    "My uncle says he dropped a ring in {DUNGEON}. Everyone else says he dropped his map.",
    "People say something shiny is hidden in {DUNGEON}. People say a lot of things.",
    "I found a POTION in {DUNGEON} once. It was empty. Still counts!");
TEXT_BANK(sRumorShip,
    "They call that old wreck the {DUNGEON}. Its cabins are all locked, and something moans in the hold!",
    "Nobody goes near the {DUNGEON}. The sailors say its doors lock by themselves.",
    "A key from the {DUNGEON} washed up on the beach once. I threw it back. Was that wrong?",
    "Fishers steer wide of the {DUNGEON}. They say their nets come up full of old keys.",
    "Old sailors say the {DUNGEON} was the finest ship they ever saw, until it hit the shoal.",
    "On foggy nights a light moves on the {DUNGEON}. Nobody's aboard. Nobody!");
TEXT_BANK(sRumorPlant,
    "The {DUNGEON} still hums at night. Its switches still work. Watch out for VOLTORB in disguise!",
    "The {DUNGEON} shut down years ago, but its lights still blink. Spooky, huh?",
    "My dad worked at the {DUNGEON}. He says never trust a POK@ BALL that hums.",
    "The {DUNGEON} used to power half of {REGION}. Now it powers a lot of VOLTORB.",
    "Kids dare each other to touch the door of the {DUNGEON}. It's always warm.");
TEXT_BANK(sRumorIce,
    "A cold wind blows out of {DUNGEON}, even in summer. The floor inside is solid ice!",
    "I slid all the way across {DUNGEON} once. On my face. Mind the ice!",
    "The ice in {DUNGEON} is so smooth you can see your face in it. Watch your step!",
    "Bring warm clothes to {DUNGEON}. And patience. The ice slides you everywhere but where you want.");
TEXT_BANK(sRumorVolcano,
    "Hikers say the heat inside {DUNGEON} could cook an egg. I believe them!",
    "The rocks round {DUNGEON} are warm even at night. Something big lives near the lava.",
    "Ash from {DUNGEON} gets on everything. My laundry is gray. My POK@MON are gray.",
    "You can feel the heat of {DUNGEON} through your boots. Bring plenty of water.");
TEXT_BANK(sRumorCleared,
    "Did you really clear {DUNGEON}? Amazing!",
    "Everyone's talking about the TRAINER who cleared {DUNGEON}. Wait, was that you?",
    "It's been quiet in {DUNGEON} since you went through. The hikers are thrilled.",
    "The old folks say {DUNGEON} hasn't been this peaceful in a long time.",
    "They say the toughest POK@MON in {DUNGEON} finally met its match. You, right?",
    "{DUNGEON}? Even the kids go there now, since you cleared it.");

// The dungeon a townsperson talks about (place a town, rng theirs): one of
// the three nearest, the nearest most often, and a cleared one less often
// (bosses: the dungeons whose boss is beaten). -1 if the region has none.
s32 RogueText_RumorDungeon(const struct RogueRegion *r, u8 place, u32 rng, u16 bosses)
{
    const struct RoguePlace *p = &r->places[place % ROGUE_MAX_PLACES];
    s32 near[3] = { -1, -1, -1 };
    u32 cost[3] = { 0xFFFF, 0xFFFF, 0xFFFF }, i, n = 0, roll;
    for (i = 0; i < r->nDungeons; i++)
    {
        const struct RoguePlace *h = &r->places[r->dungeons[i].host];
        u32 c = abs(h->gridX - p->gridX) + abs(h->gridY - p->gridY), j;
        if (r->dungeons[i].boss != RBOSS_NONE && ((bosses >> i) & 1))
            c += 20;
        for (j = 0; j < 3; j++)
            if (c < cost[j])
            {
                u32 m;
                for (m = 2; m > j; m--)
                    near[m] = near[m - 1], cost[m] = cost[m - 1];
                near[j] = i, cost[j] = c;
                break;
            }
    }
    for (i = 0; i < 3; i++)
        n += near[i] >= 0;
    if (n == 0)
        return -1;
    // Half the time the nearest, then the second, then the third; a far
    // one (more than four cells further) only rarely.
    roll = (rng >> 3) % 10;
    i = roll < 5 ? 0 : roll < 8 ? 1 : 2;
    while (i > 0 && (near[i] < 0 || cost[i] > cost[0] + 4 + (roll == 9 ? 4 : 0)))
        i--;
    return near[i];
}

// A townsperson's rumor of c->dungeon (rng theirs). The game tells the
// ruins' riddle, the tunnel and a seal's key itself.
u32 RogueText_DungeonRumor(struct RogueTextCtx *c, u32 rng, char *out, u32 size)
{
    const struct RogueRegion *r = c->r;
    const struct RogueDungeon *d;
    const struct RogueTextBank *b;
    if (c->dungeon >= r->nDungeons)
    {
        out[0] = 0;
        return 0;
    }
    d = &r->dungeons[c->dungeon];
    if (d->boss != RBOSS_NONE && c->state && ((c->state->bosses >> c->dungeon) & 1))
        b = &sRumorCleared;
    else if ((rng & 3) == 0 && d->nItems != 0 && d->boss != RBOSS_LEGEND && d->kind != RDUN_SHIP && d->kind != RDUN_PLANT
          && d->kind != RDUN_VICTORY && d->kind != RDUN_HIDEOUT)
        b = &sRumorTreasure;
    else if (d->kind == RDUN_SHIP)
        b = &sRumorShip;
    else if (d->kind == RDUN_PLANT)
        b = &sRumorPlant;
    else if (d->boss == RBOSS_LEGEND)
        b = &sRumorLegend;
    else if (d->boss == RBOSS_MASTER)
        b = &sRumorMaster;
    else if (d->boss == RBOSS_ADMIN)
        b = &sRumorAdmin;
    else if (d->kind == RDUN_ICE && (rng & 4))
        b = &sRumorIce;
    else if (d->kind == RDUN_VOLCANO && (rng & 4))
        b = &sRumorVolcano;
    else if (d->boss == RBOSS_GUARDIAN)
        b = &sRumorGuardian;
    else
        b = &sRumorTreasure;
    // Spread over the region: a town's people take its lines in turn.
    return DunSay(c, b, (c->place < r->nPlaces ? r->places[c->place].number * 8 : 0) + (rng >> 5) % 8, out, size);
}

// What townsfolk say opens a legend's sealed way ({NAME}: the move a rite
// wants, or the key). Every name comes from a slot: the game once filled
// the dungeon's name from a buffer nobody had set, and a town said "deep in
// Giga Drain".
TEXT_BANK(sGateBoulder,
    "A huge boulder blocks the way deep in {DUNGEON}. The POK@MON beyond it hasn't been seen in years.",
    "Hikers say there's a boulder deep in {DUNGEON} nobody can shift. Something lives past it.",
    "My dad tried to push the big boulder in {DUNGEON}. The boulder won. Something waits behind it, they say.");
TEXT_BANK(sGateRock,
    "A cracked rock blocks the way deep in {DUNGEON}. Something waits behind it, they say.",
    "There's a cracked wall deep in {DUNGEON}. Put your ear to it, and you can hear breathing.",
    "Old folks say a rock was rolled across the way in {DUNGEON} long ago. To keep something in, or out?");
TEXT_BANK(sGateRitual,
    "Old words in {DUNGEON} tell of a POK@MON. They say {NAME} calls it.",
    "The old stories say what sleeps in {DUNGEON} wakes for a POK@MON that uses {NAME}.",
    "Ruin maniacs read the words in {DUNGEON} for years. All they agree on is the word {NAME}.");
TEXT_BANK(sGateGift,
    "They say the {NAME} opens the way in {DUNGEON}.",
    "{LEADER} of {GYM_TOWN} keeps the {NAME}, they say. It opens the way in {DUNGEON}.",
    "Only someone with the {NAME} may pass in {DUNGEON}. The GYM in {GYM_TOWN} keeps one, I hear.");
TEXT_BANK(sGateFound,
    "They say the {NAME} opens the way in {DUNGEON}.",
    "A statue in {DUNGEON} has a hollow in its base. The {NAME} fits it, they say.",
    "The {NAME} was lost long ago. Find it, and the way in {DUNGEON} opens. That's the story, anyway.");
TEXT_BANK(sGateVillain,
    "Shady types in uniforms keep asking the way to {DUNGEON}.",
    "{TEAM} has been poking round {DUNGEON}. Their boss wants what sleeps in there.",
    "Someone in a {TEAM} uniform asked me how deep {DUNGEON} goes. I said I didn't know. I didn't!");
TEXT_BANK(sGateAfter,
    "An old statue in {DUNGEON} bears the marks of legends. It waits for them all to be met, they say.",
    "The statue in {DUNGEON} has marks for every legend of {REGION}. Meet them all, and it will wake.");

// The key a legend's gift or found gate wants, as people say it.
static const char *DunKeyName(u8 legend)
{
    switch (legend)
    {
    case RLEG_LATIAS:
    case RLEG_LATIOS:    return "EON TICKET";
    case RLEG_LUGIA:
    case RLEG_HO_OH:     return "MYSTIC TICKET";
    case RLEG_TORNADUS:
    case RLEG_THUNDURUS:
    case RLEG_LANDORUS:  return "REVEAL GLASS";
    case RLEG_KYOGRE:    return "BLUE ORB";
    case RLEG_GROUDON:   return "RED ORB";
    case RLEG_HEATRAN:   return "RUBY";
    case RLEG_CRESSELIA: return "OLD SEA MAP";
    case RLEG_MEWTWO:    return "SECRET KEY";
    }
    return NULL;
}

// A townsperson's rumor of what opens c->dungeon's sealed way (a legend's;
// rng theirs). 0 where its way has no seal to tell of (open).
u32 RogueText_GateRumor(struct RogueTextCtx *c, u32 rng, char *out, u32 size)
{
    static const char *const moves[] = { "FLY", "DIVE", "WATERFALL" };
    const struct RogueRegion *r = c->r;
    const struct RogueDungeon *d;
    const struct RogueTextBank *b;
    u32 src;
    out[0] = 0;
    if (c->dungeon >= r->nDungeons || r->dungeons[c->dungeon].boss != RBOSS_LEGEND)
        return 0;
    d = &r->dungeons[c->dungeon];
    src = ROGUE_GATE_SRC(d->gate);
    switch (ROGUE_GATE_KIND(d->gate))
    {
    case RGATE_FIELD:   b = src == ROBST_STRENGTH ? &sGateBoulder : &sGateRock; break;
    case RGATE_RITUAL:  b = &sGateRitual; c->name = moves[src < 3 ? src : 2]; break;
    case RGATE_GIFT:    b = &sGateGift; c->gym = src % ROGUE_NUM_GYMS; c->name = DunKeyName(d->legend); break;
    case RGATE_FOUND:   b = &sGateFound; c->name = DunKeyName(d->legend); break;
    case RGATE_VILLAIN: b = &sGateVillain; break;
    case RGATE_AFTER:   b = &sGateAfter; break;
    default:            return 0;
    }
    if (c->name == NULL && (b == &sGateGift || b == &sGateFound))
        return 0;
    return DunSay(c, b, (c->place < r->nPlaces ? r->places[c->place].number * 4 : 0) + (rng >> 5) % 4, out, size);
}

#ifdef ROGUE_HOST
// For the checks: a tower floor's graves one can stand before, and how many
// of them say something when read (all of them).
void RogueText_CountGraves(const struct RogueRegion *r, u8 dungeon, u8 floor, u32 *graves, u32 *read)
{
    u8 id = RogueGen_FloorRoom(r, dungeon, floor), k;
    bool8 lore;
    u32 x, y;
    *graves = *read = 0;
    if (r->dungeons[dungeon].kind != RDUN_TOWER || id == 0xFF)
        return;
    for (y = 0; y + 1 < sRogueRooms[id].h; y++)
        for (x = 0; x < sRogueRooms[id].w; x++)
            if (ThingOfMetatile(RDUN_TOWER, sRogueRooms[id].blocks[y * sRogueRooms[id].w + x] & 0x3FF) == RTHING_GRAVE
             && ((sRogueRooms[id].blocks[(y + 1) * sRogueRooms[id].w + x] >> 10) & 3) == 0)
            {
                u8 t = RogueGen_FloorThingAt(r, dungeon, floor, x, y, &k, &lore);
                char out[600];
                struct RogueTextCtx c;
                (*graves)++;
                RogueText_Init(&c, r, r->dungeons[dungeon].host, RSPK_SIGN);
                c.dungeon = dungeon;
                *read += t != RTHING_NONE && RogueText_FloorThing(&c, floor, t, k, lore, out, sizeof(out)) != 0;
            }
}

// The rumor a townsperson at place tells (rng theirs, as the game's
// Rogue_BufferDungeonRumor gets it), for the townsfolk's dump; 0 where the
// game may tell one of its own instead (the tunnel's far end, a seal's key,
// the ruins' riddle).
u32 RogueText_DumpRumor(const struct RogueRegion *r, u8 place, u32 rng, const struct RogueTextState *state, char *out, u32 size)
{
    struct RogueTextCtx c;
    s32 d;
    out[0] = 0;
    if ((rng & 0x80) && r->tunnel < r->nDungeons)
        return 0;
    d = RogueText_RumorDungeon(r, place, rng, state ? state->bosses : 0);
    if (d < 0 || r->dungeons[d].kind == RDUN_RUINS)
        return 0;
    RogueText_Init(&c, r, place, RSPK_FOLK);
    c.state = state;
    c.dungeon = d;
    // Half the time a legend's rumor tells what opens its way (shut at the
    // start of a run), as the game's Rogue_BufferDungeonRumor.
    if (r->dungeons[d].boss == RBOSS_LEGEND && (rng & 0x40) && !(state && ((state->bosses >> d) & 1)))
    {
        u32 n = RogueText_GateRumor(&c, rng, out, size);
        if (n != 0)
            return n;
        c.name = NULL;
        c.gym = 0xFF;
    }
    return RogueText_DungeonRumor(&c, rng, out, size);
}
#endif

// ---------------------------------------------------------------------------
// Paintings, the rhyme, the museum

TEXT_BANK(sPaintingsHome,
    "It's a painting of {LANDMARK}. Someone in the family painted it. It's... very brave.",
    "It's a painting of {TOWN} long ago. The roofs were all the same color then.",
    "It's a painting of a POK@MON asleep in a field. It looks very content.",
    "It's a family portrait. Everyone is smiling except the POK@MON, who is chewing the frame.",
    "It's a painting of the {FESTIVAL}. You can almost hear the music.",
    "It's a portrait of {FIGURE}. The eyes seem to follow you around the room.",
    "It's a painting of the road out of {TOWN}, the way every TRAINER goes.",
    "It's a painting of a bowl of BERRIES. One of them has a bite taken out of it.",
    "It's a painting of {LOCALMON}. The painter clearly loved it, and was clearly not a painter.",
    "It's a painting of the {TRADE_FOLK} of {TOWN} coming home at sunset.",
    "It's a painting of a TRAINER setting out on a journey. It's signed: GRANDMA, AGE 10.",
    "It's a painting of a rainy day. Somehow it's very cheerful.");
TEXT_BANK(sPaintingsCenter,
    "It's a painting of {TOWN} on a bright day. A card reads: GIFT OF THE {STEM} ART CLUB.",
    "It's a calm painting of a meadow. It's here to help tired POK@MON relax.",
    "It's a painting of {LANDMARK}. A card reads: PAINTED BY A NURSE ON HER DAY OFF.",
    "It's a painting of a CHANSEY carrying an egg. It looks very proud of itself.",
    "It's a painting of the {TRADE_FOLK} of {TOWN} at work, long ago.",
    "It's a painting of the {FESTIVAL}. Someone has drawn a tiny hat on one of the POK@MON.",
    "It's a painting of {NEIGHBOR}, seen from very far away. A card reads: OUR GOOD NEIGHBORS.",
    "It's a painting of {LOCALMON} playing by the road out of {TOWN}.",
    "It's a painting of the first POK@MON CENTER in {REGION}. It looks a lot like this one.",
    "It's a painting of a sleepy POK@MON on a warm windowsill. It makes you yawn.",
    "It's a painting of the night sky over {TOWN}. Someone has counted the stars on the back.",
    "It's a portrait of {FIGURE} the {FIGURE_TITLE}. A card reads: A FRIEND OF THIS CENTER.");

// The painting on a wall: a home's (house; its family's taste) or the
// POKéMON CENTER's (house 0xFF). The legend's (LORE_WAY_PAINTING) hangs in
// the town's first home with a painting, else in the POKéMON CENTER; the
// plaque of a town with no museum (LORE_WAY_MUSEUM) hangs under the
// POKéMON CENTER's. paintingHouse: RogueGen_PaintingHouse for the town.
u32 RogueText_Painting(struct RogueTextCtx *c, u8 house, s32 paintingHouse, char *out, u32 size)
{
    const struct RogueRegion *r = c->r;
    s32 d = RogueGen_LoreAt(r, LORE_WAY_PAINTING, c->place);
    c->town = c->place;
    if (d >= 0 && (house == 0xFF ? paintingHouse < 0 : (s32)house == paintingHouse))
        return RogueGen_LoreText(r, d, LORE_HINT, LORE_WAY_PAINTING, out, size);
    if (house == 0xFF && (d = RogueGen_LoreAt(r, LORE_WAY_MUSEUM, c->place)) >= 0 && !RogueGen_TownHasMuseum(r, c->place))
        return RogueGen_LoreText(r, d, LORE_STORY, LORE_WAY_MUSEUM, out, size);
    if (house == 0xFF)
        return DunSay(c, &sPaintingsCenter, r->places[c->place].number, out, size);
    return DunSay(c, &sPaintingsHome, r->places[c->place].number + house * 5, out, size);
}

// The child in a POKéMON CENTER who knows the town's skipping rhyme
// (LORE_WAY_RHYME), or 0 where there is none.
u32 RogueText_Rhyme(struct RogueTextCtx *c, char *out, u32 size)
{
    s32 d = RogueGen_LoreAt(c->r, LORE_WAY_RHYME, c->place);
    out[0] = 0;
    return d < 0 ? 0 : RogueGen_LoreText(c->r, d, LORE_HINT, LORE_WAY_RHYME, out, size);
}

// A museum's exhibits: every painting, case, statue and machine of its
// vanilla room says something, and says what it shows (a painting is a
// painting, the stone slab a stone). The town's people talk of a fossil
// older than the region, of old POKé BALLS behind glass: both are on show.
enum
{
    MOBJ_PAINTING,  // a painting on the wall
    MOBJ_GREAT,     // the great painting (the Lilycove Museum's legendary one)
    MOBJ_SLAB,      // the Lilycove Museum's stone tablet
    MOBJ_FOSSIL,    // the Oceanic Museum's fossil case
    MOBJ_CASE,      // a glass case or a tall glass tube
    MOBJ_TALL,      // the Oceanic Museum's first tall tube (the legend's, where told)
    MOBJ_MACHINE,   // the Oceanic Museum's depth machine
    MOBJ_STATUE,    // the Lilycove Museum's bird statue
    MOBJ_BALL,      // its POKé BALL sculpture
    MOBJ_DESK,      // a counter, with the guest book
};

TEXT_BANK(sExhibitPaintings,
    "A painting of {OLDTOWN} long ago. A few houses, a lot of sky.",
    "A portrait of {FIGURE} the {FIGURE_TITLE}. The eyes follow you round the room.",
    "A painting of the {FESTIVAL}. The painter put themselves in the corner, waving.",
    "A painting of {LOCALMON} asleep in a meadow. The card says: PLEASE DO NOT WAKE.",
    "A painting of {TOWN} at dawn, by a painter nobody remembers. It's lovely.",
    "A painting of the {TRADE_FOLK} of {TOWN} at work, every one of them smiling.",
    "A painting of the road to {NEIGHBOR}. Someone has painted a little rain cloud over {NEIGHBOR}.",
    "A child's painting of {LOCALMON}, framed in gold. The card says: OUR FINEST WORK.",
    "A stormy sky in thick paint. The card says: UNTITLED. It looks like a bad day.",
    "A painting of a TRAINER and a POK@MON walking off down a long road. No names. None needed.");
TEXT_BANK(sExhibitCases,
    "A case of old POK@ BALLS, made of wood and brass. One was made right here in {TOWN}.",
    "Old tools in a glass case. {TOWN} has lived by {TRADE} longer than anyone remembers.",
    "A model of {TOWN} built from matchsticks by a local child. It took four years.",
    "The bones of an ancient POK@MON stand in the glass. It lived here before there was a {TOWN}.",
    "A trophy from a race against {NEIGHBOR}. The plaque says we won. {NEIGHBOR} says otherwise.",
    "An old map of {REGION}. {OLDTOWN} is on it, but most of the towns are not.",
    "A display about {LOCALMON}, found on the roads round {TOWN}. Please do not tap the glass.",
    "A case of BADGES from GYMS long closed. Nobody remembers who won them.",
    "A glass case holds a worn pair of boots. The card says they walked every road in {REGION}.");

// The objects of the two museum rooms: a rectangle of blocks each (x, y,
// width, height), its kind, and which line of its kind's bank it takes.
struct MuseumObj { u8 x, y, w, h, kind, k; };
static const struct MuseumObj sMuseumOceanic[] =
{
    {  2, 1, 2, 1, MOBJ_FOSSIL, 0 },
    {  9, 1, 2, 1, MOBJ_TALL, 0 },
    { 12, 1, 2, 1, MOBJ_CASE, 1 },
    { 16, 1, 2, 1, MOBJ_MACHINE, 0 },
    {  2, 4, 1, 1, MOBJ_CASE, 2 },
    {  2, 7, 1, 1, MOBJ_CASE, 3 },
    { 15, 4, 1, 1, MOBJ_CASE, 4 },
    { 18, 4, 1, 1, MOBJ_CASE, 5 },
    { 18, 7, 1, 1, MOBJ_CASE, 6 },
    {  5, 6, 4, 3, MOBJ_DESK, 0 },
    { 11, 6, 4, 3, MOBJ_DESK, 1 },
};
static const struct MuseumObj sMuseumLilycove[] =
{
    { 19, 1, 2, 1, MOBJ_GREAT, 0 },
    { 15, 7, 5, 3, MOBJ_SLAB, 0 },
    {  1, 1, 2, 1, MOBJ_PAINTING, 0 },
    {  5, 1, 2, 1, MOBJ_PAINTING, 1 },
    {  9, 1, 1, 1, MOBJ_PAINTING, 2 },
    { 11, 1, 1, 1, MOBJ_PAINTING, 3 },
    {  2, 6, 2, 1, MOBJ_PAINTING, 4 },
    {  6, 6, 1, 1, MOBJ_PAINTING, 5 },
    { 15, 12, 1, 1, MOBJ_STATUE, 0 },
    { 18, 11, 1, 1, MOBJ_BALL, 0 },
    {  1, 10, 6, 4, MOBJ_DESK, 0 },
};

static const struct MuseumObj *MuseumObjs(bool8 lilycove, u32 *n)
{
    *n = lilycove ? LORE_COUNT_OF(sMuseumLilycove) : LORE_COUNT_OF(sMuseumOceanic);
    return lilycove ? sMuseumLilycove : sMuseumOceanic;
}

// The object of a museum's room at (x, y) (lilycove: the Lilycove Museum's
// room, else the Oceanic Museum's), as RogueText_MuseumExhibit's k, or -1.
s32 RogueGen_MuseumCase(bool8 lilycove, u32 x, u32 y)
{
    u32 i, n;
    const struct MuseumObj *o = MuseumObjs(lilycove, &n);
    for (i = 0; i < n; i++)
        if (x >= o[i].x && x < o[i].x + o[i].w && y >= o[i].y && y < o[i].y + o[i].h)
            return i;
    return -1;
}

// TRUE if c->place's museum is the Lilycove Museum's room (a cove town's).
static bool8 MuseumIsLilycove(const struct RogueTextCtx *c)
{
    return c->place < c->r->nPlaces && c->r->places[c->place].theme == RTHEME_COVE;
}

// What museum object k of c->place's museum shows: the legend's tale on the
// great painting or the first tall case where LORE_WAY_MUSEUM tells it here,
// else the town's and the region's past. Returns 0 past the last object.
u32 RogueText_MuseumExhibit(struct RogueTextCtx *c, u32 k, char *out, u32 size)
{
    const struct RogueRegion *r = c->r;
    s32 d = RogueGen_LoreAt(r, LORE_WAY_MUSEUM, c->place);
    const char *t = NULL;
    u32 n, len;
    const struct MuseumObj *o = MuseumObjs(MuseumIsLilycove(c), &n);
    out[0] = 0;
    if (k >= n)
        return 0;
    c->town = c->place;
    c->speaker = RSPK_SIGN;
    switch (o[k].kind)
    {
    case MOBJ_GREAT:
    case MOBJ_TALL:
        if (d >= 0)
        {
            const char *head = o[k].kind == MOBJ_GREAT ? "A great painting, dark with age, fills the wall.|"
                                                       : "A tall glass case holds an old carved stone.|";
            len = RogueText_Say(out, size, head, c);
            return len + RogueGen_LoreText(r, d, LORE_STORY, LORE_WAY_MUSEUM, out + len, size - len);
        }
        if (o[k].kind == MOBJ_GREAT)
            t = "A great painting, dark with age. The card says: THE FIRST DAY OF {OLDTOWN}.";
        else
            t = RogueText_Pick(&sExhibitCases, c, 0);
        break;
    case MOBJ_PAINTING:
        t = RogueText_Pick(&sExhibitPaintings, c, o[k].k);
        break;
    case MOBJ_CASE:
        t = RogueText_Pick(&sExhibitCases, c, o[k].k);
        break;
    case MOBJ_SLAB:
        t = "A slab of rock with a fossil pressed into it. The card says: OLDER THAN {REGION} ITSELF.";
        break;
    case MOBJ_FOSSIL:
        t = "A great fossil from near {TOWN}. The card says: OLDER THAN {REGION} ITSELF. Under it, in pencil: PROBABLY BIG.";
        break;
    case MOBJ_MACHINE:
        t = "A big old machine, all brass dials. The card says nobody remembers what it measured. It still ticks.";
        break;
    case MOBJ_STATUE:
        t = "A stone statue of a bird POK@MON. Its beak is shiny where visitors rub it for luck.";
        break;
    case MOBJ_BALL:
        t = "A POK@ BALL carved from one great stone. A sign says: PLEASE DO NOT THROW.";
        break;
    case MOBJ_DESK:
        t = o[k].k == 0 ? "A guest book lies open on the counter. The last visitor wrote: CAME FROM {NEIGHBOR}. NOT BAD."
                        : "A stack of maps of {TOWN} sits on the counter. FREE, says a sign. FREE, says another.";
        if (!RogueText_CanFill(t, c))
            t = "A guest book lies open on the counter. Every page says: LOVELY. One says: TOO QUIET.";
        break;
    }
    if (t == NULL || !RogueText_CanFill(t, c))
        t = "Photos of {TOWN} through the years. Everyone looks happy, and a little tired.";
    return RogueText_Say(out, size, t, c);
}

// ---------------------------------------------------------------------------
// The dump

#ifdef ROGUE_HOST
// For the checks: a town's museum's objects, and how many say something
// when read (all of them).
void RogueText_CountMuseum(const struct RogueRegion *r, u8 place, u32 *objs, u32 *said)
{
    struct RogueTextCtx c;
    char out[1000];
    u32 k, n;
    *objs = *said = 0;
    if (!RogueGen_TownHasMuseum(r, place))
        return;
    RogueText_Init(&c, r, place, RSPK_SIGN);
    MuseumObjs(MuseumIsLilycove(&c), &n);
    for (k = 0; k < n; k++)
    {
        RogueText_Init(&c, r, place, RSPK_SIGN);
        (*objs)++;
        *said += RogueText_MuseumExhibit(&c, k, out, sizeof(out)) != 0 && strchr(out, '{') == NULL;
    }
}

static struct RogueMap sDunDumpMap;
static u16 sDunDumpBlocks[ROGUE_MAX_BLOCKS];

static void DunDump(struct TextDump *d, struct RogueTextCtx *c, u8 speaker, const char *who, const char *bank, u32 len,
                    const char *out)
{
    if (len == 0 || out[0] == 0)
        return;
    TextDump_Line(d, c->place, speaker, who, bank, 0, out);
}

static void TextDump_Dungeons(struct TextDump *d)
{
    static const char *const kinds[RDUN_COUNT] = { "cave", "victory", "forest", "tower", "hideout", "lair", "ruins", "ship",
                                                   "plant", "ice", "volcano", "mirage" };
    const struct RogueRegion *r = d->r;
    struct RogueTextCtx c;
    struct RogueCast cast;
    char out[1000], who[80], name[48];
    u32 i, f, k, line;
    const char *species = getenv("ROGUE_TEXT_LONGEST") ? "CRABOMINABLE" : "Golbat";

    RogueGen_Cast(r, &cast);
    for (i = 0; i < r->nDungeons; i++)
    {
        const struct RogueDungeon *dun = &r->dungeons[i];
        struct RogueDungeonExtras e;
        const char *kind = dun->kind < RDUN_COUNT ? kinds[dun->kind] : "?";
        RogueGen_DungeonExtras(r, i, &e);
        RogueGen_DungeonName(r, i, name);
#define DUN_CTX(spk) (RogueText_Init(&c, r, dun->host, (spk)), c.state = d->state, c.dungeon = i)
        // The sign by its door in town, or by the woods.
        if (dun->kind == RDUN_TOWER || dun->kind == RDUN_VOLCANO || dun->kind == RDUN_ICE || dun->kind == RDUN_FOREST)
        {
            char line2[200];
            DUN_CTX(RSPK_SIGN);
            c.number = dun->kind == RDUN_FOREST && dun->exit != 0xFF;
            RogueText_DungeonSign(&c, out, sizeof(out));
            snprintf(line2, sizeof(line2), "%s/<%s>", name, out);
            snprintf(who, sizeof(who), "%s sign (%s)", kind, name);
            TextDump_Line(d, dun->host, RSPK_SIGN, who, "dungeonsign", 0, line2);
        }
        // The boss.
        if (dun->boss == RBOSS_GUARDIAN || dun->boss == RBOSS_LEGEND)
        {
            DUN_CTX(RSPK_NONE);
            c.name = species;
            snprintf(who, sizeof(who), "%s %s guardian", kind, name);
            if (dun->boss == RBOSS_GUARDIAN)
                DunDump(d, &c, RSPK_NONE, who, "guardian", RogueText_GuardianAppears(&c, out, sizeof(out)), out);
            DunDump(d, &c, RSPK_NONE, who, "gone", RogueText_BossGone(&c, dun->boss == RBOSS_LEGEND, out, sizeof(out)), out);
        }
        // What the towns round it say opens a legend's way.
        if (dun->boss == RBOSS_LEGEND)
            for (k = 0; k < 4; k++)
            {
                DUN_CTX(RSPK_FOLK);
                c.place = RogueGen_TownNear(r, dun->host);
                c.town = c.place;
                snprintf(who, sizeof(who), "%s %s gate rumor %u", kind, name, k);
                DunDump(d, &c, RSPK_FOLK, who, "gaterumor", RogueText_GateRumor(&c, k << 5, out, sizeof(out)), out);
            }
        if (dun->boss == RBOSS_MASTER)
            for (line = RMASTER_INTRO; line <= RMASTER_AFTER; line++)
            {
                DUN_CTX(RSPK_TRAINER);
                snprintf(who, sizeof(who), "%s %s master %s", kind, name, line == 0 ? "intro" : line == 1 ? "defeat" : "after");
                DunDump(d, &c, RSPK_TRAINER, who, "master", RogueText_MasterLine(&c, line, out, sizeof(out)), out);
            }
        if (e.nChoice)
            for (line = 0; line < 3; line++)
            {
                DUN_CTX(RSPK_FOLK);
                snprintf(who, sizeof(who), "%s %s choice %s", kind, name, line == 0 ? "keeper" : line == 1 ? "keeper after" : "balls");
                DunDump(d, &c, RSPK_FOLK, who, "choice", line < 2 ? RogueText_GuideLine(&c, line, out, sizeof(out))
                                                                 : RogueText_ChoiceGone(&c, e.nChoice > 2, out, sizeof(out)), out);
            }
        if (e.camp != 0xFF)
            for (line = RCAMP_OFFER; line <= RCAMP_USED; line++)
            {
                DUN_CTX(RSPK_FOLK);
                snprintf(who, sizeof(who), "%s %s camper %u", kind, name, line);
                DunDump(d, &c, RSPK_FOLK, who, "camp", RogueText_CampLine(&c, line, out, sizeof(out)), out);
            }
        if (e.explorer != 0xFF)
            for (line = REXPLORER_ASK; line <= REXPLORER_FULL; line++)
            {
                DUN_CTX(RSPK_EXPLORER);
                snprintf(who, sizeof(who), "%s %s explorer %u", kind, name, line);
                DunDump(d, &c, RSPK_EXPLORER, who, "explorer", RogueText_ExplorerLine(&c, line, out, sizeof(out)), out);
            }
        if (e.prize != 0xFF)
        {
            DUN_CTX(RSPK_NONE);
            snprintf(who, sizeof(who), "%s %s prize", kind, name);
            DunDump(d, &c, RSPK_NONE, who, RogueGen_LoreAt(r, LORE_WAY_DIARY, i) >= 0 ? "prize+diary" : "prize",
                    RogueText_PrizeLine(&c, out, sizeof(out)), out);
        }
        // Things to read on its floors. (The plain graves' lines, each
        // once a dungeon: a tower has dozens of graves, and nobody reads
        // them all.)
        u32 plainSeen = 0;
        if (dun->kind == RDUN_TOWER || dun->kind == RDUN_SHIP || dun->kind == RDUN_PLANT)
            for (f = 1; f <= dun->nFloors; f++)
            {
                u8 id = RogueGen_FloorRoom(r, i, f), gen = 0;
                const struct RogueRoom *room;
                u32 x, y;
                if (id == 0xFF)
                    continue;
                room = &sRogueRooms[id];
                for (y = 0; y < room->h; y++)
                    for (x = 0; x < room->w; x++)
                    {
                        u8 kk;
                        bool8 lore;
                        u8 t = RogueGen_FloorThingAt(r, i, f, x, y, &kk, &lore);
                        static const char *const things[] = { "-", "grave", "bin", "terminal", "generator", "oldgrave" };
                        if (t == RTHING_NONE || (t == RTHING_TERMINAL && x > 0
                         && ThingOfMetatile(dun->kind, room->blocks[y * room->w + x - 1] & 0x3FF) == t))
                            continue;
                        if (t == RTHING_GENERATOR && gen++)
                            continue;
                        if (t == RTHING_PLAIN_GRAVE)
                        {
                            if ((plainSeen >> (kk % sPlainGraves.count)) & 1)
                                continue;
                            plainSeen |= 1u << (kk % sPlainGraves.count);
                        }
                        DUN_CTX(RSPK_SIGN);
                        snprintf(who, sizeof(who), "%s %s %uF %s %u (%u,%u)%s", kind, name, f, things[t], kk, x, y,
                                 lore ? " keeper" : "");
                        DunDump(d, &c, RSPK_SIGN, who, lore ? "lore-grave" : things[t],
                                RogueText_FloorThing(&c, f, t, kk, lore, out, sizeof(out)), out);
                    }
            }
#undef DUN_CTX
    }
    // The camps by the road: Victory Road's and LATE CAMP's.
    for (i = 0; i < r->nPlaces; i++)
        if (RogueGen_RoadCampFlag(r, i) >= 0)
            for (line = RCAMP_OFFER; line <= RCAMP_USED; line++)
            {
                RogueText_Init(&c, r, i, RSPK_FOLK);
                c.state = d->state;
                snprintf(who, sizeof(who), "road camper %u", line);
                DunDump(d, &c, RSPK_FOLK, who, "camp", RogueText_CampLine(&c, line, out, sizeof(out)), out);
            }
    // The towns: rumors (what the game tells when a townsperson's topic is
    // the dungeons), the POKéMON CENTER's picture, child and explorer, the
    // paintings at home, a museum's exhibits.
    for (i = 0; i < r->nPlaces; i++)
    {
        const struct RoguePlace *p = &r->places[i];
        s32 ph;
        if (p->kind != RPLACE_TOWN || p->type == RTOWN_SUMMIT)
            continue;
        RogueGen_GenerateMap(r, i, &sDunDumpMap, sDunDumpBlocks);
        // (The rumors are in the townsfolk's dump: RogueText_DumpRumor.)
        ph = RogueGen_PaintingHouse(r, i, sDunDumpMap.interiors, sDunDumpMap.nHouses);
        RogueText_Init(&c, r, i, RSPK_SIGN);
        c.state = d->state;
        DunDump(d, &c, RSPK_SIGN, "center picture", "painting", RogueText_Painting(&c, 0xFF, ph, out, sizeof(out)), out);
        for (k = 0; k < sDunDumpMap.nHouses; k++)
            if (sDunDumpMap.interiors[k] == RINT_HOME && RogueGen_HomeRoomHasPainting(RogueGen_HomeRoom(r, i, k)))
            {
                RogueText_Init(&c, r, i, RSPK_SIGN);
                c.state = d->state;
                snprintf(who, sizeof(who), "house %u painting", k);
                DunDump(d, &c, RSPK_SIGN, who, "painting", RogueText_Painting(&c, k, ph, out, sizeof(out)), out);
            }
        RogueText_Init(&c, r, i, RSPK_FOLK);
        c.state = d->state;
        DunDump(d, &c, RSPK_FOLK, "center child", "rhyme", RogueText_Rhyme(&c, out, sizeof(out)), out);
        if (cast.explorerTown == i && d->state != NULL)
            for (k = 0; k < 2; k++)
            {
                RogueText_Init(&c, r, i, RSPK_EXPLORER);
                c.state = d->state;
                c.number = k;
                snprintf(who, sizeof(who), "center explorer %u", k + 1);
                DunDump(d, &c, RSPK_EXPLORER, who, "explorer", RogueText_ExplorerLine(&c, REXPLORER_TOWN, out, sizeof(out)), out);
            }
        // A museum's every exhibit (the signature's), or every town's.
        for (k = 0; k < (getenv("ROGUE_TEXT_MUSEUMS") || RogueGen_TownHasMuseum(r, i) ? 16u : 0u); k++)
        {
            RogueText_Init(&c, r, i, RSPK_SIGN);
            c.state = d->state;
            snprintf(who, sizeof(who), "museum case %u", k);
            if (RogueText_MuseumExhibit(&c, k, out, sizeof(out)) == 0)
                break;
            DunDump(d, &c, RSPK_SIGN, who, "museum", 1, out);
        }
    }
}
#endif
