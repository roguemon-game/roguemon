// Town signatures' people (src/rogue_gen_signatures.h): what the fishers,
// bathers, scientists, merchants, ranchers and the rest say, and the lines of
// their small gifts, the hot spring's soak, the market stall, the landmark's
// sign and a museum's exhibits. Included at the end of src/rogue_gen_text.h;
// the game asks here (src/rogue_town.c) and gen_host --script prints all of it
// (TextDump_Signatures).
//
// The landmark is on the map in these towns, so a line may point to it.
// {LANDMARK}, {TRADE_FOLK}, {GOODS} and {LOCALMON} are the town's own facts
// (src/rogue_gen_facts.h), which the signature drives (FactsSignature).

TEXT_BANK(sSigTalkFishing,
    "The fish bite best at dawn. I've been out here since before it!",
    "Fishing's all patience. The water always gives in the end. My knees don't, though.",
    "My father fished off {LANDMARK}. His father fished off the rocks.",
    "Got a ROD? Then you're one of us. Cast it anywhere there's water!",
    "I caught a MAGIKARP this long once. Nobody believes me. It was THIS long!",
    "The {TRADE_FOLK} here eat fish for breakfast, lunch and dinner. I'd like a BERRY now and then.");
TEXT_BANK(sSigTalkHotSpring,
    "Ahh... The hot spring melts every ache right out of you.",
    "Folks come from all over just to soak in {LANDMARK}.",
    "The water's warmed by the mountain. NUMEL like to doze by the edge.",
    "Bury yourself in the warm sand. You'll sleep like a SNORLAX tonight.",
    "My aunt soaks in the spring each day. She says it's why she never catches a cold.",
    "A good soak, a LAVA COOKIE, and you're ready for anything!");
TEXT_BANK(sSigTalkSpace,
    "We study the stars and the rocks that fall from them.",
    "One day a rocket will lift off from {LANDMARK}. One day!",
    "On clear nights CLEFAIRY dance on the hill. I've counted eleven.",
    "I want to be an astronaut! I practice floating in the bath.",
    "A meteor fell near {TOWN} once. The whole town ran out in their pajamas.",
    "The sky's so clear here, you can see the moon's freckles.");
TEXT_BANK(sSigTalkMarket,
    "Fresh goods every morning at {LANDMARK}! Come and look!",
    "Ships and carts bring wares from all over. We sell them by the afternoon.",
    "A KECLEON hides among the stalls. It's been here longer than me!",
    "Haggle all you like. The price still won't change!",
    "I came for one POTION and left with a bag of BERRIES and a hat.",
    "The {TRADE_FOLK} here can sell sand to a SANDSHREW.");
TEXT_BANK(sSigTalkMuseum,
    "{^LANDMARK} keeps the old stories of {REGION} safe.",
    "Some of the exhibits are older than anyone can remember.",
    "Our class came to see the fossils. They were SO big!",
    "I paint the exhibits. The old stones have the best faces.",
    "Every town has a past. Ours just has it labeled.",
    "I go into {LANDMARK} every Sunday. The guard knows my name now. I still pay.",
    "The curator walks to work reading. Nobody in {TOWN} knows how he hasn't fallen in the fountain.");
// The museum's people inside (RogueText_SigTurn's RSIG_INSIDE): about
// what stands round them.
TEXT_BANK(sSigTalkMuseumIn,
    "Mind the glass cases! I polished them all this morning.",
    "Please don't touch the exhibits. Looking is free. Touching is very, very expensive.",
    "Take your time. The exhibits have waited this long. They can wait for you.",
    "Shh. People come here to think, and to look at old things.");
TEXT_BANK(sSigTalkBerry,
    "Berries grow best with care and sunshine. Help yourself to what's ripe!",
    "My partner talks to the berry trees. They say the trees listen.",
    "A PECHA BERRY fixes poison. I keep one in my pocket!",
    "ODDISH sneak into the plots at night. We let them. They're cute.",
    "Everyone in {TOWN} has a favorite berry. Mine's ORAN. Simple, but reliable.",
    "Pick a berry, plant a berry. That's the rule at {LANDMARK}.");
TEXT_BANK(sSigTalkFlowers,
    "The whole town smells of flowers in spring. Even the POK@MON CENTER!",
    "COMBEE come for the nectar. We come for the COMBEE!",
    "Every house here grows something. It's a point of pride.",
    "Pick a bunch for someone you love. They'll grow back.",
    "I named every flower in {LANDMARK}. There are a lot of BOBs.",
    "Bees in the morning, butterflies at noon. The flowers are never lonely.");
