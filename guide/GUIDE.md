# Roguémon Player Guide

Version 1.1.6-beta

## What Roguémon is

Roguémon is a Pokémon Emerald romhack built on pokeemerald-expansion. Every run grows a brand new region: its towns, routes, caves, gym leaders, wild Pokémon, trainers and items. You play each run as a Nuzlocke. You catch one Pokémon per area, fainted Pokémon are gone, and if your whole party faints, the run is over.

It is meant to feel like a real Pokémon game, not a string of battle rooms. You walk routes, explore caves, win eight badges, climb Victory Road and face the Elite Four and the Champion. When a run ends, win or lose, you keep what you earned toward the next one: unlocks, feats and a record of every legend you caught.

## Installing

1. Get your own **Pokémon Emerald (USA, Europe)** ROM, dumped from your cartridge. It must be the clean English ROM: 16 MiB, SHA-1 `f3ae088181bf583e55daf962a92bb46f4f1d07b7`.
2. Apply the Roguémon **.bps** patch to it. The easiest way, on a computer or a phone, is Rom Patcher JS in a web browser: pick the ROM and the patch, press **Apply patch**, and save the result as `Roguemon.gba`. Floating IPS (Flips) works too.
3. Open the patched ROM in your emulator. mGBA 0.10 is tested on computers, and RetroArch with the mGBA core on Android. If an emulator asks for a save type, pick **Flash 128K**.

The patch only works on that exact ROM, and the patcher refuses any other. A trimmed, already patched or non-English ROM will not work. The README that comes with the patch has more detail.

To update to a new version, patch a clean Emerald ROM with the new patch and give your save file the same name as the new ROM. Save in the game from the START menu: emulator save states do not carry over between versions.

## Starting a run

### The title menu

- **NEW RUN** starts a new region. If a run is saved, the game warns you first: starting a new run ends the saved one.
  Press left or right on it to turn it into **DAILY REGION**: one region a day, the same for everyone who plays that day, with the CLASSIC POKéDEX mix. Prof. Birch tells you the date. It uses your console's or emulator's clock, so without a clock the row stays NEW RUN.
- **CONTINUE RUN** appears while a run is in progress.
- **UNLOCKS** appears once you have played a run. It shows your unlocks, feats and legends (see "The record and unlocks").
- **OPTION** works as in Emerald.

Beside NEW RUN, the menu shows how many runs and wins you have.

### Prof. Birch's region map

On your first run, Prof. Birch explains the game and asks your name. Then he shows you the region he found, on its map. He tells you its name, what the land looks like, a fact about it, and a rumor about the first gym leader's type. Then he asks "Will you explore it?":

- **YES** starts the run in that region.
- **NO** grows a different region and shows it.
- **TYPE NAME** lets you type a region's name, or a friend's run code, and go there.

Pressing B on this screen does nothing, so mashing B through his text will not skip a region by accident.

On later runs, Birch skips the long speech. He recaps your last run and shows you the next region.

### Region names

A region's name is its seed. There are 16,384 regions, each named with three syllables (like KALONA or TOVASSA). Typing any word with TYPE NAME leads to some region too.

The same name always gives the same land, towns, trainers, leaders and items. The wild Pokémon also depend on your POKéDEX mix (see "The POKéDEX mix"). To share a run with a friend, give them its run code. When a run ends, the run page of the summary shows it, like `Run code: KALONA-N`: the region's name and a letter for the POKéDEX mix (C for CLASSIC, N for NATIONAL, G for GLOBAL). Typed with TYPE NAME, the code gives the same region with the same mix, whatever mix your friend picked. Prof. Birch will also remind you of the name and mix if you talk to him in his lab.

### The lab and your starter

The run begins in Prof. Birch's lab in the first town. He welcomes you to the region and tells you any news: new unlocks, a rare find, how many legends live here, and the Hardcore rules if they are on.

Then you pick a starter. Each row in the menu is a kind of starter. The region rolls its own Grass, Fire and Water starters, so they change from run to run. Unlocks add more rows (partners, dragons, babies and more). When a row shows `< 1/9 >`, press left and right to see the other choices. Birch asks you to confirm, then offers a nickname.

