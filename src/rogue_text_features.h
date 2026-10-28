// Route features' people (src/rogue_gen_features.h): the Day Care couple, the
// BERRY MASTER, the glassblower, the Seashore House, the sleeping SNORLAX and
// the child who waits by it, the battling family, the gauntlet's prize man,
// the fishing guru and the old sailor; the double battle pairs' lines; the
// gauntlet trainers' count; the feature signs; and what route walkers say of
// their route's facts (its local name, its feature, who lives there).
// Included at the end of src/rogue_gen_text.h; the game asks here
// (src/rogue_town.c, src/rogue.c) and gen_host --script prints all of it
// (TextDump_Features).
//
// {NICK} is ctx.route's local name ("the LONG FORD"), {FEATURE} its feature
// ("the DAY CARE"), {RESIDENT} the feature's person (OLWEN; the battling
// family's name, HARDING). {NAME} is the old sailor's Wingull.

// --- The people of each feature, talking (not the host's gift or reward) ---

TEXT_BANK(sFeatTalkDayCare,
    "Our yard is full of POK@MON whose TRAINERS are off on the road. We feed them all.",
    "My partner does the talking. I do the feeding. It works out.",
    "That one in the yard snores. The other one pretends not to hear.",
    "Raising POK@MON isn't hard. It's just every single day, that's all.",
    "TRAINERS from {TOWN} leave their POK@MON with us and forget to come back. Can you imagine?",
    "Folks call this stretch {NICK}. On account of us, I suppose.");
TEXT_BANK(sFeatTalkBerry,
    "My partner treats every berry tree like family. Better than family, some days.",
    "Take what's ripe out in the garden. Plant one back if you've a berry to spare.",
    "{RESIDENT} has grown berries on {ROUTE} since before {TOWN} had a POK@MON CENTER.",
    "Up before the birds to water the garden. Every day. I've stopped asking why.",
    "We call the soil out front black gold. It's mostly mud, but it's our mud.");
TEXT_BANK(sFeatTalkWorkshop,
    "The ash falls on {ROUTE} every day. We sweep it up and melt it down.",
    "One flute takes a whole sack of ash. I carry a lot of sacks.",
    "Hold a flute up to the light. You can see the ash glitter in it.",
    "{RESIDENT} says glass remembers the fire. I say it remembers my burnt fingers.");
TEXT_BANK(sFeatTalkGuru,
    "My sibling lives in the hut and calls themself the guru. I taught them everything.",
    "Shh. Something's biting... Never mind. Something was biting.",
    "The water here is deep and cold. That's where the good ones hide.",
    "Folks call this place {NICK}, on account of us. Fishing all day, every day.",
    "Got a ROD? Then sit down. The fish come to those who wait.");

// --- The hosts, once their gift or reward is given (or with none to give) ---

TEXT_BANK(sFeatDoneDayCare,
    "We've looked after POK@MON on {ROUTE} since we were young. They all remember us.",
    "Is that little one well? Good. You have kind hands, dear.",
    "My partner says the yard needs a bigger fence. They've been saying it since spring.");
TEXT_BANK(sFeatDoneBerry,
    "A berry a day keeps a POK@MON on its feet. Ha! I made that up.",
    "Plant a berry and wait. Patience is the whole secret. Well, and water.",
    "Pick a tree bare and the soil's still good. Plant a berry in it and it grows again. Funny thing, berries.");
TEXT_BANK(sFeatDoneWorkshop,
    "Glass is only ash, fire and patience.",
    "Careful with that flute! It's tough, but it's still glass.",
    "I sold a flute to a famous TRAINER once. Or someone who said they were famous.");
TEXT_BANK(sFeatDoneSeashore,
    "The sea air makes everyone thirsty. Good thing we sell SODA POP!",
    "Come back any time! The TRAINERS here always want a rematch.",
    "I named this place myself. The SEASHORE HOUSE. Took me a week.");
TEXT_BANK(sFeatDoneFamily,
    "We battle every TRAINER who comes down {ROUTE}. Not many get past all four of us!",
    "The little one wants a rematch already. They'll have to wait for spring.",
    "Sit a while, dear. The {RESIDENT} house is always open to a good TRAINER.");