TEXT_BANK(sSigTalkRanch,
    "MILTANK milk is the best in {REGION}. Ask anyone!",
    "We're up before the TAILLOW to look after the herd.",
    "MAREEP wool keeps the whole town warm in winter.",
    "Don't chase the PONYTA. They like being chased a little too much.",
    "A MILTANK knows its own name. Ours answers to BUTTERCUP.",
    "The {TRADE_FOLK} here wake at four. The MILTANK insist.");
TEXT_BANK(sSigTalkWeather,
    "We watch the sky from {LANDMARK}. Rain's coming. It always is.",
    "CASTFORM change with the weather. They make the best forecasters!",
    "The woods hold the rain like a sponge. Good for the trees.",
    "A storm in the mountains always reaches us a day later.",
    "I forecast sunshine yesterday. It snowed. Nobody let me forget it.",
    "Red sky at night, happy SWABLU. That's an old saying here.");
TEXT_BANK(sSigTalkFossil,
    "Dig carefully! A fossil cracks if you hurry.",
    "FOSSILS! The ground here is full of them! I can feel it in my bones!",
    "Ancient POK@MON lived here long before {TOWN} did.",
    "Smash a rock, and who knows? There might be history inside.",
    "I found a shell yesterday. It was a bottle cap. Still counts!",
    "The {TRADE_FOLK} here dig all day and argue all night.");
TEXT_BANK(sSigTalkMining,
    "The mine's for miners only. It's dark and twisty down there.",
    "GEODUDE turn up in every cart we haul. We just let them go.",
    "{TOWN} was built on the ore from {LANDMARK}.",
    "MACHOP carry more than any of us. They work for snacks!",
    "Sing in the mine and the walls sing back. Spooky, but nice.",
    "My helmet has a lamp on it. I wear it to bed too.");
TEXT_BANK(sSigTalkDojo,
    "Hup! Hup! Training never stops at {LANDMARK}!",
    "Our MASTER can break a boulder with one chop. I've seen it!",
    "Strong body, strong heart! That's what the MASTER says.",
    "Want to test yourself? The DOJO takes on any challenger.",
    "I run three laps round {TOWN} before breakfast. Then I eat three breakfasts.",
    "MAKUHITA train with us. They never skip leg day.");

// The dojo's three: a Black Belt and a Battle Girl (its disciples) and their
// MASTER, an Expert. Intro, defeat and after lines; each region hears one
// of each (RogueText_PickRegion).
TEXT_BANK(sDojoIntro0,
    "Hup! A challenger at {LANDMARK}! Show me how you train!",
    "I chopped forty logs this morning. You're number forty-one!",
    "Bow first, then battle. Those are the rules here. Hup!");
TEXT_BANK(sDojoDefeat0,
    "Oof! Your POK@MON hit hard!",
    "My stance... was all wrong!",
    "I need to chop more logs.");
TEXT_BANK(sDojoAfter0,
    "I run laps round {TOWN} each day. Today I'll only run two.",
    "The MASTER says losing teaches more than winning. I've learned a lot today.",
    "My team wants a rematch already. Not today, team.");
TEXT_BANK(sDojoIntro1,
    "We train here from dawn to dusk. Let's see what you've got!",
    "You walked right past me? Rude! Battle!",
    "I'm the fastest kicker at {LANDMARK}. Watch closely!");
TEXT_BANK(sDojoDefeat1,
    "Ow, ow! All right, you win!",
    "Too fast... even for me!",
    "Hmph! Lucky kick.");
TEXT_BANK(sDojoAfter1,
    "Our MASTER is at the back. Bow before you battle. It's polite!",
    "I'm going to stretch, then sulk a little.",
    "Come back when the MASTER's in a good mood. That's never, by the way.");
TEXT_BANK(sDojoIntro2,
    "So you came all the way in. Good. Show me your spirit!",
    "Many come to {LANDMARK}. Few reach this mat. Let us begin.",
    "Your POK@MON's eyes are bright. Let me see if their hearts are too.");
TEXT_BANK(sDojoDefeat2,
    "Ha! A fine fight. You and your POK@MON move as one.",
    "Well fought! I haven't sweated like that in years.",
    "I see. You have trained well, young one.");