Every run starts with:

- 5 Poké Balls
- 5 Potions
- an Old Rod
- Running Shoes and a National POKéDEX

Unlocks add more to this kit.

You cannot leave the first town until you have a starter. Talk to Birch again any time to hear where the first gym is and the region's name to give a friend.

## Nuzlocke rules

The game enforces these for you.

- **One catch per area.** You may only catch the first wild Pokémon you meet in each area. The battle tells you when it starts. You cannot throw a ball at later ones. The area is used as soon as that first battle starts, so if it flees, faints or you run away, the area is still used.
- **Dupes clause.** If you meet a Pokémon you have already caught, it does not use up the area. The battle tells you ("You already have this POKéMON"). You cannot catch it, but the area stays open for the next encounter.
- **Guardians and legends.** A dungeon's guardian or legend can always be caught. It never uses up an area.
- **Shiny clause** (an unlock). With it, you can always catch a shiny, even in a used area or as a dupe, and it does not use up the area.
- **Fainted means gone.** A Pokémon that faints leaves your party when the battle ends. Its held item goes back to your bag.
- **A party wipe ends the run.** If your whole party faints, the game saves the loss, shows the run summary and returns to the title. That run cannot be continued.

**What counts as an area:** each route, each cave, and each town (for fishing and surfing there). The floors of a dungeon share one area.

**Good to know:**

- Your own Pokémon always obey you, whatever their level and your badges.
- If your party is full, a new catch or gift goes to a BOX. Use the PC in any POKéMON CENTER.
- Open START > MAP to see each area's catch: "Can catch here", "Caught here" or "Encounter used".

## How a region is laid out

### The main road

Every region follows the shape of a real Pokémon game:

1. Three or four routes lead to the first gym, usually with a village between them. The first route is gentle.
2. Eight gym towns follow, with one to three places between each pair.
3. A cave on the main road before the fourth gym. The way through goes down a ladder, across the floor below and back up.
4. The sea after the fifth gym. You cross it with Surf.
5. Victory Road, a cave that leads up to the summit.
6. The Pokémon League at the summit: a lobby with a nurse and a shop, then the four Elite Four rooms and the Champion, one after another.

Winning shows the Hall of Fame and then a summary of your run.

### Chapters and badges

The eight gyms come in **chapters** of one to three gyms. You can win the gyms of a chapter in any order. A chapter ends at a block on the main road: small trees, a cracked rock, a boulder, or the sea.

- Which badge gives Cut, Rock Smash, Strength and Surf is **rolled per region**. The leader tells you what their badge lets you do.
- **A badge alone clears the way.** You do not need a Pokémon that knows the move. Walk up to the tree, rock, boulder or water and press A.
- The badge that clears a chapter's block always comes from one of that chapter's gyms.
- Some gyms give no field move. Their badges still count for the League.
- Some field moves come with a number of badges, whichever gyms they are: Flash with 2, Fly with 6, Dive with 7 and Waterfall with 8.
- The gate to Victory Road needs all eight badges.

**Fly:** with six badges, open START > MAP and press A on a town you have visited (shown in pink) to fly there. You do not need a Pokémon that knows Fly.

### Off the main road

The main road is only part of the region. Look for:

- **Side paths**, one or two routes deep, behind a small tree, a cracked rock, a boulder or water. They end in a hamlet, a dark cave, or a clearing with the best items.
- **Shortcuts** back from a later stretch to an earlier town, and **forks** that give a second way between two places.
- **Loops** like tunnels and ring roads that join distant parts of the region. Townsfolk near a tunnel tell you where it comes out and what blocks it.
- **Ways that open later.** Every region has at least one path that needs a badge you win later, so it pays to come back.
- **Dark caves.** Some side caves stay dark until you have two badges.

Some routes have a **rest house** where you can heal for free.

### The region map

Open START > MAP. It fits the whole region on one screen. Move the cursor with the D-pad, or press L and R to jump between towns. The strip at the bottom tells you:

