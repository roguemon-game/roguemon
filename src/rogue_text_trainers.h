// Route, cave, dungeon and gym trainers' lines (the trainers domain).
// Included at the end of src/rogue_gen_text.h. The game asks
// RogueText_TrainerTemplate for a class trainer's intro, defeat and after
// line (src/rogue.c, ClassLine); leaders, the rival, the villains and
// dungeon masters have their own (other domains).
//
// Each class has its own sets of three lines (intro, defeat, after) that
// belong together: one small person per set. A set may need something of
// where the trainer stands (TC_* below: the sea, rain, a cave, a gym ahead,
// a ship...), and avoid other things. Gym trainers draw from the gym sets,
// which talk about their leader by name.
//
// Who says what: the region's trainers of a class (or every gym trainer)
// take sets in slot order, each the set its fellows have used least, those
// that fit the spot best first, ties broken by a shuffle of the region's
// own. So a run hears a set once before any twice, and the Hiker in the
// volcano talks about the heat. It is worked out from the seed each time
// (no RAM): the game and gen_host's dump agree on every line.

// Where a trainer stands (struct TrainerSpot.bits).
#define TC_OUT     (1u << 0)  // outdoors: a route, a forest
#define TC_UNDER   (1u << 1)  // underground: a cave, its floors, ruins, ice, the volcano
#define TC_INDOOR  (1u << 2)  // a tower, a ship, a power plant, a gym
#define TC_PLACE   (1u << 3)  // on a route or in a cave of the map: {ROUTE} is it
#define TC_ROAD    (1u << 4)  // on a route (a road under the sky)
#define TC_SEA     (1u << 5)  // a coast or sea route, a shipwreck
#define TC_FRESH   (1u << 6)  // a lake or river route
#define TC_SAND    (1u << 7)  // a desert of sand (sandstorms)
#define TC_ASH     (1u << 8)  // ash country: falling ash
#define TC_WOODS   (1u << 9)  // woods: a forest or wooded route, a forest's floors
#define TC_FIELD   (1u << 10) // wide meadows of tall grass
#define TC_HILLS   (1u << 11) // hills, cliffs, the mountain road
#define TC_RAIN    (1u << 12) // it rains here (RogueGen_RouteWeather)
#define TC_EARLY   (1u << 13) // one of the first routes
#define TC_LATE    (1u << 14) // the last stretch before the League
#define TC_TOWN    (1u << 15) // a town nearby: {TOWN} and its facts
#define TC_NEXT    (1u << 16) // a town further along the road: {NEXT}
#define TC_GYMA    (1u << 17) // a gym ahead: {LEADER}, {GYM_TOWN}, {TYPE}, {BADGE}
#define TC_PAIR    (1u << 18) // {LEADER} is two people (TATE&LIZA): no "says"
#define TC_MON     (1u << 19) // wild POKeMON here: {MON} (wild ones: a line that
                                  // claims one as theirs says {OWN}, their own lead)
#define TC_RIVAL   (1u << 20) // the rival has gone along this road before: {RIVAL}
#define TC_VICTORY (1u << 21) // Victory Road: eight BADGES, the League ahead
#define TC_SIDE    (1u << 22) // a side branch, off the main road
#define TC_DUNGEON (1u << 23) // a dungeon's floor: {DUNGEON}
#define TC_GYM     (1u << 24) // in a gym: {LEADER} is its own
#define TC_SUN     (1u << 25) // outdoors under a clear sky
#define TC_LAKE    (1u << 26) // a lake route (ponds in the woods)
#define TC_RIVER   (1u << 27) // a river route
#define TC_WATER   (1u << 28) // any water route: TC_SEA or TC_FRESH
#define TC_BADGES  (1u << 29) // the road here comes after two gyms or more: {BADGES} reads well
#define TC_STANDIN (1u << 30) // in a gym of a type no class has (a BLACK BELT or EXPERT stands in)

struct RogueTrainerSet
{
    const char *line[3]; // RTRAINER_INTRO, _DEFEAT, _AFTER
    u32 need;            // TC_* the spot must have
    u32 avoid;           // TC_* it must not
    u32 who;             // gym sets: the classes that may say it (a bit per RCLASS_*), 0 for all
    u8 kind;             // RDUN_* + 1: only in that kind of dungeon; 0 anywhere
};

#define T(i, d, a)                { { i, d, a }, 0, 0, 0, 0 }
#define TN(need, i, d, a)         { { i, d, a }, (need), 0, 0, 0 }
#define TNA(need, avoid, i, d, a) { { i, d, a }, (need), (avoid), 0, 0 }
#define TK(kind, need, i, d, a)   { { i, d, a }, (need), 0, 0, (kind) + 1 }
#define TG(who, avoid, i, d, a)   { { i, d, a }, 0, (avoid), (who), 0 }
#define TGN(who, need, avoid, i, d, a) { { i, d, a }, (need), (avoid), (who), 0 }
// A gym set for a gym of one type (RTYPE_*): gyms' spots are of kind
// GYM_KIND + their leader's type.
#define GYM_KIND 0x40
#define TGT(type, who, avoid, i, d, a) { { i, d, a }, 0, (avoid), (who), GYM_KIND + (type) + 1 }

// ---------------------------------------------------------------------------
// The classes. Each person's three lines: what they say when they spot you,
// when they lose (one short line, often a pun), and after (what they will do
// now, what they noticed about you).

static const struct RogueTrainerSet sYoungsterSets[] =
{
    T("I'm gonna be the best TRAINER ever! Starting now!", "No fair! I wasn't ready!",
      "Okay, the best TRAINER ever starts tomorrow. Today was just practice."),
    T("Mom gave me a POTION for emergencies. Is this one?", "It's an emergency!",
      "Mom said to call home if I lose. I'm not calling. She'd laugh."),
    TN(TC_ROAD, "I've trained on {ROUTE} all week! Well, since lunch.", "Lunch was a long time ago...",
      "Tomorrow I'll train all day. Right after breakfast!"),
    T("Shorts in every weather! My legs are tough!", "My knees are cold...",
      "Everybody asks if I'm cold. I am. I'll never admit it, though."),
    TN(TC_NEXT, "I'm walking to {NEXT} all by myself! Battle!", "...I'm still going!",
      "My sister says I'm too little to walk to {NEXT}. I'll show her!"),
    TN(TC_GYMA, "I'm gonna beat {LEADER} someday! When I'm taller.", "I have to grow a LOT.",
      "{LEADER} waved at me once. That counts as a battle, right? Right?"),
    TN(TC_EARLY, "Hey! You look new around here. Me too!", "New, but better. Hmph.",
      "I'll catch more POK@MON. Then we'll see who's new around here!"),
    T("Is that {LEAD}? Cool! Mine's cooler, though!", "Yours IS cooler...",
      "Can I trade you for your {LEAD}? No? I had to ask."),
    TN(TC_OUT, "I've caught {OWN} three times! Same one!", "Now I got caught too!",
      "It keeps getting out. I think it likes getting caught. It always waits by the same bush."),
    TN(TC_RIVAL, "A kid with {RIVAL_MON} just beat me. Now you? No fair!", "Beaten again already!",
      "That kid said they're PROF. BIRCH's kid. {RIVAL}, I think. Do you know them?"),
    TN(TC_RAIN, "Mom said take an umbrella. I took a POK@MON!", "I'm soaked AND beat...",
      "The grass gets all slippy in the rain. That's why I lost. Totally."),
    TN(TC_BADGES, "How many BADGES you got? {^BADGES}? Whoa!", "Whoa, for real.",
      "I've got zero BADGES. But I've got a lot of heart!"),
    TN(TC_WOODS, "I'm exploring the woods alone! I'm not scared!", "...Okay, a little scared.",
      "Which way is home? Never mind! Explorers don't ask. They just walk. A lot."),
};

static const struct RogueTrainerSet sLassSets[] =
{
    T("My POK@MON and I match today. That means we win.", "But we matched!",
      "Okay, buddy. We'll match tomorrow too. Then we'll win for sure."),
    T("You're a TRAINER? Then we have to battle. It's a rule!", "Hmph. Beginner's luck.",
      "Fine, it's not really a rule. But it should be!"),
    TN(TC_TOWN, "All of {TOWN} says my POK@MON is the cutest!", "Cute isn't everything...",
      "It's still the cutest. Even when it loses. Especially when it loses."),
    T("I tied a ribbon on my buddy. Now watch it fight!", "The ribbon came undone!",
      "Next time I'll tie a double knot. Being stylish takes practice."),
    T("Your {LEAD} is cute! But mine's cuter. Let's prove it!", "Okay, yours is a little cuter.",
      "Can my buddy and your {LEAD} be friends? They kept looking at each other."),
    TN(TC_GYMA, "I'm writing a fan letter to {LEADER}! Battle first!", "...You can't read it now.",
      "Dear {LEADER}, today I lost to someone really strong. That's all I have so far."),
    T("We need practice for the {FESTIVAL}. You'll do!", "Ouch. More practice, then.",
      "The {FESTIVAL} is in {FEST_TOWN}. We'll be the cutest ones there. You'll see."),
    T("My POK@MON is my best friend. We're unbeatable!", "Beatable. We're beatable.",
      "My old best friend moved away. I'm writing to tell them about you. In my letter, you lose."),
    TN(TC_SIDE | TC_ROAD, "Shh! This is my secret spot. ...Well, it was.", "Now my secret's out.",
      "You can come back. Just keep it our secret, okay? Pinky promise."),
    TN(TC_SUN, "It's such a nice day! My buddy wants to battle!", "My buddy's all tired now.",
      "We'll lie in the grass a while. My buddy likes the sun."),
};

static const struct RogueTrainerSet sBugCatcherSets[] =
{
    TN(TC_OUT, "Shh! A big one in that tree! ...It flew off. Battle!", "Bzzzt... You win.",
      "Bugs are the best. They're small, so they're easy to carry. And they're tough!"),
    T("My jar's empty! They all got out! Let's battle!", "Squashed!",
      "My mom won't let bugs in the house. So I play out here all day."),
    TN(TC_MON | TC_ROAD, "I counted eleven {MON} on {ROUTE} today!", "Twelve! I lost count!",
      "Counting is hard work. Tomorrow I'm counting BUG POK@MON only. There are more of them."),
    T("Net, jar and a sandwich. I'm ready for anything!", "Not for that.",
      "The food is for me. The bugs eat leaves. Everybody's happy."),
    T("My bug can lift fifty times its own weight! Battle!", "Lifted and dropped...",
      "Fifty times its weight is about one pebble. Still. It's a very big pebble."),
    TN(TC_WOODS, "The shade's cool and the bugs are thick. Best day!", "Best day... ruined.",
      "Bugs love the dark under the trees. So do I. It's nice and quiet."),
    TN(TC_EARLY, "I caught my first bug this morning! It's perfect!", "My tiny perfect one!",
      "We'll grow up together. It'll get bigger, I'll get taller. Then, a rematch."),
    // (No town facts such as {TRADE_FOLK} here: working them out takes two
    // frames on the GBA, and the line is wanted at once.)
    TN(TC_TOWN, "Dad wants me to work in {TOWN}. No way!", "Bzzt... Down.",
      "My dad thinks bugs aren't a real job. Wait till I'm the bug CHAMPION!"),
    TN(TC_OUT, "Hey! You walked right past the biggest bug!", "You didn't look at mine either!",
      "It's still there, on the third leaf. I named it Bertha. Say hi on your way."),
    TNA(TC_FIELD, TC_RAIN, "The tall grass is buzzing! Hear it? Battle!", "Bzz... bzz... done.",
      "Lie down in the grass and listen. It sounds like the whole field is singing."),
    // The forests.
    TK(RDUN_FOREST, TC_DUNGEON, "Welcome to {DUNGEON}! This is bug country!", "Bug country lost...",
      "There are more bugs in here than leaves. I know. I've been counting."),
    TK(RDUN_FOREST, 0, "Two days lost in here! Best vacation ever!", "Still lost. Now beaten.",
      "I've been living on BERRIES. Which way is out? ...No, wait. I'm not done yet."),
    TK(RDUN_FOREST, 0, "Shh... Hear that? A hundred wings at once!", "All gone quiet...",
      "My net's too small for in here. Next time I'm bringing a bigger one."),
    TK(RDUN_FOREST, 0, "Everyone in here is a bug catcher. It's a club!", "Okay, you can join.",
      "Club rule one: never squish. Club rule two: battle everyone. Welcome!"),
    TK(RDUN_FOREST, 0, "I climbed the tallest tree in here! Then I fell!", "Ow... I fell again.",
      "The best bugs live up high. Just watch out for the branches!"),
    TK(RDUN_FOREST, 0, "My bug just learned a new move! You have to see!", "It's still learning...",
      "It's a good move. It just needs a few more tries. Like me."),
};