// The MASTER once beaten, whatever was given: about the player's win.
TEXT_BANK(sDojoAfter2,
    "You came through my pupils and through me. {LANDMARK} has not seen that in a long while.|Come back and train with us any time.",
    "Strong body, strong heart. You have both. My pupils will talk of this battle for a year.",
    "I will train harder now. At my age! You have done me a kindness, young one.",
);
// The MASTER's reward, where the town has no pupil to give.
TEXT_BANK(sDojoReward,
    "You fight with heart. {LANDMARK} does not let that go unrewarded.|Take this. Let a POK@MON of yours wear it, and remember today.",
    "Well fought! In my day, a challenger who beat the MASTER took home a belt.|It is still my day. Take it!",
    "My pupils will be sulking for a week. Never mind them.|Here. You have earned this.",
);

static const struct RogueTextBank *const sDojoLines[3][3] =
{
    { &sDojoIntro0, &sDojoDefeat0, &sDojoAfter0 },
    { &sDojoIntro1, &sDojoDefeat1, &sDojoAfter1 },
    { &sDojoIntro2, &sDojoDefeat2, &sDojoAfter2 },
};

static const struct RogueTextBank *const sSigTalk[RSIG_BUILT_COUNT] =
{
    [RSIG_FISHING] = &sSigTalkFishing, [RSIG_HOT_SPRING] = &sSigTalkHotSpring, [RSIG_SPACE] = &sSigTalkSpace,
    [RSIG_MARKET] = &sSigTalkMarket, [RSIG_MUSEUM] = &sSigTalkMuseum, [RSIG_BERRY] = &sSigTalkBerry,
    [RSIG_FLOWERS] = &sSigTalkFlowers, [RSIG_RANCH] = &sSigTalkRanch, [RSIG_WEATHER] = &sSigTalkWeather,
    [RSIG_FOSSIL] = &sSigTalkFossil, [RSIG_MINING] = &sSigTalkMining, [RSIG_DOJO] = &sSigTalkDojo,
};

// Offering the gift Pokemon. The game names it after ("{PLAYER} received
// ..."), so the offer need not.
static const char *const sSigGiftOffer[RSIG_BUILT_COUNT] =
{
    [RSIG_FISHING]    = "Look what I fished up! An odd catch, but it likes you.|Would you take it with you?",
    [RSIG_HOT_SPRING] = "This one was found asleep by the spring, warm as toast.|It wants to travel. Will you take it along?",
    [RSIG_SPACE]      = "This one came to us the night a meteor fell. Strange, isn't it?|It should see the world. Will you take it?",
    [RSIG_MARKET]     = "This one keeps wandering into my stall. Bad for business!|You look like a good TRAINER. Want it? No charge!",
    [RSIG_MUSEUM]     = "Our lab brought one of the exhibits back to life!|It needs a TRAINER more than a glass case. Will you take it?",
    [RSIG_FLOWERS]    = "This one grew up in my flower beds. It's ready to go.|Will you take it out into the world?",
    [RSIG_RANCH]      = "This one was born on the ranch. Strong and healthy!|It wants to see what's past the fence. Will you take it?",
    [RSIG_WEATHER]    = "This one helps with our forecasts. It changes with the sky.|We have plenty here. Will you take one along?",
    [RSIG_FOSSIL]     = "I dug up a fossil, and the lab brought it back to life!|It deserves better than a shelf. Will you take it?",
    [RSIG_MINING]     = "This one was living deep in the mine. It followed me out!|It'd be happier with a TRAINER. Will you take it?",
    [RSIG_DOJO]       = "You fight with heart. Then take my youngest pupil.|Train it well, and it will never let you down. Will you?",
};

// Once the gift Pokemon is given: each signature its own, so a region (one
// signature a town) never says one twice.
static const char *const sSigGiftAfter[RSIG_BUILT_COUNT] =
{
    [RSIG_FISHING]    = "It never took to the water much. Maybe it just wanted to see the road.",
    [RSIG_HOT_SPRING] = "Keep it warm on cold nights. Not that it needs the help.",
    [RSIG_SPACE]      = "If it ever looks up at the night sky for a long time, let it.",
    [RSIG_MARKET]     = "Best deal I ever made, and I didn't even charge you! Take good care of it.",
    [RSIG_MUSEUM]     = "It slept for a very long time. Show it everything it missed.",
    [RSIG_BERRY]      = "Feed it BERRIES now and then. Ours, if you're passing.",
    [RSIG_FLOWERS]    = "Every flower bed in {TOWN} will miss it. Mine most of all.",
    [RSIG_RANCH]      = "It'll want a good run each day. Let it. It's a ranch thing.",
    [RSIG_WEATHER]    = "It knows when rain is coming before we do. Trust it.",
    [RSIG_FOSSIL]     = "Millions of years old, and it still wants a snack every hour.",
    [RSIG_MINING]     = "It's used to the dark. Show it some sun, would you?",
    [RSIG_DOJO]       = "Train it well. One day, bring it back and let it beat me.",
};

