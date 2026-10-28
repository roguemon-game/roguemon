// Gym leaders, the Elite Four, the rival, the villain team and the
// CHAMPION, who is the wanderer (the leaders domain). Included at the end
// of src/rogue_gen_text.h, so the host tools (gen_host --script,
// scripttest.py, leadertest.py) see every line; the game asks here
// (src/rogue.c) and converts what it gets.
//
// - Gym leaders and the Elite Four: two of each line per leader, picked per
//   region, tied to the gym's town (its name, its trade's people, its
//   rival town and what they argue over) and to the other leaders ({NAME}
//   in {OTHER}: another of the region's gym leaders and their town).
// - The gym guide, the TM handover, the last Elite Four member's hint.
// - The rival: four battles that grow (eager, cocky, shaken or proud,
//   respectful), naming the partner ({RIVAL_MON}) and where you met before
//   ({OTHER}); now and then, gently, the POK@MON the player has lost.
// - The wanderer: three or four meetings on the road (RogueCast), then the
//   CHAMPION's room, in the voice of their look.
// - TEAM AQUA and TEAM MAGMA: grunts, admins (MATT, SHELLY, TABITHA,
//   COURTNEY), the leaders, the blockade and the hideout town's people,
//   each team in its own voice, after the region's own legend.

#define LEADERS_SALT 41500

static const char *GiovanniGymLine(struct RogueTextCtx *c, u8 leader, u8 field, bool8 again);
static u8 GiovanniGym(const struct RogueRegion *r);
static const char *GiovanniGossip(const struct RogueRegion *r, u8 gym);

// ---------------------------------------------------------------------------
// Gym leaders and the Elite Four