static const struct RogueTrainerSet sCamperSets[] =
{
    TNA(TC_OUT, TC_RAIN, "Camping's the best! Stars, a fire... and battles!", "Well, that burned out fast.",
      "Wild POK@MON come right up to camp at night. I share my marshmallows."),
    TN(TC_ROAD, "I'm camping on {ROUTE} till I catch something!", "Day three... not great.",
      "Day four, I'm moving the tent. Maybe that's the trick."),
    TN(TC_RAIN, "My tent leaks! A battle will dry me off!", "Still wet. Now beaten too.",
      "A good camper always packs a spare tent. I forgot mine."),
    T("I can light a fire with two sticks! And a FIRE POK@MON.", "Fizzled out!",
      "Scout rule: leave your camp cleaner than you found it. And battle everyone."),
    TN(TC_TOWN, "I hiked from {TOWN} with forty pounds of snacks!", "Should've packed lighter.",
      "I'll eat a few snacks. Then my pack's lighter and I'm faster. Science!"),
    T("My tent's been fighting me all morning. Battle break!", "My tent's still not up...",
      "Do you know which pole goes where? I've been at it all morning."),
    TN(TC_NEXT, "Tonight I camp halfway to {NEXT}!", "Maybe a quarter of the way.",
      "My scout leader says camping isn't about getting places. Usually when we're lost."),
    T("Map? Check. Bug spray? Check. Battle? Check!", "Lost? ...Check.",
      "I keep a checklist of everything I meet. You go under <tough>."),
    TN(TC_MON | TC_OUT, "{MON} keeps visiting my camp! It likes my cooking!", "Burnt to a crisp!",
      "Nobody else likes my cooking. {MON} does. That's enough for me."),
    TNA(TC_HILLS, TC_RAIN, "From up here you can see for miles! Battle!", "Best... loss ever?",
      "Pitch your tent out of the wind. I learned that the hard way."),
    TK(RDUN_FOREST, 0, "My whole scout troop was meant to come. Just me!", "They were right to skip it.",
      "I'm earning my woods badge. It's not a GYM BADGE. It's a cloth one."),
    // (v1.1 peaks) The desert tower.
    TK(RDUN_MIRAGE, 0, "I ran in here out of the sandstorm! Battle!", "Blown away!",
      "Sand in my tent, sand in my boots. I'm camping indoors from now on."),
};

static const struct RogueTrainerSet sPicnickerSets[] =
{
    TNA(TC_OUT, TC_RAIN, "I packed two lunches! One's for whoever beats me!", "Oh no. Here, have a lunch.",
      "That was my favorite one. Enjoy it. ...Are you enjoying it?"),
    TN(TC_SUN, "Flowers, sunshine and POK@MON. What a lovely day!", "Oh, my picnic's ruined!",
      "I'll pick flowers on the way home. That always cheers me up."),
    TN(TC_RAIN, "My picnic blanket's soaked! Let's battle anyway!", "Soggy and beaten!",
      "Rain makes GRASS POK@MON so happy. Look at mine. It's practically glowing."),
    T("Pies for the {FESTIVAL}! Taste one after we battle!", "You won the battle, not the pie.",
      "Come to the {FESTIVAL} in {FEST_TOWN} and try one. They're best a little burnt."),
    TN(TC_OUT, "Ants got my sandwich! I need to battle somebody!", "Ants and you. What a day.",
      "I'll eat the crusts. The ants don't like those either."),
    TN(TC_MON | TC_OUT, "{MON} keeps sneaking up for my cookies!", "Are you after my cookies too?",
      "I'll leave one cookie out for {MON}. Sharing is caring."),
    T("I'm making a flower crown for my POK@MON. Battle!", "The crown fell off!",
      "It's a crown of daisies. It's not for winning. It's just pretty."),
    TN(TC_FIELD, "A meadow picnic! And tall grass for battles!", "Lost in the meadow...",
      "Watch where you step. I lost a shoe in the tall grass today."),
    TN(TC_TOWN, "I walked from {TOWN} just for this view!", "Still a lovely view...",
      "Next time I'll bring my whole family to see it."),
    TK(RDUN_FOREST, 0, "A picnic in the deep woods! So cozy! So many bugs!", "So many bugs...",
      "I'll find a sunny spot on the way out. A picnic needs a little sun."),
    TK(RDUN_MIRAGE, 0, "A tower in the desert! I came up for the view!", "View's still nice!",
      "They say there are fossils at the very top. Old shells and claws, sleeping in the sand."),
};

static const struct RogueTrainerSet sFishermanSets[] =
{
    T("Shh! You'll scare the fish. ...Too late. Battle me.", "The one that got away...",
      "I've waited here all morning for one bite. Patience is the whole trick."),
    T("Hooked one! Oh, it's a TRAINER. That'll do!", "Snapped my line!",
      "Biggest catch of my life was a boot. Second biggest was you. You got away."),
    TN(TC_FRESH, "There's a biter in here as big as a bathtub!", "Nobody believes me...",
      "Twenty years I've fished here. It's grown a foot every time I tell the story."),
    TNA(TC_SEA, TC_RAIN, "The sea's calm today. Good for fishing AND battles!", "Washed ashore!",
      "The tide brings new POK@MON in every day. I like seeing who's visiting."),
    TN(TC_RAIN, "Rain's the best fishing weather! Battle me!", "Soaked and skunked.",
      "Fish can't tell you're wet. Wet clothes dry. A good fishing spot is forever."),
    TN(TC_TOWN, "No fish today means I sleep in the boat. Battle!", "Sleeping in the boat.",
      "Everyone back in {TOWN} will ask what I caught. I'll tell them about you."),
    TN(TC_OUT, "{OWN} took my bait three times. So I kept it!", "It's laughing at me.",
      "Some fishers want the big one. I keep the ones that keep coming back."),
    T("I fish, I nap, I battle. In that order. You woke me!", "Back to fishing...",
      "Funny thing about fishing: the less you try, the more they bite."),
    TN(TC_LATE, "I've fished every water in {REGION}. This is the best!", "Still the best spot.",
      "Rods break and lines snap. But a good spot? You keep that one secret."),
    T("Lucky hat, lucky rod, lucky worm. I can't lose!", "...Unlucky worm.",
      "I'll get a new worm. The hat and the rod stay. They're still lucky."),
    TNA(TC_FRESH, TC_RAIN, "The water's so clear I can see the fish laugh!", "Now you're laughing too.",
      "On a still day the fish come up to look at you. It's only polite to look back."),
};

// v1.1 sea: Swimmers out on a maze's or the shallows' water, as on Routes
// 109 and 132 to 134.
static const struct RogueTrainerSet sSwimmerSets[] =
{
    T("I swim out here every morning. Battle me before I head back!", "Out of breath...",
      "The trick is to float when you're tired. The sea holds you up if you let it."),
    TN(TC_SEA, "Careful! The current out here pulls hard!", "Pulled under!",
      "Ride the current, don't fight it. Swim across it and it lets you go."),
    T("You're surfing? Lazy! I swam all the way!", "Beaten by a lazy one...",
      "Fine, fine. Your POK@MON is a strong swimmer. Mine are too, really."),
    TN(TC_SEA, "Salt water's good for you! And so is battling!", "Too much of both...",
      "I'll float here a while. The waves rock you like a cradle."),
    TN(TC_RAIN, "Rain? I'm already soaked! Battle!", "Wet AND beaten.",
      "The sea's warmer than the rain. That's my secret. Jump in!"),
    TN(TC_NEXT, "I'm swimming to {NEXT}! Halfway there!", "Maybe a quarter of the way...",
      "When I get to {NEXT}, I'm going to eat everything. Swimming makes you hungry."),
    T("My POK@MON taught me to swim. Now we race!", "It won the race too.",
      "It swims circles round me. Literally. It thinks it's funny."),
    TN(TC_SEA, "I lost my goggles somewhere out here. Seen them?", "Lost the battle too...",
      "If you find goggles on a sandbar, they're mine. Probably."),
    TN(TC_LATE, "Every sea in {REGION}, I've swum it! This one's the roughest!", "Rough day...",
      "Out here you learn to read the water. Calm lanes hug the rocks."),
};

static const struct RogueTrainerSet sBirdKeeperSets[] =
{
    TN(TC_OUT, "My birds saw you from way up high! No hiding!", "Shot down!",
      "They're still circling. They like you. They don't like most people."),
    T("Tweet tweet! That means <battle me> in bird!", "Squawk...!",
      "I can talk to birds, you know. They only ever ask for seeds."),
    TN(TC_SUN, "Perfect wind today. My birds are itching to fly!", "The wind went out of me!",
      "Watch the sky when you walk. The birds always know the weather first."),
    TN(TC_RAIN, "My birds love rain! It rolls right off their wings!", "Wet and beaten!",
      "When the rain stops, they all sing at once. Stick around and listen."),
    TN(TC_GYMA, "{LEADER} let me watch a training once! I didn't blink!", "I blinked that time.",
      "One day my birds will fly right over {GYM_TOWN}. The whole flock. In a V."),
    T("I whistle and they come. Watch! ...Usually they come.", "They came for that, at least.",
      "Each bird has its own whistle. I know eleven of them. My lips hurt."),
    TN(TC_HILLS, "From these cliffs my birds see half of {REGION}!", "They saw me lose, too.",
      "The wind comes up the cliffs here. Birds ride it like a slide."),
    TN(TC_NEXT, "My birds fly to {NEXT} at sunrise! Showoffs!", "Flew off without me!",
      "I walk to {NEXT}. They fly. They're always waiting for me when I get there."),
    T("I dream of flying over all of {REGION}!", "Grounded again...",
      "Until then, I keep my eyes up. You'd be surprised what flies past."),
    TN(TC_MON | TC_OUT, "I've watched {MON} all day. It never looked up!", "I should've looked up.",
      "Birds see everything from up there. I just see grass. And now, my defeat."),
    TN(TC_WOODS, "Every tree in here has a nest! Hear them?", "Back to the nest...",
      "If you're quiet in the woods, the birds forget you're there. Then they sing."),
    TK(RDUN_FOREST, 0, "The birds in here sing all day. I'm learning!", "That one was a sad song.",
      "I've learned six songs so far. I'm terrible at all of them."),
};

static const struct RogueTrainerSet sTuberSets[] =
{
    T("I can swim! ...With this on.", "My floatie popped!",
      "Don't tell anyone, okay? Without my floatie I sink like a rock."),
    TNA(TC_WATER, TC_RAIN, "Splash splash! The water's so nice today!", "Waaah! I'm all wet!",
      "I'm a WATER POK@MON. That's why I'm wet. That's my story."),
    TN(TC_SEA, "I found the best shell ever! Battle me for it!", "You can't have my shell!",
      "Hold it to your ear. Hear the sea? I could listen all day."),
    TN(TC_FRESH, "I'm gonna swim all the way across! After this!", "My arms are tired now...",
      "I'll swim across tomorrow. Today I'll swim to that rock!"),
    TN(TC_RAIN, "It's raining, but I'm already wet! I win at rain!", "I lose at battling...",
      "I have to come out when it thunders. It hasn't thundered yet!"),
    T("Hi, Mommy! Watch me battle!", "Mommy saw that...",
      "She's waving. She always waves. Even when I lose."),
    T("Can your {LEAD} swim? Mine can! Let's battle!", "It splashed me!",
      "Maybe your {LEAD} can teach me to float. Will you ask it for me?"),
    T("My new floatie looks like a POK@MON! Let's go!", "My new floatie!",
      "It's got a leak now. That's okay. I can still kick really fast."),
    TN(TC_SEA, "Sand in my swimsuit again! It always gets in!", "Itchy AND beaten!",
      "If I take home all the sand, the beach gets smaller. So I'm careful."),
    TK(RDUN_SHIP, 0, "I snuck onto this old ship! Is it haunted?", "It IS haunted!",
      "Something went <oooo> down the hall. ...It was me. I was practicing."),
    TK(RDUN_SHIP, 0, "This ship's got seawater inside! It's a pool!", "Waaah! Tipped over!",
      "My mommy thinks I'm at the beach. There's water in here, so it counts!"),
};

static const struct RogueTrainerSet sSchoolKidSets[] =
{
    T("Teacher says battles teach more than books!", "I need to take more notes...",
      "Battle notes, page forty: <Lost. Very fast.> I'll underline it."),
    T("I studied type matchups all night. Watch this!", "But I got full marks!",
      "WATER beats FIRE, FIRE beats GRASS. Past that it's extra homework."),
    TN(TC_TOWN, "Field trip! Our class walked here from {TOWN}!", "Not the strongest, though.",
      "I'm the fastest in my class. The others are way back there. They have the lunches."),
    TN(TC_GYMA, "I'm writing a report on {LEADER}! This is research!", "Research... complete.",
      "My report says {LEADER} is the best in {GYM_TOWN}. I might add a page about you."),
    T("I'm top of my class! Out of three. Battle!", "Three out of three...",
      "The other two are really good at art. I was good at this. Until today."),
    TN(TC_MON, "I've watched {MON} all morning! Battle!", "Notebook says: ouch.",
      "I'm writing down everything {MON} eats. So far: grass, more grass, my eraser."),
    T("I'll be just like {FIGURE} someday! Starting now!", "Maybe like someone else...",
      "We learned about {FIGURE} in class. They started out small too!"),
    T("I left my homework at home. If I win, it doesn't count!", "It counts...",
      "I'll tell my teacher a POK@MON ate it. I'll need to catch one that eats paper."),
    TN(TC_OUT, "I have to press ten leaves for class! Battle me!", "Pressed flat!",
      "Leaf number eleven: a TRAINER who beat me. That's not a leaf, but I'll draw it."),
};