- what a place is, and what its way in needs ("Needs STRENGTH")
- whether you can still catch there
- which gyms you can take on now (they blink red)
- the badge you won at a gym and the field move it gave
- what dungeon a place holds once you have been there or heard of it, how many floors it has, and "Cleared!" once its boss is beaten
- "A legend waits." where you have been told of a legend

The map never names gym leaders, their types or the Champion.

### Dungeons

Every cave has floors below it. Most regions also have other dungeons: forests, towers, a villain team's hideout, legends' lairs, and sometimes ruins, ice caves, a volcano, a shipwreck or a power plant. Each kind has its own wild Pokémon, and its floors get a little stronger as you go down (or up).

Inside you may find:

- **A guardian:** a strong, boosted Pokémon at the bottom. You can always catch it, and it does not use up the area.
- **A master:** a tough trainer (a Hiker, Cool Trainer, Bug Maniac or Hex Maniac). Talk to them to battle. Beat them for a TM or a good held item.
- **A prize ball** beside the boss, with the dungeon's best item.
- **A choice room:** two or three balls in a row. You may take only one, and the others roll away.
- **A camper** halfway down deep dungeons who heals your team once.
- **Someone lost** who asks you to lead them out. Say yes and they give you a reward and take you straight back to the entrance, so you may want to finish the dungeon first.

A dungeon counts as cleared once you beat or catch its boss.

### Legends

Every region has some legendary Pokémon, usually three to five, and they come in sets as in the real games (the Regis, the birds, the beasts and so on). Each waits in a chamber at the end of its own dungeon.

Each chamber is sealed until you open its way. The seal tells you what it wants and where to look. Ways to open one include:

- a badge (for Rock Smash or Strength)
- an old statue that needs Fly (6 badges), Dive (7) or Waterfall (8)
- a ticket a gym leader gives you with their badge
- a key found deep in another dungeon (like an orb)
- beating the villain team's leader, who waits in front of it
- meeting the rest of its set first

Townsfolk, signs, books and Prof. Birch all drop hints about where legends live and how to reach them.

A legend's level is set when you meet it, from the badges you hold, so it is never far behind your team. Legends have their real, low catch rates. Their dungeons hold ten balls for them (Dusk Balls underground, Timer Balls in the open air). Putting a legend to sleep helps a lot. Once caught or beaten, a legend is gone for the run.

### People

- **Your rival** is Prof. Birch's kid (May or Brendan, whichever you are not). They battle you four times, the last at the foot of Victory Road. After the second battle they give you a DOWSING MACHINE for hidden items.
- **Gym leaders** come from a pool of 32: Roguémon's own eight leaders, Emerald's gym leaders and Elite Four, and FireRed and LeafGreen's gym leaders and Elite Four. Each run picks twelve with different types: eight lead gyms and four sit in the Elite Four.
- **Villain teams:** a region has one, two or three of TEAM AQUA, TEAM MAGMA and TEAM ROCKET. The main team blocks a town on the main road. Clear their hideout under that town and beat their admin to open the way.
- **A traveler** waits by POKéMON CENTER doors in some towns. Talk to them. On your second meeting they give you a gift.
- **Houses:** you can enter every house. The first house in each town gives a gift once per run. Look for a Move Tutor, the Name Rater and a Move Deleter too.

## Levels and difficulty

There are no level caps in a normal run.

- Each region rolls its own level curve around Emerald's. Gym aces are near 15, 19, 24, 29, 31, 33, 42 and 46. The Elite Four are near 49 to 55, and the Champion near 58.
- **Places keep their level.** Routes, caves and their trainers do not scale to you. If you run ahead through a shortcut into a later stretch, the trainers there are tougher.
- **Gyms scale to your badges.** A leader keeps their town and type, but fights at the strength of the badges you hold when you walk in. The first gym you fight is always a first gym's fight, wherever it is. The second is a second gym's fight, and so on.
- Some later leaders fight **double battles**.
- **Optional dungeons** follow the badges you hold when you enter, so they stay a fair fight whenever you visit.
- Wild Pokémon sit a few levels below the trainers around them, and further below late in the run.
- **Your whole party shares EXP** from every battle, so your bench keeps up.