// Every leader's lines, in gRogueGenLeaders order (RLEADER_*), two of each
// (field * 2 + which): intro and defeat as a gym leader, after the battle,
// the townsfolk's gossip, and the Elite Four's intro and after line. A
// region picks one of each (LeaderPick). Slots: {TOWN}, {STEM},
// {TRADE_FOLK}, {TRADE}, {LANDMARK} of the gym's town; {NAME} and {OTHER},
// another of the region's gym leaders and their town (LeaderCtx); the
// gossip is told by townsfolk in the gym's town and by the POKeMON
// SCHOOL's teacher elsewhere, so it names the town as {GYM_TOWN}.
static const char *const sLeaderLines[ROGUE_LEADER_COUNT][RLEADER_LINES * 2] =
{
    { // CRAG
        "I'm CRAG. I've climbed every cliff around {STEM}, and dug under most of them.|My ROCK POK@MON are older than the hills. Let's see if you crack!",
        "Hah! The {TRADE_FOLK} of {TOWN} said a challenger was coming. I've been waiting all morning.|My ROCK POK@MON don't budge. Can yours?",
        "Ha! Solid as bedrock, you are.",
        "Cracked! Clean through!",
        "Somewhere under {REGION} there's a cave nobody has mapped.|When I find it, you'll be the first I tell.",
        "{NAME} in {OTHER} says my GYM is just a pile of rocks.|Tell {NAME} it's a very good pile of rocks.",
        "Our LEADER CRAG spends more time in caves than in the GYM.|His ROCK POK@MON shrug off NORMAL and FLYING moves like pebbles.",
        "CRAG has dug a tunnel under half of {GYM_TOWN}. The other half is next, he says.|WATER and GRASS moves wear his ROCK POK@MON down.",
        "I'm CRAG. I've climbed every cliff in {REGION}. This summit, too.|My ROCK POK@MON are older than the mountains. Will you crack?",
        "So you climbed all the way up. I came the hard way, up the cliff face.|My ROCK POK@MON are ready. Are you?",
        "Solid. Go on, the next room is waiting. Mind the step.",
        "You're tougher than this mountain. Go on ahead.",
    },
    { // MARINA
        "...|Water doesn't care how brave you are. Neither do I.|Show me you can keep your head above water.",
        "...|The {TRADE_FOLK} of {TOWN} talk enough for all of us. I don't.|Let my WATER POK@MON speak.",
        "...You swim well.",
        "...The tide turned. Fine.",
        "Currents change. Teams change. Keep yours afloat.",
        "...{NAME} in {OTHER} talks too much. You'll see. Go.",
        "MARINA hardly says a word. Her WATER POK@MON do the talking.|GRASS and ELECTRIC moves are the way to go, if you ask me.",
        "MARINA said three words at the last {FESTIVAL}. Folks in {GYM_TOWN} still talk about it.|GRASS and ELECTRIC moves cut through her team.",
        "...|The LEAGUE is just another shore. Waves break here, too.|Show me you can keep your head above water.",
        "...|Every stream in {REGION} runs down from somewhere. Up here, it's me.|Swim.",
        "...Keep your team afloat. The next room is deeper.",
        "...Go on. Don't look back.",
    },
    { // BASIL
        "Hohoho. Welcome, young one.|I have tended this garden for sixty years. My GRASS POK@MON grew up with the flowers.|Let us see what you have grown!",
        "Hohoho. Welcome to {TOWN}, young one. Mind the seedlings by the door.|The {TRADE_FOLK} say I talk to my plants. I do. They listen.|Now, let us see what you have grown!",
        "Hoho! A fine harvest indeed.",
        "Hohoho! Pulled up by the roots!",
        "A team is like a garden. Water it every day, and never pull up the seedlings.",
        "{NAME} in {OTHER} once stepped on my best flower bed. I have forgiven {NAME}.|Grow well, young one.",
        "Old BASIL talks to his plants more than to people.|FIRE and FLYING moves singe his GRASS POK@MON, they say.",
        "BASIL planted the first garden in {GYM_TOWN}, before most of us were born.|They say FIRE and ICE moves wilt his GRASS POK@MON.",
        "Hohoho. Welcome, young one.|Even up here, I keep flowers by the window.|Let us see what you have grown!",
        "Hohoho. All of {REGION} is a garden, if you look at it right.|Show me what grew in you on the way up!",
        "A team is like a garden. Water it, and it will carry you to the top.",
        "Hohoho. Go on. The flowers past that door have thorns.",
    },
    { // CINDER
        "You feel that heat? That's my GYM's furnace, and my POK@MON.|I forge champions here. Most of them melt. Let's see if you're different!",
        "The {TRADE_FOLK} of {TOWN} bring me their tools to mend. I bring the fire.|Let's see if you hold your shape in the heat!",
        "Tempered steel! You held your shape.",
        "Hah! The forge has gone cold!",
        "The fire in you burns hot. Don't let it burn out before the summit.",
        "{NAME} in {OTHER} asked me to make a new GYM sign. If you see {NAME}, say it's nearly done.|It's not nearly done.",
        "CINDER used to be a blacksmith. His FIRE POK@MON are hotter than his forge.|WATER, ROCK and GROUND moves cool them off.",
        "CINDER mended every gate in {GYM_TOWN}. Now he mends challengers.|WATER and GROUND moves put his fire out, they say.",
        "You feel that heat? That's my FIRE POK@MON, stoked for you.|I forge champions here. Most of them melt. Will you?",
        "I've brought my forge up the mountain. Well, the fire in it.|Let's see if you're tempered for the top!",
        "Hot work. Go on. The next room may run hotter than mine.",
        "You held your shape. Keep it, all the way up.",
    },
    { // VOLTZ
        "Welcome to my lab! Er, GYM.|I've been tinkering with ELECTRIC POK@MON since before you were born.|Time for a field test. Hold still!",
        "Ah! A test subject! Er, challenger!|I wired half of {TOWN} for light. The {TRADE_FOLK} were very grateful.|Time for a field test. Hold still!",
        "Short circuit! Fascinating data!",
        "Bzzt! Results unclear! No, wait. You won.",
        "GROUND POK@MON ignore my sparks completely. Most annoying thing in the world.",
        "I'm building a machine to call {NAME} in {OTHER} from here. It hums. It doesn't call yet.",
        "VOLTZ blew out the town's power again this year.|A GROUND POK@MON would laugh off his ELECTRIC moves.",
        "VOLTZ wired the lights all over {GYM_TOWN}. They blink when he's losing.|GROUND POK@MON don't feel his ELECTRIC moves at all.",
        "Welcome to the LEAGUE! I brought my best ELECTRIC POK@MON.|Time for a field test. Hold still!",
        "Ah, the challenger! I rewired this whole room for you. Hear it hum?|Let's see how you conduct yourself!",
        "GROUND POK@MON still ignore my sparks. Most annoying thing in the world. Go on.",
        "Fascinating data! Go on, I'll be busy taking notes.",
    },
    { // MAKO
        "Hi-yah! I train from sunrise to sunset, every single day!|My FIGHTING POK@MON hit hard and never back down. Neither do I!",
        "Hi-yah! I ran twenty laps around {TOWN} this morning!|The {TRADE_FOLK} waved each lap. Twenty waves! Let's battle!",
        "Ouch! OK, OK, you win!",
        "Hi... yah... I'm winded!",
        "Back to training! A hundred laps around town before dinner!",
        "I race {NAME} from {OTHER} every spring. I've never won. Yet!",
        "MAKO jogs past my window each morning, rain or shine.|PSYCHIC and FLYING moves are what her FIGHTING POK@MON hate.",
        "MAKO jogs a lap of {GYM_TOWN} before the sun is up. Then another.|FLYING and PSYCHIC moves beat her FIGHTING POK@MON.",
        "Hi-yah! Welcome to the LEAGUE!|I ran all the way up here this morning. Just a warm-up!|My FIGHTING POK@MON never back down. Neither do I!",
        "Hi-yah! You climbed all the way up! So did I, at a run!|Let's see who's still standing!",
        "Back to training! A hundred laps of the summit before dinner!",
        "Ouch. Good form! Go on, the next room awaits!",
    },
    { // LUNA
        "I saw you coming, {PLAYER}. In a dream, three nights ago.|In my dream, you lost. Shall we find out?",
        "The {TRADE_FOLK} of {TOWN} come to me to read their dreams.|Last night I dreamed of you, {PLAYER}. It ended badly. For you.|Shall we see?",
        "That... was not in my dream.",
        "My dream was wrong. How rare.",
        "I dreamed of the summit last night. Someone was waiting there.|I couldn't see their face.",
        "I dreamed of {WANDERER} last night. Do you know the name?|I feel you will.",
        "LUNA knows what you'll say before you say it.|BUG, DARK and GHOST moves are said to rattle PSYCHIC POK@MON.",
        "LUNA dreamed the last {FESTIVAL} would have fine weather. It did.|BUG, DARK and GHOST moves rattle her PSYCHIC team.",
        "I saw you coming, {PLAYER}. In a dream, a week ago.|In my dream, you fell at this door. Shall we find out?",
        "I dream of everyone who reaches this room. You were taller in the dream.|Shall we see how it ends?",
        "I dreamed of the summit. Someone was waiting there. I couldn't see their face.",
        "Go. The one at the top has been dreaming of you too, I think.",
    },
    { // ACE
        "Hah! Welcome to the finest GYM under the sky!|My FLYING POK@MON rule the winds. Can you even reach us up here?",
        "Ladies and gentlemen of {TOWN}! A challenger!|...Nobody's watching? Never mind. My FLYING POK@MON perform for anyone!",
        "Grounded! What a performance!",
        "Bravo! Even I'd clap for that!",
        "That was a show! Come back when you're CHAMPION and we'll do an encore.",
        "{NAME} in {OTHER} says I'm all feathers and no fight.|Go prove {NAME} wrong for me!",
        "ACE loves an audience more than his POK@MON.|ELECTRIC, ICE and ROCK moves knock his FLYING POK@MON out of the sky.",
        "ACE flies loops over {GYM_TOWN} at sunset. Nobody asked him to.|ELECTRIC and ROCK moves ground his FLYING POK@MON.",
        "Hah! Welcome to the finest stage under the sky!|My FLYING POK@MON rule the winds. Can you even reach us up here?",
        "Hah! The whole of {REGION} below us, and you, center stage!|Let's give them a show!",
        "What a show! Go on, the next act is waiting.",
        "Take a bow! Then go through that door.",
    },
    { // ROXANNE
        "Hello, I am ROXANNE, the {STEM} GYM LEADER.|I became a LEADER to put what I learn at school to use in battle.|Would you kindly show me how you battle?",
        "Hello, I am ROXANNE. I teach the children of {TOWN} after school.|Today, you are my student. Show me what you have studied!",
        "So... I lost... It seems that I still have much more to learn...",
        "I see. I will need to review my notes.",
        "There is always more to learn about POK@MON. Keep at it!",
        "{NAME} in {OTHER} and I trade notes by letter. I will write that you are strong.",
        "ROXANNE studies every battle she has ever fought. She keeps the notes in binders.|WATER and GRASS moves are what her ROCK POK@MON fear.",
        "ROXANNE wrote a book about the history of {GYM_TOWN}. Few of us have finished it.|WATER and GRASS moves crack her ROCK team, they say.",
        "Hello, I am ROXANNE.|I taught at a school once. Now the LEAGUE is my class.|Show me everything you have studied!",
        "Hello, I am ROXANNE. I have read about every TRAINER who reached this room.|Now I will see one with my own eyes!",
        "There is always more to learn. Go on, the next lesson is through there.",
        "I will add you to my notes. Go on ahead.",
    },
    { // BRAWLY
        "I'm BRAWLY! I've ridden big waves all over the world. Now I ride big battles!|Let's see if you can survive my FIGHTING POK@MON!",
        "Yo! I'm BRAWLY! The {TRADE_FOLK} of {TOWN} train with me at sunrise. Tough crowd!|Let's see if you're tougher!",
        "Whoa, wow! You made a much bigger splash than I expected!",
        "Wipeout! Totally wiped out!",
        "Training is like surfing. Keep paddling, and the big wave always comes.",
        "{NAME} in {OTHER} is a big wave too. Paddle hard!",
        "BRAWLY trains outside before anyone else is up.|FLYING, PSYCHIC and FAIRY moves knock his team flat.",
        "BRAWLY says {GYM_TOWN} has the best sunrises in {REGION}. He's up for every one.|FLYING and PSYCHIC moves beat his FIGHTING POK@MON.",
        "I'm BRAWLY! The ELITE FOUR is the biggest wave I've ridden!|Let's see if you can survive my FIGHTING POK@MON!",
        "Yo! You made it! I've been waiting on this wave all day!|Let's ride it together!",
        "Training is like surfing. Keep paddling. The next wave is bigger.",
        "Gnarly. Go on. Next room!",
    },
    { // WATTSON
        "Wahahahah! A new challenger! Now, that is amusing!|I'm WATTSON! Let me charge you up with my ELECTRIC POK@MON!",
        "Wahahahah! I've lit up {TOWN} since before you were born! The {TRADE_FOLK} still pay me in pie!|Let me charge you up!",
        "Wahahahah! Fine, I lost! You ended up giving me a thrill!",
        "Wahahahah! I'm shocked! Shocked!",
        "Wahahahah! I'm getting on in years, but a battle like that makes me feel young again!",
        "Wahahahah! Off you go to {NAME} in {OTHER}! Give {NAME} a jolt from me!",
        "WATTSON laughs at everything, even losing.|GROUND POK@MON don't feel his ELECTRIC moves at all.",
        "WATTSON's laugh carries across all of {GYM_TOWN}. Great for finding him.|A GROUND POK@MON shrugs off his ELECTRIC moves.",
        "Wahahahah! A challenger made it all the way up here!|I'm WATTSON of the ELITE FOUR! Let me charge you up!",
        "Wahahahah! I took the stairs! At my age! Wahahahah!|Now, let me charge you up!",
        "Wahahahah! I feel young again! Go on through!",
        "Wahahahah! What a thrill! Next room, next thrill!",
    },
    { // FLANNERY
        "Welcome! I'm FLANNERY. I'm the LEADER here, so... um...|I'm going to show you just how hot my FIRE POK@MON can burn!",
        "Welcome to {TOWN}! I'm FLANNERY! I only just became LEADER, but the {TRADE_FOLK} cheer for me anyway!|So let's make it a good one!",
        "Oh... I guess I was trying too hard...",
        "Oh... I got burned. By my own battle.",
        "I'm still learning how to be a good LEADER. Thank you for the battle, really!",
        "{NAME} in {OTHER} gave me advice when I started.|Now I'm passing it on: don't give up!",
        "FLANNERY is new at being a LEADER, but her FIRE POK@MON are no joke.|WATER, GROUND and ROCK moves put them out.",
        "FLANNERY practices her speeches out loud all over {GYM_TOWN}. We pretend we can't hear.|WATER and GROUND moves put out her FIRE POK@MON.",
        "Welcome! I'm FLANNERY. I'm ELITE FOUR now, so... um...|I'm going to show you just how hot my FIRE POK@MON can burn!",
        "I'm FLANNERY! I made it into the ELITE FOUR! I still can't believe it!|So I can't lose now, right? Let's go!",
        "I'm still new to the ELITE FOUR. Thank you, really!",
        "You're so strong! Go on, go on!",
    },
    { // NORMAN
        "I'm NORMAN. I train hard, and I expect the same of you.|I'll give you everything I've got. No holding back!",
        "I'm NORMAN, LEADER of {TOWN}'s GYM. The {TRADE_FOLK} here work hard. So do I.|Show me what your hard work looks like!",
        "I... I can't... I can't believe it. I lost.",
        "Hm. You've earned this. Fair and square.",
        "The {TRADE_FOLK} of {TOWN} will hear about this by supper. Stay humble.|You have the makings of a great TRAINER.",
        "{NAME} in {OTHER}? Train well before you go. I'll know if you didn't.",
        "NORMAN is a strict LEADER with strong NORMAL POK@MON.|FIGHTING moves are the only kind that really hurt them.",
        "NORMAN walks the streets of {GYM_TOWN} at six every morning. You could set a clock by him.|FIGHTING moves are what his NORMAL POK@MON fear.",
        "I'm NORMAN of the ELITE FOUR.|I train hard, and I expect the same of you.|I'll give you everything I've got. No holding back!",
        "I'm NORMAN. I've watched many TRAINERS climb to this room. Few looked as ready as you.|Prove it.",
        "You have the makings of a great TRAINER. Stay humble. Go on.",
        "Hard work. I can see it. Go on ahead.",
    },
    { // WINONA
        "I am WINONA, LEADER of {TOWN}'s GYM.|I have become one with the skies and my FLYING POK@MON.|Witness our elegant battle style!",
        "I am WINONA. From the air, {TOWN} looks like a quilt. The {TRADE_FOLK} are the stitches.|Witness our elegant battle style!",
        "Never before have I seen a TRAINER command POK@MON with such grace...",
        "You've brought us down to earth. Gracefully.",
        "The sky is wide. I hope you keep flying higher.",
        "When you reach {OTHER}, give my regards to {NAME}. We fly together some mornings.",
        "WINONA flies with her POK@MON. Says she feels the wind like they do.|ELECTRIC, ICE and ROCK moves bring her FLYING POK@MON down.",
        "WINONA lands on the roofs of {GYM_TOWN} when she's thinking. Gave us all a fright the first time.|ELECTRIC and ROCK moves bring her team down.",
        "I am WINONA of the ELITE FOUR.|Up here, the sky feels close enough to touch.|Witness our elegant battle style!",
        "I am WINONA. I can see all of {REGION} from this window. I can see the road you took.|Witness our elegant battle style!",
        "The sky is wide. I hope you keep flying higher.",
        "You fly well. Go on. Higher still.",
    },
    { // TATE&LIZA
        "TATE: Hehehe... Were you surprised?|LIZA: Fufufu... There are two of us!|BOTH: Can you beat us two at once?",
        "TATE: Hehehe... Welcome to {TOWN}!|LIZA: Fufufu... We knew you would come today!|BOTH: Two against one? That's fair!",
        "TATE: What? Our combination...|LIZA: ...was beaten!",
        "LIZA: We didn't see...|TATE: ...that coming!",
        "TATE: You and your POK@MON...|LIZA: ...think as one, too!",
        "TATE: {NAME} in {OTHER}...|LIZA: ...can't read minds. Hehehe!",
        "The LEADERS here are twins, TATE and LIZA. They battle two at a time!|BUG, DARK and GHOST moves rattle their PSYCHIC POK@MON.",
        "TATE and LIZA finish each other's sentences. All of {GYM_TOWN} has given up talking to just one.|BUG and DARK moves rattle their team.",
        "TATE: Hehehe... You came all this way!|LIZA: Fufufu... And we saw you coming!|BOTH: Can you beat the ELITE FOUR's twins?",
        "TATE: Hehehe... We knew you'd come...|LIZA: Fufufu... Before you did!|BOTH: Two at once! Ready?",
        "TATE: You and your POK@MON...|LIZA: ...think as one, too!",
        "LIZA: Go on...|TATE: ...through there!",
    },
    { // JUAN
        "Ah, this GYM had returned to its usual state of serenity...|But you, young typhoon, have come to stir it up. Let us dance!",
        "Ah, welcome to {TOWN}. The {TRADE_FOLK} here have a rhythm to their work. Did you hear it?|Let us dance to it!",
        "Ahahaha, excellent! Very well, you are the winner.",
        "Bravo! You stepped on my toes, beautifully.",
        "Your battle style was as elegant as a waltz. Bravo!",
        "When you meet {NAME} in {OTHER}, do take the lead. {NAME} never will.",
        "JUAN battles like he's dancing. His WATER POK@MON are beautiful.|GRASS and ELECTRIC moves cut through them.",
        "JUAN taught half of {GYM_TOWN} to dance. The other half hides when he passes.|GRASS and ELECTRIC moves cut through his team.",
        "Ah, this hall had returned to its usual state of serenity...|But you, young typhoon, have come to stir it up. Let us dance!",
        "Ah. You have danced all the way up the mountain. I can tell from the way you walk.|One more dance, then!",
        "Your battle style was as elegant as a waltz. Bravo!",
        "Bravo. The next room plays a slower song. Go on.",
    },
    { // SIDNEY
        "Welcome, challenger! I'm SIDNEY.|I like that look you're giving me. Let's enjoy a real battle!",
        "So you're the one the {TRADE_FOLK} of {TOWN} keep talking about. I'm SIDNEY.|Let's see if you live up to the gossip!",
        "Well, how do you like that? I lost! Eh, it was fun.",
        "Hah! Dirty trick, winning like that.",
        "You've got what it takes to go far. Now go on!",
        "Tell {NAME} in {OTHER} I said hi. {NAME} hates it when I say hi.",
        "SIDNEY likes a dirty fight. His DARK POK@MON play mean.|FIGHTING, BUG and FAIRY moves are what they hate.",
        "SIDNEY sits on the GYM steps in {GYM_TOWN} and heckles TRAINERS. Friendly heckling.|FIGHTING and BUG moves beat his DARK POK@MON.",
        "Welcome, challenger! I'm SIDNEY of the ELITE FOUR.|I like that look you're giving me. Let's enjoy a real battle!",
        "Heh. Made it this far, huh? I'm SIDNEY.|No hard feelings when I win. That's how it goes up here!",
        "You've got what it takes to go far. Now go on!",
        "Fun! Go on. The next one's not as nice as me.",
    },
    { // PHOEBE
        "Ahahaha! I'm PHOEBE.|I trained on a mountain where the spirits of POK@MON rest.|You'll see what a bond with GHOST POK@MON can do!",
        "Ahahaha! I'm PHOEBE! The old houses of {TOWN} are full of friendly spirits. I visit them all.|Let's battle! They're watching!",
        "Oh, darn. I've gone and lost.",
        "Ahaha... The spirits say you won. Rude!",
        "There's a definite bond between you and your POK@MON. I can see it.",
        "The spirits say {NAME} in {OTHER} is next for you. The spirits are usually right!",
        "PHOEBE grew up among old graves. Her GHOST POK@MON give me the chills.|DARK and GHOST moves are what get through to them.",
        "PHOEBE swears the old houses of {GYM_TOWN} are haunted. Nobody's checked.|DARK and GHOST moves get through to her team.",
        "Ahahaha! I'm PHOEBE.|I trained on a mountain where the spirits of POK@MON rest.|You'll see what a bond with GHOST POK@MON can do!",
        "Ahahaha! The spirits told me you'd come!|They also said you'd lose. Let's see who's right!",
        "There's a definite bond between you and your POK@MON. I can see it.",
        "Ahaha! Go on. The spirits like you.",
    },
    { // GLACIA
        "Welcome. My name is GLACIA.|I have come all this way to find a TRAINER worth my ICE POK@MON.|Please don't disappoint me.",
        "Welcome to {TOWN}. I am GLACIA. I find it far too warm here.|My ICE POK@MON and I will cool things down.",
        "You and your POK@MON... How hot your spirits burn!",
        "Melted. How unexpected.",
        "That fire in you is real. Go on. Melt the rest of them.",
        "{NAME} in {OTHER} keeps a warmer GYM than mine. You'll like it more.",
        "GLACIA's ICE POK@MON froze the GYM's doorstep solid last winter.|FIRE and FIGHTING moves break the ice.",
        "GLACIA keeps her GYM in {GYM_TOWN} so cold, people store food in it.|FIRE, ROCK and STEEL moves melt her ICE POK@MON.",
        "Welcome. My name is GLACIA.|I have come all this way to find a TRAINER worth my ICE POK@MON.|Please don't disappoint me.",
        "Ah. A challenger who climbed through the cold. I am GLACIA.|Let us see if you are warm enough.",
        "That fire in you is real. Go on. Melt the rest of them.",
        "Go on. Keep that fire burning.",
    },
    { // DRAKE
        "I am DRAKE.|Do you know what it takes to raise DRAGON POK@MON? Virtue!|Let me see if you have it!",
        "I am DRAKE. I sailed far before I settled in {TOWN}. The {TRADE_FOLK} here are honest. I like that.|Let me see if you are honest in battle!",
        "Superb, it should be said.",
        "Virtue. You have it.",
        "You deserve every credit for coming this far. Go on.",
        "{NAME} in {OTHER} is soft on challengers. I am not. Remember that.",
        "DRAKE was a sailor once. Now he commands DRAGON POK@MON.|ICE, DRAGON and FAIRY moves are the only things they fear.",
        "DRAKE stands at the edge of {GYM_TOWN} at dawn and stares at nothing. A sailor's habit.|ICE and FAIRY moves are what his dragons fear.",
        "I am DRAKE.|Do you know what it takes to raise DRAGON POK@MON? Virtue!|Let me see if you have it!",
        "I am DRAKE. I have sailed every sea there is. None was as rough as this room.|Let me see if you have virtue!",
        "You deserve every credit for coming this far. Go on.",
        "Virtue. Carry it through that door.",
    },
    { // BROCK
        "So, you're here. I'm BROCK, {STEM}'s GYM LEADER.|My rock-hard willpower shows even in my POK@MON. All ROCK type!|Fine, then! Show me your best!",
        "I'm BROCK. When the {TRADE_FOLK} of {TOWN} work late, I cook for them. And for my nine brothers and sisters.|But I always have time for a challenger. Show me your best!",
        "I took you for granted, and so I lost.",
        "Your willpower is harder than rock!",
        "All kinds of TRAINERS come through {TOWN}. You're one of the tough ones.|Someday I want to be a great POK@MON BREEDER, too.",
        "{NAME} in {OTHER} won't go easy. I'd pack some extra POTIONS.",
        "BROCK has rock-hard resolve. He feeds nine little siblings!|WATER and GRASS moves crack his ROCK POK@MON, they say.",
        "BROCK cooks for half of {GYM_TOWN} when folks work late.|WATER and GRASS moves crack his ROCK POK@MON.",
        "I'm BROCK. I climbed all the way to the ELITE FOUR.|My ROCK POK@MON never cracked on the way up. Show me your best!",
        "I'm BROCK. I packed lunch for the climb. Want some? After the battle.|Show me your best!",
        "Go on ahead. Rock or not, the next room won't go easy.",
        "Your willpower's harder than rock. Go on.",
    },
    { // MISTY
        "Hi, you're a new face!|TRAINERS who want to turn pro have to have a policy.|Mine is an all-out offensive with WATER POK@MON!",
        "Hi! I'm MISTY, the tomboyish mermaid of {TOWN}!|The {TRADE_FOLK} here call me that. I like it. All-out offensive, go!",
        "Wow! You're too much, all right!",
        "Hmph! You got lucky! ...OK, you're good.",
        "Someday I'll be the greatest WATER POK@MON TRAINER ever!|Next time, I won't go easy!",
        "{NAME} in {OTHER} next? Ha! Make {NAME} sweat for me!",
        "MISTY calls herself the tomboyish mermaid. Ha!|GRASS and ELECTRIC moves cut her WATER POK@MON down.",
        "MISTY says she'll be the best WATER TRAINER in {REGION}. She says it daily, all over {GYM_TOWN}.|GRASS and ELECTRIC moves beat her team.",
        "I'm MISTY, the ELITE FOUR's tomboyish mermaid!|My policy? An all-out offensive with WATER POK@MON! Keep your head up!",
        "Hi again, new face! Well, you're not new anymore, are you?|All-out offensive! Ready?",
        "You're too much, all right! Go on, the next room awaits.",
        "Next time I won't go easy! Go on!",
    },
    { // LT. SURGE
        "Hey, kid! What do you think you're doing here?|ELECTRIC POK@MON got me through the war, kid! They'll zap you stiff!",
        "Hey, kid! The {TRADE_FOLK} of {TOWN} salute me in the street! You will too, after this!|ELECTRIC POK@MON, attack!",
        "Whoa! You're the real deal, kid!",
        "At ease, kid. You won.",
        "A little word of advice, kid! ELECTRICITY is sure powerful!|But it's useless against GROUND POK@MON!",
        "Next stop, {NAME} in {OTHER}? Move out, kid! Double time!",
        "They say LT. SURGE was a soldier. He calls everybody kid!|A GROUND POK@MON shrugs off his ELECTRIC moves entirely.",
        "LT. SURGE marches around {GYM_TOWN} counting out loud.|A GROUND POK@MON shrugs off his ELECTRIC moves.",
        "Hey, kid! LT. SURGE of the ELITE FOUR, on duty!|My ELECTRIC POK@MON never lose. You won't last a minute!",
        "Kid! You made it up the mountain! Good discipline!|Now let's see your battle drill!",
        "You've got guts, kid. Move out! Next room!",
        "Dismissed, kid! Next room, double time!",
    },
    { // ERIKA
        "Hello. Lovely weather, isn't it? ...Oh, dear. I must have dozed off.|My name is ERIKA. I teach the art of flower arranging.|A challenge? Very well. I shall not lose.",
        "Hello. Welcome to {TOWN}. The {TRADE_FOLK} here bring me flowers. I make them perfume.|...Oh, a challenge? Very well. I shall not lose.",
        "Oh! I concede defeat. You are remarkably strong.",
        "Oh, my. I'm quite awake now.",
        "Do come again. The flowers will be lovely in a week.|...If I'm dozing, do wake me gently.",
        "{NAME} in {OTHER} never stops to smell the flowers. How sad.",
        "ERIKA naps in her GYM's garden most afternoons.|FIRE and FLYING moves wilt her GRASS POK@MON, they say.",
        "ERIKA arranges the flowers all over {GYM_TOWN}. She knows every bloom by name.|FIRE and FLYING moves wilt her GRASS POK@MON.",
        "Oh... Welcome. I'm ERIKA. Forgive me, I was dozing.|The ELITE FOUR's halls are so quiet... like a greenhouse.|Shall we?",
        "...Oh! A challenger. I'm ERIKA. I brought flowers up the mountain. They like the air.|Shall we?",
        "You are remarkably strong. The next room awaits you.",
        "Do go on. I think I'll have a little nap.",
    },
    { // KOGA
        "Fwahahaha! A child dares to challenge me?|I shall show you true terror as a ninja master!|Despair at the creeping horror of POISON POK@MON!",
        "Fwahahaha! You never saw me in {TOWN}. But I saw you.|The {TRADE_FOLK} never see me either. Despair!",
        "Humph! You have proven your worth!",
        "Fwahaha... I vanish in defeat!",
        "TOXIC works slowly, like a ninja. It worsens every turn.|Use that knowledge well.",
        "{NAME} in {OTHER} has never once seen me. I have watched every battle in that GYM. Fwahaha!",
        "KOGA is a ninja. Some days you can't even see him in his GYM!|GROUND and PSYCHIC moves cut through his POISON POK@MON.",
        "KOGA could be anywhere in {GYM_TOWN} right now. Even behind you.|GROUND and PSYCHIC moves cut through his team.",
        "Fwahahaha! I am KOGA of the ELITE FOUR!|I live in the shadows, a master of the ninja arts.|My style will confound and destroy you!",
        "Fwahahaha! I followed you up the mountain. You never noticed.|Now face me!",
        "Humph! You have proven worthy. Go on to the next room.",
        "Go. I will be watching. You won't see me.",
    },
    { // SABRINA
        "I had a vision of your arrival.|I have had psychic powers since a spoon I tossed... bent.|I dislike battling. But if you wish, behold my powers!",
        "I foresaw you would come to {TOWN} today. The {TRADE_FOLK} doubted me.|They no longer doubt me. Behold my powers!",
        "This loss shocks me! But a loss is a loss.",
        "I did not foresee this. Interesting.",
        "Everyone has psychic power. People just don't realize it.",
        "I foresee you will meet {NAME} in {OTHER}. I foresee it will be loud.",
        "SABRINA bent spoons with her mind as a little girl.|BUG, GHOST and DARK moves are said to shake PSYCHIC POK@MON.",
        "SABRINA knew the date of the {FESTIVAL} before anyone planned it.|BUG, GHOST and DARK moves shake her PSYCHIC POK@MON.",
        "I foresaw you would reach this room. I am SABRINA.|My powers have only grown. Show me a future I did not foresee.",
        "I am SABRINA. I have seen you climb this mountain a hundred times in my mind.|Today, let us see how it really ends.",
        "A loss is a loss. Go. Your future lies ahead.",
        "I see someone waiting for you. Go.",
    },
    { // BLAINE
        "Hah! I am BLAINE! The hotheaded quiz master here!|My fiery POK@MON will incinerate all challengers!|Hah! You'd better have BURN HEAL!",
        "Hah! Quiz! Which town has the hottest GYM in {REGION}? {TOWN}! Hah!|You'd better have BURN HEAL!",
        "I have burned down to nothing! Not even ashes remain!",
        "Hah! Wrong answer! ...Mine, I mean.",
        "Quiz time! Which type douses FIRE POK@MON best?|WATER! Hah! Of course you knew that.",
        "Quiz! Who runs the GYM in {OTHER}? {NAME}! Hah! Too easy!",
        "BLAINE asks everyone quizzes. Get one wrong and he laughs!|WATER, GROUND and ROCK moves put out his FIRE POK@MON.",
        "BLAINE quizzes everyone in {GYM_TOWN}. The mail carrier hides from him.|WATER and GROUND moves put out his FIRE POK@MON.",
        "Hah! I am BLAINE of the ELITE FOUR!|Quiz! What hotheaded old man is about to scorch you?|Me! Hah! You'd better have BURN HEAL!",
        "Hah! Quiz! How many TRAINERS reach this room each year? Not many! Hah!|Now burn!",
        "Burned down to nothing! Go on, then. Hah!",
        "Correct answer! Go on, the next room awaits!",
    },
    { // GIOVANNI
        "Welcome. I am GIOVANNI.|This GYM is merely one of my many... business interests.|You shall face GIOVANNI, the greatest TRAINER!",
        "Ah. A challenger. I am GIOVANNI. I own a share of most of the {TRADE} in {TOWN}.|Business is slow today. Entertain me.",
        "Ha! That was a truly intense fight. I have lost.",
        "Hm. A poor investment on my part.",
        "Having lost, I cannot face my... associates.|I shall go away for a while. Perhaps we'll meet again.",
        "{NAME} in {OTHER} owes me money. Don't mention that.",
        "GIOVANNI is never in his GYM. Men in dark suits visit him.|WATER, GRASS and ICE moves sweep his GROUND POK@MON away.",
        "GIOVANNI bought half the houses in {GYM_TOWN} last year. Nobody saw him sign anything.|WATER and GRASS moves sweep his team away.",
        "So. You've reached the LEAGUE. I am GIOVANNI.|My business brought me to the ELITE FOUR. Power is business.|Let me show you why I am the greatest!",
        "I am GIOVANNI. I have invested a great deal in this room.|Let us see if you are worth it.",
        "Hmph. My associates will hear nothing of this. Go on.",
        "A good return. Go.",
    },
    { // LORELEI
        "Welcome. I am LORELEI.|I left my cold island home to run this GYM.|No one can best me when it comes to icy POK@MON. Are you ready?",
        "Welcome to {TOWN}. I am LORELEI. The {TRADE_FOLK} here think I'm cold. I'm not.|I just like my POK@MON icy. Are you ready?",
        "...Things shouldn't be this way!",
        "...Thawed. Well done.",
        "You're better than I thought.|Please, take good care of your POK@MON. Be kind to them.",
        "I knitted a scarf for {NAME} in {OTHER}. It has never been worn. If you pass that way, say I noticed.",
        "LORELEI seems icy, but she collects POK@MON dolls!|FIRE, FIGHTING, ROCK and STEEL moves shatter her ICE POK@MON.",
        "LORELEI keeps a doll from every town she's lived in. The one from {GYM_TOWN} is her favorite.|FIRE and FIGHTING moves shatter her team.",
        "Welcome to the POK@MON LEAGUE. I am LORELEI of the ELITE FOUR.|No one can best me when it comes to icy POK@MON.|Hahaha! Are you ready?",
        "I am LORELEI. The air up here is cold. I like it.|Frozen solid, your POK@MON will be at my mercy. Ready?",
        "You're better than I thought. Go on ahead.",
        "You only got a taste of the LEAGUE's power. Go.",
    },
    { // BRUNO
        "I am BRUNO!|This GYM is where I train with my FIGHTING POK@MON, all day.|We will grind you down with our superior power! Hoo hah!",
        "I am BRUNO! I help the {TRADE_FOLK} of {TOWN} with the heavy lifting. It is good training!|Now, we battle! Hoo hah!",
        "Why? How could I lose?",
        "Hoo... hah... You are strong!",
        "I'll go train in the mountains again. Hoo hah!",
        "{NAME} in {OTHER} is a strong TRAINER. We trained together once. My legs still ache. Hoo hah!",
        "BRUNO trains until sundown, shouting HOO HAH! all day.|PSYCHIC and FLYING moves beat his FIGHTING POK@MON.",
        "BRUNO's HOO HAH! wakes all of {GYM_TOWN} at dawn. Nobody here needs a clock.|PSYCHIC and FLYING moves beat his team.",
        "I am BRUNO of the ELITE FOUR!|Through rigorous training, we grow stronger without limit.|{PLAYER}! We will grind you down! Hoo hah!",
        "I am BRUNO! I ran up the mountain this morning. With my POK@MON on my back!|Now, we battle! Hoo hah!",
        "My job is done. Go face your next challenge.",
        "Hoo hah! Go! Train! Win!",
    },
    { // AGATHA
        "I am AGATHA. Don't let my age fool you, child.|I hear PROF. BIRCH has taken an interest in you. Bah! He'd rather run around in the grass.|I'll show you how a real TRAINER battles!",
        "I am AGATHA. I've lived in {TOWN} longer than any of the {TRADE_FOLK} here.|POK@MON are for battling, child! Show me!",
        "Oh, my! You're something special, child!",
        "Hmph! Not bad. Not bad at all.",
        "You win! I have nothing else to say. Run along now, child!",
        "{NAME} in {OTHER} coddles every POK@MON in that GYM. Don't you pick up that habit!",
        "Old AGATHA scolds anyone who coddles their POK@MON.|Only DARK and GHOST moves get through to her GHOST POK@MON.",
        "AGATHA has lived in {GYM_TOWN} longer than anyone. She tells young TRAINERS they're doing it wrong.|DARK and GHOST moves get through to her.",
        "I am AGATHA of the ELITE FOUR.|I hear PROF. BIRCH has taken an interest in you, child. Bah!|{PLAYER}! I'll show you how a real TRAINER battles!",
        "I am AGATHA. I've watched TRAINERS climb this mountain for fifty years.|Most of them coddled their POK@MON. Let's see about you!",
        "I see what the professor sees in you now. Run along, child!",
        "Hmph. Go on. Don't keep the next one waiting.",
    },
    { // LANCE
        "Ah! I heard about you, {PLAYER}!|I'm LANCE, the dragon TRAINER. I lead {TOWN}'s GYM for now.|Dragons are mythical POK@MON. Their powers are superior! Ready?",
        "I'm LANCE. The {TRADE_FOLK} of {TOWN} still jump when my dragons land in the square.|I tell them dragons are gentle. Shall we see?",
        "I still can't believe my dragons lost to you, {PLAYER}!",
        "Dragons... beaten. Remarkable.",
        "You and your POK@MON fought as one. Dragons respect that.",
        "{NAME} in {OTHER} once asked to ride one of my dragons. I said no. I still feel bad.",
        "LANCE wears a cape everywhere. Even to the POK@MON MART!|ICE, DRAGON and FAIRY moves are all his dragons fear.",
        "LANCE lands his dragon in the middle of {GYM_TOWN} every evening. It scares everyone silly.|ICE and DRAGON moves are what his dragons fear.",
        "Ah! I heard about you, {PLAYER}! You can call me LANCE, the dragon TRAINER.|Dragons are hard to catch and raise... But nearly indestructible!|Are you ready to lose?",
        "I'm LANCE. I've been waiting at this door for a long time.|My dragons are restless. Let's not keep them waiting.",
        "I hate to admit it, but you have the makings of a master. Go on.",
        "Go on. Don't keep the next room waiting.",
    },
};