static const struct RogueTrainerSet sHikerSets[] =
{
    TN(TC_OUT, "Hah! Nothing beats fresh air and a good battle!", "Tougher than a mountain goat!",
      "A good hiker knows when to stop and rest. I'm stopping right here."),
    TN(TC_ROAD | TC_NEXT, "Hah! {ROUTE}'s just a stroll! Battle me!", "Not much of a stroll now.",
      "I'm walking to {NEXT} and back by supper! Boots on, eyes forward! That's the hiker's way."),
    TN(TC_HILLS, "These boots have climbed every peak in sight!", "My knees give out...",
      "Every hill has a top, and every top has a view. That's why I climb."),
    TNA(TC_UNDER | TC_PLACE, TC_DUNGEON, "I know every rock in {ROUTE}, even in the dark!", "Didn't know that one!",
      "Feel the walls when it's dark. The rock's colder on the north side. Hah! Old hiker trick."),
    TN(TC_UNDER, "HAH! Echo! ...Echo! Sorry. I just love caves.", "...ow ...ow ...ow",
      "Shout in here and the cave shouts back. Best talk I've had all week. Hah!"),
    TN(TC_UNDER, "My lamp ran out an hour ago! Battling by feel!", "Felt that one!",
      "Good thing you came along. Now I can see where I'm going."),
    T("Walking since sunrise! Battle break? Battle break!", "Break's over!",
      "Walk a little, battle a little, eat a lot. That's a good day."),
    TN(TC_LATE, "All these years of hiking, and I still get lost! Hah!", "Lost again!",
      "Lost isn't so bad. Lost is how you find the good spots. Hah!"),
    TN(TC_SAND, "Sand in my boots, sand in my beard! Hah!", "Sand in my eyes!",
      "Out here you carry water, you keep your hat on, and you never fight the wind."),
    TN(TC_ASH, "The ash comes down like snow here! Warm snow!", "Buried in ash!",
      "Folks round here say ash makes the crops grow. It sure makes my boots gray."),
    TN(TC_TOWN, "Born in {TOWN}! I've walked every road out of it!", "Hah! You beat a local!",
      "Back in {TOWN} they'll ask where I've been. Everywhere! Losing, today."),
    TN(TC_RAIN, "Rain on the trail! Hah! Mud's just soft ground!", "Slipped in the mud!",
      "Muddy boots are happy boots. My old dad said that. He was a hiker too."),
    TN(TC_MON | TC_OUT, "Hah! {MON} followed me for a mile!", "It can have my lunch now.",
      "I think it was hungry. Out here, something's always watching."),
    TN(TC_UNDER, "My pack's full of rocks I found down here!", "Should've packed lighter!",
      "Every rock tells you where it's been. This one's been here forever. Hah!"),
    TN(TC_UNDER, "Watch your head in here! I didn't. Hah!", "Bonk!",
      "Low roof, low light, good company. Cave life's not bad. Hah!"),
    TN(TC_OUT, "These boots and I have walked a thousand miles!", "Boots, we're done for!",
      "When they wear out, I'll leave them on a mountain top. They've earned the view."),
    TN(TC_RIVAL, "A kid with {RIVAL_MON} raced right past me! Hah!", "Everyone's faster than me today!",
      "That {RIVAL} said they'd see me at the LEAGUE. Hah! I'm only out for a walk."),
    TNA(0, TC_EARLY, "A calm TRAINER named {NAME} came through!", "Hah! Two of you now!",
      "{NAME} knew more about rocks than I do. Just smiled and walked on. Strong look, though."),
    // Below ground, by the kind of dungeon.
    TK(RDUN_CAVE, TC_DUNGEON, "I came to find the bottom of {DUNGEON}!", "Hit bottom!",
      "The bottom's not far now. Or it's very far. Caves are like that. Hah!"),
    TK(RDUN_CAVE, 0, "Smell that? Damp rock and old moss! Hah!", "Smells like defeat now.",
      "Mark the walls as you go, kid. Chalk's lighter than a map and just as useful."),
    TK(RDUN_VOLCANO, 0, "Phew! It's hot in here! Battle before I melt!", "Melted!",
      "My boots have holes from the hot rocks. Watch where you step, kid."),
    TK(RDUN_VOLCANO, 0, "The ground's rumbling today! So's my stomach!", "Rumbled out!",
      "The rock's warm under your feet here. Folks say you could fry an egg on it. Hah!"),
    TK(RDUN_ICE, 0, "Brr! My beard froze solid! Battle to warm up!", "Still frozen!",
      "The ice cracks and the rocks slide. Mind your footing in here, kid."),
    TK(RDUN_ICE, 0, "Slid all the way here on my backside! Hah!", "Slid right past a win!",
      "The trick on ice is to pick where you'll stop before you start moving."),
    TK(RDUN_RUINS, 0, "I came for the old stones and got lost! Battle!", "Lost again!",
      "These old walls are smoother than any rock I know. Somebody worked hard on them."),
    TK(RDUN_MIRAGE, 0, "This tower wasn't here last time I passed! Battle!", "It's real, all right!",
      "Watch the cracked floor, kid. It holds you once. Step on it again and down you go."),
    TN(TC_VICTORY, "Hah! Last climb before the LEAGUE! For one of us!", "Great view of losing!",
      "Every TRAINER who goes up there passes me first. I like to give them a cheer. Hah!"),
};

static const struct RogueTrainerSet sRuinManiacSets[] =
{
    T("I've dug here since I was your age! Three rocks and a spoon!", "Buried again...",
      "The spoon is very old, I'm sure of it. Losing to you was my best find all week."),
    TN(TC_OUT, "Looking for old ruins. Seen any? No? Battle!", "Unearthed and defeated...",
      "Old ruins hide under hills like these. I just need to find the right hill."),
    TN(TC_SAND, "Sand hides everything! Pots, bones, my keys...", "Sand in my defeat...",
      "Any dune might have a ruin under it. Or more sand. Usually more sand."),
    T("I've dug under half of {OLDTOWN}! Battle!", "Dug myself a hole.",
      "{OLDTOWN} is the oldest town in {REGION}. They've asked me to fill my holes back in."),
    T("Careful! That pebble could be a fossil!", "It was a pebble.",
      "Fossils are just POK@MON that waited a very long time. I'm patient too."),
    TNA(TC_UNDER, TC_DUNGEON, "These caves go deeper than any map shows!", "Mapped and beaten!",
      "I map every cave I find. This one's on page sixty. You're in the margin."),
    TN(TC_UNDER, "Ancient writing on the wall! ...Or claws. Battle!", "Claws. Definitely claws.",
      "Old writing looks like scratches, and scratches look like writing. It's a tricky job."),
    T("Brush, trowel, magnifying glass! Ready to battle!", "My trowel broke!",
      "A good digger never digs fast. Slow and careful finds the treasure."),
    T("How did {REGION} begin? I've studied it for decades!", "Hah! Well earned.",
      "Long ago, {FOUNDING}.|That's the old tale. I'm still digging for proof."),
    TN(TC_HILLS, "Each layer in that cliff is a thousand years!", "A thousand years of losing...",
      "Read a cliff from the bottom up. It's the oldest book in {REGION}."),
    TK(RDUN_CAVE, TC_DUNGEON, "The deeper in {DUNGEON}, the older the rock!", "All the way down...",
      "Down here the rock is older than any town. Older than {OLDTOWN}, even."),
    TN(TC_UNDER, "A fossil! Wait. That's my old boot. Battle!", "My boot!",
      "Down here everything looks ancient. Even lunch. Especially after a week."),
    TNA(TC_UNDER | TC_PLACE, TC_DUNGEON, "I've mapped every tunnel in {ROUTE}! Well, nearly!", "Lost my map!",
      "Left, left, right, down. Or was it right, right? Maps are hard, I'll admit."),
    T("I found an old coin! It's older than me! Battle!", "Older than my luck...",
      "There's a POK@MON on one side. I can't tell which one. I like it that way."),
    TK(RDUN_CAVE, TC_DUNGEON, "Someone carved steps down here long ago! Who?", "Not you, surely.",
      "Old steps, worn smooth. People walked {DUNGEON} long before there were maps."),
    TN(TC_FIELD, "Plowed fields turn up old pots! I follow the plows!", "Plowed under!",
      "The farmers don't mind. I always fill my holes back in."),
    TN(TC_TOWN, "{TOWN} is built on very old stones!", "Dug too deep!",
      "Dig under any town in {REGION} and you'll find an older one. Or a very old boot."),
    // Ruins.
    TK(RDUN_RUINS, 0, "These ruins are older than every town in {REGION}!", "Ruined... fittingly.",
      "Every stone here was set by hand, thousands of years ago. I get chills."),
    TK(RDUN_RUINS, 0, "Don't touch the walls! I'm reading them!", "My notes!",
      "Ten years on these walls. I can read three words. Two are <the>. The third might be <sand>."),
    TK(RDUN_RUINS, 0, "The floor moved! Battle me before it moves again!", "It moved me.",
      "The old builders loved traps. I love the old builders. It's complicated."),
    TK(RDUN_RUINS, 0, "I came looking for the riddle door. Found you!", "No riddle there.",
      "The riddle's carved in the old words. I think the answer is a move."),
    TK(RDUN_RUINS, 0, "I've been down here so long I forgot the sun!", "Is it yellow?",
      "Tell the sun I'll be up soon. A week. A month at most."),
    // (v1.1 peaks) The desert tower.
    TK(RDUN_MIRAGE, 0, "Who built a tower out in the sand? I must know!", "Still don't know!",
      "Some days the sandstorm hides it. Folks call it a mirage. It's stone, I promise."),
};

static const struct RogueTrainerSet sNinjaBoySets[] =
{
    T("Hah! You didn't see me hiding there, did you?", "My cover is blown!",
      "I practice hiding every day. My mom still finds me at dinner time."),
    T("You walked right past me! Ninja skills!", "I vanish in defeat...",
      "Okay, I'm vanishing now. ...I'm still here. Vanishing takes a minute."),
    TN(TC_WOODS, "I am one with the leaves. You cannot see me.", "Leaf me alone...",
      "Every leaf is a hiding spot. Every tree is a fort. The woods are the best!"),
    TN(TC_GYMA, "One day, {LEADER} will fear my ninja ways!", "Not today, though.",
      "I'll train in secret. So secret, nobody will see. Not even me."),
    T("Shh! I'm on a secret mission! It's battling you!", "Mission failed!",
      "Secret mission two: get home before dark. That one I always pass."),
    TN(TC_ROAD, "I hid on {ROUTE} all morning. Nobody came!", "At least you found me.",
      "Next time I'll hide somewhere people walk past more. Lonely ninjas are sad ninjas."),
    T("Ninja rule one: strike from the shadows! Hi!", "Rule three: lose with honor.",
      "Rule two is say hi first. There are forty rules. I made them all up."),
    TN(TC_RAIN, "The rain hides my footsteps! Ninja weather!", "Achoo! Found me.",
      "A ninja who catches a cold isn't much of a ninja. I'm going home for soup."),
    TK(RDUN_PLANT, 0, "The lights went out, so I hid. I'm still hiding!", "Lights out for me.",
      "This place hums like a giant beehive. Great for sneaking. Nobody hears you."),
    TK(RDUN_PLANT, 0, "I hid behind this machine. It's warm! Battle!", "Short-circuited!",
      "The machines here click and buzz all night. I haven't slept a wink."),
};