static const char *const sSigLines[RSIGTEXT_COUNT] =
{
    [RSIGTEXT_GIFT_AFTER] = "Take good care of it! It'll grow up strong with you.",
    [RSIGTEXT_SOAK]       = "The water's lovely, dear. Sit on the edge and let your POK@MON soak.",
    [RSIGTEXT_COOKIE]     = "And here, a LAVA COOKIE. I baked it in the hot sand!",
    [RSIGTEXT_MILK]       = "Fresh MOOMOO MILK, straight from the herd. Have a bottle or two!",
    [RSIGTEXT_STALL]      = "Welcome to {LANDMARK}! Everything's fresh today!",
};

// A museum's two exhibits.
TEXT_BANK(sSigExhibits,
    "An old stone carving found near {TOWN}. Its markings are worn almost smooth.",
    "The bones of an ancient POK@MON. It swam here when {REGION} lay under the sea.");

// The line under the landmark's name on its sign: the landmark's motto.
static void SigText(struct RogueTextCtx *c, struct RogueTownFacts *f, const struct RogueRegion *r, u8 place)
{
    RogueText_Init(c, r, place, RSPK_FOLK);
    c->town = place;
    RogueGen_TownFacts(r, place, f);
    c->facts = f;
}

// A signature person's turn among their town's (RSIGTEXT_TALK's k): those
// in the street in map order (m the town's map, obj their index there),
// then those inside the landmark (obj -1, inside their index in the room),
// so no two people of a town say one line while the bank lasts. (Their
// look, which the game used, is shared by people side by side.)
u32 RogueText_SigTurn(const struct RogueMap *m, s32 obj, u32 inside)
{
    u32 i, k = 0, n = (obj >= 0 && (u32)obj < m->nObjs) ? (u32)obj : m->nObjs;
    for (i = 0; i < n; i++)
        k += m->objs[i].kind == ROBJ_SIGFOLK;
    // (Those inside are marked: some say what only fits in there.)
    return obj >= 0 ? k : (k + inside) | RSIG_INSIDE;
}

u32 RogueText_SigLine(const struct RogueRegion *r, u8 place, u8 what, u32 k, char *out, u32 size)
{
    struct RogueTextCtx c;
    struct RogueTownFacts f;
    u8 sig = RogueGen_TownSignature(r, place);
    const char *t = NULL;
    u32 n;
    out[0] = 0;
    if (sig == RSIG_NONE || sig >= RSIG_BUILT_COUNT)
        return 0;
    SigText(&c, &f, r, place);
    switch (what)
    {
    case RSIGTEXT_TALK:
        // The museum's glass cases are inside it.
        t = RogueText_Pick(sig == RSIG_MUSEUM && (k & RSIG_INSIDE) ? &sSigTalkMuseumIn : sSigTalk[sig], &c, k & ~RSIG_INSIDE);
        break;
    case RSIGTEXT_GIFT:
        t = sSigGiftOffer[sig];
        break;
    case RSIGTEXT_EXHIBIT:
        t = sSigExhibits.lines[k % sSigExhibits.count];
        break;
    case RSIGTEXT_DOJO:
        // k: the trainer (0-2) * 3 + the line (RTRAINER_INTRO, _DEFEAT, _AFTER).
        t = RogueText_PickRegion(sDojoLines[(k / 3) % 3][k % 3], &c, 0);
        break;
    case RSIGTEXT_DOJO_REWARD:
        t = RogueText_PickRegion(&sDojoReward, &c, 0);
        break;
    case RSIGTEXT_GIFT_AFTER:
        t = sSigGiftAfter[sig];
        break;
    case RSIGTEXT_SIGN:
    {
        // The landmark's name, and its motto under it.
        char *s = RogueGen_LandmarkName(r, &f, out);
        if (out[0] >= 'a' && out[0] <= 'z')
            out[0] -= 'a' - 'A'; // The RANCH
        if (f.landmark < RLANDMARK_COUNT && (u32)(s - out) + 2 < size)
        {
            *s++ = '\n';
            LoreCopy(s, gRogueLandmarks[f.landmark].motto);
        }
        return strlen(out);
    }
    default:
        t = what < RSIGTEXT_COUNT ? sSigLines[what] : NULL;
        break;
    }
    if (t == NULL)
        return 0;
    n = RogueText_Say(out, size, t, &c);
    return n;
}