// How a leader's {NAME} line (their second after line) names another:
// NEXT says the other is the very next gym ("Next stop, X?"), so it names
// the first gym along the road not yet won; AHEAD speaks of meeting them
// later ("Tell X I said hi"), so any gym not won; ANY names any of the
// others. The leaders by gRogueGenLeaders index.
enum { XREF_ANY, XREF_AHEAD, XREF_NEXT };
static const u8 sLeaderXref[ROGUE_LEADER_COUNT] =
{
    [0] = XREF_AHEAD,  /* CRAG */     [1] = XREF_AHEAD,  /* MARINA */  [3] = XREF_AHEAD,  /* CINDER */
    [7] = XREF_AHEAD,  /* ACE */      [9] = XREF_NEXT,   /* BRAWLY */  [10] = XREF_NEXT,  /* WATTSON */
    [12] = XREF_NEXT,  /* NORMAN */   [13] = XREF_AHEAD, /* WINONA */  [15] = XREF_AHEAD, /* JUAN */
    [16] = XREF_AHEAD, /* SIDNEY */   [17] = XREF_NEXT,  /* PHOEBE */  [18] = XREF_AHEAD, /* GLACIA */
    [20] = XREF_AHEAD, /* BROCK */    [21] = XREF_NEXT,  /* MISTY */   [22] = XREF_NEXT,  /* LT. SURGE */
    [25] = XREF_AHEAD, /* SABRINA */  [28] = XREF_AHEAD, /* LORELEI */
};

// What a leader's {NAME} line names (LeaderCtx's answer): nobody (the line
// is not said), another leader, or GIOVANNI where he is TEAM ROCKET's
// BOSS too, of whom the leaders speak uneasily (sGiovanniUneasy).
enum { XNAME_NONE, XNAME_LEADER, XNAME_GIOVANNI };

// GIOVANNI's twist (his gym, TEAM ROCKET's BOSS) as far as the player
// knows it: 0 nothing yet, 1 TEAM ROCKET's hideout cleared, 2 the BOSS
// beaten (he has said who he is).
static u8 GiovanniKnown(const struct RogueTextCtx *c)
{
    u8 t = RogueGen_TeamWithId(c->r, ROGUE_VILLAIN_ROCKET);
    if (t == 0xFF || c->state == NULL)
        return 0;
    if ((c->state->teamsBeaten >> t) & 1)
        return 2;
    return (c->state->teamsCleared >> t) & 1;
}

// Another of the region's gym leaders and their town, for {NAME} and
// {OTHER} in a gym leader's lines, by how their line names them
// (sLeaderXref). The won gyms are the state's; with none (the dump), the
// gyms up to this one, as a player who takes them in order has them. The
// others a line may name are spread over the region (each leader counts
// on from their own gym by the region's step), so one leader is not named
// by everyone. GIOVANNI where he is the BOSS too is never named as next or
// ahead: the leaders whose turn names him speak of him uneasily. any:
// name any other (the dump's checks). An Elite Four member names one of
// the eight.
static u8 LeaderCtx(struct RogueTextCtx *c, u8 leader, bool8 any)
{
    const struct RogueRegion *r = c->r;
    u8 gyms[ROGUE_NUM_GYMS], e4[ROGUE_NUM_E4], o, won, gio, kind;
    u32 h = RogueRng_Hash(r->seed, LEADERS_SALT + c->gym), step, k;
    c->name = NULL;
    c->other = 0xFF;
    if (c->gym >= ROGUE_NUM_GYMS + ROGUE_NUM_E4)
        return XNAME_NONE;
    RogueGen_RegionLeaders(r, gyms, e4);
    if (c->gym >= ROGUE_NUM_GYMS)
    {
        o = h % ROGUE_NUM_GYMS;
        c->name = gRogueGenLeaders[gyms[o] % ROGUE_LEADER_COUNT].name;
        c->other = r->gymTown[o];
        return XNAME_LEADER;
    }
    if (any)
        won = 1 << c->gym;
    else if (c->state != NULL)
        won = c->state->wonGyms | (1 << c->gym);
    else
        won = (2 << c->gym) - 1;
    gio = GiovanniGym(r);
    kind = any ? XREF_ANY : sLeaderXref[leader % ROGUE_LEADER_COUNT];
    o = 0xFF;
    if (kind == XREF_NEXT)
    {
        for (k = 0; k < ROGUE_NUM_GYMS && o == 0xFF; k++)
            if (!((won >> k) & 1))
                o = k;
        if (o != 0xFF && o == gio && GiovanniKnown(c) == 0)
            o = 0xFF;
    }
    else
    {
        // Counting on from this gym by the region's step: with every gym
        // free, each leader names a different one.
        step = 1 + RogueRng_Hash(r->seed, LEADERS_SALT + 9) % (ROGUE_NUM_GYMS - 1);
        for (k = 0; k < ROGUE_NUM_GYMS - 1 && o == 0xFF; k++)
        {
            u8 g = (c->gym + step + k) % ROGUE_NUM_GYMS;
            if (g == c->gym || (kind == XREF_AHEAD && ((won >> g) & 1)))
                continue;
            if (g == gio && !any && GiovanniKnown(c) == 0 && kind == XREF_AHEAD)
                continue;
            o = g;
        }
    }
    if (o == 0xFF)
        return XNAME_NONE;
    c->name = gRogueGenLeaders[gyms[o] % ROGUE_LEADER_COUNT].name;
    c->other = r->gymTown[o];
    return o == gio && !any ? XNAME_GIOVANNI : XNAME_LEADER;
}

// What the other leaders say of GIOVANNI where he is TEAM ROCKET's BOSS
// too ({NAME} in {OTHER}), in place of their line about another leader:
// uneasy while nobody knows (absences, men in black), wary once TEAM
// ROCKET's hideout is cleared, and once he has said who he is, shocked.
TEXT_BANK(sGiovanniUneasy0,
    "{NAME} in {OTHER} missed the LEADERS' meeting again. Men in black suits came to say sorry for him.",
    "Have you met {NAME} of {OTHER}? Neither have I, lately. That GYM is always shut when I visit.",
    "{NAME} in {OTHER} sent me a gift. A very expensive one. I didn't ask what he wanted for it.",
    "{NAME} of {OTHER} asked how many rare POK@MON live near my GYM. Odd thing for a LEADER to ask.");
TEXT_BANK(sGiovanniUneasy1,
    "TEAM ROCKET's hideout is empty, and {NAME} of {OTHER} closed his GYM that same week. Odd timing.",
    "They say TEAM ROCKET's BOSS got away. {NAME} in {OTHER} left town the same night. I don't like it.",
    "Since TEAM ROCKET cleared out, {NAME} in {OTHER} won't answer my letters. Not even the angry ones.",
    "The LEAGUE wants to talk to {NAME} of {OTHER} about TEAM ROCKET. Nobody can find him.");
TEXT_BANK(sGiovanniUneasy2,
    "So {NAME} was TEAM ROCKET's BOSS all along. I sat beside him at every LEAGUE dinner!",
    "{NAME} of {OTHER}, a crook! And you beat his TEAM ROCKET. The whole LEAGUE owes you.",
    "I always wondered where {NAME} went at night. Now the whole region knows. Thanks to you.",
    "{NAME} in {OTHER} ran TEAM ROCKET from his own GYM. I'll never trust a nice suit again.");

static bool8 HasNameSlot(const char *t)
{
    for (; *t; t++)
        if (t[0] == '{' && t[1] == 'N' && t[2] == 'A' && t[3] == 'M' && t[4] == 'E')
            return TRUE;
    return FALSE;
}

static const char *GiovanniUneasy(struct RogueTextCtx *c)
{
    static const struct RogueTextBank *const banks[3] = { &sGiovanniUneasy0, &sGiovanniUneasy1, &sGiovanniUneasy2 };
    return RogueText_PickRegion(banks[GiovanniKnown(c)], c, c->gym);
}

// Lines a leader says only in a town with this signature (its landmark is
// there on the map), in place of their usual one: LORELEI in a hot spring
// town, BROCK by a mine. Leaders by their gRogueGenLeaders index.
struct LeaderTownLine
{
    u8 leader, field, sig;
    const char *line;
};
static const struct LeaderTownLine sLeaderTownLines[] =
{
    { 28 /* LORELEI */, RLEADER_INTRO, RSIG_HOT_SPRING,
      "Welcome to {TOWN}. I am LORELEI. Everyone here soaks in the hot spring all day.|I prefer my water frozen. So do my ICE POK@MON. Are you ready?" },
    { 28 /* LORELEI */, RLEADER_AFTER, RSIG_HOT_SPRING,
      "Go and warm up in the spring, if you must. I'll stay right here, where it's cold." },
    { 18 /* GLACIA */, RLEADER_INTRO, RSIG_HOT_SPRING,
      "Welcome to {TOWN}. Steam from the spring outside, and ice in here. Lovely, isn't it?|Let's see how you stand the cold after a nice hot soak!" },
    { 11 /* FLANNERY */, RLEADER_INTRO, RSIG_HOT_SPRING,
      "Welcome to {TOWN}! The spring out there is hot. My POK@MON are hotter!|I'm FLANNERY, and I'm going to show you why!" },
    { 26 /* BLAINE */, RLEADER_AFTER, RSIG_HOT_SPRING,
      "Hah! Quiz! What's best after a hot battle? A hotter spring! It's just outside. Go soak!" },
    { 20 /* BROCK */, RLEADER_AFTER, RSIG_MINING,
      "Every stone out of the mine in {TOWN} comes past my GYM. You're tougher than most of them.|Someday I want to be a great POK@MON BREEDER, too." },
    { 8 /* ROXANNE */, RLEADER_AFTER, RSIG_FOSSIL,
      "The fossil dig in {TOWN} taught me patience. You just taught me the rest. Thank you." },
    { 21 /* MISTY */, RLEADER_AFTER, RSIG_FISHING,
      "The fishers of {TOWN} say my GYM scares the fish. I say they just fish badly. Anyway, you were good!" },
    { 23 /* ERIKA */, RLEADER_AFTER, RSIG_FLOWERS,
      "The flower fields of {TOWN} are lovely this time of year. Do walk through them.|You battled beautifully." },
    { 2 /* BASIL */, RLEADER_AFTER, RSIG_BERRY,
      "Take a BERRY from the farm on your way out. The farmers know BASIL's friends." },
    { 29 /* BRUNO */, RLEADER_AFTER, RSIG_DOJO,
      "Hoo hah! The dojo in {TOWN} trains at dawn. I train before dawn. You should try both!" },
    { 25 /* SABRINA */, RLEADER_AFTER, RSIG_SPACE,
      "The scientists at the SPACE CENTER watch the stars over {TOWN}. I foresaw your win. They did not." },
    { 22 /* LT. SURGE */, RLEADER_AFTER, RSIG_WEATHER,
      "The forecasters next door said storms today. Ha! The only storm in here was you, kid!" },
    { 10 /* WATTSON */, RLEADER_AFTER, RSIG_MARKET,
      "Wahahahah! Go spend your prize at the market, {PLAYER}! The sellers there are sharper than me!" },
    { 13 /* WINONA */, RLEADER_AFTER, RSIG_WEATHER,
      "The forecasters of {TOWN} tell me the winds each morning. Today they missed one. You." },
};

static const char *LeaderTownLine(struct RogueTextCtx *c, u8 leader, u8 field)
{
    u32 i;
    u8 sig;
    if (c->gym >= ROGUE_NUM_GYMS)
        return NULL;
    sig = RogueGen_TownSignature(c->r, c->r->gymTown[c->gym]);
    for (i = 0; i < LORE_COUNT_OF(sLeaderTownLines); i++)
    {
        const struct LeaderTownLine *t = &sLeaderTownLines[i];
        if (t->leader == leader && t->field == field && t->sig == sig && RogueText_CanFill(t->line, c))
            return t->line;
    }
    return NULL;
}

// Which of a leader's two lines a region hears: one per region, by the
// seed, or the other if this one's slots cannot be filled here; a line of
// the town's signature first. again: the next of those (the after line
// when talked to again).
static const char *LeaderPickIn(struct RogueTextCtx *c, u8 leader, u8 field, bool8 again, u8 xname)
{
    const char *const *lines = &sLeaderLines[leader % ROGUE_LEADER_COUNT][(field % RLEADER_LINES) * 2];
    const char *v[2] = { lines[0], lines[1] };
    const char *ok[3];
    u32 pick = RogueRng_Hash(c->r->seed, LEADERS_SALT + 100 + leader * 8 + field) & 1, n = 0;
    // GIOVANNI, where he is TEAM ROCKET's BOSS too.
    if ((ok[0] = GiovanniGymLine(c, leader, field, again)) != NULL)
        return ok[0];
    // Their line about another leader, about GIOVANNI instead.
    if (xname == XNAME_GIOVANNI)
    {
        if (HasNameSlot(v[0]))
            v[0] = GiovanniUneasy(c);
        if (HasNameSlot(v[1]))
            v[1] = GiovanniUneasy(c);
    }
    if ((ok[n] = LeaderTownLine(c, leader, field)) != NULL)
        n++;
    if (RogueText_CanFill(v[pick], c))
        ok[n++] = v[pick];
    if (RogueText_CanFill(v[pick ^ 1], c))
        ok[n++] = v[pick ^ 1];
    if (n == 0)
        return v[pick];
    return ok[again && n > 1 ? 1 : 0];
}

static const char *LeaderPick(struct RogueTextCtx *c, u8 leader, u8 field, u8 xname)
{
    return LeaderPickIn(c, leader, field, FALSE, xname);
}

const char *RogueText_LeaderLine(u8 leader, u8 field)
{
    return sLeaderLines[leader % ROGUE_LEADER_COUNT][(field % RLEADER_LINES) * 2];
}

// A gym leader's (c->gym 0-7) or Elite Four member's (8-11) line, with
// {NAME} and {OTHER} set.
const char *RogueText_LeaderLineIn(struct RogueTextCtx *c, u8 leader, u8 field)
{
    return LeaderPick(c, leader, field, LeaderCtx(c, leader, FALSE));
}

// A gym leader's after line: as the badge and TM are handed over, and
// (again) their other one when talked to after that.
const char *RogueText_LeaderAfter(struct RogueTextCtx *c, u8 leader, bool8 again)
{
    return LeaderPickIn(c, leader, RLEADER_AFTER, again, LeaderCtx(c, leader, FALSE));
}

// What the townsfolk say about a gym's leader (a gym 0-7), a template.
const char *RogueText_LeaderGossip(const struct RogueRegion *r, u8 gym)
{
    u8 gyms[ROGUE_NUM_GYMS], e4[ROGUE_NUM_E4], leader;
    RogueGen_RegionLeaders(r, gyms, e4);
    leader = gyms[gym % ROGUE_NUM_GYMS] % ROGUE_LEADER_COUNT;
    if (GiovanniGossip(r, gym % ROGUE_NUM_GYMS) != NULL)
        return GiovanniGossip(r, gym % ROGUE_NUM_GYMS);
    return sLeaderLines[leader][RLEADER_GOSSIP * 2 + (RogueRng_Hash(r->seed, LEADERS_SALT + 100 + leader * 8 + RLEADER_GOSSIP) & 1)];
}

// The gym guide by each gym's door: one line per gym, spread so a region
// hears each once.
TEXT_BANK(sGymGuide,
    "Yo! How's it going, CHAMPION-bound {PLAYER}?|{LEADER} uses {TYPE} POK@MON. Plan your team around that!",
    "Yo, {PLAYER}! Welcome to {STEM}'s GYM.|{LEADER} trains {TYPE} POK@MON here. Pick your team with care!",
    "Hey, {PLAYER}! The {TRADE_FOLK} of {STEM} come every week to cheer for {LEADER}.|It's {TYPE} POK@MON all the way here. Think about that!",
    "Yo! Another challenger! {LEADER} has been waiting all morning.|Word is, {TYPE} POK@MON are the specialty here. Be ready!",
    "Yo! I'm the guide. I go from GYM to GYM, and {TOWN}'s is one of my favorites.|{LEADER} battles with {TYPE} POK@MON. Got a plan?",
    "Yo, {PLAYER}! You made it to {STEM}!|{LEADER} goes with {TYPE} POK@MON. Don't say I didn't warn you!",
    "Yo! Folks here say {TOWN} has {PRIDE}. Me, I say it has a great GYM.|{LEADER} uses {TYPE} POK@MON. Go get 'em!",
    "Hey, {PLAYER}! I've seen {LEADER} beat TRAINERS twice your size.|{TYPE} POK@MON, every one. You've been warned!",
    "Yo! The whole of {STEM} turns out when there's a challenger. No pressure!|{LEADER}'s team is all {TYPE}. Good luck!");

// Once the gym is won, the guide cheers instead (as Emerald's does: "That
// was a great battle!"), and no longer warns.
TEXT_BANK(sGymGuideWon,
    "Whoa! You beat {LEADER}! That was one great battle, {PLAYER}!|Folks in {STEM} will be talking about it all week.",
    "Yo, {PLAYER}! The {BADGE} looks good on you!|I knew you had it in you. Well, I hoped.",
    "Hey, CHAMPION-bound {PLAYER}! {LEADER} hasn't lost like that in ages.|On to the next GYM! I'll be there before you. I always am.",
    "Yo! I saw the whole thing from the door. What a finish!|{LEADER} will be training extra hard now. Watch out next time you're in {STEM}.",
    "You did it! The {BADGE}! I'd shake your hand, but I'm still shaking.|Rest up. The next GYM won't wait forever.",
    "Yo, {PLAYER}! {TYPE} POK@MON, beaten fair and square.|I'll tell the next guide you're coming. Well, I am the next guide.",
    "Ha! You made it look easy. It wasn't, was it?|Go on, show that {BADGE} around {STEM}. You earned it.",
    "Yo! Another BADGE for the case! How many is that now? {^BADGES}?|Keep going, {PLAYER}. I've got a good feeling about you.");

const char *RogueText_GymGuide(struct RogueTextCtx *c)
{
    if (c->gym < ROGUE_NUM_GYMS && c->state != NULL && ((c->state->wonGyms >> c->gym) & 1))
        return RogueText_PickRegion(&sGymGuideWon, c, c->gym);
    return RogueText_PickRegion(&sGymGuide, c, c->gym);
}

// The TM a gym leader gives with the badge, before and after it is given.
TEXT_BANK(sLeaderTM,
    "Take this TECHNICAL MACHINE, too. It holds one of my favorite {TYPE} moves.",
    "And this TM is yours. My {TYPE} POK@MON taught me what it's worth.",
    "Here, a TM from our GYM. It holds a move my {TYPE} POK@MON love. Use it well!",
    "One more thing: this TM. {TYPE} moves are what I'm known for.",
    "Take this TM. Everyone who beats me gets one. Not many do!",
    "This TM is a gift from {STEM}'s GYM. Take it with you.",
    "You've earned this TM as well. It holds a move of my type, of course.",
    "Here. A TECHNICAL MACHINE, from me to you.");