static const struct RogueTrainerSet sBlackBeltSets[] =
{
    T("Hoo-hah! My fists and my POK@MON are ready!", "Hrrgh! A true fighter!",
      "Train body and spirit alike. Then eat a big dinner. That is the way."),
    T("One thousand punches a day! I'm at nine hundred!", "Nine hundred and one... Hrrgh.",
      "Ninety-nine punches to go. Then a thousand kicks. Then dinner."),
    T("I split boulders with one chop! Can you stand it?", "My chop... was blocked!",
      "A boulder never hits back. You did. I learned more from you."),
    TNA(TC_OUT, TC_RAIN, "I train under this sky in any weather! Hoo!", "The sky saw me fall.",
      "My old master trained here, and the master before that. One day, my student will. Hoo!"),
    TN(TC_RAIN, "Rain sharpens the spirit! Hoo-hah!", "My spirit is soggy...",
      "Training in the rain is good for the heart. Bad for the uniform."),
    TNA(TC_UNDER, TC_DUNGEON, "I train in the dark to sharpen my senses! Hoo!", "Sensed that too late!",
      "Without light, you feel a punch coming. Like that one. I felt that one coming."),
    TN(TC_TOWN, "All of {TOWN} knows my yell! HOO-HAH!", "Hoo... hah...",
      "I yell when I train. The neighbors in {TOWN} sent me out here. Fair enough."),
    TN(TC_LATE, "Twenty years of training for this battle! Hoo!", "Twenty more years, then.",
      "Well, for battles in general. A black belt is just a white belt that never quit."),
    TN(TC_GYMA, "I wish to challenge {LEADER}. First, you!", "Hrrgh! Not ready!",
      "Tell {LEADER} a fighter is coming. Slowly. Very slowly."),
    TN(TC_MON, "I spar with the wild {MON} here! Hoo!", "So you win too, it seems.",
      "{MON} wins most mornings. No style at all, which makes it hard to read."),
    T("I bow to my POK@MON. It bows to me. Then, battle!", "I bow to you, too.",
      "Respect first, then fists. My master was very clear about the order."),
    TN(TC_UNDER, "I punch the cave wall! The cave wall wins! Hoo!", "You win too!",
      "The rock has trained longer than I have. A million years longer. Hrrgh."),
    T("My uniform was white when I left home! Hoo!", "Now it's grass-colored.",
      "Every stain is a lesson. This one is the lesson of you."),
    TN(TC_RIVAL, "A youngster with {RIVAL_MON} bested me! Now you!", "Hrrgh! A strong generation!",
      "That {RIVAL} bowed after our battle. Good manners. You both have them."),
    TN(TC_LATE, "My final test before I go home! Hoo-hah!", "Failed it!",
      "Home can wait. A fighter who loses still has things to learn. Good. Hoo."),
    TN(TC_UNDER, "Breathe in the cave air! Breathe out! HOO!", "Out of breath!",
      "Down here the air is cool and still. A fine place for the mind. My fists disagree."),
    TNA(0, TC_EARLY, "{NAME} came by. One battle, and I bowed. Now you!", "I bow again.",
      "Two strong TRAINERS in one season. {REGION} is lucky. Or I am unlucky. Hoo."),
    // Dungeons.
    TK(RDUN_CAVE, TC_DUNGEON, "Deep in {DUNGEON}, no one hears my yell! HOO!", "...hah.",
      "I came down here to train in silence. I am terrible at silence."),
    TK(RDUN_CAVE, 0, "Low ceilings! Good for training humility! Hoo!", "Hrrgh! Bumped my head!",
      "My master said a fighter must bow often. I don't think the ceiling counts."),
    TK(RDUN_VOLCANO, 0, "Heat forges steel! It forges fighters too! HOO!", "Overcooked!",
      "Train in the heat, and the cold is easy. Train in the cold, and... I forget the rest."),
    TK(RDUN_VOLCANO, 0, "I meditate by the lava. Calm spirit, hot feet!", "Hot feet! Hot feet!",
      "To find calm in a hot place is the true test. I am still taking it."),
    TK(RDUN_ICE, 0, "Cold air, cold floor! Perfect training! Brr!", "Hrrgh... frozen fists.",
      "I punched a boulder here and slid for a minute. A good minute, though."),
    TK(RDUN_TOWER, 0, "Even here among the spirits, I train! Hoo!", "The spirits went quiet.",
      "I came to honor my old master's POK@MON. And to do push-ups. Both are respect."),
    TN(TC_VICTORY, "Eight BADGES and still training! Hoo-hah!", "Hrrgh! Your spirit is strong!",
      "The road to the LEAGUE is the best dojo there is. Go on. I'll train here a while yet."),
};

static const struct RogueTrainerSet sBattleGirlSets[] =
{
    T("One hundred push-ups done! Now, one battle!", "I need a hundred more...",
      "Rest day tomorrow. Just kidding! There's no such thing."),
    TN(TC_TOWN, "I jogged here from {TOWN}! Warm-up's over!", "Cool-down time...",
      "I'll jog back to {TOWN} now. Fast. Maybe that'll make me feel better."),
    T("Hi-yah! My POK@MON and I train from dawn to dusk!", "I'll train even harder!",
      "My POK@MON does the kicks. I do the yelling. Teamwork!"),
    T("Rep twenty! Rep twenty-one! Battle!", "Lost count...",
      "Twenty-two... twenty-three... Okay, back to work. Thanks for the break."),
    T("Your {LEAD} looks strong! How many push-ups?", "Okay, a lot!",
      "Tell your {LEAD} I said good form. That's my highest praise."),
    TN(TC_RAIN, "Rain's just free cold water! Hi-yah!", "Cold and beaten!",
      "Sparring in the rain teaches footwork. You learn not to slip!"),
    T("My big brother's a BLACK BELT. I'll beat him!", "Not this, though.",
      "He says I can't chop a board yet. I've chopped forty. He's stopped saying it."),
    TN(TC_GYMA, "I'm training to beat {LEADER}! You're my warm-up!", "Ouch! Some warm-up!",
      "{LEADER}'s GYM is in {GYM_TOWN}. I'll jog there tomorrow!"),
    TN(TC_BADGES, "BADGES? {^BADGES}? I've done two hundred push-ups!", "Fine. BADGES win.",
      "Push-ups don't come in a case, sadly. I'd have the biggest case in the region."),
    TN(TC_HILLS, "Hill sprints! Up, down, up, down! Battle!", "Down I go...",
      "Run up a hill every day and every road feels flat. Try it. It hurts."),
    TK(RDUN_VOLCANO, 0, "Training in the heat! Sweat is weakness leaving!", "Lots of weakness left...",
      "Drink lots of water down here. Coach says I always forget. Coach is right."),
    TK(RDUN_ICE, 0, "Ice drills! Balance and power! Watch! ...Whoa!", "Slipped!",
      "Training on ice teaches balance, and how to fall well!"),
    TN(TC_VICTORY, "Last stretch before the LEAGUE. Make it count!", "That counted. Ow.",
      "I'm not giving up. Every climb here makes me stronger. You too."),
};

static const struct RogueTrainerSet sKindlerSets[] =
{
    T("Feel that? The heat of a TRAINER who never sleeps!", "Burned out...",
      "I'm worn out. I'll sleep well tonight."),
    T("Feel the heat! My FIRE POK@MON are burning to go!", "Extinguished!",
      "Fire's not just hot. It's bright, it's loud, it's alive. That's why I love it."),
    TN(TC_RAIN, "Rain?! My flames are fine. My hair is not!", "Fizzle...",
      "Rain can't put out a real flame. It put out mine. But not a real one."),
    TN(TC_ASH, "The air out here tastes like smoke. I love it!", "Smoked out!",
      "The ash falls here like snow. It's warm in your hands. Try it."),
    T("I'm on fire today! Nobody can put me out!", "Somebody put me out.",
      "When I lose, I get fired up. So losing helps me win!"),
    TN(TC_MON, "Know what {MON} hates? Heat! I've got heat!", "Heat wasn't enough!",
      "{MON} ran off the moment I came by. Everything does. I'm a lot."),
    T("The {FESTIVAL} needs my flames! Battle!", "Light's out...",
      "Every year at the {FESTIVAL} I bring the biggest flame. Nobody asks me to. I just do."),
    TN(TC_TOWN, "In {TOWN} they call me the spark! I'm always lit!", "The spark went out.",
      "If {TOWN} hears about this, I'll never live it down."),
    TN(TC_SAND, "Hot sand, hot sun, hot battle! My kind of day!", "Too hot even for me!",
      "Sand gets in the flames and they sparkle. Nobody else notices that. I do."),
    TN(TC_HILLS, "Climbing makes me hot. Battling makes me hotter!", "Cooled right off...",
      "Up on these hills the sun feels close enough to touch. I like that."),
};

static const struct RogueTrainerSet sAromaLadySets[] =
{
    T("Smell that? A GRASS POK@MON in a good mood.", "Wilted...",
      "Even when they lose, my POK@MON smell lovely. That's a comfort, dear."),
    T("My garden grows the loveliest POK@MON, dear.", "My poor petals!",
      "Plant a flower and a POK@MON will come. It's an old saying. It's also true."),
    TN(TC_TOWN, "Mm. I can smell {TOWN} from here, dear.", "Oh! I lost the scent.",
      "Every town has its own scent. You'd know your home anywhere, wouldn't you?"),
    TN(TC_RAIN, "Rain on the leaves. The finest perfume there is.", "Washed away...",
      "Don't hurry out of the rain, dear. It's good for the skin. And the soul."),
    T("I'm making a perfume called Victory. Smell!", "...I'll call it Defeat.",
      "It smells of mint and wet grass. And now, just a little, of you."),
    TN(TC_MON, "{MON} came sniffing at my basket. Sweet nose!", "My sweet petals...",
      "Leave a little honey out and {MON} will visit. Not too much, dear. They get spoiled."),
    TN(TC_WOODS, "Moss and rain. The woods smell lovely, dear.", "Mossy and beaten.",
      "Moss grows on the shady side of trees. Follow your nose and you'll never get lost."),
    TN(TC_SUN, "Sunshine brings out every bloom. Shall we?", "Wilted in the sun!",
      "Water your flowers in the evening, dear. Never at noon. They'll thank you."),
    TN(TC_FIELD, "A whole meadow in flower! Let's battle, dear.", "Perhaps not all day.",
      "I could battle here all day. Pick one flower for your POK@MON on the way. Just one."),
};

static const struct RogueTrainerSet sBeautySets[] =
{
    T("Don't scuff my POK@MON. They've just been brushed.", "How terribly rude of you to win!",
      "Off to the salon. All of us!"),
    T("Oh my, a traveler! An elegant little battle?", "Not the face!",
      "A lady always carries a POTION. Or three. Or six, in matching colors."),
    TN(TC_RAIN, "Rain! My hair! Make this quick, darling.", "Ruined. Completely ruined.",
      "I'll need a towel, a mirror and a long rest."),
    TN(TC_TOWN, "All of {TOWN} turns to look when I pass by!", "How rude!",
      "They'll still turn to look. Just for a different reason today."),
    T("Your {LEAD} has lovely eyes, darling.", "Fine. Yours is lovely too.",
      "Mine has lovely everything, of course. Brush your {LEAD} after battles. It notices."),
    TNA(TC_SEA, TC_RAIN, "Sun, sand, and a little battle to keep in shape!", "Sand in my shoes!",
      "I came to the shore for the sun. My POK@MON came for the snacks."),
    T("Contest coming up! This is the dress rehearsal!", "The dress tore!",
      "Beauty is nothing without care. And a good stylist. And nerves of steel."),
    T("I'll be the loveliest at the {FESTIVAL}!", "Not the loveliest loser...",
      "The {FESTIVAL} in {FEST_TOWN} is the event of the year. I'm already dressed."),
    TK(RDUN_SHIP, 0, "This ship had a ballroom, darling! Let's dance!", "Danced right off my feet!",
      "Picture it: lights, music, gowns... and now just creaking. Such a pity."),
};

static const struct RogueTrainerSet sGuitaristSets[] =
{
    T("Yeah! Let's rock this place! One, two, three, four!", "My strings snapped!",
      "I'm writing a song about you. It's called <Oh No.>"),
    T("This battle's gonna be electric! Feel the beat?", "Bad feedback!",
      "Every battle has a rhythm. Yours was too fast for me."),
    TN(TC_TOWN, "I'm writing a song about {TOWN}. No rhymes!", "Off-key!",
      "Maybe I'll write about the people instead. They've all got stories. You too."),
    TN(TC_RAIN, "Rain on my guitar! It sounds kinda cool!", "Wet strings...",
      "Between you and me, the rain's the best drummer I've ever had."),
    T("Heard me tuning up from way down the road?", "And that's the finale...",
      "That was the warm-up. Next show's tomorrow. Bring friends. Bring earplugs."),
    TN(TC_ROAD, "Live from {ROUTE}! My one-man show!", "My one-man show!",
      "My only crowd out here is POK@MON. They're honest. They leave when it's bad."),
    T("Big gig at the {FESTIVAL}! Warm-up time!", "Warmed down...",
      "They don't know I'm playing yet. I'll just show up. That's rock."),
    T("My band broke up. My POK@MON are the band now!", "The band broke up again!",
      "Back together tomorrow. We always get back together."),
    TK(RDUN_PLANT, 0, "Hear that hum? Ten thousand volts of backup!", "Shorted out!",
      "The machines here hum in one long note. I tuned to it. Best room I ever played."),
    TK(RDUN_PLANT, 0, "I'm recording the hum in here! Bzzz! Battle!", "Recording... stopped.",
      "Play the tape backwards and it sounds like a ghost. Forwards, it's still kind of spooky."),
    TK(RDUN_PLANT, 0, "Watch the lights flicker on my power chord!", "Power's out!",
      "When I play loud, the lights in here blink. I like to think they're clapping."),
};