TEXT_BANK(sFeatDoneGauntlet,
    "That NUGGET is real gold, you know. Any MART will buy it.",
    "Lots of TRAINERS try {NICK}. Not many make it to me.",
    "My job? I stand at the end of {NICK} and hand out the prize. It's a good job.");
TEXT_BANK(sFeatDoneGuru,
    "Cast quietly. The fish on {ROUTE} hear everything.",
    "The biggest one I ever saw got away. It's still out there somewhere.",
    "My sibling fishes from the bank and says it's better. We've argued about it for years.");
TEXT_BANK(sFeatDoneSailor,
    "The sea past {NEXT} is rough in a storm. Hug the islands and you'll be fine.",
    "{NAME} and I sailed together for years. Now we sit on the porch and watch the boats.",
    "Never whistle on a boat. I did once. Lost my hat, and very nearly the boat.");

static const struct RogueTextBank *const sFeatTalk[RFEAT_COUNT] =
{
    [RFEAT_DAYCARE] = &sFeatTalkDayCare, [RFEAT_BERRY] = &sFeatTalkBerry, [RFEAT_WORKSHOP] = &sFeatTalkWorkshop,
    [RFEAT_GURU] = &sFeatTalkGuru,
};
static const struct RogueTextBank *const sFeatDone[RFEAT_COUNT] =
{
    [RFEAT_DAYCARE] = &sFeatDoneDayCare, [RFEAT_BERRY] = &sFeatDoneBerry, [RFEAT_WORKSHOP] = &sFeatDoneWorkshop,
    [RFEAT_SEASHORE] = &sFeatDoneSeashore, [RFEAT_FAMILY] = &sFeatDoneFamily, [RFEAT_GAUNTLET] = &sFeatDoneGauntlet,
    [RFEAT_GURU] = &sFeatDoneGuru, [RFEAT_SAILOR] = &sFeatDoneSailor,
};

// Offering the gift POK@MON or the reward. The game names the gift after
// ("{PLAYER} received ..."), so the offer need not.
static const char *const sFeatGift[RFEAT_COUNT] =
{
    [RFEAT_DAYCARE]  = "Hello, dear. A TRAINER left this one with us long ago and never came back.|It's grown restless. Would you take it on your journey?",
    [RFEAT_SAILOR]   = "Ahoy! I'm {RESIDENT}. I sailed these waters for fifty years with {NAME}.|{NAME} had a chick this spring, and it wants to see the world. Will you take it?",
    [RFEAT_BERRY]    = "Ah, a TRAINER! I'm {RESIDENT}, the BERRY MASTER. Take these, fresh from my best tree!",
    [RFEAT_WORKSHOP] = "Welcome to the GLASS WORKSHOP! I'm {RESIDENT}. I blow glass from the ash of {ROUTE}.|Here, take one of my flutes. Play it when a POK@MON needs a lift.",
    [RFEAT_SEASHORE] = "You beat every TRAINER in the house! Here, a cold SODA POP for each of your POK@MON. Well, six.",
    [RFEAT_FAMILY]   = "Four of us, and you beat every one! The {RESIDENT} family salutes you. Take this, dear!",
    [RFEAT_GAUNTLET] = "You beat every TRAINER on {NICK}! Congratulations! Here's your prize!",
    [RFEAT_GURU]     = "Now that's a fine catch from {ROUTE}! These LURE BALLS were made for fishing. They're yours.",
};
static const char *const sFeatGiftAfter[RFEAT_COUNT] =
{
    [RFEAT_DAYCARE] = "Look how happy it is! Visit the DAY CARE whenever you pass, dear.",
    [RFEAT_SAILOR]  = "Take care of that one. It'll always know the way back to {ROUTE}.",
};
// Before the reward is earned.
// The fishing guru once the route's one catch went to something else: any
// WATER POKeMON the player caught will do.
static const char *const sGuruNotYetAny = "I'm {RESIDENT}, the fishing guru. You've had your catch on {ROUTE} already? Then any WATER POK@MON you caught will do.|Bring one in your party or your BOX, and I'll make it worth your while!";
static const char *const sGuruGiftAny = "A WATER POK@MON from elsewhere, eh? Still a fine catch. These LURE BALLS were made for fishing. They're yours.";