// (After the TM the leader says their own after line, RogueText_LeaderAfter.)
const char *RogueText_LeaderTM(struct RogueTextCtx *c, bool8 after)
{
    (void)after;
    return RogueText_PickRegion(&sLeaderTM, c, c->gym);
}

// What the last Elite Four member adds: the CHAMPION is someone the player
// has met.
TEXT_BANK(sE4LastHint,
    "The CHAMPION has been asking about you. Did you know that?",
    "Go on. The CHAMPION's been waiting for you for a long time. Longer than you know.",
    "The CHAMPION walks the roads of {REGION} more than anyone. Somehow, never yours.",
    "The CHAMPION heard about you in every town. Now you'll finally meet.");
// When the player met the wanderer on the road.
TEXT_BANK(sE4LastHintMet,
    "The CHAMPION is through that door. Funny... I think you two have met.",
    "I'd tell you about the CHAMPION, but I think you two have already met.",
    "The CHAMPION has been asking about you. Did you know that?",
    "Go on. The CHAMPION's been waiting for you for a long time. Longer than you know.");

const char *RogueText_E4LastHint(struct RogueTextCtx *c, u8 met)
{
    return RogueText_PickRegion(met ? &sE4LastHintMet : &sE4LastHint, c, 0);
}

// ---------------------------------------------------------------------------
// The rival: four battles (on the second route, before the fourth gym's
// cave, after the sixth gym and at the foot of VICTORY ROAD). {RIVAL_MON}
// is the partner as it is now; {OTHER} is where the last battle was (at
// the last, where the first was); {GYM_TOWN} the next gym's town from here.

TEXT_BANK(sRival1Intro,
    "Hey! You're the new TRAINER Dad told me about, right?|I'm {RIVAL}. PROF. BIRCH is my dad. And this is {RIVAL_MON}, my partner!|Let's see who trained harder!",
    "There you are! I'm {RIVAL}. Dad said you'd come down {PLACE} today.|{RIVAL_MON} and I have been waiting since breakfast. Battle!",
    "Oh! You're the one Dad sent out! I'm {RIVAL}, PROF. BIRCH's kid.|I picked {RIVAL_MON} this morning. We already match. Let's go!",
    "Hi! I'm {RIVAL}! Dad told me all about you. Well, he told me your name.|{RIVAL_MON} wants to meet your POK@MON. In battle!");
TEXT_BANK(sRival1Defeat,
    "No way! You're good, {PLAYER}!",
    "Aww! {RIVAL_MON}, we'll get them next time!",
    "Wow... OK! Beginner's luck, right?",
    "Already? We only just started!");
TEXT_BANK(sRival1After,
    "Dad says you only get one chance with each POK@MON out here. So take care of yours!|I'm heading on to the GYM in {GYM_TOWN}. See you there, {PLAYER}!",
    "{RIVAL_MON} needs a rest. And a snack. Mostly a snack.|The next GYM is in {GYM_TOWN}. Race you there!",
    "OK, you win this one. But I'm writing it down.|I'll see you in {GYM_TOWN}, {PLAYER}! I'm going to train the whole way!",
    "I'm telling Dad you're strong. He'll be so happy he picked you.|I'm off to {GYM_TOWN}! Come on, {RIVAL_MON}!");

TEXT_BANK(sRival2Intro,
    "{PLAYER}! Last time, on {OTHER}, I went easy on you.|{RIVAL_MON} and I have trained every day since. No more easy!",
    "Look who it is! {RIVAL_MON}, it's our favorite rival!|You got lucky back on {OTHER}. Let's see you do it twice!",
    "Ha! I knew I'd catch up with you, {PLAYER}! I took the long way and still made it.|Ready to lose this time?",
    "{PLAYER}! Look at those BADGES! I've got a few myself.|{RIVAL_MON} has grown a lot since {OTHER}. Want to see?");
TEXT_BANK(sRival2Defeat,
    "Ugh! You keep getting stronger!",
    "Again?! {RIVAL_MON}, how is this happening?",
    "Hey! I was just warming up!",
    "Two for you. I'm counting, you know!");
TEXT_BANK(sRival2After,
    "OK, OK. Two wins for you. I'm counting, you know.|The next GYM's in {GYM_TOWN}. I'm going to train till I can beat you, then go there!",
    "{RIVAL_MON} says it's fine. {RIVAL_MON} is too nice.|I'll see you down the road, {PLAYER}. I'll be stronger. Watch!",
    "You know what? Losing to you is making me better.|Don't tell Dad I said that. See you in {GYM_TOWN}!");
TEXT_BANK(sRival2Fallen,
    "...Hey. Dad says you've lost a friend or two out there.|That's hard. You're still going, though. That's brave.",
    "I heard some of your POK@MON didn't make it this far. I'm sorry.|They'd want you to keep going. I know they would.");

TEXT_BANK(sRival3Shaken,
    "{PLAYER}... You haven't lost a single POK@MON out there, have you?|I keep losing to you. {RIVAL_MON} says it's fine. It's not fine! Battle!",
    "Three times. If I lose three times, I'm... I'm going to try again.|{RIVAL_MON}, let's show them how far we've come!",
    "I've been training since {OTHER}. Every day. Every single day.|So why am I nervous? Let's battle before I think about it!");
TEXT_BANK(sRival3Proud,
    "So many BADGES between us now. We're really doing this, huh?|{RIVAL_MON} and I came all the way from {OTHER}. I'm proud of us. Don't hold back!",
    "There you are! Do you know how many TRAINERS I've beaten since {OTHER}? A lot!|Every one of them talked about you. Let's give them something to talk about.",
    "{PLAYER}! I caught some tough ones since last time.|{RIVAL_MON} is the toughest of all. Ready?");
TEXT_BANK(sRival3Defeat,
    "...Wow. OK. That was a real battle.",
    "{RIVAL_MON}... we gave it everything. I'm not even mad.",
    "How do you DO that?",
    "...Huh. You really are strong.");
TEXT_BANK(sRival3After,
    "A couple more GYMS, then the LEAGUE. Can you believe it?|Don't you dare lose before we get there, {PLAYER}!",
    "I used to think I'd be the one to win it all. Now I'm not so sure.|But I'm not giving up! See you at the top, {PLAYER}!",
    "{RIVAL_MON} needs a nap. I need a nap.|Go on ahead, {PLAYER}. We'll catch up. We always do!");
TEXT_BANK(sRival3Fallen,
    "Hey... I heard about your team. Not everyone made it, huh?|Your team got you this far. Let's make it count.",
    "...It's been a hard road for your team, hasn't it? Your friends would want you to keep going.|So keep going, {PLAYER}. For them, too.");

TEXT_BANK(sRival4Intro,
    "There you are, {PLAYER}! Eight BADGES. Me too.|Before either of us climbs VICTORY ROAD, let's settle this. For real!",
    "{PLAYER}. Remember {OTHER}? Feels like a year ago.|{RIVAL_MON} and I have one more battle in us. Let's make it a good one.",
    "I've lost to you three times. I wrote them all down.|This time, {RIVAL_MON} and I are bringing everything. Ready?",
    "So this is it. VICTORY ROAD's right behind me.|One last battle, {PLAYER}. Just you and me. Like the first time.");
TEXT_BANK(sRival4Defeat,
    "...You really are something.",
    "That's it. You're the real deal, {PLAYER}.",
    "...Yeah. I knew it. I think I always knew it.",
    "{RIVAL_MON}... We did great. We just didn't win.");
TEXT_BANK(sRival4After,
    "Go on. Beat the ELITE FOUR and the CHAMPION for both of us.|I'll tell Dad you made it all the way to the top!",
    "I'm going home to tell Dad everything. Well, most of it.|Go win, {PLAYER}. {RIVAL_MON} and I will be cheering.",
    "You know, I'm glad it was you. Out of everyone.|Now go. The top's waiting!");
TEXT_BANK(sRival4Fallen,
    "Whoever didn't make it this far... they'd be proud of you. I know I am.|Go win it, {PLAYER}.",
    "Not all of your team will see the top. But your team got you here.|Go on. Win it for them, too.");

static const struct RogueTextBank *const sRivalBanks[4][3] =
{
    { &sRival1Intro, &sRival1Defeat, &sRival1After },
    { &sRival2Intro, &sRival2Defeat, &sRival2After },
    { &sRival3Proud, &sRival3Defeat, &sRival3After },
    { &sRival4Intro, &sRival4Defeat, &sRival4After },
};
static const struct RogueTextBank *const sRivalFallen[4] = { NULL, &sRival2Fallen, &sRival3Fallen, &sRival4Fallen };

// The place of a rival battle (0-3), or 0xFF.
static u8 RivalPlace(const struct RogueRegion *r, u8 battle)
{
    u32 p;
    for (p = 0; p < r->nPlaces; p++)
        if (r->places[p].rival == battle + 1)
            return p;
    return 0xFF;
}

// One rival line (field RRIVAL_*) for a battle (0-3). The fallen are
// mentioned only when there are some, after the battle, and in about one
// meeting in three (style guide section 5).
const char *RogueText_RivalLine(struct RogueTextCtx *c, u8 battle, u8 field)
{
    const struct RogueRegion *r = c->r;
    const struct RogueTextBank *b;
    u8 here = RivalPlace(r, battle & 3), gym;
    battle &= 3;
    field %= 3;
    if (here != 0xFF)
        c->place = here, c->town = RogueGen_TownNear(r, here);
    // Where you last met; at the last battle, where you first met ("Remember
    // ROUTE 2?").
    c->other = battle == 3 ? RivalPlace(r, 0) : battle > 0 ? RivalPlace(r, battle - 1) : 0xFF;
    gym = c->place < r->nPlaces ? RogueText_NextGym(r, c->place) : ROGUE_NUM_GYMS;
    c->gym = gym < ROGUE_NUM_GYMS ? gym : 0xFF;
    b = sRivalBanks[battle][field];
    if (battle == 2 && field == RRIVAL_INTRO && c->state != NULL && c->state->fallen == 0)
        b = &sRival3Shaken;
    if (field == RRIVAL_AFTER && sRivalFallen[battle] != NULL && c->state != NULL && c->state->fallen > 0
     && RogueRng_Hash(r->seed, LEADERS_SALT + 200 + battle) % 3 == 0)
        b = sRivalFallen[battle];
    return RogueText_Pick(b, c, 0);
}

// ---------------------------------------------------------------------------
// The wanderer: three or four meetings on the road (RogueCast.wandererTowns),
// each in the voice of their look, then the CHAMPION's room.

// A dungeon worth a hint at a meeting: one named after the town, else one
// with a legend, not yet cleared; not the villains' hideout or VICTORY ROAD.
// Away from any, the meetings take turns over the region's, so two do not
// hint at the same place.
static s32 WandererHint(const struct RogueRegion *r, u8 town, u8 meeting, const struct RogueTextState *st)
{
    s32 d, near = -1, far[ROGUE_MAX_DUNGEONS];
    u32 nFar = 0, legends = 0;
    for (d = 0; d < r->nDungeons; d++)
    {
        const struct RogueDungeon *dg = &r->dungeons[d];
        if (dg->kind == RDUN_HIDEOUT || dg->kind == RDUN_VICTORY)
            continue;
        if (st != NULL && ((st->bosses >> d) & 1))
            continue;
        if (RogueGen_TownNear(r, dg->host) == town)
        {
            if (near < 0 || (dg->boss == RBOSS_LEGEND && r->dungeons[near].boss != RBOSS_LEGEND))
                near = d;
            continue;
        }
        // Legends first.
        if (dg->boss == RBOSS_LEGEND)
        {
            memmove(&far[legends + 1], &far[legends], (nFar - legends) * sizeof(far[0]));
            far[legends++] = d;
        }
        else
        {
            far[nFar] = d;
        }
        nFar++;
    }
    if (near >= 0)
        return near;
    if (nFar == 0)
        return -1;
    return far[meeting % (legends ? legends : nFar)];
}

// Their words, by look (RLOOK_*): the first meeting's greeting and one for
// later meetings, two of each.
static const char *const sWandererFirst[RLOOK_COUNT][2] =
{
    [RLOOK_STEVEN] = {
        "Oh, hello. My name is {WANDERER}. I'm interested in rare stones, so I travel here and there.",
        "Hm? You must be the TRAINER PROF. BIRCH sent out. I'm {WANDERER}. I collect rare stones.",
    },
    [RLOOK_WALLACE] = {
        "Ah! A TRAINER with a spark in their eye. I'm {WANDERER}. I travel to see beautiful things.",
        "Well, hello. I'm {WANDERER}. I go wherever the view is best. Today, it's {STEM}.",
    },
    [RLOOK_ACE] = {
        "Hey! You're a TRAINER too, right? I'm {WANDERER}. I train wherever the road goes.",
        "Oh! You're the one PROF. BIRCH sent out? I'm {WANDERER}. I'm on the road too!",
    },
    [RLOOK_VETERAN] = {
        "Well now. A young TRAINER on the road. I'm {WANDERER}. I've walked these roads a long time.",
        "Hello, dear. You're PROF. BIRCH's new TRAINER, aren't you? I'm {WANDERER}.",
    },
};
// Met before: [0] and [2] after one meeting, [1] after two or more.
static const char *const sWandererAgain[RLOOK_COUNT][3] =
{
    [RLOOK_STEVEN] = {
        "{PLAYER}, isn't it? We meet again. The road keeps putting us in the same places.",
        "{PLAYER}. Our paths keep crossing, don't they? I'm starting to think it isn't luck.",
        "Oh, {PLAYER}. I had a feeling I'd see you here. Call it a hunch.",
    },
    [RLOOK_WALLACE] = {
        "{PLAYER}! What luck. I was hoping our paths would cross again.",
        "{PLAYER}, again! Every time we meet, you move a little more like a CHAMPION.",
        "{PLAYER}, how marvelous! You look stronger. And a little dustier.",
    },
    [RLOOK_ACE] = {
        "{PLAYER}! I was hoping I'd run into you again. How's the team?",
        "{PLAYER}! We keep running into each other. The road's small, or we're going the same way.",
        "Hey, {PLAYER}! Fancy meeting you here!",
    },
    [RLOOK_VETERAN] = {
        "Hello again, {PLAYER}. The road's been good to you. I can tell.",
        "Well, if it isn't {PLAYER}. These old feet keep finding you. You've grown each time. Don't argue.",
        "Ah, {PLAYER}. Sit a moment. You've been walking hard, I can tell.",
    },
};
// What they talk about at each meeting: early, middle, later, and on the
// eve of the LEAGUE (the last meeting, whichever it is).
static const char *const sWandererTalk[RLOOK_COUNT][4] =
{
    [RLOOK_STEVEN] = {
        "The {TRADE_FOLK} of {TOWN} let me look at the stones in their walls. Ordinary, every one. Lovely.",
        "This rock by the road is older than every town in {REGION}. Isn't that something?",
        "{^LANDMARK} is fine work. But I'd trade it for the rock it stands on.",
        "I've walked {REGION} from end to end. Every road here leads up, in the end.",
    },
    [RLOOK_WALLACE] = {
        "The light in {TOWN} is lovely at this hour. Even the {TRADE_FOLK} look like a painting.",
        "Did you see how your {LEAD} turned just then? Beautiful. Never lose that.",
        "They told me {TOWN} has {PRIDE}. I came to see if it's true. It is.",
        "Every road in {REGION} winds its way upward. So does every TRAINER worth watching.",
    },
    [RLOOK_ACE] = {
        "I've been training at the edge of {TOWN} all week. The {TRADE_FOLK} bring me lunch. Nice town.",
        "{^BADGES} BADGES already? Then I'd better train harder myself.",
        "Your {LEAD} has gotten tougher. I can see it in how it stands.",
        "You've come a long way. So have I. Let's compare notes at the top.",
    },
    [RLOOK_VETERAN] = {
        "I walked this road when I was your age. The grass was taller then. Or I was shorter.",
        "Long ago, {FOUNDING}. The old ones in {OLDTOWN} still tell it.",
        "The {TRADE_FOLK} of {TOWN} have done the same work for a hundred years. There's wisdom in that.",
        "I'll tell you a secret. The top of the road isn't the end of it. But you'll see.",
    },
};
// A hint about a dungeon nearby: one with a legend, one without.
static const char *const sWandererHint[RLOOK_COUNT][2] =
{
    [RLOOK_STEVEN] = {
        "They say {LEGEND} rests in {DUNGEON}. Stones remember things like that.",
        "Have you been inside {DUNGEON}? The walls there are full of things worth finding.",
    },
    [RLOOK_WALLACE] = {
        "Something graceful sleeps in {DUNGEON}. {LEGEND}, the old songs say.",
        "{DUNGEON}... I hear it's dazzling inside. Go and see. Then tell me.",
    },
    [RLOOK_ACE] = {
        "{LEGEND}... I heard it lives in {DUNGEON}. Can you imagine battling it?",
        "Strong POK@MON wait in {DUNGEON}. I'd go myself, but I'd rather see what you do there.",
    },
    [RLOOK_VETERAN] = {
        "An old woman once told me {LEGEND} sleeps in {DUNGEON}. I believe her.",
        "In my day, nobody went into {DUNGEON}. You're braver than we were.",
    },
};
// Before the gift (at the second meeting), and each farewell: on the road,
// and on the eve of the LEAGUE.
static const char *const sWandererGift[RLOOK_COUNT] =
{
    [RLOOK_STEVEN]  = "Here. I found this on my travels. I think it suits you better than me.",
    [RLOOK_WALLACE] = "Take this. A gift should be given while the giver is still moved.",
    [RLOOK_ACE]     = "Here, take this. I had two. Well, I had one, and now you do.",
    [RLOOK_VETERAN] = "Here. Someone gave me this when I was starting out. It's your turn.",
};
static const char *const sWandererBye[RLOOK_COUNT][4] =
{
    [RLOOK_STEVEN] = {
        "Well, I'll be going. I hope we meet again.",
        "I'm off to look at a cliff I've heard about. Take care, {PLAYER}.",
        "If you find any interesting stones, do hold on to them. Until next time.",
        "I'll be waiting where the road ends. You'll see what I mean.",
    },
    [RLOOK_WALLACE] = {
        "Until next time, {PLAYER}. Do keep moving beautifully.",
        "I must go. The light is changing, and I mustn't miss it. Farewell!",
        "Keep your team shining, {PLAYER}. I'll know if you don't.",
        "I'll be waiting where the road ends. Do come dressed for it.",
    },
    [RLOOK_ACE] = {
        "Gotta go! I'll be watching for you, {PLAYER}!",
        "Back to training for me! Next time, let's see whose team grew more.",
        "See you down the road! I'm not letting you get too far ahead.",
        "I'll be waiting where the road ends. Don't keep me waiting too long!",
    },
    [RLOOK_VETERAN] = {
        "Off you go, dear. Mind the long grass.",
        "Go on, then. Eat something warm tonight. TRAINERS forget to.",
        "Off you go. I'll take the long way, as usual.",
        "I'll be waiting where the road ends. Take your time.",
    },
};
// Talked to again at the same meeting.
static const char *const sWandererRepeat[RLOOK_COUNT][4] =
{
    [RLOOK_STEVEN] = {
        "I'm still looking at this rock. It's a very good rock.",
        "Hm? Oh, don't mind me. I'm reading the layers in this stone.",
        "There's a seam of quartz here. Small. Perfect.",
        "Go on ahead, {PLAYER}. I'll be there soon enough.",
    },
    [RLOOK_WALLACE] = {
        "Mm. I'm admiring the view. You may admire it too.",
        "Shh. A POK@MON is grooming itself over there. Exquisite.",
        "Still here? Then let's admire the clouds together for a moment.",
        "Go on, {PLAYER}. The best is saved for last.",
    },
    [RLOOK_ACE] = {
        "Go on, I'm still warming up. Don't let me catch up!",
        "Push-ups! Want to join? No? Your loss!",
        "I'm writing down what I saw in your battles. For research!",
        "Go on! I need to finish my training first.",
    },
    [RLOOK_VETERAN] = {
        "Go on, dear. These old legs need a rest anyway.",
        "I'm watching the clouds. They always tell me when rain is coming.",
        "Still here? Well, sit a while, if you like. Then go.",
        "Go on ahead, dear. I know the way.",
    },
};