static const struct RogueTrainerSet sSailorSets[] =
{
    T("Ahoy! Never whistle on a boat. Battles are fine.", "Sunk! Blow me down!",
      "Whistling calls up the wind. Every sailor knows it. Now you do too."),
    T("Ahoy! You've got the look of a TRAINER who battles!", "Abandon ship!",
      "I've sailed far and wide, and never met a TRAINER quite like you."),
    TN(TC_SEA, "Smell that salt air! Best smell there is, matey!", "Washed overboard!",
      "Land's nice for a visit. But the sea's home. You'll see someday."),
    TN(TC_LAKE, "Fresh water? I'm used to salt! Still, battle!", "Sunk in a lake!",
      "Out at sea we'd call this a puddle. A pretty puddle, mind you."),
    TN(TC_RIVER, "A river's just the sea in a hurry! Ahoy!", "Swept downstream!",
      "Every river ends up at the sea. So will I, once I'm done here."),
    TN(TC_TOWN, "Ahoy! Just in from {TOWN}! My legs are wobbly!", "Wobbled right over!",
      "Give me a deck any day. Land doesn't rock you to sleep."),
    T("All hands on deck! A challenger comes aboard!", "Man overboard!",
      "Every sailor I know has a WATER POK@MON and a good map. I've got one of those."),
    TN(TC_RAIN, "Rain's nothing! You should see a storm at sea!", "Swamped!",
      "Out at sea you learn: storms always pass. So do losing streaks."),
    T("Your {LEAD} has good sea legs, I can tell!", "Good sea legs indeed!",
      "I'd sign your {LEAD} on as crew any day. The pay's bad. The views aren't."),
    T("My old captain said: never turn down a battle!", "...He never said how to lose.",
      "The captain lives in a little house by the shore now. He still never turns down a battle."),
    TN(TC_SEA | TC_LATE, "I've sailed every sea. This one's the oddest!", "Odd AND strong!",
      "Islands that weren't on my charts, currents that change their minds. I love it here."),
    // The wreck.
    TK(RDUN_SHIP, 0, "I was a deckhand on this ship! I came to visit her!", "Sunk, just like her!",
      "She was a fine ship. Leaky, but fine. Mind the third stair, it's soft."),
    TK(RDUN_SHIP, 0, "Ahoy! Lost my cabin key in here. Seen it?", "Lost the key AND the battle!",
      "Keys roll everywhere on a tilted ship. Look in the corners, matey."),
    TK(RDUN_SHIP, 0, "Hear that creak? Just the old hull, I hope.", "That creak was me.",
      "Old sailors say a ship remembers. This one remembers a lot of storms."),
};

static const struct RogueTrainerSet sPokefanSets[] =
{
    T("Look at my darlings! Aren't they the cutest?", "My precious babies!",
      "I brush them every night. They each have their own brush. With their names on."),
    T("Your {LEAD}! May I pet it? After I win!", "Oh dear, oh dear!",
      "Your {LEAD} is darling. I can tell you love it. It shows."),
    T("You love POK@MON too? Let's battle like friends!", "Oh my! Such strength!",
      "Friends forever? Friends forever. I've decided."),
    TN(TC_TOWN, "My darlings won Most Cuddly in {TOWN}!", "Most Cuddly... lost.",
      "We're going for Most Brave next. Wish us luck!"),
    T("I knitted sweaters for all my POK@MON! Battle!", "The sweaters are ruined!",
      "I'll knit new ones. Bigger. With pockets. Pockets for treats."),
    TN(TC_MON, "I just saw {MON}! So precious! I want a hug!", "Hug me, I lost!",
      "I can't catch them. I just want to hug them all. That's not how it works, apparently."),
    T("Every POK@MON is precious. Even the biters!", "Oh, my precious ones!",
      "A happy POK@MON fights harder. Give yours a cuddle tonight, won't you?"),
    T("I'm sewing costumes for the {FESTIVAL}!", "Costume drama!",
      "Matching costumes for me and all my darlings. It takes all year. Worth it."),
    TN(TC_RAIN, "My darlings have little raincoats! Precious!", "The raincoats didn't help!",
      "Dry them off well after a battle in the rain. A warm towel and a cuddle work wonders."),
};

static const struct RogueTrainerSet sPsychicSets[] =
{
    TN(TC_ROAD, "I knew you'd come down {ROUTE} today.", "I didn't see THAT.",
      "I've been waiting since Tuesday. I foresaw you. I didn't foresee losing."),
    T("I can see your next move... And the one after...", "My visions failed me...",
      "Next time I'll close my eyes. Maybe I'll see better."),
    T("I sensed you coming a mile away. I sense a battle!", "Sensed wrong...",
      "I sense you're kind to your POK@MON. That one wasn't hard."),
    T("Your {LEAD}... is thinking about lunch.", "Now it's thinking about winning.",
      "Feed your {LEAD} soon. I have a strong feeling about it."),
    T("I dreamed about you last night. You won. Let's see!", "Dreams do come true...",
      "Tonight I'll dream that I win. Then we'll see who's right."),
    TN(TC_RAIN, "I foresaw rain. Not forgetting my umbrella.", "Nor this.",
      "The future's clear. The present is wet."),
    TN(TC_NEXT, "I see you in {NEXT}... smiling. Battle!", "I see me... losing.",
      "Don't ask what you'll be smiling about in {NEXT}. Visions are vague. Just go."),
    T("Spoons bend for me. Battles... we'll see.", "Bent out of shape...",
      "I bent a spoon this morning. With my hands. Just to stay in practice."),
    // The tower and the ruins.
    TK(RDUN_TOWER, 0, "The spirits showed me your face... and a battle.", "They didn't show me this.",
      "They're friendly, the spirits. They only want to be remembered."),
    TK(RDUN_TOWER, 0, "A quiet mind hears far. I come here to listen.", "My mind is very quiet now.",
      "Up here the noise of the world fades. You can almost hear the past."),
    TK(RDUN_RUINS, 0, "These stones hum with old thoughts!", "...The stones laughed.",
      "Someone stood where you're standing a thousand years ago. Strange, isn't it?"),
    TK(RDUN_RUINS, 0, "I foresaw ruins. Then I found ruins. Then you!", "Didn't foresee that.",
      "My visions point deeper. Always deeper. I'm going to need more snacks."),
    TN(TC_VICTORY, "I foresaw you reaching the LEAGUE. Am I right?", "I was right.",
      "Go on. The future's waiting at the top. I've seen it. Well, a little."),
};

static const struct RogueTrainerSet sHexManiacSets[] =
{
    T("The spirits say you're strong. And that it'll rain.", "The spirits were right...",
      "Hee hee... They're always right about the rain. Bring an umbrella."),
    T("Hee hee... Stay a while... Stay forever...", "The curse... has lifted...",
      "Or just for a battle. I'm not scary, really. I just like the dark. And spooky things."),
    T("Something followed you... hee hee... your POK@MON!", "Spooked...",
      "Your POK@MON is fond of you. I can tell. The spirits can too."),
    T("Your {LEAD}'s shadow is so pretty... hee hee.", "Shadows lost...",
      "I collect shadows. Not really. I just look at them a lot."),
    TN(TC_ROAD, "{ROUTE} at dusk... the shadows grow so long...", "Hee... ugh.",
      "I come out here every evening. The shadows and I go way back. Hee hee."),
    TN(TC_RAIN, "Rainy days are the best! So gloomy! Hee hee...", "Too gloomy...",
      "Puddles are little mirrors. Look in one. Hee hee... Who's that behind you?"),
    T("I put a tiny curse on you. You'll stub a toe.", "...I cursed myself by mistake.",
      "Curses aren't real. Hee hee. Still, watch your toes."),
    T("The {FESTIVAL}... so many people... hee hee.", "Back to the shadows...",
      "Everyone has fun at the {FESTIVAL}. Even me. Quietly. In a corner."),
    TK(RDUN_SHIP, 0, "This ship's ghosts love company. Hee hee... Stay!", "The ghosts laughed at me...",
      "Something walks the deck at midnight. A GHOST POK@MON, I hope. Hee hee."),
    TK(RDUN_TOWER, 0, "Hee hee... The tower whispers your name.", "Now it says <oops>.",
      "The spirits here are POK@MON who were loved. That's why they stay."),
    TK(RDUN_TOWER, 0, "I climb up here to visit... old friends.", "Old friends, new loss.",
      "Leave a flower at the top. They like flowers. Everyone likes flowers. Hee hee."),
    TK(RDUN_RUINS, 0, "Old places have old spirits. They don't mind me.", "The spirits sided with you.",
      "Be polite in here. Say excuse me to the walls. It can't hurt. Hee hee."),
};

static const struct RogueTrainerSet sGentlemanSets[] =
{
    T("Ah, a challenger. Allow me to remove my hat. There.", "Splendid! Most splendid!",
      "I'll put my hat back on now. A gentleman knows when he's beaten."),
    T("A gentleman never refuses a challenge. En garde!", "I tip my hat to you.",
      "My POK@MON have been with me for years. Treasure yours, young one."),
    TN(TC_TOWN, "My house in {TOWN} has a lovely garden. Shall we?", "Splendidly done.",
      "Do visit. Tea at four, every day. My POK@MON prefer biscuits."),
    TN(TC_RAIN, "Rain is no excuse for poor manners. Shall we?", "Splendidly soaked.",
      "I always carry an umbrella. Today I forgot it. Most unlike me."),
    T("In my day, we bowed before battles. Like so!", "Most impressive!",
      "Manners cost nothing, young one. A good battle costs a POTION or two."),
    T("A fine {LEAD}! I had one like it as a boy.", "Splendid, simply splendid!",
      "Look after your {LEAD}. One day you'll tell stories about it. As I do."),
    T("I've read every book on {FIGURE}. Are you as bold?", "Bolder, it seems!",
      "{FIGURE} {FIGURE_DEED}. Curious and brave, young one. Like you."),
    T("Stamps, coins and POK@MON. I collect all three!", "Splendid!",
      "Only one of those battles back. The coins are worth more. The POK@MON are worth everything."),
    TK(RDUN_SHIP, 0, "This was a fine ship once. I sailed on her!", "Splendid, as in those days.",
      "The dining room served the best soup at sea. Long ago now. Mind the stairs."),
};

static const struct RogueTrainerSet sCooltrainerMSets[] =
{
    TN(TC_ROAD, "You don't get to {ROUTE} by luck. Show me!", "So that's what got you here.",
      "Some TRAINERS make it this far and stop. I don't think you're one of them."),
    TN(TC_ROAD, "I've beaten every TRAINER on this road. You're next!", "Not every TRAINER, then.",
      "I'll start again from the bottom of the road. A fresh streak."),
    T("Good eyes. Let's see if your team matches them.", "They do.",
      "I write down every battle I lose. My notebook used to be thin."),
    TN(TC_GYMA, "Beaten {LEADER} yet? I have. Your turn!", "Ouch!",
      "{LEADER} told me I'd meet someone tougher on the road. I didn't believe it."),
    TN(TC_LATE, "Real TRAINERS train where it's hard. Welcome.", "You belong here.",
      "The LEAGUE's not far now. You can feel it in every battle, can't you?"),
    T("Your {LEAD} looks battle-hardened. I like it.", "Hardened indeed!",
      "Train your {LEAD} with care. A team that trusts you will follow you anywhere."),
    T("No warm-ups. Every battle counts!", "That one counted.",
      "Lose with your head up, then go and train. That's all I know."),
    TK(RDUN_PLANT, 0, "Nobody trains in here but me. The hum keeps me sharp.", "Sharp, but not enough.",
      "Sparks, switches, VOLTORB in every corner. This place is a test. You passed it."),
    TN(TC_VICTORY, "Eight BADGES and you're still here. So am I.", "Then you go on, and I stay.",
      "I've climbed this road three times. One day I'll walk through that door at the top."),
    TN(TC_VICTORY, "This road weeds out the ones who aren't ready.", "You're ready.",
      "Go on. The ELITE FOUR aren't kind. But you've earned your shot."),
};

static const struct RogueTrainerSet sCooltrainerFSets[] =
{
    T("Only the best walk this road. Are you one?", "You're the real deal.",
      "Keep your HYPER POTIONS close. You'll need every one of them."),
    T("I train here every day. I won't be easy to beat!", "...You're good. Really good.",
      "Switching out isn't running away. It's how the best TRAINERS win."),
    TN(TC_TOWN, "I left {TOWN} with one POK@MON and a dream!", "Not far enough yet...",
      "The dream's still big. The team's just catching up to it."),
    T("Nice form. Let's see if there's anything behind it.", "There was.",
      "I'll remember this battle. That's a compliment, by the way."),
    TN(TC_GYMA, "{LEADER} is tough, but I'm tougher. Want proof?", "No proof today.",
      "Tell {LEADER} I'm coming next. I'm sure that'll get a laugh."),
    TNA(TC_BADGES, TC_VICTORY, "How many BADGES? {^BADGES}? Then no holding back.", "Earned, every one.",
      "Some TRAINERS collect BADGES. Others earn them. You can tell by how they battle."),
    TN(TC_LATE, "A few roads left before the LEAGUE. Don't slow down!", "You didn't.",
      "I'll make it to the LEAGUE someday too. Maybe we'll meet there."),
    TNA(0, TC_EARLY, "A TRAINER called {NAME} beat me in three turns.", "And you, in four.",
      "{NAME} didn't even look surprised. Just asked about my team, then walked on."),
    TN(TC_VICTORY, "Last road before the LEAGUE. Prove it's not luck!", "It wasn't.",
      "Go on up. The ELITE FOUR are waiting. I'll be here, training for next year."),
    TN(TC_VICTORY, "Every step up here is harder. Ready for this one?", "You were ready.",
      "Rest before the LEAGUE, trust me. Everyone wants to rush it."),
};