Battles use pokeemerald-expansion's modern rules and moves, including the Fairy type.

### Hardcore rules

Once you become Champion, you can turn on the **HARDCORE RULES** in UNLOCKS. They are off until you switch them on, and they hold for the whole run.

- **Level cap:** no Pokémon can gain EXP or use a Rare Candy past the next gym leader's ace level. After eight badges, the cap follows each Elite Four member's ace and then the Champion's.
- **No items in battle.** You can still throw Poké Balls in wild battles.
- **SET battle style** only.
- Marts sell battle berries instead of X items.

## Items, shops and healing

Most things work as in Emerald. These are the differences that may surprise you:

- **No Revives.** They are never found or sold, since a fainted Pokémon is gone anyway.
- **Trade evolutions use the item from the bag.** There is no one to trade with, so use a Metal Coat, Dragon Scale, Up-Grade or Linking Cord from the bag to evolve.
- **Marts grow with your badges.** From the fifth badge they sell Timer Balls and the plain evolution stones (Fire, Water, Thunder, Leaf, Moon and Sun). From the seventh badge they sell Dusk and Quick Balls. No Mart sells Ethers or Elixirs, but you can find some late in the run.
- **Specialty counters.** Some gym cities have a second clerk with a specialty: X items, vitamins, evolution stones, special balls, TMs, held items, herbs or type items.
- **Gym leaders** give a TM of their type with the badge.
- **Held items are rare.** A run hands out at most one of each held item. Rare Candies start after the second badge, and Full Restores after the seventh.
- **Hidden items** lie on every route, at dead ends, on beaches and in caves. Use the DOWSING MACHINE your rival gives you.
- **Healing:** POKéMON CENTERS heal for free as usual. Rest houses on some routes heal too. Campers in deep dungeons and on Victory Road heal your team once.

## The record and unlocks

Your runs, wins, best result, unlocks, feats and the LEGEND LOG are kept in your save file and carry into every new run. The back of your trainer card shows your record.

When you earn an unlock, the game tells you on the spot. At the start of your next run, Prof. Birch tells you the news. Runs count from when they start, so an unlock for "start three runs" applies to your third run.

### The UNLOCKS screen

Choose UNLOCKS on the title menu. It has three pages: UNLOCKS, FEATS and LEGENDS. Switch pages with left and right, or L and R.

- Earned unlocks show their name and ON or OFF. Locked ones show "???".
- The box below says what an unlock does, or how to earn it.
- Press A to switch an earned unlock on or off. Switching one off never loses it.

### Every unlock