// Which townsperson of a town's map makes way for the wanderer (an index
// of m->objs, -1 for none): one whose turn the town's special people (the
// gift, a legend's tale, the villains, the ruins' riddle; TownSpecialTurns
// in rogue_text_towns.h) and the reporter pair leave free, one who stands
// still first. With none free, the meeting is not held in that town.
static u32 TownSpecialTurns(const struct RogueRegion *r, u8 place, u32 nFolk);
s32 RogueText_WandererObj(const struct RogueRegion *r, const struct RogueMap *m)
{
    u32 i, turn = 0, nFolk = 0, taken;
    s32 first = -1;
    for (i = 0; i < m->nObjs; i++)
        nFolk += m->objs[i].kind == ROBJ_VILLAGER;
    taken = TownSpecialTurns(r, m->place, nFolk);
    for (i = 0; i < m->nObjs; i++)
    {
        if (m->objs[i].kind != ROBJ_VILLAGER)
            continue;
        if (!((taken >> turn) & 1) && RogueText_ReporterRole(r, m->place, nFolk, turn) == 0)
        {
            if (m->objs[i].slot % 3 != 0)
                return i;
            if (first < 0)
                first = i;
        }
        turn++;
    }
    return first;
}

// Where the wanderer waits at a meeting: by the POKeMON CENTER's door, where
// every TRAINER comes, as STEVEN waits by a cave's mouth. A block beside
// the way out of the door or a step below it: plain ground (not a door, a
// sign, a person or the block in front of any door), whose neighbours stay
// joined round it, so nobody's way is closed. FALSE if none fits (they
// stand where the townsperson stood).
static bool8 SpotFree(const struct RogueMap *m, int x, int y)
{
    u32 i;
    u16 b;
    if (x < 0 || y < 0 || x >= m->w || y >= m->h)
        return FALSE;
    b = m->blocks[y * m->w + x];
    if (((b >> 10) & 3) != 0 || (b >> 12) != 3)
        return FALSE;
    for (i = 0; i < m->nWarps; i++)
        if (m->warps[i].x == x && (m->warps[i].y == y || m->warps[i].y + 1 == y))
            return FALSE;
    for (i = 0; i < m->nSigns; i++)
        if (m->signs[i].x == x && m->signs[i].y == y)
            return FALSE;
    for (i = 0; i < m->nGates; i++)
        if (m->gates[i].x == x && m->gates[i].y == y)
            return FALSE;
    return TRUE;
}

static bool8 SpotTaken(const struct RogueMap *m, int x, int y, s32 self)
{
    u32 i;
    for (i = 0; i < m->nObjs; i++)
        if ((s32)i != self && m->objs[i].x == x && m->objs[i].y == y)
            return TRUE;
    return FALSE;
}

// The ground round (x, y) stays one piece without it: its walkable
// neighbours are joined through the ring of eight round it.
static bool8 SpotTaken(const struct RogueMap *m, int x, int y, s32 self);
static bool8 SpotKeepsWay(const struct RogueMap *m, int x, int y, s32 self)
{
    static const s8 ring[8][2] = { { 0, -1 }, { 1, -1 }, { 1, 0 }, { 1, 1 }, { 0, 1 }, { -1, 1 }, { -1, 0 }, { -1, -1 } };
    u8 open = 0, seen, k, changed;
    for (k = 0; k < 8; k++)
    {
        int nx = x + ring[k][0], ny = y + ring[k][1];
        u16 b;
        if (nx < 0 || ny < 0 || nx >= m->w || ny >= m->h)
            continue;
        b = m->blocks[ny * m->w + nx];
        if (((b >> 10) & 3) == 0 && ((b >> 12) == 3 || (b >> 12) == 0) && !SpotTaken(m, nx, ny, self))
            open |= 1 << k;
    }
    // Spread from the first open side along the ring (side to corner to
    // side: neighbours on the ring touch).
    for (k = 0; k < 8 && !((open >> k) & 1 && !(k & 1)); k++)
        ;
    if (k == 8)
        return FALSE;
    seen = 1 << k;
    do
    {
        changed = FALSE;
        for (k = 0; k < 8; k++)
        {
            u8 a = (k + 1) & 7, b2 = (k + 7) & 7;
            if (((open >> k) & 1) && !((seen >> k) & 1) && (((seen >> a) & 1) || ((seen >> b2) & 1)))
            {
                seen |= 1 << k;
                changed = TRUE;
            }
        }
    } while (changed);
    // Every open side reached.
    for (k = 0; k < 8; k += 2)
        if (((open >> k) & 1) && !((seen >> k) & 1))
            return FALSE;
    return TRUE;
}

bool8 RogueText_WandererSpot(const struct RogueMap *m, s32 self, u8 *outX, u8 *outY)
{
    static const s8 tries[][2] = { { -1, 1 }, { 1, 1 }, { -2, 1 }, { 2, 1 }, { -1, 2 }, { 1, 2 }, { -2, 2 }, { 2, 2 } };
    u32 i, k;
    for (i = 0; i < m->nWarps && m->warps[i].kind != RWARP_PC; i++)
        ;
    if (i == m->nWarps)
        return FALSE;
    for (k = 0; k < LORE_COUNT_OF(tries); k++)
    {
        int x = m->warps[i].x + tries[k][0], y = m->warps[i].y + tries[k][1];
        if (SpotFree(m, x, y) && !SpotTaken(m, x, y, self) && SpotKeepsWay(m, x, y, self))
        {
            *outX = x;
            *outY = y;
            return TRUE;
        }
    }
    return FALSE;
}

// A townsperson who points the wanderer out, while they wait in town and
// the player has not talked to them yet: the first townsperson whose turn
// nobody special takes (and not the one the wanderer stands in for). By
// look; once met before, by name.
static const char *const sWandererPointer[RLOOK_COUNT][4] =
{
    [RLOOK_STEVEN] = {
        "There's a TRAINER by the POK@MON CENTER looking at a rock. Just looking at it. For an hour now.",
        "{WANDERER} is back, by the POK@MON CENTER. Still looking at rocks. Go say hello.",
        "That stone collector, {WANDERER}? By the POK@MON CENTER. Asked me what our walls are made of.",
        "{WANDERER} is waiting by the POK@MON CENTER. Said a TRAINER would come by. I think that's you.",
    },
    [RLOOK_WALLACE] = {
        "Did you see the TRAINER by the POK@MON CENTER? Such style! Even the birds stopped to stare.",
        "{WANDERER} is by the POK@MON CENTER again, admiring the view. Or being the view.",
        "There's a crowd by the POK@MON CENTER. {WANDERER} is in the middle of it, posing. Go on.",
        "{WANDERER} is by the POK@MON CENTER, asking everyone about a TRAINER who looks just like you!",
    },
    [RLOOK_ACE] = {
        "A TRAINER by the POK@MON CENTER has been doing push-ups since breakfast. Go say hi. Maybe they'll stop.",
        "{WANDERER} is training by the POK@MON CENTER again. Asked if I'd seen you, too.",
        "{WANDERER} beat three of us by the POK@MON CENTER this morning. Then bought us all lunch.",
        "{WANDERER} is stretching by the POK@MON CENTER. Says it's for something big. Wonder what.",
    },
    [RLOOK_VETERAN] = {
        "An old TRAINER is resting by the POK@MON CENTER. Walked here from the far end of {REGION}, apparently.",
        "{WANDERER} is back, resting by the POK@MON CENTER. Asked after you, I think.",
        "{WANDERER} is telling stories by the POK@MON CENTER. The kids won't leave. Neither will I.",
        "{WANDERER} is sitting by the POK@MON CENTER, watching the road. Waiting for someone, it seems.",
    },
};

s32 RogueText_WandererPointerTurn(const struct RogueRegion *r, const struct RogueMap *m)
{
    u32 i, turn = 0, nFolk = 0, taken;
    s32 standIn = RogueText_WandererObj(r, m);
    for (i = 0; i < m->nObjs; i++)
        nFolk += m->objs[i].kind == ROBJ_VILLAGER;
    taken = TownSpecialTurns(r, m->place, nFolk);
    for (i = 0; i < m->nObjs; i++)
    {
        if (m->objs[i].kind != ROBJ_VILLAGER)
            continue;
        if ((s32)i != standIn && !((taken >> turn) & 1) && RogueText_ReporterRole(r, m->place, nFolk, turn) == 0)
            return turn;
        turn++;
    }
    return -1;
}

// At meeting (0-3); met: met on the road before.
const char *RogueText_WandererPointer(struct RogueTextCtx *c, u8 meeting, bool8 met)
{
    struct RogueCast cast;
    RogueGen_Cast(c->r, &cast);
    return sWandererPointer[cast.wandererLook % RLOOK_COUNT][met ? 1 + (meeting + 2) % 3 : 0];
}

// The meeting that is the last before the LEAGUE.
static bool8 WandererLast(const struct RogueCast *cast, u8 meeting)
{
    return meeting >= 3 || cast->wandererTowns[meeting + 1] == 0xFF;
}

// Says a template into out at n, starting a new box after what is there.
static u32 SayPart(char *out, u32 size, u32 n, const char *t, struct RogueTextCtx *c)
{
    if (t == NULL || n + 2 >= size)
        return n;
    if (n > 0)
        out[n++] = '\f';
    return n + RogueText_Say(out + n, size - n, t, c);
}

// How many meetings a mask of meetings had holds.
static u32 WandererCount(u8 met)
{
    u32 n = 0;
    for (; met; met >>= 1)
        n += met & 1;
    return n;
}

// What the wanderer says at a meeting (0-3) in c's town: RWANDER_TALK the
// first time (met: a bit per meeting had on the road before; gift: the
// gift comes after), RWANDER_BYE after a gift, RWANDER_REPEAT when talked
// to again.
u32 RogueText_Wanderer(struct RogueTextCtx *c, u8 meeting, u8 mode, u8 met, bool8 gift, char *out, u32 size)
{
    const struct RogueRegion *r = c->r;
    struct RogueCast cast;
    u32 n = 0, h;
    u8 look;
    bool8 last;
    s32 d;
    RogueGen_Cast(r, &cast);
    look = cast.wandererLook % RLOOK_COUNT;
    meeting &= 3;
    last = WandererLast(&cast, meeting);
    h = RogueRng_Hash(r->seed, LEADERS_SALT + 300 + meeting);
    out[0] = 0;
    // Talked to again: what they are doing, then (so a meeting never comes
    // down to one line, if the first talk went by) the place's dungeon or
    // their talk, or at the last, where they will wait.
    if (mode == RWANDER_REPEAT)
    {
        const char *t = sWandererTalk[look][last ? 3 : meeting];
        n = SayPart(out, size, 0, sWandererRepeat[look][last ? 3 : meeting], c);
        if (last)
            return SayPart(out, size, n, sWandererBye[look][3], c);
        d = WandererHint(r, c->town, meeting, c->state);
        if (d >= 0)
        {
            c->dungeon = d;
            return SayPart(out, size, n, sWandererHint[look][r->dungeons[d].boss == RBOSS_LEGEND ? 0 : 1], c);
        }
        return RogueText_CanFill(t, c) ? SayPart(out, size, n, t, c) : n;
    }
    if (mode == RWANDER_BYE)
        return SayPart(out, size, 0, sWandererBye[look][last ? 3 : meeting], c);
    met &= (1 << meeting) - 1;
    n = SayPart(out, size, n, WandererCount(met) >= 2 ? sWandererAgain[look][1]
                            : met ? sWandererAgain[look][((RogueRng_Hash(r->seed, LEADERS_SALT + 300) + meeting) & 1) * 2]
                                  : sWandererFirst[look][h & 1], c);
    {
        const char *t = sWandererTalk[look][last ? 3 : meeting];
        if (!RogueText_CanFill(t, c))
            t = sWandererTalk[look][(meeting + 1) % 3];
        if (RogueText_CanFill(t, c))
            n = SayPart(out, size, n, t, c);
    }
    d = last ? -1 : WandererHint(r, c->town, meeting, c->state);
    if (d >= 0)
    {
        c->dungeon = d;
        n = SayPart(out, size, n, sWandererHint[look][r->dungeons[d].boss == RBOSS_LEGEND ? 0 : 1], c);
    }
    if (gift)
        return SayPart(out, size, n, sWandererGift[look], c);
    return SayPart(out, size, n, sWandererBye[look][last ? 3 : meeting], c);
}

// The CHAMPION's room: the wanderer, at last. Before the battle, the
// defeat, after it, and in the HALL OF FAME; two of each, picked per
// region, each saying only as much of the road as the player had of it:
// need is the meetings it takes (a line about "every time we met" two,
// about where you met one). {OTHER} is the town of the first meeting had.
// [2] is for a player who never met them on the road.
struct ChampionLine
{
    const char *t;
    u8 need;
};
static const struct ChampionLine sChampionLines[RLOOK_COUNT][RCHAMP_LINES][3] =
{
    [RLOOK_STEVEN] = {
        { { "So you made it, {PLAYER}.|I'm {WANDERER}. I'm the CHAMPION of {REGION}. I hoped it would be you.|I've seen every stone in this region. You're the rarest thing I've found. Come!", 1 },
          { "Welcome, {PLAYER}. Yes, it's me.|Since {OTHER}, I've wondered how strong you'd become. I'm the CHAMPION of {REGION}.|Show me. Hold nothing back.", 1 },
          { "Welcome, {PLAYER}. We never met on the road, but I heard about you in every town.|I'm {WANDERER}, the CHAMPION of {REGION}. I collect rare stones.|Let's see if you're one. Come!", 0 } },
        { { "I, the CHAMPION, fall in defeat... That was truly something.", 0 },
          { "Well. That was harder than any rock I've ever cracked.", 0 } },
        { { "Every time we met on the road, your team was stronger. I noticed.|Come. Your POK@MON have earned their place in the HALL OF FAME.", 2 },
          { "A stone shines brightest after it's been cut. You've been through a lot, haven't you?|Come. The HALL OF FAME is waiting.", 0 } },
        { { "This room remembers every TRAINER who ever won {REGION}.|Let's record you, and the partners who made it here with you.", 0 },
          { "Every name in this hall started somewhere small. I first saw yours in {OTHER}.|Let's record you and your partners.", 1 } },
    },
    [RLOOK_WALLACE] = {
        { { "So you made it, {PLAYER}. Welcome.|I'm {WANDERER}, the CHAMPION of {REGION}. I hoped it would be you.|Now, let us make this the most beautiful battle {REGION} has ever seen!", 1 },
          { "Ah, {PLAYER}. I knew it the moment I saw you in {OTHER}.|I am {WANDERER}, the CHAMPION of {REGION}. Shall we dance?", 1 },
          { "Ah, so you are {PLAYER}. Our paths never crossed on the road. A pity.|I am {WANDERER}, the CHAMPION of {REGION}.|Let us make up for lost time. Beautifully!", 0 } },
        { { "Bravo. That was the most beautiful battle I've ever lost.", 0 },
          { "Exquisite... I've been bested, and I couldn't be happier.", 0 } },
        { { "I watched you grow on the road, one town at a time.|Come. The HALL OF FAME is waiting for you.", 2 },
          { "You and your POK@MON moved as one. That is all beauty ever is.|Come. Let the HALL OF FAME see you.", 0 } },
        { { "This hall remembers every TRAINER who won {REGION}. Each one beautiful.|Let's add you, and the partners who carried you here.", 0 },
          { "Stand tall, {PLAYER}. This hall has waited a long time for someone like you.|Let's add you and your partners.", 0 } },
    },
    [RLOOK_ACE] = {
        { { "So you made it! I knew you would, {PLAYER}.|I'm the CHAMPION of {REGION}. Surprise! I hoped it would be you.|I trained on every road to be ready for this. Let's go!", 1 },
          { "Hey, {PLAYER}! Remember me, from {OTHER}?|Well... this is the top. I'm the CHAMPION of {REGION}. Surprise! Let's go!", 1 },
          { "So you're {PLAYER}! I kept missing you on the road. Every town said I'd just missed you.|I'm {WANDERER}, the CHAMPION of {REGION}. Let's finally battle!", 0 } },
        { { "That's it. You're better than me. Wow. OK.", 0 },
          { "Ha... I lost. I actually lost. That was amazing!", 0 } },
        { { "You got stronger each time we met on the road. I learned from you, too.|Come on. Your team's earned the HALL OF FAME.", 2 },
          { "I've been CHAMPION a long time. I forgot what it felt like to lose. Thank you.|Come on. The HALL OF FAME's this way.", 0 } },
        { { "This room remembers every TRAINER who ever won {REGION}.|Let's record you, and the partners who made it here with you!", 0 },
          { "Every TRAINER in here earned it. Now you have too.|Let's record you and your team!", 0 } },
    },
    [RLOOK_VETERAN] = {
        { { "So you made it, dear. I knew you would.|I'm {WANDERER}, the CHAMPION of {REGION}. I hoped it would be you.|Now, show me everything the road taught you.", 1 },
          { "Hello again, {PLAYER}. Did you guess? I think a part of you did, back in {OTHER}.|I'm the CHAMPION of {REGION}. Come, dear. Let's see.", 1 },
          { "So you're the one, dear. I looked for you on every road and always came a day late.|I'm {WANDERER}, the CHAMPION of {REGION}. Come. Show me what the road taught you.", 0 } },
        { { "Well, well. The road's in good hands.", 0 },
          { "Ohoho. These old bones knew this day would come.", 0 } },
        { { "I've walked these roads longer than you've been alive. You walked them better.|Come along. The HALL OF FAME is waiting.", 0 },
          { "I'm proud of you, dear. Not as a CHAMPION. As someone who watched you grow on the road.|Come. The HALL OF FAME is this way.", 2 } },
        { { "This room remembers every TRAINER who ever won {REGION}. I was one, long ago.|Now it's your turn, you and your partners.", 0 },
          { "My name's on these walls, from a long time ago. Now yours will be too.|Let's record you and your partners.", 0 } },
    },
};

// met: a bit per meeting had on the road (FLAG_ROGUE_WANDERER_0 on).
const char *RogueText_ChampionLine(struct RogueTextCtx *c, u8 field, u8 met)
{
    struct RogueCast cast;
    const struct ChampionLine *v;
    u32 pick, n = WandererCount(met), i;
    RogueGen_Cast(c->r, &cast);
    c->other = 0xFF;
    for (i = 0; i < 4; i++)
    {
        if ((met >> i) & 1)
        {
            c->other = cast.wandererTowns[i];
            break;
        }
    }
    v = sChampionLines[cast.wandererLook % RLOOK_COUNT][field % RCHAMP_LINES];
    pick = RogueRng_Hash(c->r->seed, LEADERS_SALT + 400 + field % RCHAMP_LINES) & 1;
    if (v[pick].need > n || !RogueText_CanFill(v[pick].t, c))
        pick ^= 1;
    if (v[pick].need > n || !RogueText_CanFill(v[pick].t, c))
        pick = v[2].t != NULL ? 2 : v[0].need <= v[1].need ? 0 : 1;
    return v[pick].t;
}

// ---------------------------------------------------------------------------
// The villain teams: TEAM AQUA, TEAM MAGMA and TEAM ROCKET, one to three a
// region. {TEAM}, {BOSS} and {ADMIN} are the speaker's team (ctx.team),
// {OTHER} its hideout's town, {LEGEND} and {DUNGEON} the legend it is after
// (RogueText_TeamGoal), {TEAM2} and {BOSS2} the team it quarrels with
// (ctx.team2). AQUA: loud, salty, the sea taking the land back, "matey".
// MAGMA: clipped, cold, "progress", grunts like bored office workers.
// ROCKET: FireRed's crooks: profit, stealing and selling POK@MON, pride in
// the name, the BOSS's cold menace; grunts never name him (GIOVANNI may run
// a gym of the region: the twist is his to tell).

// How much a legend is to a team (ROGUE_VILLAIN_*): AQUA the sea, rain and
// storms; MAGMA fire and stone; ROCKET the rare ones that sell (the birds,
// the beasts, MEWTWO).
static s32 TeamLegendScore(u8 id, u8 legend)
{
    u8 e = sLegendElem[legend % RLEG_COUNT];
    if (id == ROGUE_VILLAIN_AQUA)
        return e == LELEM_WATER ? 3 : e == LELEM_STORM || e == LELEM_ICE || e == LELEM_SKY ? 2 : 1;
    if (id == ROGUE_VILLAIN_MAGMA)
        return e == LELEM_FIRE ? 3 : e == LELEM_STONE || e == LELEM_GRASS ? 2 : 1;
    return (legend >= RLEG_ARTICUNO && legend <= RLEG_SUICUNE) || legend == RLEG_MEWTWO ? 3
         : legend >= RLEG_LUGIA ? 2 : 1;
}