static const char *const sFeatNotYet[RFEAT_COUNT] =
{
    [RFEAT_SEASHORE] = "Welcome to the SEASHORE HOUSE! Beat every TRAINER here and the SODA POP is on me!",
    [RFEAT_FAMILY]   = "Beat the rest of the {RESIDENT} family first, dear. Then come and see me!",
    [RFEAT_GAUNTLET] = "This is {NICK}! Beat every TRAINER on the crossing, and a fabulous prize is yours!",
    [RFEAT_GURU]     = "I'm {RESIDENT}, the fishing guru. Show me a WATER POK@MON you caught here on {ROUTE}.|In your party or your BOX, I don't mind. Do that, and I'll make it worth your while!",
};
// The feature's sign: its name and a motto.
static const char *const sFeatSign[RFEAT_COUNT] =
{
    [RFEAT_DAYCARE]  = "POK@MON DAY CARE\nWe raise POK@MON with care.",
    [RFEAT_BERRY]    = "BERRY MASTER'S HOUSE\nPick a berry, leave a smile.",
    [RFEAT_WORKSHOP] = "GLASS WORKSHOP\nGlass blown from falling ash.",
    [RFEAT_SEASHORE] = "SEASHORE HOUSE\nCold drinks and hot battles!",
    [RFEAT_SNORLAX]  = "CAUTION!\nA POK@MON sleeps in the way.",
    [RFEAT_FAMILY]   = "THE {RESIDENT} FAMILY\nTRAINERS, come in and battle!",
    [RFEAT_GAUNTLET] = "{^NICK}\nA prize for beating them all!",
    [RFEAT_GURU]     = "FISHING HUT\nAll anglers welcome!",
    [RFEAT_SAILOR]   = "{RESIDENT}'S COTTAGE\nAn old sailor's home.",
};

// The sleeping SNORLAX and the child who waits by it.
static const char *const sSnorlaxLine = "A huge POK@MON is fast asleep in the way. It's snoring like thunder!";
TEXT_BANK(sFeatKid,
    "It's been asleep there for days! My dad's POK@ FLUTE wakes anything up. But I'm too scared.|Will you play it?",
    "I want to get past, but it won't move! Dad gave me his POK@ FLUTE, but my hands are shaking.|Could you play it for me?");
TEXT_BANK(sFeatKidAfter,
    "It woke up! I bet it comes back next spring, to the same spot.",
    "Wow, you were brave! I'm still shaking. Good shaking, though.",
    "Now I can get to the berries back there. I've been waiting all week!");

// --- Route walkers: their route's facts ---