static const struct RogueTrainerSet sExpertSets[] =
{
    TN(TC_ROAD, "Fifty years I've battled on this road. Let's see you.", "Well fought, youngster.",
      "The road hasn't changed in fifty years. I have. And you're still a surprise."),
    T("Your eyes have seen many battles. Let me test you.", "Hah! You passed.",
      "My first team trained with sticks and patience. You have it easy."),
    T("Experience beats youth every time. Shall I prove it?", "Hah! Not every time, it seems.",
      "Next time I'll use more experience. I've got plenty left."),
    T("I was born in {OLDTOWN} when it had six houses!", "Hah! Well done.",
      "{OLDTOWN} has more houses now. Fewer good TRAINERS, though. You might change that."),
    T("Sit, youngster. Battle first, stories after.", "Hah! You've earned a story.",
      "Long ago, {FOUNDING}.|That's how {REGION} began, my grandmother said."),
    T("{LEAD}... I trained one like it long ago.", "Yours is better trained than mine was.",
      "Your {LEAD} trusts you. Keep it that way, and it'll never let you down."),
    TN(TC_RAIN, "Rain or shine, I battle. My knees don't agree.", "Hah! A creaky defeat.",
      "At my age, rain is a weather report from my knees."),
    TN(TC_GYMA, "I sparred with {LEADER} long ago. Let's see you!", "Better than me, indeed.",
      "Give {LEADER} my regards. Say the old one from the road says hello."),
    TNA(0, TC_EARLY, "{NAME} beat me this spring. Let's see you!", "Hah! Busy year.",
      "That {NAME} had the calmest eyes I've ever seen. Yours are getting there."),
    TK(RDUN_ICE, 0, "The cold keeps an old body sharp! Battle!", "Hah! Cold comfort.",
      "The ice taught me patience. Push the boulder, wait, think, push again."),
    TN(TC_VICTORY, "Nine times I've climbed this road! Battle!", "Hah! You'll go through.",
      "I've never gone through the door at the top. Some of us love the climb more. Go on."),
    TN(TC_VICTORY, "All eight BADGES. In my day, that was everything.", "Well fought.",
      "It still is. The LEAGUE is past this road. Show them a road-hardened team."),
};

#define CLASS_SETS(s) { s, LORE_COUNT_OF(s) }
static const struct { const struct RogueTrainerSet *sets; u8 count; } sClassSets[ROGUE_CLASS_COUNT] =
{
    [RCLASS_YOUNGSTER]     = CLASS_SETS(sYoungsterSets),
    [RCLASS_LASS]          = CLASS_SETS(sLassSets),
    [RCLASS_BUG_CATCHER]   = CLASS_SETS(sBugCatcherSets),
    [RCLASS_CAMPER]        = CLASS_SETS(sCamperSets),
    [RCLASS_PICNICKER]     = CLASS_SETS(sPicnickerSets),
    [RCLASS_FISHERMAN]     = CLASS_SETS(sFishermanSets),
    [RCLASS_BIRD_KEEPER]   = CLASS_SETS(sBirdKeeperSets),
    [RCLASS_TUBER]         = CLASS_SETS(sTuberSets),
    [RCLASS_SCHOOL_KID]    = CLASS_SETS(sSchoolKidSets),
    [RCLASS_HIKER]         = CLASS_SETS(sHikerSets),
    [RCLASS_RUIN_MANIAC]   = CLASS_SETS(sRuinManiacSets),
    [RCLASS_NINJA_BOY]     = CLASS_SETS(sNinjaBoySets),
    [RCLASS_BLACK_BELT]    = CLASS_SETS(sBlackBeltSets),
    [RCLASS_BATTLE_GIRL]   = CLASS_SETS(sBattleGirlSets),
    [RCLASS_KINDLER]       = CLASS_SETS(sKindlerSets),
    [RCLASS_AROMA_LADY]    = CLASS_SETS(sAromaLadySets),
    [RCLASS_BEAUTY]        = CLASS_SETS(sBeautySets),
    [RCLASS_GUITARIST]     = CLASS_SETS(sGuitaristSets),
    [RCLASS_SAILOR]        = CLASS_SETS(sSailorSets),
    [RCLASS_POKEFAN]       = CLASS_SETS(sPokefanSets),
    [RCLASS_PSYCHIC]       = CLASS_SETS(sPsychicSets),
    [RCLASS_HEX_MANIAC]    = CLASS_SETS(sHexManiacSets),
    [RCLASS_GENTLEMAN]     = CLASS_SETS(sGentlemanSets),
    [RCLASS_COOLTRAINER_M] = CLASS_SETS(sCooltrainerMSets),
    [RCLASS_COOLTRAINER_F] = CLASS_SETS(sCooltrainerFSets),
    [RCLASS_EXPERT]        = CLASS_SETS(sExpertSets),
    [RCLASS_SWIMMER_M]     = CLASS_SETS(sSwimmerSets),
    [RCLASS_SWIMMER_F]     = CLASS_SETS(sSwimmerSets),
};

// ---------------------------------------------------------------------------
// Gym trainers: they talk about their leader by name ({LEADER}), the gym's
// type and town. A leader who is two people (TATE&LIZA) takes no "says":
// those sets avoid TC_PAIR. Sets for one class come first in their gym.

#define W(c) (1u << RCLASS_##c)
#define W_KID (W(YOUNGSTER) | W(LASS) | W(BUG_CATCHER) | W(CAMPER) | W(PICNICKER) | W(TUBER) | W(SCHOOL_KID) | W(NINJA_BOY))
#define W_GROWN (~W_KID & ((1u << ROGUE_CLASS_COUNT) - 1))

static const struct RogueTrainerSet sGymSets[] =
{
    TG(0, TC_PAIR, "{LEADER} says {TYPE} POK@MON never back down!", "Backed down...",
      "I'm still working on that part. {LEADER} will be glad you got this far."),
    TG(0, 0, "Nobody gets to {LEADER} without getting past me!", "...Except you, it seems.",
      "Go on, then. {LEADER} is waiting. Good luck. You'll need it."),
    TG(0, 0, "Welcome to our GYM! Know much about {TYPE} POK@MON?", "Now I know about YOURS.",
      "I joined the {STEM} GYM to learn from {LEADER}. Today I learned from you."),
    TG(W_GROWN, 0, "Three years I've trained under {LEADER}. Watch!", "Three more years, then.",
      "{LEADER} once told me losing is just learning out loud. I learned very loudly today."),
    TG(0, 0, "All of {TOWN} cheers for {LEADER}. Today, for me!", "They're not cheering...",
      "Win or lose, {TOWN} loves its GYM. You'll see, when you win."),
    TG(W_KID, 0, "I'm {LEADER}'s youngest student! Don't go easy!", "You didn't go easy...",
      "When I'm big, I'll be a GYM LEADER too. Just like {LEADER}!"),
    TG(0, 0, "{TYPE} POK@MON are the best! We all agree!", "Mostly agree...",
      "Go show {LEADER} what you've got. I want to hear all about it."),
    TG(0, 0, "We polish this GYM's floor till it shines. Don't slip!", "I slipped.",
      "{LEADER} makes us all clean the GYM. It builds character. And clean floors."),
    TG(0, 0, "Another challenger! {LEADER} will want to know!", "...Especially about this.",
      "I'll tell {LEADER} you're coming. Actually, no. Surprise everyone."),
    TG(W_GROWN, 0, "I moved to {TOWN} to train under {LEADER}!", "...Worth it, still.",
      "Before the GYM I couldn't win a battle. Now I can lose properly. That's progress."),
    TG(0, TC_PAIR, "{LEADER} never loses here. Well, hardly ever.", "Hardly ever... starting today?",
      "If you beat {LEADER}, come back and tell me how. I've been trying for years."),
    TG(0, 0, "The {BADGE}'s not for just anyone! Prove it!", "Fine, you deserve a shot.",
      "The {BADGE} is the finest in {REGION}. We all think so. We're not biased."),
    TG(W_GROWN, 0, "I lost to {LEADER} ten times. Then I joined up!", "Ten losses and one more.",
      "It's the best seat in the house. I see every challenger. You'll do well."),
    TG(0, TC_PAIR, "Shh! {LEADER} is watching. Look sharp!", "{LEADER} saw that...",
      "Don't worry about me. {LEADER} says every loss is a lesson. I'm very well taught."),
    TG(0, 0, "My whole team is {TYPE}. Not one weakness!", "...Okay, a few.",
      "{LEADER} helped me pick every one of them. They suit me, don't they?"),
    TG(0, 0, "Want to reach {LEADER}? You go through me!", "Wrong way, it seems.",
      "{LEADER} is just ahead. Take a breath first. Trust me."),
    TG(0, 0, "The GYM's been buzzing since you got to {TOWN}!", "They'll talk more now.",
      "Say hi to {LEADER} for me. I'll be right here. Practicing."),
    TG(W_KID, 0, "Mom brings me to the GYM after school! Battle!", "Second best student...",
      "{LEADER} gave me a sticker for trying. I've got twelve now!"),
    TG(W_GROWN, 0, "You've got the look. I've seen it a hundred times.", "The look was right.",
      "{LEADER} will see it too. Go on. Make it a good one."),
    TG(0, 0, "Our {TYPE} types are the best in {REGION}!", "Tested. Failed.",
      "The rest of {REGION} could learn a lot from {LEADER}. So could I, clearly."),
    TG(0, 0, "I'm the pride of this corner of the GYM!", "Corner... lost.",
      "{LEADER} is the pride of {TOWN}. Corners are underrated, though. You see everyone coming."),
    TG(0, TC_PAIR, "{LEADER} spots talent at a glance. So can I!", "Spotted it.",
      "When you face {LEADER}, watch the eyes. That's where every battle starts."),
    // Classes in their own gyms.
    TG(W(PICNICKER), 0, "We grow flowers in the GYM! {LEADER}'s idea!", "Trampled!",
      "Water them on your way out? {LEADER} likes the flowers to have visitors."),
    TG(W(AROMA_LADY), 0, "{LEADER} keeps this GYM full of blooms, dear.", "Wilted, dear.",
      "Every flower in here was planted by {LEADER}. Every single one. Imagine that."),
    TG(W(FISHERMAN), 0, "Fishing in a GYM! {LEADER} lets me!", "Reeled in... by you!",
      "As long as I battle every challenger. There's nothing in this pool, but I fish all the same."),
    TG(W(TUBER), TC_PAIR, "{LEADER} is teaching me to swim! No floatie!", "Waaah!",
      "When I can swim the whole pool, {LEADER} says I can battle for real."),
    TG(W(SAILOR), 0, "Ahoy! {LEADER} runs a tight ship!", "Sunk in a pool!",
      "Even a sailor needs a GYM. Best crew I've ever served with."),
    TG(W(BIRD_KEEPER), 0, "My birds train here because {LEADER} does!", "Feathers everywhere!",
      "{LEADER}'s birds fly higher than anything. Mine are getting closer!"),
    TG(W(BIRD_KEEPER), 0, "The wind's our friend in here! Got a coat?", "Blown away!",
      "The fans keep the air moving. {LEADER} wants the birds to feel at home."),
    TG(W(HIKER), 0, "Hah! This GYM's floor is solid rock!", "Solid defeat!",
      "{LEADER} likes it that way. Rock under your feet keeps you honest. Hah!"),
    TG(W(HIKER), 0, "Rock is patient, rock is strong. Battle!", "Crumbled!",
      "{LEADER} taught me that. I've got patient down. I'm working on strong."),
    TG(W(RUIN_MANIAC), 0, "I dig in the GYM's floor! Found a coin! Battle!", "Buried!",
      "{LEADER} let me keep the coin. It's in a glass case now. Good LEADER."),
    TG(W(RUIN_MANIAC), 0, "{TYPE} types are the oldest of all! I'm sure!", "Old and beaten!",
      "I'll prove it someday. {LEADER} told me to go ahead and try."),
    TG(W(NINJA_BOY), 0, "Can't see us? That's {LEADER}'s training!", "Spotted!",
      "{LEADER} teaches us to hide in plain sight. I'm still on the plain sight part."),
    TG(W(NINJA_BOY), 0, "Mist, shadow, poison! {LEADER}'s ninja way!", "My way failed!",
      "Training here means staying still for hours. I'm good at that. I'm bad at the rest."),
    TG(W(BLACK_BELT), TC_STANDIN, "HOO-HAH! In {LEADER}'s GYM we train till we drop!", "Dropped!",
      "We all drop here. Then we get up. {LEADER} calls that the lesson."),
    TG(W(BLACK_BELT), TC_STANDIN, "{LEADER} wakes us at dawn for drills! Hoo!", "Hrrgh! Not awake yet!",
      "Drills at dawn again tomorrow. Maybe before dawn. I'll suggest it."),
    TG(W(BATTLE_GIRL), 0, "{LEADER}'s plan: run, punch, battle, repeat!", "Repeat later...",
      "I've followed the plan for a year. It works. Just not today."),
    TG(W(KINDLER), 0, "This GYM's the hottest spot in {TOWN}!", "Cooled off!",
      "{LEADER} lit the first flame in here. It's never gone out."),
    TG(W(KINDLER), 0, "Burn bright! That's the rule in here!", "Fizzled!",
      "I'm burning second brightest. {LEADER} is brighter. Obviously."),
    TG(W(GUITARIST), 0, "{LEADER}'s GYM rocks! Feel those speakers!", "Unplugged!",
      "We jam after closing. Some nights {LEADER} comes by to listen."),
    TG(W(GUITARIST), 0, "Every challenger gets a song. Here's yours!", "Song's over!",
      "I'll finish your song tonight. I hope it ends with you beating {LEADER}."),
    TG(W(PSYCHIC), 0, "I foresaw you coming. {LEADER} foresaw it first.", "Didn't see this part...",
      "{LEADER} sees further than any of us. You'll find out."),
    TG(W(PSYCHIC), 0, "{LEADER}'s GYM tests the mind. Clear yours!", "My mind is very clear now.",
      "Breathe before you face {LEADER}. In, and out. There."),
    TG(W(HEX_MANIAC), TC_PAIR, "Hee hee... {LEADER}'s GYM is so cozy and dark...", "Hee... ouch.",
      "The candles in here never go out. {LEADER} says they're watched. Hee hee."),
    TG(W(HEX_MANIAC), 0, "The spirits here adore {LEADER}. And you?", "They liked that.",
      "They'll follow you to {LEADER}. Don't mind them. Hee hee."),
    TGN(W(BLACK_BELT), TC_STANDIN, 0, "Even in a {TYPE} GYM, a fighter trains! Hoo!", "Hoo... down I go!",
      "{LEADER} took me in when no other GYM would. I'll repay that, one punch at a time."),
    TG(W(BLACK_BELT), TC_PAIR, "{LEADER} says my spirit is strong. My fists, less so.", "Hoo... my fists lost again.",
      "I'll train here until my fists catch up with my spirit. That could take a while. Hoo."),
    TG(W(EXPERT), 0, "A thousand battles I've seen here. Show me a new one!", "Hah! That was new.",
      "{LEADER} still surprises me after all these years. So did you."),
    TG(W(EXPERT), 0, "Old hands guard this door. Mind your step, youngster.", "Hah! Sure-footed.",
      "Go on through. {LEADER} doesn't bite. Well. The POK@MON might."),
    TG(W(EXPERT), 0, "I left retirement to train under {LEADER}!", "Worth it.",
      "In {TOWN} they think I'm too old for this. {LEADER} didn't. Neither do you, I think."),
    TG(W(EXPERT), 0, "{TYPE}... a strong type, used well. Meet it!", "Hah! Met and bested.",
      "{LEADER} uses {TYPE} better than anyone I've seen. Watch closely in there."),
    // Fighters and veterans standing in, in gyms of a type no class has:
    // they talk about the gym's type, not their own.
    TGT(RTYPE_ICE, W(BLACK_BELT), 0, "Hoo! I train barefoot on the ice in here!", "Brr... beaten!",
      "My feet are numb, but my spirit is warm. {LEADER} says that's the whole point."),
    TGT(RTYPE_ICE, W(EXPERT), 0, "Cold sharpens the mind, youngster. Feel it?", "Hah! Sharp indeed.",
      "Thirty winters I've trained in this cold. Mind the slippery bits on your way to {LEADER}."),
    TGT(RTYPE_DRAGON, W(BLACK_BELT), TC_PAIR, "I came here to out-punch a dragon! Hoo!", "Dragon-punched again!",
      "{LEADER} says dragons respect a strong spirit. I'm working on mine."),
    TGT(RTYPE_DRAGON, W(EXPERT), 0, "Dragons live long, youngster. So do old fools.", "Hah! Well flown.",
      "I've watched {LEADER} raise dragons from eggs. Mind the big one. It likes to sit on people."),
    TGT(RTYPE_STEEL, W(BLACK_BELT), TC_PAIR, "I harden my fists like STEEL! Hoo!", "Dented!",
      "{LEADER} says steel isn't made by punching walls. My knuckles agree."),
    TGT(RTYPE_STEEL, W(EXPERT), 0, "Steel is patient, youngster. It waits you out.", "Hah! You didn't tire.",
      "{LEADER} polishes every POK@MON in here by hand. Mind your fingerprints."),
    TGT(RTYPE_DARK, W(BLACK_BELT), 0, "Hoo! I train with the lights off in here!", "Didn't see that coming!",
      "{LEADER} says a true fighter doesn't need to see. I still bump into things."),
    TGT(RTYPE_DARK, W(EXPERT), 0, "Shadows teach patience. Have you any, youngster?", "Hah! Plenty, it seems.",
      "Don't let {LEADER}'s grin fool you. Every move in there is a trick."),
    TGT(RTYPE_NORMAL, W(BLACK_BELT), TC_PAIR, "{LEADER} says NORMAL POK@MON have the most spirit!", "Normally I win!",
      "Plain and strong, that's the NORMAL way. {LEADER} lives it every day. Hoo!"),
    TGT(RTYPE_NORMAL, W(EXPERT), 0, "NORMAL, they call it. Nothing ordinary in here!", "Hah! Extraordinary.",
      "All my years of battling, and {LEADER}'s NORMAL POK@MON still surprise me."),
};