// The legend team t is after: the one its leader waits before, else the one
// most like it that no other team is after, else the one most like it.
// -1 for none.
s32 RogueText_TeamGoal(const struct RogueRegion *r, u8 t)
{
    s32 d = RogueGen_TeamLeaderDungeon(r, t), best = -1, bestScore = -1;
    u8 id = RogueGen_TeamId(r, t), u, n = RogueGen_TeamCount(r);
    u16 taken = 0;
    if (d >= 0)
        return d;
    for (u = 0; u < n; u++)
    {
        s32 g = u < t ? RogueText_TeamGoal(r, u) : RogueGen_TeamLeaderDungeon(r, u);
        if (u != t && g >= 0)
            taken |= 1 << g;
    }
    for (d = 0; d < r->nDungeons; d++)
    {
        s32 score;
        if (r->dungeons[d].boss != RBOSS_LEGEND)
            continue;
        score = TeamLegendScore(id, r->dungeons[d].legend) + (((taken >> d) & 1) ? 0 : 8);
        if (score > bestScore)
            best = d, bestScore = score;
    }
    return best;
}

s32 RogueText_VillainGoal(const struct RogueRegion *r)
{
    return RogueText_TeamGoal(r, 0);
}

// Grunts on the route before the hideout's town: intro, defeat, after.
TEXT_BANK(sAquaGruntIntro,
    "Ahoy, kid! TEAM AQUA's got business in {OTHER}. Shove off!",
    "Arr! You didn't see any TEAM AQUA out here, matey. Got it?",
    "Hey! With {LEGEND}'s power, TEAM AQUA's gonna make the seas rise. Stand back!",
    "Blast it, a kid! The boss said nobody'd come down {PLACE}. Battle!",
    "Smell that? That's the sea, coming to take the land back. TEAM AQUA's helping!",
    "Hey, matey! Know the way to {DUNGEON}? ...Forget I asked. Battle!",
    "Ahoy! Lovely day to bring back the sea, eh? Battle!",
    "Ugh, my boots are full of dust. Can't wait till this road's a river. Battle!");
TEXT_BANK(sAquaGruntDefeat,
    "Huh? I lost?!", "Ugh! Blown cover!", "Sunk! I'm sunk!", "Arr... Washed out!", "Man overboard!", "Glub glub...",
    "Run aground!", "Arr, beached!");
TEXT_BANK(sAquaGruntAfter,
    "Our hideout's under {OTHER}. Stay away from it, matey!",
    "Once we wake {LEGEND}, this whole road'll be underwater. You'll see!",
    "The boss, {BOSS}, says the sea was here first. He's loud, but he's right.",
    "I joined TEAM AQUA for the boats. We don't have any boats.",
    "{ADMIN}'s gonna be real mad at me...",
    "Go home, kid. And pack a swimsuit.",
    "Don't tell {BOSS} about this. He'd laugh for a week.",
    "One day this whole road'll be sea. And I'll be the one who lost to you on it.");
TEXT_BANK(sMagmaGruntIntro,
    "Halt. TEAM MAGMA is doing business near {OTHER}. Please leave.",
    "Hey. This road is closed. TEAM MAGMA's orders. I don't make the rules.",
    "TEAM MAGMA is going to wake {LEGEND}. The land must grow. Now move.",
    "Ugh, a kid. I'm on my break. Fine. Battle.",
    "Progress needs room, kid. More land, less sea. Simple math.",
    "You're not on my list. Nobody's on my list. Battle me anyway.",
    "Excuse me. This road is part of a TEAM MAGMA project. Please battle elsewhere. ...Or here.",
    "Ugh. Another kid. Fine. This counts as overtime.");
TEXT_BANK(sMagmaGruntDefeat,
    "Argh! Burned out!", "This wasn't in the plan.", "I'll have to file a report...", "Overheated...", "...Noted.", "Ugh. Not today.",
    "Project delayed.", "Out of order.");
TEXT_BANK(sMagmaGruntAfter,
    "Our hideout's under {OTHER}. Not that I told you.",
    "MAXIE says the land must grow. I just do the paperwork.",
    "When {LEGEND} wakes, the sea will shrink. More land. More progress.",
    "{ADMIN} is going to hear about this. I'll say it was the weather.",
    "Six years with TEAM MAGMA. Still no raise.",
    "Go home. Leave the future to professionals.",
    "More land means more roads. More roads means more of you. Hmm.",
    "If you see {ADMIN}, I was guarding the road the whole time. Very hard.");
TEXT_BANK(sRocketGruntIntro,
    "Hey, kid! TEAM ROCKET's got business in {OTHER}. Keep walking!",
    "TEAM ROCKET is here for the rare POK@MON of {REGION}. Yours look rare enough!",
    "Heard of {LEGEND}? The BOSS says it'll sell for a fortune. Now beat it!",
    "Stop right there! This road is TEAM ROCKET property. The toll is one POK@MON.",
    "A kid, all alone on {PLACE}? Your POK@MON will fetch a nice price!",
    "You didn't see any TEAM ROCKET out here. Got that?",
    "We're TEAM ROCKET! We steal POK@MON and sell them! ...I wasn't supposed to say that.",
    "The BOSS doesn't like witnesses. Neither do I. Battle!");
TEXT_BANK(sRocketGruntDefeat,
    "Blast it!", "Not again!", "You little brat!", "I blew it!", "Rats! Outdone!", "This isn't over!", "Ugh! Bad deal!",
    "Wrong mark!");
TEXT_BANK(sRocketGruntAfter,
    "Our hideout's under {OTHER}. Not that I'll tell you any more. ...Oops.",
    "Once we catch {LEGEND}, TEAM ROCKET will be rich. You'll see!",
    "Nobody's seen the BOSS's face up close. Just his PERSIAN, and his voice.",
    "I joined TEAM ROCKET for the money. I'm still waiting for the money.",
    "{ADMIN} is going to have my head for this...",
    "Fine, keep your POK@MON. I'll find a kid who battles worse.",
    "Beaten by a kid! Forget you saw this. TEAM ROCKET has a name to keep.",
    "TEAM ROCKET never forgets a face. Yours is on the list now.");

// Grunts on a route two teams share (RogueGen_Teams' guest): each team's
// in its own voice, at the other ({TEAM2}, {BOSS2}). Defeat lines are the
// route's.
TEXT_BANK(sAquaClashIntro,
    "Oi! Is that a {TEAM2} goon up the road? This road's AQUA water, matey!",
    "{TEAM2} thinks they can work this road? Not while TEAM AQUA's here! ...Oh, a kid. Battle!",
    "Arr, first {TEAM2}, now you! Can't a sailor guard a road in peace?",
    "Keep away from that {TEAM2} creep, matey. And from me. Battle!");
TEXT_BANK(sAquaClashAfter,
    "If you see any more {TEAM2}, tell 'em TEAM AQUA was here first!",
    "I'd rather lose to you than to {TEAM2}. Small mercies, matey.",
    "{TEAM2} want the opposite of us. That's how you know they're wrong.",
    "Beat that {TEAM2} grunt too, would you? For the sea.");
TEXT_BANK(sMagmaClashIntro,
    "Excuse me. {TEAM2} is on our side of the road. Please report them. ...You're not staff. Battle.",
    "Ugh. {TEAM2} keeps eating our lunches. And now you.",
    "TEAM MAGMA booked this road first. {TEAM2} didn't fill in the form.",
    "Are you with {TEAM2}? No? Good. I'm not paid to battle them. Only you.");
TEXT_BANK(sMagmaClashAfter,
    "{TEAM2} are loud. We are efficient. That's the difference.",
    "File a complaint about {TEAM2} for me, would you?",
    "{TEAM2} and TEAM MAGMA on one road. Somebody planned this badly.",
    "If {TEAM2} saw that, I'm requesting a transfer.");
TEXT_BANK(sRocketClashIntro,
    "{TEAM2}? Amateurs. Real crooks wear black. Now hand over your POK@MON!",
    "This road's TEAM ROCKET turf. {TEAM2} can find their own!",
    "Hey! You with {TEAM2}? No? Even better. Battle!",
    "{TEAM2} wants to save the world or something. TEAM ROCKET just wants your POK@MON!");
TEXT_BANK(sRocketClashAfter,
    "TEAM ROCKET will be selling {TEAM2}'s gear by next week. Mark my words.",
    "Lost to a kid with {TEAM2} watching. The BOSS can never hear of this.",
    "{TEAM2}? Never heard of them. In TEAM ROCKET we only fear the BOSS.",
    "Go bother {TEAM2} instead. They've got nothing worth stealing.");

// Grunts in the hideout.
TEXT_BANK(sAquaHideoutIntro,
    "An intruder?! Not in TEAM AQUA's hideout, you don't!",
    "Ahoy! You've got guts, sneakin' in under {OTHER}, matey!",
    "Halt! This is TEAM AQUA turf. You're finished!",
    "Hey! Who let a landlubber down here?",
    "Keep it down! {ADMIN}'s planning the big wave!",
    "The tide's coming in, kid. And you're in the way.",
    "Ahoy! I've been on watch all night down here. Finally, some action!",
    "Arr! You've got salt in your boots, sneaking in here. I like that. Battle!",
    "Halt, matey! Password? ...Wrong! There's no password! Battle!");
TEXT_BANK(sAquaHideoutDefeat,
    "No way! A kid?!", "Grr... You're tough.", "Abandon ship!", "I'm all wet!", "Blasted landlubber!", "Sunk again!",
    "Arr, my watch is over.", "Capsized!", "Washed up!");
// (Above the boss's floor: the after lines may send the player down; on
// the boss's own floor, the Last ones.)
TEXT_BANK(sAquaHideoutAfter,
    "Our ADMIN is down below. You'll be sorry!",
    "{ADMIN}'ll sink you. Just wait.",
    "We'll wake {LEGEND}. Nobody stops TEAM AQUA!",
    "I'm only down here 'cause the boss promised a sea view.",
    "Matey, you fight like a storm. I hate storms.",
    "Go on down, then. {ADMIN}'s been itching for a fight.",
    "When the sea comes back, I'm getting a house on the beach. Well, under it.",
    "Do you know how hard it is to keep a hideout dry? TEAM AQUA does not.",
    "Fine, fine. Have a look around. Mind the puddles, matey.");
TEXT_BANK(sAquaHideoutAfterLast,
    "{ADMIN}'s just over there, matey. Rather you than me.",
    "{ADMIN}'ll sink you. Just wait.",
    "We'll wake {LEGEND}. Nobody stops TEAM AQUA!",
    "I'm only down here 'cause the boss promised a sea view.",
    "Matey, you fight like a storm. I hate storms.",
    "That pool's for the sub. No swimming, matey. Not even you.",
    "When the sea comes back, I'm getting a house on the beach. Well, under it.",
    "Do you know how hard it is to keep a hideout dry? TEAM AQUA does not.",
    "Fine, fine. Have a look around. Mind the puddles, matey.");
TEXT_BANK(sMagmaHideoutIntro,
    "Intruder. Not in TEAM MAGMA's hideout.",
    "You can't be down here. This floor is staff only.",
    "Halt. Do you have an appointment?",
    "{ADMIN} said no visitors. You're a visitor.",
    "The heat down here is for the project. Not for children.",
    "TEAM MAGMA works for the future. Who do you work for?",
    "Please sign in at the front desk. ...We don't have a front desk. Battle.",
    "Visitor detected. Procedure says I battle you. So, battle.",
    "This is a private research site. You're not research.");
TEXT_BANK(sMagmaHideoutDefeat,
    "Request denied.", "Unscheduled defeat.", "That's not in my job description.", "Meltdown...", "I need a coffee.", "...Fine.",
    "Procedure failed.", "Off the clock. Now.", "I'll file that under losses.");
TEXT_BANK(sMagmaHideoutAfter,
    "Our ADMIN is down below. You'll regret it.",
    "{ADMIN} will handle you. That's in the job description.",
    "We'll wake {LEGEND}. The land will grow.",
    "I asked for a desk job. They gave me a cave.",
    "Fine. Go down. I'm not paid enough to stop you.",
    "The project goes on. With or without me.",
    "MAXIE gives us a speech at breakfast. I've heard it four hundred times.",
    "My shift ends at five. It's four fifty-eight. Go away.",
    "The heat down here is nice. The pay is fine. The kids are a problem.");
TEXT_BANK(sMagmaHideoutAfterLast,
    "{ADMIN} is in the next room. Knock first. ...We don't have doors. Knock anyway.",
    "{ADMIN} will handle you. That's in the job description.",
    "We'll wake {LEGEND}. The land will grow.",
    "I asked for a desk job. They gave me a cave.",
    "This is the bottom floor. The magma's real. Don't touch it.",
    "The project goes on. With or without me.",
    "MAXIE gives us a speech at breakfast. I've heard it four hundred times.",
    "My shift ends at five. It's four fifty-eight. Go away.",
    "The heat down here is nice. The pay is fine. The kids are a problem.");
TEXT_BANK(sRocketHideoutIntro,
    "An intruder! The BOSS said no visitors!",
    "How did a kid get past the guard upstairs?",
    "This is TEAM ROCKET's hideout. You won't leave with your POK@MON!",
    "Halt! Password? ...You don't know it. Battle!",
    "Keep it down! {ADMIN} is counting the money!",
    "Every POK@MON in {REGION} has a price. Yours too!",
    "You came all the way down under {OTHER}? Brave. Dumb, but brave.",
    "Shh! The BOSS might be listening. Battle me quietly.",
    "Nobody just walks into TEAM ROCKET's hideout! Get lost!");
TEXT_BANK(sRocketHideoutDefeat,
    "Impossible!", "Gyaah!", "I'm done for!", "You're too strong!", "Blast!", "I goofed!", "Not my day...",
    "The BOSS will hear of this!", "Argh!");
TEXT_BANK(sRocketHideoutAfter,
    "{ADMIN} is down below. You'll be sorry!",
    "Our ADMIN's on the bottom floor. Go on, see how far you get!",
    "We keep the POK@MON we take down here before we sell them. Don't ask where.",
    "TEAM ROCKET is the strongest in {REGION}! ...Apart from you, apparently.",
    "The pay's good. The BOSS is scary. Pick one.",
    "I just guard this hallway. I've never even seen the BOSS.",
    "When we catch {LEGEND}, we'll name our own price!",
    "Fine. Go down. Tell {ADMIN} I tried.",
    "If I were you, I'd run. Really.");
TEXT_BANK(sRocketHideoutAfterLast,
    "Those crates? Stolen POK@MON BALLS. Er, I mean... deliveries.",
    "This is the bottom floor. Nowhere left for you to run. Or me.",
    "We keep the POK@MON we take down here before we sell them. Don't ask where.",
    "TEAM ROCKET is the strongest in {REGION}! ...Apart from you, apparently.",
    "The pay's good. The BOSS is scary. Pick one.",
    "I just guard this hallway. I've never even seen the BOSS.",
    "When we catch {LEGEND}, we'll name our own price!",
    "Tell {ADMIN} I tried. ...Actually, don't.",
    "If I were you, I'd run. Really.");

// The admins at the bottom of each hideout (MATT or SHELLY; TABITHA or
// COURTNEY; ARCHER or ARIANA): intro (one naming the legend, one without),
// defeat, as they go.
static const char *const sVillainAdminLines[ROGUE_VILLAIN_IDS][2][4] =
{
    [ROGUE_VILLAIN_AQUA] = {
        { "Hehehe! I'm MATT, TEAM AQUA's ADMIN! You came all the way down under {OTHER}?|The boss says {LEGEND}'s power will raise the seas. And I say you're all wet!",
          "Hehehe! I'm MATT, TEAM AQUA's ADMIN! You came all the way down under {OTHER}?|The sea's coming back for this land, kid. You're gonna get soaked!",
          "Bwahaha! Not bad, kid! Not bad at all!",
          "Grunts, pull out! Hahaha! The sea will have this land yet!" },
        { "Ahahaha! A child, all the way down here? I'm SHELLY of TEAM AQUA.|We'll borrow {LEGEND}'s power and give the sea back its land. Time to go home!",
          "Ahahaha! A child, all the way down here? I'm SHELLY of TEAM AQUA.|We only want what the sea wants. Time to go home!",
          "Ahahaha... You got me.",
          "Grunts, pull out! ARCHIE won't be happy... but he'll understand. He always does." },
    },
    [ROGUE_VILLAIN_MAGMA] = {
        { "Hehehe! You made it all the way down. I'm TABITHA, ADMIN of TEAM MAGMA.|Once {LEGEND} wakes, the land will grow. And you'll get burned!",
          "Hehehe! You made it all the way down. I'm TABITHA, ADMIN of TEAM MAGMA.|Our work here will raise the land. And you'll get burned!",
          "Hehehe... I lost?!",
          "Grunts, pull out! MAXIE will hear of this. The project slows. It does not stop." },
        { "...Intruder. Analyzing. I'm COURTNEY. TEAM MAGMA. ADMIN.|{LEGEND}... the land... more land. You're in the way. ...Battle.",
          "...Intruder. Analyzing. I'm COURTNEY. TEAM MAGMA. ADMIN.|You. Me. Battle. ...Then you leave.",
          "...Unexpected. Recalculating.",
          "...Grunts. Withdraw. MAXIE... will want the data." },
    },
    [ROGUE_VILLAIN_ROCKET] = {
        { "So you're the brat who's been making trouble. I'm ARCHER, an ADMIN of TEAM ROCKET.|{LEGEND} is worth more than the whole town above us. I won't let a child spoil the BOSS's plans!",
          "So you're the brat who's been making trouble. I'm ARCHER, an ADMIN of TEAM ROCKET.|Every rare POK@MON in {REGION} has a price. I won't let a child spoil the BOSS's plans!",
          "How could I lose to a child?!",
          "Grunts, pull out of {OTHER}! ...The BOSS will not be pleased." },
        { "My, my. A child, all the way down here. I'm ARIANA, an ADMIN of TEAM ROCKET.|We'll take {LEGEND} and sell it to the highest bidder. You should have stayed home!",
          "My, my. A child, all the way down here. I'm ARIANA, an ADMIN of TEAM ROCKET.|This region's POK@MON will make us very rich. You should have stayed home!",
          "Tch! Unbelievable!",
          "Everyone, we're leaving! ...Don't look so pleased, child. TEAM ROCKET always comes back." },
    },
};

// What an admin adds as the team leaves: where the leader went (the legend
// he waits before), or that he will hear of it (no legend is his).
static const char *const sAdminLeaderGone[ROGUE_VILLAIN_IDS][2] =
{
    [ROGUE_VILLAIN_AQUA]   = { "ARCHIE's already gone to {DUNGEON}, kid. {LEGEND} will be ours!", "ARCHIE will hear about this. And he'll be back." },
    [ROGUE_VILLAIN_MAGMA]  = { "MAXIE has gone ahead to {DUNGEON}. ...You will not catch him in time.", "MAXIE... will be informed." },
    [ROGUE_VILLAIN_ROCKET] = { "The BOSS is already at {DUNGEON}. You'll never catch him!", "The BOSS will hear about this. And the BOSS never forgets." },
};

// The hideout empties once its boss is beaten: told in the field.
static const char *const sHideoutLeft[2] =
{
    "{TEAM} has cleared out of the hideout under {OTHER}! The way on is open again.",
    "{TEAM} has cleared out of the hideout under {OTHER}! The town is quiet again.",
};

const char *RogueText_HideoutLeft(struct RogueTextCtx *c)
{
    const struct RogueRegion *r = c->r;
    u8 t = c->team < RogueGen_TeamCount(r) ? c->team : 0;
    s32 h = RogueGen_TeamHideout(r, t);
    if (h < 0)
        return NULL;
    c->team = t;
    c->other = r->dungeons[h].host;
    return sHideoutLeft[!(t == 0 && RogueGen_VillainBlockade(r))];
}

// TEAM ROCKET's ADMINs as they go where the BOSS waits below them.
static const char *const sRocketAdminBeforeBoss[2] =
{
    "The BOSS is just through there. Go on, then. He'll finish what I started.",
    "Fine. Go and see the BOSS. You won't like what you find.",
};