TEXT_BANK(sWalkNick,
    "Folks call this stretch {NICK}. Ask anyone in {TOWN}.",
    "They call it {NICK} around here. The name's older than the road.",
    "{^NICK} is my favorite walk. The air's different here.",
    "I've walked {NICK} for ten years. It still surprises me.",
    "Only the maps say {ROUTE}. Everyone in {TOWN} says {NICK}.",
    "My kids named this bit {NICK}. Now it's on the signs. They're very proud.",
    "Meet me at {NICK}, people say in {TOWN}. Everyone knows where they mean.",
    "I proposed on {NICK}. Well, I tried. A POK@MON sat on the ring.",
    "{^NICK} looks different every season. I like it best in autumn.",
    "My father walked {NICK} with his POK@MON. I walk it with mine.",
    "Nobody remembers why it's called {NICK}. Everybody has a theory.",
    "{^NICK}! Best stretch of road in {REGION}. I'll argue with anyone.",
    "We had a picnic on {NICK} last week. A POK@MON had most of it.",
    "{^NICK} at sunset. That's all I'll say. Go and see.",
    "My dad used to race me down {NICK}. He still says he let me win.",
    "Every TRAINER from {TOWN} learns to battle on {NICK}. I did. I lost a lot.",
    "If you're lost, ask for {NICK}. Everyone round here can point you to it.",
    "I've lived beside {NICK} all my life. I still can't spell it.",
    "There's a song about {NICK}. It's not a good song. Everyone knows the words.",
    "I walk my POK@MON on {NICK} each day. Really, it walks me.",
    "On a clear day you can see all of {NICK} from the top of {TOWN}.",
    "Somebody carved {NICK} into a tree here a hundred years ago. You can still read it.",
    "The postman says {NICK} is the nicest part of his round. He says that about every part.",
    "I come to {NICK} to think. It's quiet, and the view helps.",
    "My uncle named his POK@MON after {NICK}. It's a long name for a small POK@MON.",
    "They hold a race along {NICK} every spring. I came in second last year!",
    "Watch your step on {NICK}. The POK@MON here don't move for anyone.",
    "Wedding photos in {TOWN} are always taken on {NICK}. Ours were too.",
    "I painted {NICK} once. It came out looking more like {TOWN}.",
    "The first time I saw {NICK}, I decided to live here. I've never left.",
    "Children from {TOWN} learn to ride bikes on {NICK}. You can tell by the scratches on the trees.",
    "{^NICK} smells of rain, even on sunny days.",
    "I found my first POK@MON on {NICK}. Or it found me. We argue about it.",
    "The old folks of {TOWN} sit out on {NICK} every evening and talk about the weather.",
    "There's a bench on {NICK} with my name carved on it. My kids did that.",
    "Strangers always ask how {NICK} got its name. I make up a new story each time.",
    "Some say {NICK} is haunted. It isn't. It's just a very shy POK@MON.",
    "{^NICK} was a muddy track long ago. It's still muddy, but now it's famous.",
    "My mother walked me down {NICK} when I was a baby. Now I walk her. She still sets the pace.",
    "I know every stone on {NICK}. That one's new. Who put that there?");
TEXT_BANK(sWalkDayCare,
    "The couple at {FEATURE} raised my first POK@MON. Well, they fed it.",
    "I walk past {FEATURE} every day. The POK@MON in the yard know me now.");
TEXT_BANK(sWalkBerry,
    "{RESIDENT}'s berries are the sweetest in {REGION}. Try one if you can.",
    "I came all the way down {ROUTE} for one of {RESIDENT}'s berries. Worth it.");
TEXT_BANK(sWalkWorkshop,
    "My flute came from {FEATURE}. It sounds like the wind over the ash.",
    "{RESIDENT} at {FEATURE} can blow glass into any shape you ask for.");
TEXT_BANK(sWalkSeashore,
    "{^FEATURE} is full of TRAINERS, and the SODA POP is cold.",
    "I lost to a kid in a swim ring at {FEATURE}. A kid!");
TEXT_BANK(sWalkSnorlax,
    "Something huge is asleep up the way. People walk round it. It doesn't notice.",
    "That big sleeper is back. Every year it picks the same spot on {ROUTE}.");
TEXT_BANK(sWalkFamily,
    "The {RESIDENT} family battles anyone who knocks. Hardly anyone beats all four.",
    "Don't knock on the {RESIDENT} house unless your POK@MON are rested.");
TEXT_BANK(sWalkGauntlet,
    "Every TRAINER on {NICK} is after the same thing: you.",
    "Beat them all on {NICK} and there's a prize at the end. I made it to three.");
TEXT_BANK(sWalkGuru,
    "Want to fish? Ask {RESIDENT} at the hut. Bring patience.",
    "{RESIDENT} once caught a fish as long as a boat. Well, a small boat.");
TEXT_BANK(sWalkSailor,
    "Old {RESIDENT} sailed the whole coast of {REGION} in a boat they built.",
    "{RESIDENT} at the cottage knows every rock in the sea. Knows them by name.");