| Unlock | How to earn it | What it does |
| --- | --- | --- |
| PARTNER STARTERS | Win a gym badge | A partner row in the starter menu (like Pikachu, Riolu or Zorua) |
| EEVEE | Start a second run | An Eevee row in the starter menu |
| BABY POKéMON | Start a second run | A baby Pokémon row (like Pichu, Azurill or Munchlax) |
| VETERAN'S COIN | Start three runs | An Amulet Coin in your starting kit |
| RISING STARS | Start three runs | A row with a Pokémon that grows in three stages (like Aron or Mareep) |
| STARTER CASE | Start four runs | Left and right on the Grass, Fire and Water rows: any of the 27 starters |
| FROM FAR AWAY | Start six runs | A regional form row (like Alolan Vulpix) |
| FREE CHOICE | Start eight runs | Left and right on any row: every Pokémon of that kind |
| POKéDEX PICK | Start ten runs | Any first-stage Pokémon you ever caught, shiny if you caught it shiny |
| DRAGON STARTERS | Win three badges in one run | A young dragon row (like Dratini or Bagon) |
| TRAINER'S CANDY | Win four badges in one run | The 4th, 5th and 6th gym leaders also give 3 Rare Candies (from your next run) |
| ROADSIDE CAMP | Win five badges in one run | A camper on the road to the 7th gym heals your team once (from your next run) |
| TYPE CHARM | Win 2 badges with your starter still in the party | Starters of that type hold their type's item (like Charcoal) |
| HIDDEN ABILITY | Win 4 badges with your starter still in the party | Starters of that type have their hidden ability |
| EGG MOVE | Win 8 badges with your starter still in the party | Starters of that type know an egg move |
| GYM SCOUTING | Lose to a gym leader | Gym guides tell you the leader's team and the ace's level |
| BERRY POUCH | Lose two runs | Oran and Sitrus Berries in your starting kit |
| SPARE STARTERS | Lose three runs | The starters you did not pick wait in the 1st and 4th gym towns |
| LUCKY EGG | Lose four runs | A Lucky Egg in your starting kit |
| SECOND CHANCE | Lose five runs | From the 4th badge, up to 3 areas whose first Pokémon got away stay open for another try |
| FALLEN FRIEND | Lose a run with 4 badges or more | Your longest companion from that run is a starter choice |
| FOSSILS | Explore five caves | A fossil hides in the cave before the 4th gym, and a scientist in the 4th gym town revives it |
| FOSSIL STARTERS | Revive a fossil | A fossil Pokémon row in the starter menu |
| RARE FINDS | Catch 50 Pokémon | A rare Pokémon lives off the main road. Prof. Birch hints where |
| SHINY CLAUSE | Meet a shiny Pokémon | You can always catch a shiny |
| SHINY CHARM | Meet three shiny Pokémon | A Shiny Charm in your kit: shinies appear three times as often |
| RIVAL'S GIFT | Beat your rival ten times | Your rival gives you a strong held item at the foot of Victory Road |
| EXPLORER'S KIT | Clear five dungeons | 3 Escape Ropes and a Max Repel in your starting kit |
| MARKING MAP | Catch three different legends | The region map shows where each legend waits, and which one |
| MASTER BALL | Catch ten different legends | A Master Ball in your starting kit |
| DOWSING MACHINE | Clear every dungeon with a boss in one run, then win it | A Dowsing Machine in your starting kit |
| CHAMPION'S KIT | Become Champion | Great Balls and Super Potions in your starting kit |
| CHAMPION'S PARTNER | Become Champion | Your winning lead's kind as a starter choice |
| HARDCORE RULES | Become Champion | Level caps, no items in battle, SET style (earned switched off) |
| NATIONAL DEX | Become Champion | The NATIONAL POKéDEX mix can be picked |
| ANY POKéMON | Become Champion three times | Any first-stage Pokémon as a starter, plus the legends you have caught |
| GLOBAL DEX | Become Champion three times | The GLOBAL POKéDEX mix can be picked |

Counts like "lose two runs" and "clear five dungeons" add up across all your runs. Counts that say "in one run" must happen in a single run.

The starter upgrades (TYPE CHARM, HIDDEN ABILITY and EGG MOVE) are earned per type. They count badges won while a starter of that type is still in your party. The UNLOCKS screen shows which types have earned each one.

### The POKéDEX mix

The POKéDEX mix decides which generations the wild Pokémon, trainers, gym leaders and Elite Four are drawn from. Starters, legends and other fixed Pokémon do not change.

| Mix | How to get it | What you meet |
| --- | --- | --- |
| CLASSIC | The default | Mostly Kanto, Johto and Hoenn (about 65% Gens 1 to 3, 15% Gen 4, 20% Gens 5 to 9) |
| NATIONAL | Become Champion | More from Sinnoh, Unova and Kalos |
| GLOBAL | Become Champion three times | All nine generations, an equal share each |

Pick the mix on the POKéDEX row of the UNLOCKS screen. Press A to step through the ones you have earned. The mix is fixed for the whole run once it starts. CLASSIC stays until you change it.

### Feats

Feats are goals that last past a win. They are on the second page of the UNLOCKS screen, with how far along you are.