#ifdef ROGUE_HOST
// Every signature town's people's lines.
void TextDump_Signatures(struct TextDump *d)
{
    const struct RogueRegion *r = d->r;
    u32 i, k;
    for (i = 0; i < r->nTowns; i++)
    {
        u8 place = r->towns[i], sig = RogueGen_TownSignature(r, place), lv;
        char out[600];
        if (sig == RSIG_NONE || sig >= RSIG_BUILT_COUNT)
            continue;
        {
            // Each of the town's signature people in their turn: the street's
            // (src/rogue_town.c SigTalkerRole), then those inside the landmark
            // (BuildSigPeople: the host and one or two more).
            static const u8 sInside[RSIG_BUILT_COUNT] = { [RSIG_SPACE] = 3, [RSIG_WEATHER] = 3, [RSIG_MUSEUM] = 3, [RSIG_FOSSIL] = 2 };
            struct RogueMap *m = &sDumpMap;
            u32 n, j;
            char who[48];
            RogueGen_GenerateMap(r, place, m, sDumpBlocks);
            n = RogueText_SigTurn(m, -1, 0) & ~RSIG_INSIDE;
            for (k = 0; k < n + sInside[sig]; k++)
            {
                RogueText_SigLine(r, place, RSIGTEXT_TALK, k < n ? k : k | RSIG_INSIDE, out, sizeof(out));
                j = k < n ? k + 1 : k - n + 1;
                snprintf(who, sizeof(who), k < n ? "signature person %u" : "signature person %u inside", j);
                TextDump_Line(d, place, RSPK_FOLK, who, sSigTalk[sig]->name, k, out);
            }
        }
        if (RogueGen_SignatureGift(r, place, &lv))
        {
            RogueText_SigLine(r, place, RSIGTEXT_GIFT, 0, out, sizeof(out));
            TextDump_Line(d, place, RSPK_FOLK, "gift", "sSigGiftOffer", sig, out);
            RogueText_SigLine(r, place, RSIGTEXT_GIFT_AFTER, 0, out, sizeof(out));
            TextDump_Line(d, place, RSPK_FOLK, "gift after", "sSigLines", RSIGTEXT_GIFT_AFTER, out);
        }
        if (sig == RSIG_HOT_SPRING)
            for (k = RSIGTEXT_SOAK; k <= RSIGTEXT_COOKIE; k++)
            {
                RogueText_SigLine(r, place, k, 0, out, sizeof(out));
                TextDump_Line(d, place, RSPK_FOLK, "bather", "sSigLines", k, out);
            }
        if (sig == RSIG_RANCH)
        {
            RogueText_SigLine(r, place, RSIGTEXT_MILK, 0, out, sizeof(out));
            TextDump_Line(d, place, RSPK_FOLK, "rancher", "sSigLines", RSIGTEXT_MILK, out);
        }
        if (sig == RSIG_MARKET)
        {
            RogueText_SigLine(r, place, RSIGTEXT_STALL, 0, out, sizeof(out));
            TextDump_Line(d, place, RSPK_CLERK, "stall keeper", "sSigLines", RSIGTEXT_STALL, out);
        }
        RogueText_SigLine(r, place, RSIGTEXT_SIGN, 0, out, sizeof(out));
        TextDump_Line(d, place, RSPK_SIGN, "landmark sign", "-", 0, out);
        if (sig == RSIG_DOJO)
            for (k = 0; k < 9; k++)
            {
                static const char *const sWho[3] = { "dojo black belt", "dojo battle girl", "dojo master" };
                RogueText_SigLine(r, place, RSIGTEXT_DOJO, k, out, sizeof(out));
                TextDump_Line(d, place, RSPK_TRAINER, sWho[k / 3], sDojoLines[k / 3][k % 3]->name, k, out);
            }
        if (sig == RSIG_DOJO && !RogueGen_SignatureGift(r, place, &lv))
        {
            RogueText_SigLine(r, place, RSIGTEXT_DOJO_REWARD, 0, out, sizeof(out));
            TextDump_Line(d, place, RSPK_FOLK, "dojo master's reward", "sDojoReward", 0, out);
        }
    }
}
#endif