static const struct RogueTextBank *const sWalkFeature[RFEAT_COUNT] =
{
    [RFEAT_DAYCARE] = &sWalkDayCare, [RFEAT_BERRY] = &sWalkBerry, [RFEAT_WORKSHOP] = &sWalkWorkshop,
    [RFEAT_SEASHORE] = &sWalkSeashore, [RFEAT_SNORLAX] = &sWalkSnorlax, [RFEAT_FAMILY] = &sWalkFamily,
    [RFEAT_GAUNTLET] = &sWalkGauntlet, [RFEAT_GURU] = &sWalkGuru, [RFEAT_SAILOR] = &sWalkSailor,
};

// --- The gauntlet's trainers, after a battle: how many are left ({NAME}, a word) ---

TEXT_BANK(sGauntletLeft,
    "That's one more of us down. {^NAME} left on {NICK}!",
    "Only {NAME} of us left on {NICK}. Keep going!",
    "You beat me? Then {NAME} more stand between you and the prize.",
    "{^NAME} more to go on {NICK}. The next one's tougher than me!");
TEXT_BANK(sGauntletDone,
    "You beat every one of us on {NICK}! Go and claim your prize at the end.",
    "That's all of us! The prize waits at the far end of {NICK}.");

// --- The feature trainers: intro, defeat and after, a set at a time ---

struct FeatTrainerSet { const char *line[3]; };

// The pairs, by kind (RPAIR_*), three sets each.
static const struct FeatTrainerSet sPairSets[RPAIR_COUNT][3] =
{
    [RPAIR_TWINS] = {
        { { "We're twins! We always battle together!", "We lost together, too...", "Next time we'll practice our teamwork. And our matching outfits." } },
        { { "I'm the left one. She's the right one. Battle us both!", "Left and right... both lost!", "Mom can't tell us apart. Can you?" } },
        { { "Two TRAINERS, two POK@MON, one battle!", "Double trouble... for us!", "We're going to practice on {ROUTE} all day!" } },
    },
    [RPAIR_YOUNG_COUPLE] = {
        { { "We're on our first trip together! Let's make it a battle to remember!", "Well, that's one way to remember it.", "We met on {ROUTE}. We come back every year." } },
        { { "Do you mind? We were having a picnic. Oh, all right, a battle!", "The picnic's ruined anyway.", "Next time bring a sandwich and we'll call it even." } },
        { { "We train as a team. Ready?", "We still make a good team. Just not today.", "We're walking all the way to {NEXT}. Hand in hand." } },
    },
    [RPAIR_OLD_COUPLE] = {
        { { "We've battled side by side for forty years. Shall we make it forty-one?", "Oh my, the young ones are strong!", "All these years, and we still argue over who gave the wrong order." } },
        { { "Our POK@MON are older than you, dear. Don't let that fool you!", "Hmph. They're tired, that's all.", "We walk {ROUTE} every day. It keeps the knees working." } },
        { { "A battle, dear? Lovely. It's been a while!", "Well! Wasn't that fun?", "Come for tea some time. We live just past {TOWN}." } },
    },
    [RPAIR_SIS_AND_BRO] = {
        { { "My sister and I are a team! I'm the brains.", "So much for brains...", "My sister says it was my fault. It was a little bit my fault." } },
        { { "Big brother says I'm too little to battle. I'll show him!", "Aww... we both lost!", "We're going swimming after this. Losers buy the snacks. Oh no." } },
        { { "We grew up right here on {ROUTE}! Home turf!", "Lost on our own turf...", "Mom's calling us in for dinner. We'll get you next time!" } },
    },
    [RPAIR_SR_AND_JR] = {
        { { "My daughter and I battle as one! Right, sweetie?", "We need more practice, sweetie.", "She wants to be a GYM LEADER one day. I believe her." } },
        { { "Mommy, can we battle that TRAINER? Pleeease?", "That was fun! Let's lose again!", "We come out to {ROUTE} every weekend. It's our special day." } },
        { { "Teaching my little one the ropes. Go easy... no, don't go easy!", "A good lesson, wasn't it?", "Remember this battle, sweetie. That's a real TRAINER." } },
    },
};