// The leaders, before the legend: intro (with the legend, without),
// defeat, as they go, and as they go where another team is in the region.
static const char *const sVillainLeaderLines[ROGUE_VILLAIN_IDS][5] =
{
    [ROGUE_VILLAIN_AQUA] = {
        "So you beat my ADMIN under {OTHER}. I'm ARCHIE, boss of TEAM AQUA!|All life came from the sea, kid. {LEGEND} can give it back. Stand aside!",
        "So you beat my ADMIN under {OTHER}. I'm ARCHIE, boss of TEAM AQUA!|All life came from the sea, kid. We're giving it back. Stand aside!",
        "Hah! You're something else.",
        "Fine. TEAM AQUA backs off. For now, kid. The sea's patient. So am I.",
        "Fine. TEAM AQUA backs off. But keep {BOSS2}'s lot away from the sea, kid. Or we'll be back!",
    },
    [ROGUE_VILLAIN_MAGMA] = {
        "So you are the child who undid our work at {OTHER}. I am MAXIE, leader of TEAM MAGMA.|{LEGEND} will give us more land. More room. Progress. You are in the way.",
        "So you are the child who undid our work at {OTHER}. I am MAXIE, leader of TEAM MAGMA.|The land must grow. You are in the way.",
        "Hmph. Brilliant... and a pest.",
        "TEAM MAGMA withdraws. We will meet again, child. Progress is patient.",
        "TEAM MAGMA withdraws. Do not mistake this for a win for {TEAM2}, child. They are worse.",
    },
    [ROGUE_VILLAIN_ROCKET] = {
        "So. You are the child who beat my ADMIN under {OTHER}. I am GIOVANNI, the BOSS of TEAM ROCKET.|{LEGEND} will sell for a fortune. I do not let children spoil my business.",
        "So. You are the child who beat my ADMIN under {OTHER}. I am GIOVANNI, the BOSS of TEAM ROCKET.|Rare POK@MON mean profit, and this region is full of them. Stand aside.",
        "...! I see. You are no ordinary child.",
        "TEAM ROCKET withdraws from here. But TEAM ROCKET never truly ends, child. Remember that.",
        "TEAM ROCKET withdraws. {TEAM2} may keep their dreams of changing the world. I only ever wanted the profit.",
    },
};

// GIOVANNI where TEAM ROCKET is in the region and he runs one of its gyms
// (the last, RocketLeaders): the BOSS met first at the legend, or in his
// gym first. As the BOSS: intro with the gym not won yet (a hint), with it
// won (he knows the player), defeat, after with the gym not won (where to
// find him), after with it won. In his gym ({BADGE}, {TOWN}): intro after
// the BOSS was beaten, after the badge then (FireRed's "TEAM ROCKET is
// finished"), after the badge with the BOSS not met yet; and the town's
// gossip.
// Where no legend's way is his to keep, he waits at the bottom of his
// hideout himself (RogueGen_TeamBossInHideout), as in FireRed's: intro, as
// he goes, and as he goes with his gym not won.
enum
{
    GIO_BOSS_INTRO, GIO_BOSS_INTRO_WON, GIO_BOSS_AFTER, GIO_BOSS_AFTER_WON,
    GIO_GYM_INTRO, GIO_GYM_AFTER, GIO_GYM_AFTER_FIRST, GIO_GOSSIP, GIO_GOSSIP_2,
    GIO_HIDE_INTRO, GIO_HIDE_INTRO_WON, GIO_HIDE_AFTER, GIO_HIDE_AFTER_GYM, GIO_LINES
};
static const char *const sGiovanniTwist[GIO_LINES] =
{
    [GIO_BOSS_INTRO]      = "So. You are the child who beat my ADMIN under {OTHER}. I am GIOVANNI, the BOSS of TEAM ROCKET.|Perhaps you have heard my name somewhere else. It does not matter. {LEGEND} is mine.",
    [GIO_BOSS_INTRO_WON]  = "So. The child who took my {BADGE}. Did you think a GYM was all I had?|I am GIOVANNI, the BOSS of TEAM ROCKET. {LEGEND} is my business. Leave it.",
    [GIO_BOSS_AFTER]      = "Hmph. TEAM ROCKET withdraws. If you want a rematch, child, you will find me in {GYM_TOWN}. At my GYM.",
    [GIO_BOSS_AFTER_WON]  = "Twice now. ...TEAM ROCKET is finished in {REGION}. I shall go away for a while, and train alone.",
    [GIO_GYM_INTRO]       = "So! You have come this far, {PLAYER}. Yes. The LEADER of the {STEM} GYM is the BOSS of TEAM ROCKET.|This time there are no grunts to beat first. Only GIOVANNI.",
    [GIO_GYM_AFTER]       = "Having lost twice, I cannot face my followers. As of today, TEAM ROCKET is finished.|Keep that {BADGE} close. As for me, I shall dedicate myself to training again.",
    [GIO_GYM_AFTER_FIRST] = "You have earned that BADGE. Enjoy it.|We will meet again, child... somewhere less public.",
    [GIO_GOSSIP]          = "Men in black with a big red R come and go from GIOVANNI's GYM at night. Odd, isn't it?|GROUND POK@MON don't like WATER or GRASS moves.",
    [GIO_GOSSIP_2]        = "GIOVANNI is never in his GYM. People say he has another business, out of town.|WATER, GRASS and ICE moves sweep his GROUND POK@MON away.",
    [GIO_HIDE_INTRO]      = "So! I am impressed you got this far under {OTHER}. I am GIOVANNI, the BOSS of TEAM ROCKET.|Every rare POK@MON in {REGION} has a price, and I mean to collect. A child will not stop me.",
    [GIO_HIDE_INTRO_WON]  = "So. The child who took my {BADGE} has found my hideout, too. Yes. I am GIOVANNI, the BOSS of TEAM ROCKET.|You will not be so lucky twice.",
    [GIO_HIDE_AFTER]      = "...I see that you raise your POK@MON with great care. A child like you would never understand.|TEAM ROCKET leaves {OTHER}. But we will meet again, child.",
    [GIO_HIDE_AFTER_GYM]  = "...I see that you raise your POK@MON with great care.|TEAM ROCKET leaves {OTHER}. If you want another battle, child, find my GYM in {GYM_TOWN}.",
};

// The gym GIOVANNI runs where TEAM ROCKET is in the region (and so its
// BOSS is met: at a legend's or in his hideout), or 0xFF.
static u8 GiovanniGym(const struct RogueRegion *r)
{
    u8 gyms[ROGUE_NUM_GYMS], e4[ROGUE_NUM_E4], g;
    if (RogueGen_TeamWithId(r, ROGUE_VILLAIN_ROCKET) == 0xFF)
        return 0xFF;
    RogueGen_RegionLeaders(r, gyms, e4);
    for (g = 0; g < ROGUE_NUM_GYMS; g++)
        if (gyms[g] == LEADER_GIOVANNI)
            return g;
    return 0xFF;
}

// The BOSS of TEAM ROCKET is beaten (the state's, else none).
static bool8 GiovanniBossBeaten(const struct RogueTextCtx *c)
{
    u8 t = RogueGen_TeamWithId(c->r, ROGUE_VILLAIN_ROCKET);
    return t != 0xFF && c->state != NULL && ((c->state->teamsBeaten >> t) & 1);
}

// GIOVANNI's own line in his gym where the twist holds, or NULL for his
// usual ones (field RLEADER_*; again: the after line said again).
static const char *GiovanniGymLine(struct RogueTextCtx *c, u8 leader, u8 field, bool8 again)
{
    if (leader != LEADER_GIOVANNI || c->gym != GiovanniGym(c->r))
        return NULL;
    if (field == RLEADER_INTRO && GiovanniBossBeaten(c))
        return sGiovanniTwist[GIO_GYM_INTRO];
    if (field == RLEADER_AFTER && !again)
        return sGiovanniTwist[GiovanniBossBeaten(c) ? GIO_GYM_AFTER : GIO_GYM_AFTER_FIRST];
    return NULL;
}

// What a town says of GIOVANNI's gym where he is the BOSS too, or NULL.
static const char *GiovanniGossip(const struct RogueRegion *r, u8 gym)
{
    if (gym != GiovanniGym(r))
        return NULL;
    return sGiovanniTwist[GIO_GOSSIP + (RogueRng_Hash(r->seed, LEADERS_SALT + 100 + LEADER_GIOVANNI * 8 + RLEADER_GOSSIP) & 1)];
}

// The blockade's grunts (one line each), the lookout by a hideout's door,
// and what a hideout's town's people say (two while the grunts are there,
// one once they are gone; the second, where two or three teams are in the
// region, of the feud: {TEAM2} in {OTHER}).
TEXT_BANK(sAquaBlock,
    "Stop! TEAM AQUA has this road. Our ADMIN says nobody passes!",
    "No landlubbers past this point! TEAM AQUA's orders, matey. {ADMIN} said so!");
TEXT_BANK(sMagmaBlock,
    "Stop. TEAM MAGMA has closed this road. Our ADMIN's orders.",
    "Road's closed. Come back never. TEAM MAGMA thanks you for understanding.");
TEXT_BANK(sRocketBlock,
    "Stop! TEAM ROCKET has this road. Our ADMIN says nobody passes!",
    "Road's closed, kid. TEAM ROCKET business. Unless you've got a POK@MON to pay the toll?");
static const struct RogueTextBank *const sVillainBlocks[ROGUE_VILLAIN_IDS] = { &sAquaBlock, &sMagmaBlock, &sRocketBlock };
static const char *const sVillainLookout[ROGUE_VILLAIN_IDS] =
{
    "Arr, no landlubbers past this door! Unless you can hold your breath all the way down. Can you? Thought not.",
    "Visitors must book three weeks ahead. The next opening is... never. TEAM MAGMA thanks you.",
    "This isn't TEAM ROCKET's hideout! It's a, uh... warehouse. Beat it!",
};
static const char *const sVillainTownTalk[ROGUE_VILLAIN_IDS][4] =
{
    [ROGUE_VILLAIN_AQUA] = {
        "TEAM AQUA has a hideout under {TOWN}. Now their grunts block our road!",
        "Those TEAM AQUA thugs keep calling everyone matey. And sneaking in by that PRIVATE PROPERTY sign.",
        "TEAM AQUA cleared out. Our road is open again, thanks to you!",
        "TEAM AQUA is here, and {TEAM2} is over in {OTHER}. They shout at each other the whole way between!",
    },
    [ROGUE_VILLAIN_MAGMA] = {
        "TEAM MAGMA has a hideout under {TOWN}. Now their grunts block our road!",
        "TEAM MAGMA's grunts sneak in by that PRIVATE PROPERTY sign. Always on time, too.",
        "TEAM MAGMA left. They even swept up. Our road is open again, thanks to you!",
        "TEAM MAGMA under {TOWN}, {TEAM2} in {OTHER}... I hear they fought over a parking spot.",
    },
    [ROGUE_VILLAIN_ROCKET] = {
        "TEAM ROCKET has a hideout under {TOWN}. Now their grunts block our road!",
        "Men in black with a big red R keep sneaking in by that PRIVATE PROPERTY sign. One took the MEOWTH from next door!",
        "TEAM ROCKET is gone! Our road is open again, and the MEOWTH from next door came home. Thanks to you!",
        "TEAM ROCKET is here, and {TEAM2} is in {OTHER}. Nobody gets along with TEAM ROCKET. Not even other crooks.",
    },
};

// The same where the team keeps its hideout with no blockade (the main
// team where the road goes round any town it could hold, and every other
// team).
static const char *const sVillainTownTalkOpen[ROGUE_VILLAIN_IDS][4] =
{
    [ROGUE_VILLAIN_AQUA] = {
        "TEAM AQUA has a hideout under {TOWN}. Their grunts come and go at all hours, singing sea shanties!",
        "Those TEAM AQUA thugs keep calling everyone matey. And sneaking in by that PRIVATE PROPERTY sign.",
        "TEAM AQUA cleared out. {TOWN} can finally get some sleep, thanks to you!",
        "TEAM AQUA is here, and {TEAM2} is over in {OTHER}. They shout at each other the whole way between!",
    },
    [ROGUE_VILLAIN_MAGMA] = {
        "TEAM MAGMA has a hideout under {TOWN}. Their grunts march in and out like it's an office.",
        "TEAM MAGMA's grunts sneak in by that PRIVATE PROPERTY sign. Always on time, too.",
        "TEAM MAGMA left. They even swept up. {TOWN} is ours again, thanks to you!",
        "TEAM MAGMA under {TOWN}, {TEAM2} in {OTHER}... I hear they fought over a parking spot.",
    },
    [ROGUE_VILLAIN_ROCKET] = {
        "TEAM ROCKET has a hideout under {TOWN}. Their grunts come and go at night with big, wriggling sacks.",
        "Men in black with a big red R keep sneaking in by that PRIVATE PROPERTY sign. One took the MEOWTH from next door!",
        "TEAM ROCKET is gone! Even the MEOWTH from next door came home. {TOWN} owes you one!",
        "TEAM ROCKET is here, and {TEAM2} is in {OTHER}. Nobody gets along with TEAM ROCKET. Not even other crooks.",
    },
};

static const struct RogueTextBank *const sVillainBanks[ROGUE_VILLAIN_IDS][2][3] =
{
    [ROGUE_VILLAIN_AQUA] = {
        { &sAquaGruntIntro, &sAquaGruntDefeat, &sAquaGruntAfter },
        { &sAquaHideoutIntro, &sAquaHideoutDefeat, &sAquaHideoutAfter },
    },
    [ROGUE_VILLAIN_MAGMA] = {
        { &sMagmaGruntIntro, &sMagmaGruntDefeat, &sMagmaGruntAfter },
        { &sMagmaHideoutIntro, &sMagmaHideoutDefeat, &sMagmaHideoutAfter },
    },
    [ROGUE_VILLAIN_ROCKET] = {
        { &sRocketGruntIntro, &sRocketGruntDefeat, &sRocketGruntAfter },
        { &sRocketHideoutIntro, &sRocketHideoutDefeat, &sRocketHideoutAfter },
    },
};
static const struct RogueTextBank *const sVillainHideoutLast[ROGUE_VILLAIN_IDS] =
{
    &sAquaHideoutAfterLast, &sMagmaHideoutAfterLast, &sRocketHideoutAfterLast,
};
static const struct RogueTextBank *const sVillainClashBanks[ROGUE_VILLAIN_IDS][2] =
{
    [ROGUE_VILLAIN_AQUA] = { &sAquaClashIntro, &sAquaClashAfter },
    [ROGUE_VILLAIN_MAGMA] = { &sMagmaClashIntro, &sMagmaClashAfter },
    [ROGUE_VILLAIN_ROCKET] = { &sRocketClashIntro, &sRocketClashAfter },
};

// A hideout grunt's after line, k its turn among the hideout's trainers:
// every one its own, from the team's lines for the floors above the boss's
// (which may send the player down) and those for the boss's floor, each
// line said once in the hideout (taken in turn by the grunts before).
static const char *HideoutAfterLine(struct RogueTextCtx *c, u8 t, u8 id, u32 k)
{
    const struct RogueRegion *r = c->r;
    const struct RogueTextBank *up = sVillainBanks[id][1][2], *last = sVillainHideoutLast[id];
    s32 h = RogueGen_TeamHideout(r, t);
    u32 used[2] = { 0, 0 }, j;
    const char *pick = NULL;
    if (h < 0)
        return NULL;
    for (j = 0; j <= k; j++)
    {
        bool8 low = RogueGen_FloorOfTrainer(r, h, r->dungeons[h].trainerBase + j) >= r->dungeons[h].nFloors;
        const struct RogueTextBank *b = low ? last : up;
        u32 i, start = RogueRng_Hash(r->seed, LEADERS_SALT + 500 + t) % b->count;
        pick = NULL;
        for (i = 0; i < b->count && pick == NULL; i++)
        {
            u32 x = (start + i) % b->count;
            const char *line = b->lines[x];
            u32 other;
            bool8 taken = (used[low] >> x) & 1;
            // (The two banks share no line, but keep apart the same text.)
            for (other = 0; other < (low ? up : last)->count && !taken; other++)
                taken = ((used[!low] >> other) & 1) && strcmp((low ? up : last)->lines[other], line) == 0;
            if (taken || !RogueText_CanFill(line, c))
                continue;
            used[low] |= 1u << x;
            pick = line;
        }
    }
    return pick;
}

// A villain's line: role RVILLAIN_*, kind 0 intro, 1 defeat, 2 after (for
// the town's people: 0 and 1 while the grunts are there, 2 once gone, 3 of
// the feud), k the grunt's turn in their group. ctx.team is the team (0xFF:
// the main one); a grunt with ctx.team2 set quarrels with that team.
const char *RogueText_VillainLine(struct RogueTextCtx *c, u8 role, u8 kind, u32 k)
{
    const struct RogueRegion *r = c->r;
    u8 t = c->team < RogueGen_TeamCount(r) ? c->team : 0, n = RogueGen_TeamCount(r), gio;
    s32 h = RogueGen_TeamHideout(r, t), goal;
    u8 id = RogueGen_TeamId(r, t) % ROGUE_VILLAIN_IDS;
    if (h < 0)
        return NULL;
    c->team = t;
    kind %= 4;
    c->other = r->dungeons[h].host;
    goal = RogueText_TeamGoal(r, t);
    c->dungeon = goal >= 0 ? goal : 0xFF;
    // The team its leader and its town's people speak of: the next one.
    if (c->team2 >= n && n > 1 && (role == RVILLAIN_LEADER || role == RVILLAIN_TOWN))
        c->team2 = (t + 1) % n;
    switch (role)
    {
    case RVILLAIN_GRUNT:
        if (c->team2 < n && kind != 1)
            return RogueText_PickRegion(sVillainClashBanks[id][kind == 2], c, k);
        // fallthrough
    case RVILLAIN_HIDEOUT:
    case RVILLAIN_HIDEOUT_LAST:
        if (role != RVILLAIN_GRUNT && kind == 2)
            return HideoutAfterLine(c, t, id, k);
        if (role == RVILLAIN_GRUNT)
            return RogueText_PickRegion(sVillainBanks[id][0][kind % 3], c, k);
        return RogueText_Pick(sVillainBanks[id][1][kind % 3], c, k);
    case RVILLAIN_ADMIN:
    {
        const char *const *l = sVillainAdminLines[id][RogueGen_TeamAdmin(r, t)];
        if (kind == 0)
            return RogueText_CanFill(l[0], c) ? l[0] : l[1];
        if (kind == 2 && RogueGen_TeamBossInHideout(r, t))
            return sRocketAdminBeforeBoss[RogueGen_TeamAdmin(r, t)];
        if (kind == 3)
            return sAdminLeaderGone[id][RogueGen_TeamLeaderDungeon(r, t) < 0];
        return l[(kind % 3) + 1];
    }
    case RVILLAIN_LEADER:
    {
        // GIOVANNI who also runs a gym ({BADGE} and {GYM_TOWN} his), and
        // GIOVANNI at the bottom of his hideout.
        bool8 won;
        gio = id == ROGUE_VILLAIN_ROCKET ? GiovanniGym(r) : 0xFF;
        won = gio != 0xFF && c->state != NULL && ((c->state->wonGyms >> gio) & 1);
        if (gio != 0xFF)
            c->gym = gio;
        if (RogueGen_TeamBossInHideout(r, t) && kind != 1)
            return sGiovanniTwist[kind == 0 ? (won ? GIO_HIDE_INTRO_WON : GIO_HIDE_INTRO)
                                            : (won ? GIO_BOSS_AFTER_WON : gio != 0xFF ? GIO_HIDE_AFTER_GYM : GIO_HIDE_AFTER)];
        if (gio != 0xFF && kind != 1)
            return sGiovanniTwist[kind == 0 ? (won ? GIO_BOSS_INTRO_WON : GIO_BOSS_INTRO) : (won ? GIO_BOSS_AFTER_WON : GIO_BOSS_AFTER)];
    }
        if (kind == 0)
            return RogueText_CanFill(sVillainLeaderLines[id][0], c) ? sVillainLeaderLines[id][0] : sVillainLeaderLines[id][1];
        if (kind == 2 && c->team2 < n && RogueText_CanFill(sVillainLeaderLines[id][4], c))
            return sVillainLeaderLines[id][4];
        return sVillainLeaderLines[id][(kind % 3) + 1];
    case RVILLAIN_BLOCK:
        return RogueText_Pick(sVillainBlocks[id], c, k);
    case RVILLAIN_LOOKOUT:
        return sVillainLookout[id];
    case RVILLAIN_TOWN:
        if (kind == 3)
        {
            // The rival team and its town.
            if (c->team2 >= n)
                return NULL;
            c->other = r->dungeons[RogueGen_TeamHideout(r, c->team2)].host;
        }
        return (t == 0 && RogueGen_VillainBlockade(r) ? sVillainTownTalk : sVillainTownTalkOpen)[id][kind];
    }
    return NULL;
}