// The first trainer in a cave the road goes on through below says so in
// their own voice, as nothing else would tell the player the way on is
// down the ladder.
static const char *const sTrainerWayBelow[] =
{
    "No way through up here, kid. The road goes on below. Take the ladder down! Hah!",
    "Seeking the way forward? It lies below! Down the ladder, then onward! Hoo!",
    "The way on? Down! Always down! The ladder's back there. The road goes on below.",
};

// If a line cannot be filled where it is said (nothing should, the dump
// checks), this is said instead.
static const char *const sTrainerFallback[3] = { "Let's battle!", "You win!", "You're strong. Keep it up!" };

#undef T
#undef TN
#undef TNA
#undef TK
#undef TG
#undef TGN
#undef TGT
#undef W

// ---------------------------------------------------------------------------
// Where a trainer stands, and which set they say.

// The weather of a route, as the game draws it (src/rogue.c, the map
// headers): the first two routes stay clear, deserts have sandstorms or
// falling ash, the woods are shady, the region's rainy and misty routes
// (RogueGen_KnownWeather) have theirs, and now and then a route has a
// shower, lake country most of all.
u8 RogueGen_RouteWeather(const struct RogueRegion *r, u8 place)
{
    const struct RoguePlace *p = &r->places[place];
    u32 rng = RogueRng_Hash(r->seed, 8000 + place);
    u8 known;
    if (p->kind == RPLACE_TOWN)
        return RWEATHER_SUN;
    if (p->kind != RPLACE_ROUTE)
        return RWEATHER_NONE;
    if (p->tier < 2 && !p->side)
        return p->type == RROUTE_WOODS ? RWEATHER_SHADE : RWEATHER_SUN;
    // v1.1: the region's rainy and misty routes (src/rogue_gen_zones.h).
    known = RogueGen_KnownWeather(r, place);
    if (known != RWEATHER_NONE)
        return known;
    if (p->type == RROUTE_DESERT)
        return p->theme == RTHEME_ASH ? RWEATHER_ASH : RWEATHER_SAND;
    if (p->type == RROUTE_WOODS)
        return RWEATHER_SHADE;
    if (p->type != RROUTE_MOUNTAIN && p->type != RROUTE_HILLS
     && RogueRng_Range(&rng, 100) < ((p->type == RROUTE_LAKE || p->type == RROUTE_RIVER || p->type == RROUTE_COAST
                                      || p->type == RROUTE_LONGGRASS) ? 15 : 6))
        return RWEATHER_RAIN;
    return RWEATHER_SUN;
}

struct TrainerSpot
{
    u32 bits; // TC_*
    u8 cls;   // RCLASS_*
    u8 kind;  // RDUN_* of the dungeon they stand in, RDUN_COUNT for none
};

static bool8 TrainerIsGym(u32 slot)
{
    return slot >= ROGUE_SLOT_GYM_0 && slot < ROGUE_SLOT_LEADER_0;
}

// The place a trainer's lines are said at: their route or cave, their
// dungeon's way in, their gym's town.
u8 RogueText_TrainerPlace(const struct RogueRegion *r, u32 slot)
{
    u8 place = RogueGen_SlotPlace(r, slot);
    s32 d;
    if (TrainerIsGym(slot))
        return r->gymTown[(slot - ROGUE_SLOT_GYM_0) / ROGUE_GYM_TRAINERS];
    if (place != 0xFF)
        return place;
    d = RogueGen_SlotDungeon(r, slot);
    return d >= 0 ? r->dungeons[d].host : r->start;
}

// A slot with a class trainer of its own lines: a route, cave or dungeon
// trainer (not a villain, not a dungeon's master) or a gym trainer.
static bool8 TrainerTalks(const struct RogueRegion *r, u32 slot)
{
    if (TrainerIsGym(slot))
        return (slot - ROGUE_SLOT_GYM_0) % ROGUE_GYM_TRAINERS < gRogueGymTrainers[(slot - ROGUE_SLOT_GYM_0) / ROGUE_GYM_TRAINERS];
    if (slot >= r->nTrainerSlots)
        return FALSE;
    if (RogueGen_SlotPlace(r, slot) == 0xFF && RogueGen_SlotDungeon(r, slot) < 0)
        return FALSE;
    return RogueGen_MasterDungeon(r, slot) < 0;
}

// Where a trainer stands, from their place (0xFF for none) or dungeon (-1
// for none): the same for every trainer there.
static void TrainerSpotOf(const struct RogueRegion *r, u32 slot, u8 cls, u8 place, s32 d, struct TrainerSpot *s)
{
    u8 host, gym, next, kind;
    const struct RoguePlace *p;
    u32 i;
    s->cls = cls;
    s->kind = RDUN_COUNT;
    if (TrainerIsGym(slot))
    {
        s->bits = TC_GYM | TC_INDOOR | TC_TOWN;
        if (RogueGen_GymLeader(r, (slot - ROGUE_SLOT_GYM_0) / ROGUE_GYM_TRAINERS)->doubles)
            s->bits |= TC_PAIR;
        return;
    }
    s->bits = 0;
    s->kind = kind = RogueGen_DungeonKindAt(r, place, d);
    host = place != 0xFF ? place : d >= 0 ? r->dungeons[d].host : r->start;
    p = &r->places[host];
    if (place != 0xFF)
    {
        s->bits |= TC_PLACE;
        if (p->kind == RPLACE_CAVE)
        {
            s->bits |= TC_UNDER;
            if (p->caveOut == r->summit)
                s->bits |= TC_VICTORY;
        }
        else
        {
            s->bits |= TC_OUT | TC_ROAD;
            switch (p->type)
            {
            case RROUTE_FOREST: case RROUTE_WOODS: s->bits |= TC_WOODS; break;
            case RROUTE_LAKE:     s->bits |= TC_FRESH | TC_LAKE | TC_WATER | TC_WOODS; break;
            case RROUTE_RIVER:    s->bits |= TC_FRESH | TC_RIVER | TC_WATER; break;
            case RROUTE_COAST: case RROUTE_SEA: s->bits |= TC_SEA | TC_WATER; break;
            case RROUTE_HILLS:    s->bits |= TC_HILLS; break;
            case RROUTE_MOUNTAIN: s->bits |= TC_HILLS | TC_VICTORY; break;
            case RROUTE_FIELD: case RROUTE_FLOWERS: case RROUTE_LONGGRASS: s->bits |= TC_FIELD; break;
            case RROUTE_DESERT:   s->bits |= p->theme == RTHEME_ASH ? TC_ASH : TC_SAND; break;
            }
            switch (RogueGen_RouteWeather(r, place))
            {
            case RWEATHER_RAIN: case RWEATHER_STORM: s->bits |= TC_RAIN; break;
            case RWEATHER_SUN:  s->bits |= TC_SUN; break;
            }
        }
        if (p->area != 0xFF)
            s->bits |= TC_MON;
        if (p->side)
            s->bits |= TC_SIDE;
        else
        {
            // The rival walks on along the road after each battle.
            for (i = 0; i < host; i++)
                if (r->places[i].rival && !r->places[i].side)
                    s->bits |= TC_RIVAL;
        }
    }
    else if (d >= 0)
    {
        s->bits |= TC_DUNGEON;
        switch (kind)
        {
        case RDUN_FOREST: s->bits |= TC_OUT | TC_WOODS; break;
        case RDUN_TOWER: case RDUN_PLANT: case RDUN_HIDEOUT: case RDUN_LAIR: case RDUN_MIRAGE: s->bits |= TC_INDOOR; break;
        case RDUN_SHIP:   s->bits |= TC_INDOOR | TC_SEA | TC_WATER; break;
        case RDUN_VICTORY: s->bits |= TC_UNDER | TC_VICTORY; break;
        default:          s->bits |= TC_UNDER; break;
        }
        if (r->dungeons[d].area != 0xFF)
            s->bits |= TC_MON;
        if (r->places[host].side)
            s->bits |= TC_SIDE;
    }
    if (kind == RDUN_VICTORY)
        s->bits |= TC_VICTORY;
    if (p->tier <= 1)
        s->bits |= TC_EARLY;
    if (p->badges >= 2)
        s->bits |= TC_BADGES;
    if (p->tier >= 6 || (s->bits & TC_VICTORY))
        s->bits |= TC_LATE;
    if (r->places[RogueGen_TownNear(r, host)].kind == RPLACE_TOWN)
        s->bits |= TC_TOWN;
    next = RogueGen_NextTownFrom(r, host);
    if (next != host && r->places[next].kind == RPLACE_TOWN && r->places[next].type != RTOWN_SUMMIT)
        s->bits |= TC_NEXT;
    gym = RogueText_NextGym(r, host);
    if (gym < ROGUE_NUM_GYMS)
    {
        s->bits |= TC_GYMA;
        if (RogueGen_GymLeader(r, gym)->doubles)
            s->bits |= TC_PAIR;
    }
}