// The battling family: the parent who opens the door, the other parent,
// the child and the grandparent who closes (two sets each; the
// grandparent's after line is the host's).
static const struct FeatTrainerSet sFamilySets[ROGUE_FAMILY_TRAINERS][2] =
{
    {
        { { "Welcome to the {RESIDENT} house! We battle every guest. You're a guest!", "Well! You're no ordinary guest.", "The rest of my family is tougher than me. Much tougher!" } },
        { { "Our family has battled on {ROUTE} for three generations. Let's see what you've got!", "Beaten in my own living room!", "Grandma has never lost in this house. Well, almost never." } },
    },
    {
        { { "My partner says I'm the soft one in this family. Let's prove them wrong!", "Oh my! You're very good.", "Our little one has practiced all week for a guest like you." } },
        { { "A {RESIDENT} never turns down a battle. Shall we?", "My, my. Well done!", "Sit and rest a while. You've earned it." } },
    },
    {
        { { "I'm the best in the family! Except Grandma. And Mom. But I'm close!", "No fair! I was just warming up!", "Grandma's the strongest. Good luck. You'll need it!" } },
        { { "I've battled every TRAINER who came down {ROUTE}! You're next!", "Aww, I lost!", "Mom says losing makes you stronger. I'm getting really strong." } },
    },
    {
        { { "So you've beaten my whole family. Let's see you beat me, dear.", "Oh! Well, I'll be.", "" } },
        { { "I've battled since before your parents were born. Ready?", "My, my. The young ones are strong these days.", "" } },
    },
};

// The Seashore House's three: a tuber, a beauty and a sailor.
static const struct FeatTrainerSet sSeashoreSets[ROGUE_SEASHORE_TRAINERS][2] =
{
    {
        { { "I'm the SEASHORE HOUSE's swimming champion! Splash!", "My floatie popped!", "The owner gives SODA POP to whoever beats us all. I just want one." } },
        { { "Beat me and I'll share my snack! Lose and you share yours!", "Aww. Here, have a cracker.", "I've been here all summer. I'm a bit sunburnt." } },
    },
    {
        { { "Battling in the sea breeze ruins my hair. You'd better be worth it!", "My hair AND my pride...", "I come to the SEASHORE HOUSE for the view. The battles are a bonus." } },
        { { "Don't splash, darling. This swimsuit is new.", "How rude! ...Well fought.", "Everyone on {ROUTE} knows me. Now they'll know you too." } },
    },
    {
        { { "Ahoy! Nobody leaves the SEASHORE HOUSE without a battle!", "Sunk in port!", "Get a SODA POP from the owner once you've beaten us all. Best on the coast." } },
        { { "I'm on shore leave. A battle's the best kind of rest!", "Back to the ship for me.", "The sea's calm today. Calm sea, sore loser. That's me." } },
    },
};

static void FeatCtx(struct RogueTextCtx *c, const struct RogueRegion *r, u8 place, u8 speaker)
{
    RogueText_Init(c, r, place, speaker);
    c->route = place;
    c->name = RogueGen_SailorPet(r);
}

u32 RogueText_FeatLine(const struct RogueRegion *r, u8 place, u8 what, u32 k, char *out, u32 size)
{
    struct RogueTextCtx c;
    u8 feat = RogueGen_RouteFeature(r, place);
    const char *t = NULL;
    out[0] = 0;
    if (feat == RFEAT_NONE)
        return 0;
    FeatCtx(&c, r, place, what == RFEATTEXT_SIGN ? RSPK_SIGN : RSPK_FOLK);
    switch (what)
    {
    case RFEATTEXT_TALK:
        if (sFeatTalk[feat])
            t = RogueText_Pick(sFeatTalk[feat], &c, k);
        else if (sFeatDone[feat])
            t = RogueText_Pick(sFeatDone[feat], &c, k);
        break;
    case RFEATTEXT_DONE:
        if (sFeatDone[feat])
            t = RogueText_Pick(sFeatDone[feat], &c, k);
        break;
    case RFEATTEXT_GIFT:
        // (k 1: the fishing guru, once the route's one catch is gone.)
        t = feat == RFEAT_GURU && k == 1 ? sGuruGiftAny : sFeatGift[feat];
        break;
    case RFEATTEXT_GIFT_AFTER:
        t = sFeatGiftAfter[feat];
        if (t == NULL && sFeatDone[feat])
            t = RogueText_Pick(sFeatDone[feat], &c, k);
        break;
    case RFEATTEXT_NOT_YET:  t = feat == RFEAT_GURU && k == 1 ? sGuruNotYetAny : sFeatNotYet[feat]; break;
    case RFEATTEXT_SIGN:     t = sFeatSign[feat]; break;
    case RFEATTEXT_SNORLAX:  t = feat == RFEAT_SNORLAX ? sSnorlaxLine : NULL; break;
    case RFEATTEXT_KID:      t = feat == RFEAT_SNORLAX ? RogueText_Pick(&sFeatKid, &c, k) : NULL; break;
    case RFEATTEXT_KID_AFTER: t = feat == RFEAT_SNORLAX ? RogueText_Pick(&sFeatKidAfter, &c, k) : NULL; break;
    }
    if (t == NULL)
        return 0;
    return RogueText_Say(out, size, t, &c);
}