#ifdef ROGUE_HOST
// TextDump_Say in the context's own state (a run at that point), not the
// dump's.
static void DumpSayIn(struct TextDump *d, struct RogueTextCtx *c, const char *who, const char *bank, s32 index, const char *t)
{
    static char out[1000];
    if (t == NULL)
        return;
    RogueText_Say(out, sizeof(out), t, c);
    TextDump_Line(d, c->place, c->speaker, who, bank, index, out);
}

// The meetings with the wanderer a player who met every one has had.
static u8 DumpWandererMet(const struct RogueRegion *r)
{
    struct RogueCast cast;
    u8 met = 0, m;
    RogueGen_Cast(r, &cast);
    for (m = 0; m < 4 && cast.wandererTowns[m] != 0xFF; m++)
        met |= 1 << m;
    return met;
}

static void TextDump_Leaders(struct TextDump *d)
{
    const struct RogueRegion *r = d->r;
    u32 g;
    char who[64];
    // The gossip about each gym's leader, as the gym town's people tell it.
    for (g = 0; g < ROGUE_NUM_GYMS; g++)
    {
        struct RogueTextCtx c;
        RogueText_Init(&c, r, r->gymTown[g], RSPK_FOLK);
        c.gym = g;
        snprintf(who, sizeof(who), "gossip gym%u %s", g, RogueGen_GymLeader(r, g)->name);
        TextDump_Say(d, &c, who, "sLeaderLines", g, RogueText_LeaderGossip(r, g));
    }
    // What the region's gym leaders and Elite Four say, the gym guide and
    // the TM handover.
    {
        static const char *const fields[RLEADER_LINES] = { "intro", "defeat", "after", "gossip", "e4 intro", "e4 after" };
        u8 gyms[ROGUE_NUM_GYMS], e4[ROGUE_NUM_E4];
        u32 f;
        RogueGen_RegionLeaders(r, gyms, e4);
        for (g = 0; g < ROGUE_NUM_GYMS + ROGUE_NUM_E4; g++)
        {
            u8 leader = g < ROGUE_NUM_GYMS ? gyms[g] : e4[g - ROGUE_NUM_GYMS];
            struct RogueTextCtx c;
            for (f = 0; f < RLEADER_LINES; f++)
            {
                // A gym leader's gym lines; an Elite Four member's own, and the defeat.
                // (The gym's after lines are below, as the game says them.)
                bool8 gymLine = f == RLEADER_INTRO || f == RLEADER_DEFEAT;
                bool8 e4Line = f == RLEADER_E4_INTRO || f == RLEADER_DEFEAT || f == RLEADER_E4_AFTER;
                if (!(g < ROGUE_NUM_GYMS ? gymLine : e4Line))
                    continue;
                RogueText_Init(&c, r, g < ROGUE_NUM_GYMS ? r->gymTown[g] : r->summit, g < ROGUE_NUM_GYMS ? RSPK_LEADER : RSPK_E4);
                c.gym = g;
                snprintf(who, sizeof(who), "%s %s", gRogueGenLeaders[leader].name, fields[f]);
                TextDump_Say(d, &c, who, "sLeaderLines", leader * RLEADER_LINES + f, RogueText_LeaderLineIn(&c, leader, f));
            }
            if (g == ROGUE_NUM_GYMS + ROGUE_NUM_E4 - 1)
            {
                snprintf(who, sizeof(who), "%s last hint", gRogueGenLeaders[leader].name);
                TextDump_Say(d, &c, who, "sE4LastHint", 0, RogueText_E4LastHint(&c, DumpWandererMet(r)));
            }
            if (g >= ROGUE_NUM_GYMS)
                continue;
            RogueText_Init(&c, r, r->gymTown[g], RSPK_FOLK);
            c.gym = g;
            snprintf(who, sizeof(who), "gym guide gym%u", g);
            TextDump_Say(d, &c, who, "sGymGuide", g, RogueText_GymGuide(&c));
            RogueText_Init(&c, r, r->gymTown[g], RSPK_LEADER);
            c.gym = g;
            snprintf(who, sizeof(who), "%s tm", gRogueGenLeaders[leader].name);
            TextDump_Say(d, &c, who, "sLeaderTM", g, RogueText_LeaderTM(&c, FALSE));
            // As the badge is won (this gym and those before it), and
            // talked to again; the guide once the gym is won.
            {
                struct RogueTextState won;
                memset(&won, 0, sizeof(won));
                won.wonGyms = (2 << g) - 1;
                won.badges = g + 1;
                won.flags = RTSTATE_STARTER;
                c.state = &won;
                snprintf(who, sizeof(who), "%s after battle", gRogueGenLeaders[leader].name);
                DumpSayIn(d, &c, who, "sLeaderLines", g, RogueText_LeaderAfter(&c, leader, FALSE));
                snprintf(who, sizeof(who), "%s after again", gRogueGenLeaders[leader].name);
                DumpSayIn(d, &c, who, "sLeaderLines", g, RogueText_LeaderAfter(&c, leader, TRUE));
                RogueText_Init(&c, r, r->gymTown[g], RSPK_FOLK);
                c.gym = g;
                c.state = &won;
                snprintf(who, sizeof(who), "gym guide gym%u won", g);
                DumpSayIn(d, &c, who, "sGymGuideWon", g, RogueText_GymGuide(&c));
            }
        }
    }
    // Every line of the region's leaders, both of each, and the rival's
    // words about the fallen, for the checks (late only).
    if (d->state != NULL && d->state->badges == ROGUE_NUM_GYMS)
    {
        u8 gyms[ROGUE_NUM_GYMS], e4[ROGUE_NUM_E4];
        u32 f, v, b;
        RogueGen_RegionLeaders(r, gyms, e4);
        for (g = 0; g < ROGUE_NUM_GYMS + ROGUE_NUM_E4; g++)
        {
            u8 leader = g < ROGUE_NUM_GYMS ? gyms[g] : e4[g - ROGUE_NUM_GYMS];
            for (f = 0; f < RLEADER_LINES; f++)
                for (v = 0; v < 2; v++)
                {
                    struct RogueTextCtx c;
                    // What each seat can say: a gym's lines, an Elite Four seat's own.
                    if (g < ROGUE_NUM_GYMS ? f >= RLEADER_E4_INTRO : f != RLEADER_DEFEAT && f < RLEADER_E4_INTRO)
                        continue;
                    RogueText_Init(&c, r, g < ROGUE_NUM_GYMS ? r->gymTown[g] : r->summit, f == RLEADER_GOSSIP ? RSPK_FOLK : RSPK_NONE);
                    c.gym = g;
                    LeaderCtx(&c, leader, TRUE);
                    snprintf(who, sizeof(who), "all %s %u.%u", gRogueGenLeaders[leader].name, f, v);
                    TextDump_Say(d, &c, who, "sLeaderLines", leader * RLEADER_LINES * 2 + f * 2 + v, sLeaderLines[leader][f * 2 + v]);
                }
        }
        for (b = 1; b < 4; b++)
        {
            struct RogueTextCtx c;
            u8 place = RivalPlace(r, b);
            if (place == 0xFF)
                continue;
            RogueText_Init(&c, r, place, RSPK_NONE);
            c.other = RivalPlace(r, b - 1);
            TextDump_Bank(d, &c, "all rival fallen", sRivalFallen[b]);
        }
    }
    // The rival's four battles.
    {
        static const char *const fields[3] = { "intro", "defeat", "after" };
        u32 b, f;
        for (b = 0; b < 4; b++)
        {
            u8 place = RivalPlace(r, b);
            if (place == 0xFF)
                continue;
            for (f = 0; f < 3; f++)
            {
                struct RogueTextCtx c;
                RogueText_Init(&c, r, place, RSPK_RIVAL);
                snprintf(who, sizeof(who), "rival%u %s", b + 1, fields[f]);
                TextDump_Say(d, &c, who, "sRival", b * 3 + f, RogueText_RivalLine(&c, b, f));
            }
        }
    }
    // The wanderer's meetings on the road, met in order, and the gift at the
    // second; then the CHAMPION.
    {
        struct RogueCast cast;
        static char out[1000];
        u32 m;
        RogueGen_Cast(r, &cast);
        for (m = 0; m < 4; m++)
        {
            struct RogueTextCtx c;
            // As the game shows it: with the badges the meeting waits for.
            struct RogueTextState st;
            if (cast.wandererTowns[m] == 0xFF)
                break;
            if (d->state != NULL)
                st = *d->state;
            else
                memset(&st, 0, sizeof(st));
            st.flags |= RTSTATE_STARTER;
            if (st.badges < cast.wandererBadges[m])
                st.badges = cast.wandererBadges[m];
            RogueText_Init(&c, r, cast.wandererTowns[m], RSPK_WANDERER);
            c.state = &st;
            RogueText_Wanderer(&c, m, RWANDER_TALK, (1 << m) - 1, m == 1, out, sizeof(out));
            snprintf(who, sizeof(who), "wanderer meeting%u", m + 1);
            TextDump_Line(d, c.place, RSPK_WANDERER, who, "sWanderer", m, out);
            if (m == 1)
            {
                RogueText_Wanderer(&c, m, RWANDER_BYE, TRUE, FALSE, out, sizeof(out));
                TextDump_Line(d, c.place, RSPK_WANDERER, "wanderer after gift", "sWandererBye", m, out);
            }
            RogueText_Wanderer(&c, m, RWANDER_REPEAT, TRUE, FALSE, out, sizeof(out));
            snprintf(who, sizeof(who), "wanderer meeting%u again", m + 1);
            TextDump_Line(d, c.place, RSPK_WANDERER, who, "sWandererRepeat", m, out);
            RogueText_Init(&c, r, cast.wandererTowns[m], RSPK_FOLK);
            c.state = &st;
            snprintf(who, sizeof(who), "wanderer pointer meeting%u", m + 1);
            DumpSayIn(d, &c, who, "sWandererPointer", m, RogueText_WandererPointer(&c, m, m > 0));
        }
        for (m = 0; m < RCHAMP_LINES; m++)
        {
            static const char *const fields[RCHAMP_LINES] = { "intro", "defeat", "after", "hall of fame" };
            struct RogueTextCtx c;
            RogueText_Init(&c, r, r->summit, RSPK_CHAMPION);
            snprintf(who, sizeof(who), "champion %s %s", RogueGen_WandererName(&cast), fields[m]);
            TextDump_Say(d, &c, who, "sChampionLines", m, RogueText_ChampionLine(&c, m, DumpWandererMet(r)));
        }
        // For the checks (late only): what they say to a player who met
        // them once, and one who never did.
        if (d->state != NULL && d->state->badges == ROGUE_NUM_GYMS)
        {
            for (m = 0; m < RCHAMP_LINES * 2; m++)
            {
                struct RogueTextCtx c;
                RogueText_Init(&c, r, r->summit, RSPK_NONE);
                snprintf(who, sizeof(who), "all champion %u met %u", m % RCHAMP_LINES, m / RCHAMP_LINES);
                TextDump_Say(d, &c, who, "sChampionLines", m, RogueText_ChampionLine(&c, m % RCHAMP_LINES, m / RCHAMP_LINES));
            }
            {
                struct RogueTextCtx c;
                RogueText_Init(&c, r, r->summit, RSPK_NONE);
                TextDump_Bank(d, &c, "all e4 hint", &sE4LastHint);
                TextDump_Bank(d, &c, "all e4 hint met", &sE4LastHintMet);
            }
        }
    }
    // The villain teams: each team's route grunts (and the other team's
    // grunt on its route, both quarrelling), hideout grunts, the admin, the
    // leader (GIOVANNI as the BOSS before and after his gym is won, where
    // he runs one), the main team's blockade, each lookout and each
    // hideout town's people (the feud too); GIOVANNI's gym lines.
    {
        struct RogueTeams tm;
        u32 i, k, f, t;
        static const char *const kinds[3] = { "intro", "defeat", "after" };
        struct RogueTextCtx c;
        u8 gio = GiovanniGym(r);
        RogueGen_Teams(r, &tm);
        for (t = 0; t < tm.n; t++)
        {
            const struct RogueDungeon *dg = &r->dungeons[tm.hideout[t]];
            const char *team = gRogueTeamNames[tm.id[t] % ROGUE_VILLAIN_IDS];
            if (tm.hint[t] != 0xFF)
            {
                u8 bits = tm.mask[t] | tm.guest[t], own = 0;
                for (i = 0; i < 8; i++)
                {
                    u8 owner = ((tm.guest[t] >> i) & 1) ? t - 1 : t;
                    if (!((bits >> i) & 1))
                        continue;
                    // (As the game counts them, VillainTurn: a team's own
                    // grunts in order, its grunt on another team's route
                    // after all of them.)
                    k = owner == t ? own++ : (u32)__builtin_popcount(tm.mask[owner]);
                    for (f = 0; f < 3; f++)
                    {
                        RogueText_Init(&c, r, tm.hint[t], RSPK_VILLAIN);
                        c.team = owner;
                        if (tm.guest[t])
                            c.team2 = owner == t ? t - 1 : t;
                        snprintf(who, sizeof(who), "%s route grunt%u%s %s", gRogueTeamNames[tm.id[owner] % ROGUE_VILLAIN_IDS], k + 1,
                                 tm.guest[t] ? " (clash)" : "", kinds[f]);
                        TextDump_Say(d, &c, who, tm.guest[t] && f != 1 ? "sVillainClash" : "sVillainGrunt", f,
                                     RogueText_VillainLine(&c, RVILLAIN_GRUNT, f, k));
                    }
                }
            }
            for (k = 0; k < dg->nTrainers; k++)
            {
                for (f = 0; f < 3; f++)
                {
                    RogueText_Init(&c, r, dg->host, RSPK_VILLAIN);
                    c.team = t;
                    snprintf(who, sizeof(who), "%s hideout grunt%u %s", team, k + 1, kinds[f]);
                    TextDump_Say(d, &c, who, "sVillainHideout", f, RogueText_VillainLine(&c,
                                 RogueGen_FloorOfTrainer(r, tm.hideout[t], dg->trainerBase + k) >= dg->nFloors ? RVILLAIN_HIDEOUT_LAST : RVILLAIN_HIDEOUT, f, k));
                }
            }
            for (f = 0; f < 3; f++)
            {
                RogueText_Init(&c, r, dg->host, RSPK_VILLAIN);
                c.team = t;
                snprintf(who, sizeof(who), "%s admin %s %s", team, RogueGen_TeamAdminName(r, t), kinds[f]);
                TextDump_Say(d, &c, who, "sVillainAdminLines", f, RogueText_VillainLine(&c, RVILLAIN_ADMIN, f, 0));
            }
            if (!RogueGen_TeamBossInHideout(r, t))
            {
                RogueText_Init(&c, r, dg->host, RSPK_VILLAIN);
                c.team = t;
                snprintf(who, sizeof(who), "%s admin %s as the team leaves", team, RogueGen_TeamAdminName(r, t));
                TextDump_Say(d, &c, who, "sAdminLeaderGone", 0, RogueText_VillainLine(&c, RVILLAIN_ADMIN, 3, 0));
            }
            RogueText_Init(&c, r, dg->host, RSPK_SIGN);
            c.team = t;
            snprintf(who, sizeof(who), "%s hideout cleared", team);
            TextDump_Say(d, &c, who, "sHideoutLeft", 0, RogueText_HideoutLeft(&c));
            if (RogueGen_TeamLeaderDungeon(r, t) >= 0 || RogueGen_TeamBossInHideout(r, t))
            {
                u32 won;
                u8 at = RogueGen_TeamLeaderDungeon(r, t) >= 0 ? r->dungeons[RogueGen_TeamLeaderDungeon(r, t)].host : dg->host;
                for (won = 0; won < (tm.id[t] == ROGUE_VILLAIN_ROCKET && gio != 0xFF ? 2u : 1u); won++)
                {
                    struct RogueTextState st;
                    memset(&st, 0, sizeof(st));
                    if (won)
                        st.wonGyms = 1 << gio, st.badges = 1;
                    for (f = 0; f < 3; f++)
                    {
                        RogueText_Init(&c, r, at, RSPK_VILLAIN);
                        c.team = t;
                        c.state = gio != 0xFF && tm.id[t] == ROGUE_VILLAIN_ROCKET ? &st : NULL;
                        snprintf(who, sizeof(who), "%s leader %s%s %s%s", team, gRogueTeamBosses[tm.id[t] % ROGUE_VILLAIN_IDS],
                                 RogueGen_TeamBossInHideout(r, t) ? " (in the hideout)" : "", kinds[f],
                                 gio == 0xFF || tm.id[t] != ROGUE_VILLAIN_ROCKET ? "" : won ? " (his GYM won)" : " (his GYM not won)");
                        DumpSayIn(d, &c, who, "sVillainLeaderLines", f, RogueText_VillainLine(&c, RVILLAIN_LEADER, f, 0));
                    }
                }
            }
            for (k = 0; k < 2 && t == 0 && RogueGen_VillainBlockade(r); k++)
            {
                RogueText_Init(&c, r, dg->host, RSPK_VILLAIN);
                c.team = t;
                snprintf(who, sizeof(who), "%s blockade grunt%u", team, k + 1);
                TextDump_Say(d, &c, who, "sVillainBlock", k, RogueText_VillainLine(&c, RVILLAIN_BLOCK, 0, k));
            }
            RogueText_Init(&c, r, dg->host, RSPK_VILLAIN);
            c.team = t;
            snprintf(who, sizeof(who), "%s lookout", team);
            TextDump_Say(d, &c, who, "sVillainLookout", 0, RogueText_VillainLine(&c, RVILLAIN_LOOKOUT, 0, 0));
            for (k = 0; k < 4; k++)
            {
                // As the game says them (Rogue_BufferVillainTownTalk): the
                // second person tells of the feud where there is one.
                if ((k == 1 && tm.n > 1) || (k == 3 && tm.n < 2))
                    continue;
                RogueText_Init(&c, r, dg->host, RSPK_FOLK);
                c.team = t;
                snprintf(who, sizeof(who), "%s hideout town folk%u%s", team, k == 3 ? 2 : k + 1, k == 2 ? " (cleared)" : k == 3 ? " (the feud)" : "");
                TextDump_Say(d, &c, who, "sVillainTownTalk", k, RogueText_VillainLine(&c, RVILLAIN_TOWN, k, 0));
            }
        }
        // GIOVANNI in his gym, as the BOSS of TEAM ROCKET too: before the
        // BOSS is beaten his usual lines (above), after it these.
        if (gio != 0xFF)
        {
            struct RogueTextState st;
            u8 rt = RogueGen_TeamWithId(r, ROGUE_VILLAIN_ROCKET);
            memset(&st, 0, sizeof(st));
            st.teamsBeaten = 1 << rt;
            for (f = 0; f < 2; f++)
            {
                RogueText_Init(&c, r, r->gymTown[gio], RSPK_LEADER);
                c.gym = gio;
                c.state = &st;
                snprintf(who, sizeof(who), "gym%u GIOVANNI %s (BOSS beaten)", gio, f == 0 ? "intro" : "after");
                DumpSayIn(d, &c, who, "sGiovanniTwist", f, GiovanniGymLine(&c, LEADER_GIOVANNI, f == 0 ? RLEADER_INTRO : RLEADER_AFTER, FALSE));
            }
            memset(&st, 0, sizeof(st));
            RogueText_Init(&c, r, r->gymTown[gio], RSPK_LEADER);
            c.gym = gio;
            c.state = &st;
            snprintf(who, sizeof(who), "gym%u GIOVANNI after (BOSS not met yet)", gio);
            DumpSayIn(d, &c, who, "sGiovanniTwist", 2, GiovanniGymLine(&c, LEADER_GIOVANNI, RLEADER_AFTER, FALSE));
        }
    }
}
#endif