static bool8 TrainerSetFits(const struct RogueTrainerSet *t, const struct TrainerSpot *s)
{
    return (t->need & ~s->bits) == 0 && (t->avoid & s->bits) == 0
        && (t->kind == 0 || t->kind - 1 == s->kind)
        && (t->who == 0 || ((t->who >> s->cls) & 1));
}

// How much a set suits where it is said, 0 to 3: a set of the dungeon's
// kind, of Victory Road or of the gym's own class first, then those of the
// land and weather, then those naming a place or a person.
static u32 TrainerSetFit(const struct RogueTrainerSet *t)
{
    if (t->kind || t->who || (t->need & TC_VICTORY))
        return 3;
    if (t->need & (TC_UNDER | TC_WATER | TC_SEA | TC_FRESH | TC_LAKE | TC_RIVER | TC_SAND | TC_ASH | TC_WOODS | TC_FIELD | TC_HILLS | TC_RAIN | TC_SUN))
        return 2;
    if (t->need)
        return 1;
    return 0;
}

#define TRAINER_POOL_MAX 64

// The set a trainer's pool gives them: the one least used by those before
// them, that suits their spot best, then the region's shuffle.
static s32 TrainerPick(const struct RogueTrainerSet *pool, u32 n, const struct TrainerSpot *s, const u8 *used, const u8 *rank)
{
    s32 best = -1;
    u32 bestScore = 0xFFFFFFFF, j;
    for (j = 0; j < n; j++)
    {
        u32 score;
        if (!TrainerSetFits(&pool[j], s))
            continue;
        score = ((u32)used[j] * 4 + 3 - TrainerSetFit(&pool[j])) * TRAINER_POOL_MAX + rank[j];
        if (score < bestScore)
        {
            bestScore = score;
            best = j;
        }
    }
    return best;
}

// The set a trainer slot says, or NULL for one with no class lines. The
// region's trainers of the slot's class (every gym trainer, in a gym) take
// their sets in slot order; this plays that out up to the slot. Slots run
// place by place, then dungeon by dungeon, so the pass goes that way and
// never has to look up where a slot stands (it runs as the player talks:
// it must fit in a frame).
static const struct RogueTrainerSet *TrainerSet(const struct RogueRegion *r, u32 slot, u32 *index)
{
    const struct RogueTrainerSet *pool;
    struct TrainerSpot spot;
    u8 used[TRAINER_POOL_MAX], rank[TRAINER_POOL_MAX];
    u8 cls = RogueGen_TrainerClass(r, slot);
    u32 n, i, k, key;
    s32 pick = -1;
    if (cls >= ROGUE_CLASS_COUNT)
        return NULL;
    if (TrainerIsGym(slot))
    {
        pool = sGymSets;
        n = LORE_COUNT_OF(sGymSets);
        key = 5250;
    }
    else
    {
        pool = sClassSets[cls].sets;
        n = sClassSets[cls].count;
        key = 5200 + cls;
    }
    for (i = 0; i < n; i++)
        used[i] = 0;
    for (i = 0; i < n; i++)
        rank[RogueText_Deal(r->seed, key, i, n)] = i;
    if (TrainerIsGym(slot))
    {
        // The leaders drawn once for the pass, not for every slot.
        u8 gyms[ROGUE_NUM_GYMS], e4[ROGUE_NUM_E4];
        RogueGen_RegionLeaders(r, gyms, e4);
        for (k = ROGUE_SLOT_GYM_0; k <= slot; k++)
        {
            const struct RogueGenLeader *l = &gRogueGenLeaders[gyms[(k - ROGUE_SLOT_GYM_0) / ROGUE_GYM_TRAINERS]];
            if (!TrainerTalks(r, k))
                continue;
            spot.cls = RogueGen_GymTrainerClass(r, k, l->type);
            spot.kind = GYM_KIND + l->type;
            spot.bits = TC_GYM | TC_INDOOR | TC_TOWN | (l->doubles ? TC_PAIR : 0)
                      | (gRogueGenClasses[spot.cls].type != l->type ? TC_STANDIN : 0);
            pick = TrainerPick(pool, n, &spot, used, rank);
            if (pick >= 0)
                used[pick]++;
        }
    }
    else
    {
        struct RogueTeams tm;
        u32 spotOf = 0xFFFF;
        RogueGen_Teams(r, &tm);
        for (i = 0; i < (u32)r->nPlaces + r->nDungeons; i++)
        {
            u8 place = i < r->nPlaces ? i : 0xFF;
            s32 d = i < r->nPlaces ? -1 : (s32)(i - r->nPlaces);
            u32 base = d < 0 ? r->places[i].trainerBase : r->dungeons[d].trainerBase;
            u32 count = d < 0 ? r->places[i].nTrainers : r->dungeons[d].nTrainers;
            if (count == 0)
                continue;
            if (base > slot)
                break;
            for (k = base; k < base + count && k <= slot; k++)
            {
                if (d >= 0 && r->dungeons[d].boss == RBOSS_MASTER && k == base + count - 1)
                    continue; // the master has lines of their own
                if (k != slot && (RogueGen_VillainRoleAt(r, k, &tm) != RVROLE_NONE
                               || RogueGen_TrainerClassAt(r, k, place, d) != cls))
                    continue;
                if (spotOf != i)
                {
                    TrainerSpotOf(r, k, cls, place, d, &spot);
                    spotOf = i;
                }
                pick = TrainerPick(pool, n, &spot, used, rank);
                if (pick >= 0)
                    used[pick]++;
            }
        }
    }
    if (pick < 0)
        return NULL;
    if (index != NULL)
        *index = pick;
    return &pool[pick];
}

// The first trainer in a cave the road goes on through below.
static bool8 TrainerAtWayBelow(const struct RogueRegion *r, u32 slot)
{
    u8 place = RogueGen_SlotPlace(r, slot);
    u32 i;
    if (place == 0xFF || r->places[place].kind != RPLACE_CAVE || slot != r->places[place].trainerBase)
        return FALSE;
    for (i = 0; i < r->nDungeons; i++)
        if (r->dungeons[i].host == place && r->dungeons[i].required && r->dungeons[i].kind != RDUN_VICTORY)
            return TRUE;
    return FALSE;
}

// A class trainer's line (RTRAINER_INTRO, _DEFEAT, _AFTER) as a template,
// NULL for a slot that is not a class trainer (a villain).
const char *RogueText_TrainerLine(const struct RogueRegion *r, u32 slot, u8 field)
{
    return RogueText_TrainerLineOfSet(r, slot, RogueText_TrainerSetIndex(r, slot), field);
}

// Which set of its pool a trainer slot says, or -1 for none. It takes a
// pass over the region's trainers, so the game works it out while it
// builds the trainer's map (in the background) and keeps it on the
// trainer's object (RogueText_TrainerLineOfSet then takes it as it is).
s32 RogueText_TrainerSetIndex(const struct RogueRegion *r, u32 slot)
{
    u32 index;
    return TrainerSet(r, slot, &index) != NULL ? (s32)index : -1;
}

// A class trainer's line from the set RogueText_TrainerSetIndex gave
// (-1: none, the plain fallback).
const char *RogueText_TrainerLineOfSet(const struct RogueRegion *r, u32 slot, s32 index, u8 field)
{
    u8 cls = RogueGen_TrainerClass(r, slot);
    const struct RogueTrainerSet *pool;
    u32 n;
    if (field > RTRAINER_AFTER || cls >= ROGUE_CLASS_COUNT)
        return NULL;
    pool = TrainerIsGym(slot) ? sGymSets : sClassSets[cls].sets;
    n = TrainerIsGym(slot) ? LORE_COUNT_OF(sGymSets) : sClassSets[cls].count;
    if (index < 0 || (u32)index >= n)
        return sTrainerFallback[field];
    if (field == RTRAINER_AFTER && TrainerAtWayBelow(r, slot))
        return sTrainerWayBelow[cls == RCLASS_BLACK_BELT ? 1 : cls == RCLASS_RUIN_MANIAC ? 2 : 0];
    return pool[index].line[field];
}

// What a trainer's lines may name: their route or cave ({ROUTE}), their
// dungeon ({DUNGEON}), their own gym or the one ahead ({LEADER}), a wild
// POKeMON of theirs ({MON}) and their own lead ({OWN}, from the slot). c is
// set up at RogueText_TrainerPlace.
void RogueText_TrainerCtx(struct RogueTextCtx *c, u32 slot)
{
    const struct RogueRegion *r = c->r;
    u8 place = RogueGen_SlotPlace(r, slot), gym;
    s32 d = RogueGen_SlotDungeon(r, slot);
    c->slot = slot;
    if (TrainerIsGym(slot))
    {
        c->gym = (slot - ROGUE_SLOT_GYM_0) / ROGUE_GYM_TRAINERS;
        return;
    }
    // The species: one of the four commonest where they stand, trainers
    // side by side naming different ones.
    if (place != 0xFF)
    {
        c->route = place;
        c->area = r->places[place].area;
        c->mon = (slot - r->places[place].trainerBase + RogueRng_Hash(r->seed, 5300 + place)) % 4;
    }
    else if (d >= 0)
    {
        c->dungeon = d;
        c->area = r->dungeons[d].area;
        c->mon = (slot - r->dungeons[d].trainerBase + RogueRng_Hash(r->seed, 5350 + d)) % 4;
    }
    gym = RogueText_NextGym(r, c->place);
    if (gym < ROGUE_NUM_GYMS)
        c->gym = gym;
}

// The template a trainer says from their set (RogueText_TrainerSetIndex),
// checked against the context: one that cannot be filled gives way to a
// plain line.
const char *RogueText_TrainerTemplate(struct RogueTextCtx *c, u32 slot, s32 index, u8 field)
{
    const char *t = RogueText_TrainerLineOfSet(c->r, slot, index, field), *k;
    // {NAME} is the wanderer: the cast worked out once here, as {WANDERER}
    // would work it out at each mention (too slow for a line wanted now).
    for (k = t; k != NULL && *k; k++)
    {
        if (k[0] == '{' && k[1] == 'N' && k[2] == 'A' && k[3] == 'M')
        {
            struct RogueCast cast;
            RogueGen_Cast(c->r, &cast);
            c->name = RogueGen_WandererName(&cast);
            break;
        }
    }
    if (t != NULL && !RogueText_CanFill(t, c))
        t = sTrainerFallback[field];
    return t;
}

#ifdef ROGUE_HOST
// Every class trainer of the region: route and cave trainers, dungeon
// floors and gyms, each with their three lines.
static void TextDump_Trainers(struct TextDump *d)
{
    const struct RogueRegion *r = d->r;
    static const char *const fields[3] = { "intro", "defeat", "after" };
    u32 slot, f;
    for (slot = 0; slot < ROGUE_SLOT_LEADER_0; slot++)
    {
        struct RogueTextCtx c;
        struct RogueTextState st;
        char who[64], bank[40];
        const struct RogueTrainerSet *t;
        u32 index = 0;
        u8 cls;
        s32 dun = RogueGen_SlotDungeon(r, slot);
        if (!TrainerTalks(r, slot))
            continue;
        cls = RogueGen_TrainerClass(r, slot);
        if (cls >= ROGUE_CLASS_COUNT)
            continue;
        t = TrainerSet(r, slot, &index);
        RogueText_Init(&c, r, RogueText_TrainerPlace(r, slot), TrainerIsGym(slot) ? RSPK_GYM : RSPK_TRAINER);
        RogueText_TrainerCtx(&c, slot);
        snprintf(bank, sizeof(bank), "%s", t == NULL ? "sTrainerFallback" : TrainerIsGym(slot) ? "sGymSets" : gRogueGenClasses[cls].name);
        // A run that has come this far has won the gyms before it.
        if (d->state != NULL)
        {
            st = *d->state;
            if (st.badges < r->places[c.place].badges && !(st.flags & RTSTATE_RIVAL1))
                st.badges = r->places[c.place].badges;
        }
        for (f = 0; f < 3; f++)
        {
            const char *line = RogueText_TrainerTemplate(&c, slot, t != NULL ? (s32)index : -1, f);
            char out[1000];
            snprintf(who, sizeof(who), "%s slot%u%s%s %s", gRogueGenClasses[cls].name, slot,
                     dun >= 0 ? " floor" : "", TrainerIsGym(slot) ? " gym" : "", fields[f]);
            c.state = d->state != NULL ? &st : NULL;
            RogueText_Say(out, sizeof(out), line, &c);
            TextDump_Line(d, c.place, c.speaker, who, line == sTrainerFallback[f] ? "sTrainerFallback" : bank, index, out);
        }
    }
}
#endif