// A feature trainer's line (RTRAINER_INTRO, _DEFEAT, _AFTER).
u32 RogueText_FeatTrainerLine(const struct RogueRegion *r, u32 slot, u8 field, char *out, u32 size)
{
    struct RogueTextCtx c;
    u8 place = RogueGen_FeatureSlotPlace(r, slot);
    u32 set = RogueRng_Hash(r->seed, FEAT_NAME_SALT + 16 + slot);
    const char *t;
    out[0] = 0;
    if (place == 0xFF || field > 2)
        return 0;
    FeatCtx(&c, r, place, RSPK_TRAINER);
    if (slot < ROGUE_SLOT_FAMILY_0)
        t = sPairSets[RogueGen_PairKind(r, slot - ROGUE_SLOT_PAIR_0) % RPAIR_COUNT][set % 3].line[field];
    else if (slot < ROGUE_SLOT_SEASHORE_0)
        t = sFamilySets[slot - ROGUE_SLOT_FAMILY_0][set & 1].line[field];
    else
        t = sSeashoreSets[slot - ROGUE_SLOT_SEASHORE_0][set & 1].line[field];
    if (t == NULL || t[0] == 0)
        return 0;
    // A line that cannot be filled here falls back to one that can.
    if (!RogueText_CanFill(t, &c))
        t = field == 0 ? "Let's battle!" : field == 1 ? "You win!" : "That was a good battle.";
    return RogueText_Say(out, size, t, &c);
}

// What a gauntlet trainer says after the battle: left of the route's
// trainers still to beat.
u32 RogueText_GauntletLine(const struct RogueRegion *r, u8 place, u32 left, u32 k, char *out, u32 size)
{
    struct RogueTextCtx c;
    const char *t;
    FeatCtx(&c, r, place, RSPK_TRAINER);
    // How many, as a word: "four left".
    c.name = sTextNumbers[left < 9 ? left : 8];
    t = left ? RogueText_Pick(&sGauntletLeft, &c, k) : RogueText_Pick(&sGauntletDone, &c, k);
    out[0] = 0;
    return t ? RogueText_Say(out, size, t, &c) : 0;
}

// A route walker's line about the route's facts, or 0 if it has none to
// tell (no local name, no feature): the feature half the time, its name the
// other half.
u32 RogueText_RouteFactLine(struct RogueTextCtx *c, u32 k, char *out, u32 size)
{
    const struct RogueRegion *r = c->r;
    u8 place = c->place, feat;
    char nick[32];
    const char *t = NULL;
    out[0] = 0;
    if (place >= r->nPlaces || r->places[place].kind != RPLACE_ROUTE)
        return 0;
    feat = RogueGen_RouteFeature(r, place);
    c->route = place;
    c->name = RogueGen_SailorPet(r);
    if (feat != RFEAT_NONE && sWalkFeature[feat] && (!(k & 1) || !RogueGen_RouteNick(r, place, nick)))
        t = RogueText_Pick(sWalkFeature[feat], c, k >> 1);
    if (t == NULL && RogueGen_RouteNick(r, place, nick))
    {
        // Each route its own: the routes with a name of their own, in turn.
        u32 i, rank = 0;
        char other[32];
        for (i = 0; i < place; i++)
            rank += r->places[i].kind == RPLACE_ROUTE && RogueGen_RouteNick(r, i, other);
        t = RogueText_PickRegion(&sWalkNick, c, rank * 2 + (k & 1));
    }
    if (t == NULL)
        return 0;
    return RogueText_Say(out, size, t, c);
}