| Feat | How | Reward |
| --- | --- | --- |
| CHAMPION | Beat the Champion | A trainer card star |
| HARDCORE | Win under the Hardcore rules | A trainer card star |
| GRASS / FIRE / WATER STARTER | Win with a starter of that type still in your party | All three: a trainer card star |
| NOBODY LEFT BEHIND | Win without losing a Pokémon | |
| EVERY DUNGEON | Clear every dungeon with a boss, then win | Dowsing Machine |
| FIRST LEGEND | Catch a legend | |
| LEGEND TRACKER | Catch three different legends | Marking Map |
| A WHOLE SET | Catch every legend of a set | |
| LEGEND HUNTER | Catch ten different legends | Master Ball and a trainer card star |
| LEGEND LOG | Catch all 29 legends | |

None of this makes a first win any easier. The rewards come after a win or a long collection.

### The LEGEND LOG

The third page of the UNLOCKS screen records every legend you have met and caught across all your runs: 29 legends in sets (the Regis, the birds, the beasts, the Lake guardians, the Swords of Justice, the Forces of Nature, the Eon duo, the tower duo, the weather trio, and the lone legends). A caught legend's icon moves. One you have only met is faded. One you have never met is "?". The legends of your current region sit on light blue cells.

### A secret

There is one more mode, hidden at first. When something new appears on the title menu, read the RUN 0 guide that came with the patch. Its first part has no spoilers.

## Tips for winning runs

**Before the first gym**

- Listen to Prof. Birch's rumor about the first gym leader's type, before you even say YES.
- Before each gym on the main road, at least one wild Pokémon has a type that hits that leader hard. Before the first two gyms, it is one of the common ones.
- The first two gyms are never Ice, Dragon, Steel, Ghost, Dark or Psychic.
- Catch in every area before the first gym. A region has 3 to 6 catch areas before it, counting towns you can fish in.

**Gyms**

- Gym guides and townsfolk tell you the leader's type. Townsfolk often gossip about what beats them.
- Within a chapter, gyms fight at the strength of the badges you hold, not by where they stand. Take on the one your team handles best first.
- Every leader in a run has a different type, and the Elite Four's four types never repeat a gym's.
- Leaders and the Elite Four send out their ace last.

**Your rival**

- Your rival picks the starter that beats yours. Plan a Pokémon that handles it.
- At the foot of Victory Road, your rival also brings a Pokémon chosen to beat your starter.

**Exploring**

- Side paths have the best items. Come back for the ones behind badges you win later.
- Dungeons have their own wild Pokémon and their own catch area. A cave is a second chance to find a good catch.
- Guardians and legends never use up an area, so catch them when you can.
- Your bench gets EXP too, so keep a varied team.
- Places keep their level. A shortcut into a later stretch is a risk.
- Choice rooms hold held items, evolution items or TMs. Pick what your team needs most.

**Staying alive**

- The area is used the moment the first battle starts. Think before you knock out or run from a first encounter.
- Victory Road has a camper who heals you once, just past your rival. Rest there before the trainers.
- Heal and shop in the League lobby. The Elite Four and the Champion come one after another.
- Use SPARE STARTERS, SECOND CHANCE and BERRY POUCH. They come from losses, so a lost run is never wasted.

## FAQ

**How do I save?**
Save from the START menu as in Emerald. Choose CONTINUE RUN on the title menu to pick up where you left off.

**What happens when my party wipes?**
The game saves the loss right away and shows the run summary. That run is over. Your record, unlocks, feats and LEGEND LOG are kept.

**Can I start a new region in the middle of a run?**
Yes. Choose NEW RUN. The game warns you first, because starting a new run ends the saved one.

**Can I replay a region I liked?**
Yes. Type its run code (like KALONA-N) with TYPE NAME on Prof. Birch's map. A plain name works too, with the POKéDEX mix you picked in UNLOCKS.

**I earned an unlock. When does it apply?**
Most apply from your next run. Prof. Birch tells you about them at the start. You can switch any earned unlock off in UNLOCKS.

**Do I need HMs?**
No. A badge alone lets you clear trees, rocks, boulders and water. With six badges you can fly from the region map.