#ifdef ROGUE_HOST
// Every route feature's lines, the pairs', and the walkers' route facts.
void TextDump_Features(struct TextDump *d)
{
    const struct RogueRegion *r = d->r;
    char out[600];
    u32 i, k, f;
    for (i = 0; i < r->nPlaces; i++)
    {
        struct RogueTextCtx c;
        u8 feat = RogueGen_RouteFeature(r, i);
        s8 pair = RogueGen_RoutePair(r, i);
        if (r->places[i].kind != RPLACE_ROUTE)
            continue;
        for (k = 0; k < 2; k++)
        {
            RogueText_Init(&c, r, i, RSPK_FOLK);
            c.state = d->state;
            if (RogueText_RouteFactLine(&c, k, out, sizeof(out)))
                TextDump_Line(d, i, RSPK_FOLK, "route walker", "sWalk", k, out);
        }
        if (pair >= 0)
            for (f = 0; f < 3; f++)
            {
                RogueText_FeatTrainerLine(r, ROGUE_SLOT_PAIR_0 + pair, f, out, sizeof(out));
                TextDump_Line(d, i, RSPK_TRAINER, "pair", "sPairSets", f, out);
            }
        if (feat == RFEAT_NONE)
            continue;
        for (f = 0; f < RFEATTEXT_COUNT; f++)
        {
            static const char *const sWhat[RFEATTEXT_COUNT] = { "feature person", "feature host", "feature gift", "feature gift after",
                                                                 "feature not yet", "feature sign", "snorlax", "snorlax kid", "snorlax kid after" };
            // (Only what someone there says: a feature with no talkers has
            // no talk lines, and the host's after-gift line is its own.)
            if ((f == RFEATTEXT_TALK && !sFeatTalk[feat]) || (f == RFEATTEXT_GIFT_AFTER && !sFeatGiftAfter[feat]))
                continue;
            for (k = 0; k < (f == RFEATTEXT_TALK || f == RFEATTEXT_DONE ? 3u
                             : feat == RFEAT_GURU && (f == RFEATTEXT_GIFT || f == RFEATTEXT_NOT_YET) ? 2u : 1u); k++)
                if (RogueText_FeatLine(r, i, f, k, out, sizeof(out)))
                    TextDump_Line(d, i, f == RFEATTEXT_SIGN ? RSPK_SIGN : RSPK_FOLK, sWhat[f], "sFeat", k, out);
        }
        if (feat == RFEAT_FAMILY)
            for (k = 0; k < ROGUE_FAMILY_TRAINERS * 3; k++)
                if (RogueText_FeatTrainerLine(r, ROGUE_SLOT_FAMILY_0 + k / 3, k % 3, out, sizeof(out)))
                    TextDump_Line(d, i, RSPK_TRAINER, "family", "sFamilySets", k, out);
        if (feat == RFEAT_SEASHORE)
            for (k = 0; k < ROGUE_SEASHORE_TRAINERS * 3; k++)
                if (RogueText_FeatTrainerLine(r, ROGUE_SLOT_SEASHORE_0 + k / 3, k % 3, out, sizeof(out)))
                    TextDump_Line(d, i, RSPK_TRAINER, "seashore", "sSeashoreSets", k, out);
        if (feat == RFEAT_GAUNTLET)
            for (k = 0; k <= 4; k++)
                if (RogueText_GauntletLine(r, i, 4 - k, k, out, sizeof(out)))
                    TextDump_Line(d, i, RSPK_TRAINER, "gauntlet", "sGauntlet", k, out);
    }
}
#endif
