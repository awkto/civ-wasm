# Civilization (1991, DOS) — rules as implemented in civ-wasm

Source-verified reference for an LLM advisor or AI player. Every number below was read from the
decompiled game code in `src/game/*.C`; where it differs from the printed manual or common
belief, the source wins and the difference is noted. Unit and improvement costs in the tables are
shown in shields as the human player pays them (the tables store tenths; human cost = value x 10).
"Human" means the player civilization (`civnow`); "AI" means computer civilizations, which run on
partly different rules (listed in section 17).

Difficulty levels (`cur_tier`): 0 Chieftain, 1 Warlord, 2 Prince, 3 King, 4 Emperor.
Governments (`civ_gov`): 0 Anarchy, 1 Despotism, 2 Monarchy, 3 Communism, 4 Republic, 5 Democracy.
Map: 80 x 50 tiles, x wraps. Distance = max(dx,dy) + min(dx,dy)/2, integer (CORE.C, map_dist).

---

## 1. Turn order

Per game turn (TURN.C, game_turn): for each civ in index order 0..7 (barbarians are civ 0):
1. `civ_economy`: every city of that civ is processed (`city_process(city, 0)`), in city-slot order.
2. Gold is capped at 30000.
3. `city_phase`: Anarchy may end, peace/contact timers, AI requests.
4. Unit phase; then that civ moves (for the human this is when you play: move units, change
   rates, buy, sell). Changes you make take effect at the start of your next turn's city processing.

After all civs: barbarian uprisings, AI events, one random-city disaster roll (after turn 50), score,
the "taxes" warning (shown when you have more than 1 city, gold < 100 and gold would reach 0 within
about 10 turns at the current loss), then the year advances (20 yrs/turn to 1000 AD, 10 to 1500,
5 to 1750, 2 to 1850, then 1). At year 0 every civ's research progress is doubled (see Science).

Per city, in this exact order (CITYMGT.C, city_process, mode 0):
1. Clear the "building sold this turn" flag.
2. **Famine**: if the food box went below 0 last turn: if the city supports a Settlers unit, that
   Settlers is disbanded ("Settlers lost"); otherwise size -1 (a size-1 city is destroyed). Box set to 0.
3. **Growth**: if box >= (size+1) x 10 (human), size +1; box reset to 0, or to (newsize x 5 + 5) with a
   Granary (half the new box). Growing from 10 to 11 without an Aqueduct is refused (size stays 10)
   but the box is still reset: the food is wasted.
4. Count unit support (shields) and Settlers food.
5. Assign worked tiles (human cities keep their assignment; the city centre is always worked free),
   sum food, shields, trade.
6. Factory/Mfg Plant/power multipliers applied to shields.
7. Food surplus (food - 2 x size - Settlers food) added to the box (can go negative -> famine next turn).
8. Shield surplus (shields - unit support) added to the production box, **0 if the city was in
   disorder** (disorder flag from the previous evaluation). If the box reaches the cost, the item is
   built. Human: any excess shields are lost (box set to 0).
9. Corruption, trade routes, split of trade into luxury / tax / science, specialists, Marketplace,
   Bank, Library, University, wonders.
10. Happiness (content base, luxury, buildings, martial law or military unhappiness, wonders).
11. If shields < unit support, the homed unit farthest from the city is disbanded (repeat until paid).
12. **Disorder check**: unhappy > happy -> city enters/stays in disorder. Taxes are added to the
    treasury only if the city is NOT in disorder this turn and the government is not Anarchy.
    We Love the King Day check.
13. Science added to research (not under Anarchy; added even when the city is in disorder).
    Advance granted if the total passes the cost.
14. Human only: pollution roll, Nuclear Plant meltdown roll, then **building maintenance** paid
    (not under Anarchy); buildings are sold if gold runs out (section 2).

---

## 2. Gold

### Income
| Source | Rule (source) |
|---|---|
| Taxes | City tax = round((trade - corruption) x taxrate/10), + 2 per Taxman specialist, then x1.5 Marketplace, then x1.5 again Bank. Paid only if city not in disorder and government not Anarchy (CITYMGT.C, city_process) |
| Selling a building | Gold equal to its shield cost (1 gold per shield: Temple 40, Library 80, Factory 200). One sale per city per turn. Palace can be sold. Wonders cannot (CITYMGT.C, label_sell) |
| Forced sale (bankruptcy) | Same price, automatic (see below) |
| Caravan trade route | One-time revenue R, added both to gold AND to research (CAPTURE.C, rpath; section 3) |
| Capturing a city | Plunder = loser_gold x city_size / (loser_total_citizens + 1), taken from the loser (CAPTURE.C, unrest_chance/capunit) |
| Barbarian leader | +100 gold for defeating a lone barbarian leader (barbarian Diplomat-type unit) (BATTLE.C, battle) |
| Tribal huts | 50 gold (other results: advance, mercenary unit, barbarians) (UNITS.C) |
| AI diplomacy | AI may pay you for peace or an alliance (AUDIENCE.C) |

### Expenses
| Expense | Rule |
|---|---|
| **Building maintenance** | Every improvement in every city, every turn, gold per table in section 9. Human only; not paid under Anarchy. **The Palace's listed upkeep (5) is never charged** (loop starts at Barracks). Wonders and spaceship parts cost nothing (CITYMGT.C, end of city_process) |
| Barracks surcharge | Barracks base upkeep 0, +1 at Prince/King/Emperor, +1 more at Emperor, +1 if you know Gunpowder, +1 if you know Combustion (so 0..4 per Barracks) |
| Buying production | See formula below |
| Tribute | Paid to an AI that demands it in an audience (multiples of 50) (AUDIENCE.C) |
| Diplomat/bribe | Incite revolt and bribe costs (section 15) |
| Being captured from | Plunder taken by the conqueror |
| **Units** | **Units never cost gold.** Unit support is paid in shields (and food for Settlers) only |

### Running out of gold
Maintenance is paid building by building, city by city. The moment gold would go below 0, gold is
set to 0, **that building is removed and its full value (cost x 1 gold/shield) is credited**, with the
message "X can't maintain Y". So a bankrupt empire loses buildings one at a time while gold
bounces back up. Nothing else (units, science) is touched.

### Buying (CITYMGT.C, buy button)
r = remaining shields = cost - shields already in the box.
- Unit: price = r x r / 20 + 2 x r.
- Improvement: price = 2 x r.
- Wonder or spaceship part: price = 4 x r.
- Any item with an EMPTY box (0 shields): price doubled.
Buying fills the box; the item completes at the next turn's city processing. Example: Temple from
scratch = 2 x 40 x 2 = 160; with 1 shield in = 78.

### "Why is my gold dropping every turn?" checklist (most likely first)
1. **Building maintenance** exceeds tax income. Sum the upkeep of every building in every city
   (Temple 1, Library 1, Marketplace 1, Granary 1, Courthouse 1, City Walls 2, Aqueduct 2,
   Bank 3, Cathedral 3, University 3, Colosseum 4, Factory 4, ...). Each new building adds a
   permanent cost. Barracks cost up to 4 each on higher levels or after Gunpowder/Combustion.
2. **Tax rate too low** relative to science/luxury. Tax is set in tenths; luxury is 10 - tax - science.
3. **Cities in civil disorder pay no tax** but still pay full maintenance.
4. **Corruption**: far-away cities lose most of their trade; losing the Palace sets distance to 32
   (massive corruption everywhere); Communism takes ~21% from every city including the capital.
5. **Despotism/Anarchy -1** on tiles producing 3+ trade (oceans under Despotism give 2, not 3).
6. **Polluted tiles** produce half trade.
7. Specialists switched from Taxmen to Entertainers, or worked tiles changed.
8. A trade-route partner city was destroyed or captured (routes are lost).
9. One-off costs: buying, tribute, bribes, incite revolt.
Not causes: unit upkeep in gold and "war weariness" (neither exists). Anarchy stops taxes and maintenance alike.

---

## 3. Trade

**Tile trade** (CITYMGT.C, tile_yield), applied in this order:
1. Base terrain/special value (section 13).
2. Road: +1 trade only on Desert, Plains, Grassland (not River, not others). City centres have a road.
3. Colossus: +1 on every tile already producing trade, in the Colossus city only.
4. Railroad: +50% (value + value/2) — applies to food, shields AND trade.
5. Anarchy/Despotism: -1 on any yield of 3 or more, unless the city is celebrating (WLTKD).
6. Republic/Democracy: +1 on each tile already producing trade. Monarchy/Communism get this +1 only
   while the city celebrates. (Celebrating Republic/Democracy still +1, not +2.)
7. Pollution: halve, rounded up.

**Corruption** (city_process):
- d = distance to the nearest own city with a Palace (0 in the capital; 32 if you have no Palace).
  Under Communism d = 10 for every city, including the capital.
- corruption = trade x d x 3 / (gov x 20 + 80) — divisor: Anarchy 80, Despotism 100, Monarchy 120,
  Communism 140, Republic 160. Democracy: 0.
- Courthouse halves it. A Courthouse cannot be built in the Palace city.
- Computed twice: first on tile trade (stored as the city's "trade" value used by partners'
  trade routes), then on tile trade + trade-route bonus (the real one).
- Examples: Despotism, 10 tiles away: 30% lost. Republic 10 away: ~19%. Communism: 30/140 = 21% everywhere.

**Trade routes** (city_process; CAPTURE.C, rpath):
- Up to 3 per city, one-way (stored only in the caravan's home city).
- Each route adds (home_trade + partner_trade + 4) / 8 trade if the partner is foreign, / 16 if
  domestic, per turn (trade values after first-pass corruption). This extra trade is then subject
  to corruption.
- A new route replaces the weakest existing one only if the new partner is better (partner trade,
  doubled if foreign).
- Establishing it: a Caravan entering a foreign city does it automatically; entering your own city
  offers it only if that city is 10+ tiles from the Caravan's home city or on another continent.
- One-time revenue R = (distance + 10) x (home_trade + dest_trade) / 24; halved if the destination is
  your own city; -1/3 if the destination's owner knows RailRoad; -1/3 if it knows Flight. A "same
  continent: halve" test exists but compares against a wrong tile (bug), so it is erratic. R is added
  to gold and to research.

**Rates**: tax, luxury and science are set in tenths (0..10 each, summing to 10). **There is no
maximum rate per government in this source** — 100% tax or 100% science is allowed under any
government (CONTROL.C, '+'/'-' keys; STATS.C, civwasm_ai_rates checks only 0..10 and sum <= 10).
Split per city: luxury = round(net x lux/10), tax = round(net x tax/10), science = remainder.

**Specialists**: each yields 2 of its kind (Taxman gold, Scientist science, Entertainer luxury).
Extra citizens become Entertainers by default; Taxmen and Scientists can be chosen only in cities of
size 5+. Marketplace/Bank multiply luxury and tax including specialists; Library/University multiply
science including scientists.

---

## 4. Science

Per city: science = trade - corruption - luxury - tax, + 2 per Scientist, then:
- Library: + science/2. University (requires Library): + science/2 (both from the same base: x2 total).
- Isaac Newton's College (owner, all cities): each Library and each University adds another
  science/3 (so Library+University+Newton = x2.67). Note: civ-wide, not just its city.
- Copernicus' Observatory: doubles the final science of its city.
- SETI Program (owner): +50% of each city's science added to research (civ-wide).
- Chieftain only: science toward an advance already discovered by any civ counts double.
- Anarchy: 0 science.

**Cost of the next advance** (city_process): research completes when
`total / (BC ? 1 : 2) > N x step`, where N = advances gained so far (starting advances given at
turn 0 do not count).
- Human: step = 2 x level + 6 (Chieftain 6, Warlord 8, Prince 10, King 12, Emperor 14) + penalty;
  if 11 - N is larger, step = 11 - N (cheap first techs).
- AI: step = 14 - level + penalty.
- penalty (recomputed every 50 turns, AD only) = clamp(most advances held by any civ - turn/9, 0, 6)
  (TURN.C) — the whole game gets dearer if anyone races ahead.
- From 1 AD the bar is doubled; existing progress is doubled once at year 0 to compensate.
- Excess research beyond the cost is lost (total reset to 0).
- Darwin's Voyage grants two advances at once on completion. Great Library: its owner receives any
  advance known by 2+ other civs. Huts can grant an advance. Capturing a city steals one advance.

**Advances** (GAME.C, `advances` table; "enables" = units, improvements and wonders whose
prerequisite it is; governments: Monarchy, Communism, The Republic, Democracy enable those governments;
RailRoad enables railroads; Bridge Building enables roads on River; Construction enables Fortress).

| Advance | Requires | Enables |
|---|---|---|
| Alphabet | - | |
| Bronze Working | - | Phalanx, Colossus |
| Ceremonial Burial | - | Temple |
| Horseback Riding | - | Cavalry |
| Masonry | - | Palace, City Walls, Pyramids, Great Wall |
| Pottery | - | Granary, Hanging Gardens |
| The Wheel | - | Chariot |
| Code of Laws | Alphabet | Courthouse |
| MapMaking | Alphabet | Trireme, Lighthouse |
| Writing | Alphabet | Library, Diplomat |
| Iron Working | Bronze Working | Legion |
| Currency | Bronze Working | MarketPlace |
| Mysticism | Ceremonial Burial | Oracle (+1 Temple effect) |
| Mathematics | Alphabet, Masonry | Catapult |
| Monarchy | Ceremonial Burial, Code of Laws | Monarchy govt |
| Literacy | Writing, Code of Laws | Great Library |
| Construction | Masonry, Currency | Aqueduct, Colosseum, Fortress |
| Trade | Currency, Code of Laws | Caravan |
| Astronomy | Mysticism, Mathematics | Copernicus' Observatory |
| Philosophy | Mysticism, Literacy | |
| The Republic | Code of Laws, Literacy | Republic govt |
| Feudalism | Masonry, Monarchy | |
| Bridge Building | Iron Working, Construction | roads on River |
| Navigation | MapMaking, Astronomy | Sail, Magellan's Expedition |
| University | Mathematics, Philosophy | University |
| Medicine | Philosophy, Trade | Shakespeare's Theatre |
| Religion | Philosophy, Writing | Cathedral, Michelangelo's, J.S.Bach's |
| Banking | Trade, The Republic | Bank |
| Chivalry | Feudalism, Horseback Riding | Knights |
| Engineering | The Wheel, Construction | |
| Physics | Mathematics, Navigation | |
| Democracy | Philosophy, Literacy | Democracy govt |
| Invention | Engineering, Literacy | |
| Magnetism | Navigation, Physics | Frigate |
| Theory of Gravity | Astronomy, University | Isaac Newton's College |
| Gunpowder | Invention, Iron Working | Musketeers |
| Chemistry | University, Medicine | |
| Steam Engine | Physics, Invention | Ironclad |
| Metallurgy | Gunpowder, University | Cannon |
| Atomic Theory | Theory of Gravity, Physics | |
| Explosives | Gunpowder, Chemistry | |
| Electricity | Metallurgy, Magnetism | |
| RailRoad | Steam Engine, Bridge Building | Darwin's Voyage, railroads |
| Conscription | The Republic, Explosives | Riflemen |
| Industrialization | RailRoad, Banking | Factory, Transport, Women's Suffrage |
| Electronics | Engineering, Electricity | Hydro Plant, Hoover Dam |
| The Corporation | Banking, Industrialization | |
| Steel | Metallurgy, Industrialization | Battleship |
| Communism | Philosophy, Industrialization | Communism govt, United Nations |
| Refining | Chemistry, The Corporation | Power Plant |
| Genetic Engineering | Medicine, The Corporation | Cure for Cancer |
| Combustion | Refining, Explosives | Cruiser |
| Automobile | Combustion, Steel | Armor |
| Flight | Combustion, Physics | Fighter |
| Mass Production | Automobile, The Corporation | Mass Transit, Submarine |
| Advanced Flight | Flight, Electricity | Bomber, Carrier |
| Nuclear Fission | Mass Production, Atomic Theory | Manhattan Project |
| Labor Union | Mass Production, Communism | Mech. Inf. |
| Recycling | Mass Production, Democracy | Recycling Center |
| Nuclear Power | Nuclear Fission, Electronics | Nuclear Plant |
| Rocketry | Advanced Flight, Electronics | Nuclear |
| Computers | Mathematics, Electronics | SETI Program |
| Space Flight | Computers, Rocketry | SS Structural, Apollo Program |
| Plastics | Refining, Space Flight | SS Component |
| SuperConductor | Plastics, Mass Production | SDI Defense |
| Robotics | Plastics, Computers | Artillery, Mfg. Plant, SS Module |
| Fusion Power | Nuclear Power, SuperConductor | stops Nuclear Plant meltdowns |

Side effects of learning: Gunpowder or Combustion **removes every existing Barracks** of that civ
(no refund) (ECONOMY.C, give_advance). A civ's own Gunpowder/Conscription/etc. make older units
unbuildable (section 11). Any civ learning a wonder's obsoleting advance disables that wonder for its
owner (section 10).

---

## 5. Food and growth

- Each citizen eats 2 food. Settlers homed in the city eat 1 more each under Anarchy/Despotism,
  2 under Monarchy and later (`sett_food_j`).
- City tiles worked = size, plus the city centre (free). The centre gets a road and, if its terrain
  can be irrigated in place (Desert, Plains, Grassland, Hills, River), automatic irrigation
  (CITY.C, city_create).
- Food box capacity = (size + 1) x 10 for the human (AI: (size+1) x (16 - 2 x level), minus 2 more
  for top-ranked human at King/Emperor in AD).
- Granary: after growth the box keeps size x 5 + 5 (half the new box).
- Aqueduct: required to grow beyond 10. Without it growth stops at 10 and the full box is emptied
  each time it fills (food wasted). Settlers can join a city only below size 10.
- Famine: box below 0 -> a homed Settlers is disbanded if one exists, else size -1, box = 0.
- Building Settlers costs 1 population (size 1 city that is not your only city is destroyed and turned
  into the Settlers; your only size-1 city keeps size 1; on Chieftain a size-1 city will not complete
  Settlers).
- **We Love the King Day** (celebration): conditions — size >= 3, zero unhappy citizens, happy >=
  (size+1)/2, government not Anarchy. Effects while celebrating: Despotism tile penalty lifted;
  Monarchy/Communism get +1 trade per trade tile; Republic/Democracy cities grow +1 size per turn
  (from the second celebrating turn) if food surplus > 0 and (size < 10 or Aqueduct). There is no
  rapture growth under Despotism, Monarchy or Communism.
- Disasters also reduce size (section 15).

---

## 6. Shields and production

- Shields = sum of worked tiles (after railroad/despotism/pollution), then x (1 + F/4 + P/4):
  Factory F=2 (+50%), Mfg. Plant F=4 (+100%, does not need a Factory and replaces it).
  Power Plant, Hydro Plant, Nuclear Plant or Hoover Dam P=2 (+50%), but P can never exceed F —
  so a plant does nothing without a Factory/Mfg. Plant. Max: Mfg + plant = +150%.
- **Unit support** (shields): every homed unit except Diplomats and Caravans (they are free) counts.
  Anarchy/Despotism: the first `size` units are free, each further one costs 1 shield.
  Monarchy, Communism, Republic, Democracy: **every** unit costs 1 shield. Settlers count as units
  (and also eat food). If shields < support, the homed unit farthest from the city is disbanded each
  turn until it fits ("can't support ... Unit Disbanded").
- No shield waste/corruption of shields exists in Civ1.
- Box cap 999; values above 999 or below 0 are reset to 0.
- **Changing production has no penalty** in this source (switching unit <-> improvement <-> wonder
  keeps all shields).
- Human: overflow beyond the cost is lost when an item completes.
- Barracks: units built are veterans. Units cannot be built at 127 units per civ; at most 128 cities in
  the world.
- Ships can be built only in coastal cities (an Ocean tile adjacent).
- Disorder: no shields are added (support still checked against raw shields).

**Pollution** (human only; CITYMGT.C): each turn
P = shields / D - 20 + size x k / 4, where D = 1, or 2 with Hydro Plant, Nuclear Plant or Hoover Dam
(same continent), or 3 with Recycling Center (overrides); k = number of Industrialization, Automobile,
Mass Production, Plastics known (0 with Mass Transit). Shields here are after Factory/plant bonuses
(a Power Plant adds pollution without reducing it). If P > 0, chance per turn ~ 2P / (256 - N x
level / 2) (N = advances known): a random non-centre tile of the city radius becomes polluted (not
ocean, not a city, not already polluted). Polluted tiles yield half. Settlers clean it in 4 turns.

---

## 7. Happiness

Computed in this order each turn (city_process):
1. **Base unhappy** (human): content limit = 6 - level - E, where E is the empire-size penalty:
   s = (gov/2 + 2) x (7 - level) (gov/2: Anarchy/Despotism 0, Monarchy/Communism 1,
   Republic/Democracy 2); E = floor((city_slot mod s + number_of_cities - s) / s) when that is >= 0,
   else 0. Roughly: once you have more than s cities, cities start losing a content citizen, one more
   per further s cities. Content limit by level before E: Chieftain 6, Warlord 5, Prince 4, King 3,
   Emperor 2. s examples: Prince Despotism 10, Prince Monarchy 15, Prince Republic 20; King Despotism 8.
   Citizens above the limit are unhappy; a negative limit makes "excess" unhappiness that must be
   removed before any citizen turns content.
   AI cities: always 3 content, no empire penalty.
2. **Luxury**: happy citizens = luxury / 2 (after Marketplace/Bank x1.5 each, Entertainers +2 each).
3. **Buildings** (unhappy reduced): Colosseum -3; Cathedral -4 (-6 with Michelangelo's), only if the owner
   knows Religion; Temple -1 with Ceremonial Burial, -2 with Mysticism; Oracle gives Temples the same
   amount again (-2/-4 total).
4. **Military**:
   - Anarchy, Despotism, Monarchy, Communism: **martial law** — each own unit with attack > 0 in the
     city tile makes 1 unhappy content, max 3.
   - Republic: +1 unhappy per homed unit with attack > 0 that is outside its home city tile (any
     air unit always counts — even inside a city). Democracy: +2 per such unit. Women's Suffrage
     reduces it by 1 per unit (Republic 0, Democracy 1). "Outside" includes fortresses and your own
     other cities. Settlers, Diplomats, Caravans never count.
5. **Wonders**: Hanging Gardens +1 happy in every owner city; Cure for Cancer +1 happy in every owner
   city; Shakespeare's Theatre: no unhappy in its city; J.S. Bach's: -2 unhappy in owner cities on the
   same continent.
6. Totals are clipped so happy + unhappy <= working citizens (specialists are neither).

**Disorder** when happy < unhappy (equal is fine): no shield production, **no tax income**, still pays
maintenance; food and science continue in this source; under Democracy a city already in disorder
that is still in disorder next turn makes the **government collapse into Anarchy** (republic does not
collapse). Order restored message when happy >= unhappy. Disorder makes a Nuclear Plant meltdown
possible (1/3 per turn until Fusion Power). Unhappy cities are also targets of riot disasters and of
defection to a happier neighbouring civ (section 15).

---

## 8. Governments

| | Anarchy | Despotism | Monarchy | Communism | Republic | Democracy |
|---|---|---|---|---|---|---|
| Requires | revolution | start | Monarchy | Communism | The Republic | Democracy |
| -1 on tile yield >= 3 | yes | yes (not while celebrating) | no | no | no | no |
| +1 trade on trade tiles | no | no | while celebrating | while celebrating | yes | yes |
| Corruption divisor (trade x d x 3 / div) | 80 | 100 | 120 | 140, d fixed 10 | 160 | none |
| Unit support (shields) | free up to size | free up to size | 1 each | 1 each | 1 each | 1 each |
| Settlers food | 1 | 1 | 2 | 2 | 2 | 2 |
| Martial law (max 3) | yes | yes | yes | yes | no | no |
| Unhappy per military unit away | 0 | 0 | 0 | 0 | 1 | 2 |
| Empire-size step multiplier | 2 | 2 | 3 | 3 | 4 | 4 |
| Rapture growth | no | no | no | no | yes | yes |
| Taxes / science / maintenance | none / none / none | normal | normal | normal | normal | normal |
| Senate | no | no | no | no | yes | yes |
| Collapse on disorder | - | no | no | no | no | yes (2nd turn) |
| Max tax/science rate | none | none | none | none | none | none |

- **Revolution** (any time) -> Anarchy. Human Anarchy ends at the first turn whose turn counter is a
  multiple of 4 (1 to 4 turns); after a Democracy collapse it lasts one more such cycle (5-8 turns).
  With the Pyramids, Anarchy ends on the next turn and every government can be chosen without its
  advance (DIPLOMAC.C, city_phase/set_government).
- **Senate** (Republic/Democracy): you cannot attack a civ you have a peace treaty with ("Overruled by
  the Senate"); if an AI offers peace and you refuse, the Senate signs it anyway; the war-declaring
  option in audiences is disabled (BATTLE.C attack_ok; AUDIENCE.C).
- AI civs reset their tax rate on a government change; tax/science rates otherwise persist.

---

## 9. City improvements

Cost in shields (human), upkeep in gold per turn (GAME.C `improvements`; effects from CITYMGT.C).

| Improvement | Cost | Upkeep | Requires | Effect in this source |
|---|---|---|---|---|
| Palace | 200 | 5 (never charged) | Masonry | Capital: corruption distance 0; building it elsewhere moves the capital. First city gets it free. Barbarians attack it at half strength |
| Barracks | 40 | 0 + surcharge (0-4) | - | New units veteran. All removed when you learn Gunpowder or Combustion. Prevents pirate disaster |
| Granary | 60 | 1 | Pottery | Keeps half the food box after growth; prevents famine disaster |
| Temple | 40 | 1 | Ceremonial Burial | -1 unhappy (-2 with Mysticism; doubled by Oracle). Prevents volcano disaster |
| MarketPlace | 80 | 1 | Currency | +50% tax and luxury |
| Library | 80 | 1 | Writing | +50% science (+33% more with Newton's) |
| Courthouse | 80 | 1 | Code of Laws | Halves corruption. Not buildable in the Palace city |
| City Walls | 120 | 2 | Masonry | Defence x3 (instead of x1/x1.5) vs all attackers except Artillery and Bomber (attack 12); no population loss when attacked; prevents flood disaster |
| Aqueduct | 120 | 2 | Construction | Allows size > 10; prevents plague and fire disasters |
| Bank | 120 | 3 | Banking (needs MarketPlace) | +50% tax and luxury, applied after MarketPlace (compounding: x2.25 total) |
| Cathedral | 160 | 3 | Religion | -4 unhappy (-6 with Michelangelo's) |
| University | 160 | 3 | University (needs Library) | +50% science (+33% more with Newton's) |
| Mass Transit | 160 | 4 | Mass Production | Removes population pollution |
| Colosseum | 100 | 4 | Construction | -3 unhappy |
| Factory | 200 | 4 | Industrialization | +50% shields. Not buildable once a Mfg. Plant exists |
| Mfg. Plant | 320 | 6 | Robotics | +100% shields (no Factory needed) |
| SDI Defense | 200 | 4 | SuperConductor | Units in the city survive a nuclear attack (city still loses half its size to fallout) |
| Recycling Cntr. | 200 | 2 | Recycling | Production pollution / 3 |
| Power Plant | 160 | 4 | Refining (needs a Factory; a Mfg.-Plant-only city cannot build it) | +50% shields, capped by the Factory bonus; no pollution reduction |
| Hydro Plant | 240 | 4 | Electronics | As Power Plant + production pollution / 2. Only in cities next to River or Mountains |
| Nuclear Plant | 160 | 2 | Nuclear Power | As Hydro; meltdown risk 1/3 per turn while the city is in disorder, until Fusion Power |
| SS Structural | 80 | 0 | Space Flight | Spaceship part; needs Apollo Program built by anyone |
| SS Component | 160 | 0 | Plastics | Spaceship part |
| SS Module | 320 | 0 | Robotics | Spaceship part |

Only one power source counts: Power/Hydro/Nuclear cannot be built if the city already has one or is
powered by Hoover Dam. When a city is captured: Palace, Temple, Courthouse, Cathedral are always lost;
in a conquest roughly half the other buildings are lost (alternate bits kept at random) and all of SDI,
Recycling, plants and spaceship parts are lost (CAPTURE.C, capunit).

---

## 10. Wonders

Built once in the world; lost forever if its city is destroyed. Effects stop ("obsolete") as soon as **any** non-barbarian civ — not just
the owner — knows the obsoleting advance (CITYMGT.C, civ_wonder/active_wonder_city). Wonders have no
upkeep. Each built wonder is worth score.

| Wonder | Cost | Requires | Effect in this source | Obsolete by |
|---|---|---|---|---|
| Pyramids | 300 | Masonry | Anarchy ends next turn; any government selectable without its advance | Communism |
| Hanging Gardens | 300 | Pottery | +1 happy citizen in every owner city | Invention |
| Colossus | 200 | Bronze Working | +1 trade on every trade-producing tile of its city | Electricity |
| Lighthouse | 200 | MapMaking | +1 movement for all owner ships (not cumulative with Magellan's) | Magnetism |
| Great Library | 300 | Literacy | Owner gets every advance known by 2+ other civs | University |
| Oracle | 300 | Mysticism | Temples in owner cities count double | Religion |
| Great Wall | 300 | Masonry | Only diplomatic: AI leaders do not take a hostile/tribute stance toward the owner in audiences; affects AI alliance logic. **No combat effect** (it is not free City Walls here) | Gunpowder |
| Magellan's Expedition | 400 | Navigation | +1 movement for all owner ships (same as Lighthouse, not cumulative) | never |
| Michelangelo's Chapel | 300 | Religion | Cathedrals give -6 unhappy instead of -4 (no effect in cities without a Cathedral) | Communism |
| Copernicus' Observatory | 300 | Astronomy | Science of its city x2 | never |
| Shakespeare's Theatre | 400 | Medicine | No unhappy citizens in its city | Electronics |
| Isaac Newton's College | 400 | Theory of Gravity | In every owner city, each Library and University adds +33% science | Nuclear Fission |
| J.S. Bach's Cathedral | 400 | Religion | -2 unhappy in every owner city on the same continent | never |
| Darwin's Voyage | 300 | RailRoad | Two free advances on completion | never |
| Hoover Dam | 600 | Electronics | Acts as a Hydro Plant (+50% with Factory, pollution /2) in all owner cities on the same continent | never |
| Women's Suffrage | 600 | Industrialization | Republic/Democracy unhappiness from units away -1 per unit | never |
| Manhattan Project | 600 | Nuclear Fission | Lets every civ with Rocketry and Nuclear Fission build Nuclear units | never |
| United Nations | 600 | Communism | Same diplomatic effect as Great Wall (no hostile AI stance in audiences) | never |
| Apollo Program | 600 | Space Flight | Reveals all cities to the owner (human); allows every civ to build spaceship parts | never |
| SETI Program | 600 | Computers | +50% science in all owner cities | never |
| Cure for Cancer | 600 | Genetic Engineering | +1 happy citizen in every owner city | never |

This source has 21 wonders; there is no Marco Polo's Embassy or King Richard's Crusade. Caravans
can add 50 shields to a wonder under construction in an own city (section 11).

---

## 11. Units

Cost in shields (human). Moves in tiles. Domain L land, S sea, A air. "Obs" = advance after which
YOU can no longer build it (only your own knowledge counts) (GAME.C `utypes`; ECONOMY.C find_slot).

| Unit | Cost | A | D | Mv | Dom | Requires | Obs | Notes |
|---|---|---|---|---|---|---|---|---|
| Settlers | 40 | 0 | 1 | 1 | L | - | - | Builds cities, terrain work; costs 1 pop; eats food |
| Militia | 10 | 1 | 1 | 1 | L | - | Gunpowder | |
| Phalanx | 20 | 1 | 2 | 1 | L | Bronze Working | Gunpowder | |
| Legion | 20 | 3 | 1 | 1 | L | Iron Working | Conscription | |
| Musketeers | 30 | 2 | 3 | 1 | L | Gunpowder | Conscription | |
| Riflemen | 30 | 3 | 5 | 1 | L | Conscription | - | |
| Cavalry | 20 | 2 | 1 | 2 | L | Horseback Riding | Conscription | |
| Knights | 40 | 4 | 2 | 2 | L | Chivalry | Automobile | |
| Catapult | 40 | 6 | 1 | 1 | L | Mathematics | Metallurgy | |
| Cannon | 40 | 8 | 1 | 1 | L | Metallurgy | Robotics | |
| Chariot | 40 | 4 | 1 | 2 | L | The Wheel | Chivalry | |
| Armor | 80 | 10 | 5 | 3 | L | Automobile | - | |
| Mech. Inf. | 50 | 6 | 6 | 3 | L | Labor Union | - | |
| Artillery | 60 | 12 | 2 | 2 | L | Robotics | - | Ignores City Walls |
| Fighter | 60 | 4 | 2 | 10 | A | Flight | - | Fuel 1: must end every turn in a city or with a Carrier, else lost. Only Fighters can attack air units outside cities |
| Bomber | 120 | 12 | 1 | 8 | A | Advanced Flight | - | Fuel 2 (may end one turn in the air). Attacking ends its move. Ignores City Walls |
| Trireme | 40 | 1 | 0 | 3 | S | MapMaking | Navigation | Carries 2. Human Trireme ending a turn with no land adjacent: 50% lost |
| Sail | 40 | 1 | 1 | 3 | S | Navigation | Magnetism | Carries 3 |
| Frigate | 40 | 2 | 2 | 3 | S | Magnetism | Industrialization | Carries 4 |
| Ironclad | 60 | 4 | 4 | 4 | S | Steam Engine | Combustion | |
| Cruiser | 80 | 6 | 6 | 6 | S | Combustion | - | Sight 3 |
| Battleship | 160 | 18 | 12 | 4 | S | Steel | - | Sight 3 |
| Submarine | 50 | 8 | 2 | 3 | S | Mass Production | - | Sight 3 |
| Carrier | 160 | 1 | 12 | 5 | S | Advanced Flight | - | Air units refuel on it; capacity limit unverified |
| Transport | 50 | 0 | 3 | 4 | S | Industrialization | - | Carries 8 |
| Nuclear | 160 | 99 | 0 | 16 | A | Rocketry | - | Needs Manhattan Project (anyone) + Nuclear Fission. Destroys all units on the 9 tiles (except inside SDI cities), halves city sizes there, pollutes land tiles (2/3 each) |
| Diplomat | 30 | 0 | 0 | 2 | L | Writing | - | No support cost. Ignores zones of control. Actions in section 15 |
| Caravan | 50 | 0 | 1 | 1 | L | Trade | - | No support cost. Ignores ZOC. Trade route or +50 shields to a wonder |

Land units carried: transport capacity counts only land units. Sea and air units ignore terrain
movement cost (1 per tile). Lighthouse or Magellan's: +1 move to all your ships.

**Settlers actions**: found city (anywhere on land not at the poles), join city (+1 size, size < 10),
road, railroad, irrigate, mine, terrain change, fortress (Construction), clean pollution (section 14).
Units can be re-homed in a city they stand in. Pillage (any unit except Fighters, Diplomats and
Caravans) removes a tile improvement.

**Movement**: costs are in thirds of a move. Along a road (both tiles have road): 1/3. From a
railroad tile onto road/rail: free for human units moved manually (GoTo and AI pay 1/3). Otherwise
cost = 3 x terrain move cost; if the unit has less than that left, it may still enter with
probability (moves left)/(3 x cost) — a unit with exactly 1 full move left always enters. **Rivers
give no movement bonus** (River is a terrain with cost 1). Zones of control: a land unit cannot
move from a tile next to an enemy unit to another tile next to an enemy unit (Diplomats/Caravans
exempt; moving onto an own unit is allowed). Attacking with partial moves scales attack by
moves/3.

---

## 12. Combat

(BATTLE.C, battle) Single roll, no hit points:
- Attack strength = A x 8. Veteran: x1.5. Partial move: x (thirds left)/3.
- Defence (land unit) = D x 8 x terrain factor x one of:
  - City Walls (defender in a walled city, attacker not Artillery/Bomber): x3;
  - else Fortress: x2;
  - else Fortified: x1.5;
  - else x1.
  These do not stack with each other. Veteran defender: x1.5.
- Terrain factor (the defence column in section 13): Desert/Plains/Grassland/Tundra/Arctic/Ocean x1,
  Forest/Jungle/Swamp/River x1.5, Hills x2, Mountains x3.
- Sea or air defenders: D x 8 flat (no terrain, no fortify); a sea unit in a walled city does get the
  walls bonus.
- Difficulty: a non-barbarian attacking the human on Chieftain/Warlord has attack halved; the human
  attacking on Chieftain has attack doubled.
- Barbarian attackers: x(level+1)/4 vs human, /2 vs AI; vs a civ's only city: attack 0; vs a Palace
  city: halved; and they must win a second roll to take a city.
- Result: attacker wins if random(0..attack-1) > random(0..defence-1). Approximately
  P(win) = 1 - D'/(2A') when A' >= D', else A'/(2D').
- Loss outside a city or fortress: the **whole defending stack is destroyed**. In a city or fortress
  only the defender dies.
- A winning attack on a city without walls (not from the sea) reduces the city's size by 1 (not on
  Chieftain for human cities); size 0 destroys it.
- The winner (either side) becomes veteran with 50% chance.
- Attacking costs one move (Bomber: all moves). Only Fighters can attack an air unit outside a city.
- Attacking a civ you have peace with breaks the treaty (and is blocked by the Senate under
  Republic/Democracy).
- Great Wall has no combat effect in this source.

Capturing a city (moving into an empty enemy city): plunder (section 2), one advance stolen, city loses
1 size (size 1 is destroyed), buildings lost (section 9), the old owner's units homed there but outside
it are disbanded. Capturing a civ's capital can split it in civil war (CIVWAR.C) and loses a launched
spaceship.

---

## 13. Terrain

(GAME.C `terrains`; defence in halves in the table, shown here as a factor.)

| Terrain | Move | Def | Food | Shield | Trade | Special (F/S/T) | Irrigate (turns) | Mine (turns) |
|---|---|---|---|---|---|---|---|---|
| Desert | 1 | x1 | 0 | 1 | 0 (+1 road) | Oasis 3/1/0 | +1 food (5) | +1 shield (5) |
| Plains | 1 | x1 | 1 | 1 | 0 (+1 road) | Horses 1/3/0 | +1 food (5) | -> Forest (15) |
| Grassland | 1 | x1 | 2 | 1* | 0 (+1 road) | same as base (2/1*/0) | +1 food (5) | -> Forest (10) |
| Forest | 2 | x1.5 | 1 | 2 | 0 | Game 3/2/0 | -> Plains (5) | no |
| Hills | 2 | x2 | 1 | 0 | 0 | Coal 1/2/0 | +1 food (10) | +3 shields (10) |
| Mountains | 3 | x3 | 0 | 1 | 0 | Gold 0/1/6 | no | +1 shield (10) |
| Tundra | 1 | x1 | 1 | 0 | 0 | Game 3/0/0 | no | no |
| Arctic | 2 | x1 | 0 | 0 | 0 | Seals 2/0/0 | no | no |
| Swamp | 2 | x1.5 | 1 | 0 | 0 | Oil 1/4/0 | -> Grassland (15) | -> Forest (15) |
| Jungle | 2 | x1.5 | 1 | 0 | 0 | Gems 1/0/4 | -> Grassland (15) | -> Forest (15) |
| Ocean | 1 | x1 | 1 | 0 | 2 | Fish 3/0/2 | no | no |
| River | 1 | x1.5 | 2 | 1* | 1 | same as base | +1 food (5) | no |

\* Grassland and River produce their 1 shield only on tiles where ((x x 7 + y x 11) AND 2) = 0 —
about half the tiles, in a fixed pattern ("shield grassland"); otherwise 0 shields. The pattern
applies even to the special.
- Irrigation and a mine exclude each other (building one removes the other). Irrigation needs water:
  the tile is River, or an orthogonally adjacent tile (N/E/S/W) is River, Ocean or irrigated
  land (city tiles do not count) (CONTROL.C, build_ok).
- Road +1 trade only on Desert, Plains, Grassland. Railroad +50% of food, shields and trade on its
  tile. Despotism -1 on 3+ yields is applied after these, so irrigated Grassland (3 food) gives 2.
- Pollution halves all yields of the tile.
- There is no Harbour, Supermarket or Superhighway in Civ1: Ocean is always 1/0/2 (Fish 3/0/2),
  +1 trade under Republic/Democracy.

---

## 14. Settlers work

Each Settlers unit counts its own turns; several Settlers on one tile do NOT combine (each has its own
counter) (CONTROL.C, case 'r'/'i'/'m'/'f'/'p').

| Job | Turns | Requirement |
|---|---|---|
| Road | 2 x terrain move cost (Grassland 2, Forest/Hills/Swamp 4, Mountains 6, River 2) | River needs Bridge Building |
| Railroad (on a road) | 4 x move cost (Grassland 4, Hills 8, Mountains 12) | RailRoad |
| Irrigate / terrain change by irrigating | table in section 13 | water source for +food irrigation |
| Mine / terrain change by mining | table in section 13 | |
| Fortress | move cost + 4 | Construction; not on a city |
| Clean pollution | 4 | |
| Found city | immediate | land, not within 2 rows of the poles |
| Join city | immediate | city size < 10 |

Railroad bonus: +50% (rounded down) of each yield on the tile, e.g. Hills with mine (3 shields)
-> 4; Grassland irrigated (3 food) -> 4 then -1 under Despotism.

---

## 15. Diplomacy, diplomats, barbarians, disasters, pollution, spaceship

**Diplomat actions** entering a foreign city (DIPLOMAT.C):
- Establish Embassy (see their data), Investigate City.
- Steal Technology: random advance they have and you lack; once per city ever. Breaks peace with that civ.
- Industrial Sabotage: 50% destroy a random building, otherwise (or if none) the production box is emptied.
- Incite a Revolt: cost = (target_gold + 1000) / (d + 3) x size, d = distance to their Palace (max 16);
  halved if the city is in disorder. Not possible against capitals or barbarians. At peace you may pay
  double to keep the treaty ("Subvert city"). The city and its contents become yours.
- Meet with King (audience).
- Bribe a lone enemy unit: (owner_gold + 750) / (d + 2) x unit cost/10, halved unless it is Settlers.
**Caravans**: see section 3; "Help build WONDER" adds 50 shields to an own city building a wonder
(only wonders, not normal improvements).

**AI diplomacy**: AI leaders may demand tribute (multiples of 50, scaled by difficulty and their
military advantage), offer or demand peace, ask for alliances, ask you to pay them to fight a third
civ (cost based on their gold). Republic/Democracy: Senate rules (section 8). Great Wall or United Nations owner: AI leaders
are not hostile in audiences.

**Barbarians** (TURN.C, uprising): start after turn (5 - level) x 30 (Chieftain 150, Emperor 30).
Sea landings every 8 turns at a random empty ocean spot (a Sail, or a Frigate after turn 300,
carrying 1-3 Legions or Knights and a leader); the condition that ends them is unverified. Land uprisings: each turn one random city slot is picked; if it is a human city, the chance
in percent = your_gold x city_size / (your_total_citizens + 1) — **a big treasury attracts barbarians**.
They appear 6+ tiles from the city. Barbarian attacks on a civ's only city have strength 0.
Killing a lone barbarian leader: +100 gold. A city taken by a barbarian unit with attack <= 2 is
destroyed.

**Disasters** (CITY.C, city_disaster), after turn 50, ~1 city slot per turn, city size >= 5, owner with
2+ cities; one of 10 results:
| Roll | Event | Condition | Effect |
|---|---|---|---|
| 0 | Earthquake | Hills nearby | random building destroyed |
| 1 | Plague | no Aqueduct, owner knows Construction but not Medicine | size -1/4 |
| 2 | Flood | River nearby, no City Walls, knows Masonry | size -1/4 |
| 3 | Volcano | Mountains nearby, no Temple, knows Ceremonial Burial | size -1/3 |
| 4 | Famine | no Granary, knows Pottery | food box 0, size -1/3 |
| 5 | Fire | no Aqueduct, knows Construction | random building destroyed |
| 6 | Pirates | Ocean nearby, no Barracks | food and shield boxes 0 |
| 7-9 | Riot/Corruption/Scandal | only if happy < unhappy | food and shield boxes 0; a non-capital city of a civ with 4+ cities may defect to a happier nearby civ |

**Pollution and global warming** (TURN.C): polluted tiles accumulate a warming counter; when it passes
16 global warming changes terrain (WORLDGEN.C, global_warming; details not summarised). Your score
is reduced 10 per polluted tile.
Only human cities generate pollution in this source; nuclear attacks and meltdowns also pollute.

**Spaceship** (SPACESHP.C): after Apollo Program (built by anyone), build SS Structural/Component/Module
(80/160/320 shields) in any city, then launch. Arrival at Alpha Centauri wins; capturing the
launcher's capital destroys the ship. Exact success-probability formula not summarised here
(unverified).

---

## 16. Common mistakes (from Civ II and later games)

1. Units cost gold. **No** — never in Civ1; support is shields (+ food for Settlers).
2. Gold falling = "you are spending it". Usually building maintenance > taxes (section 2 checklist).
3. Max tax/science rates by government (60%/70%/80%). **No limit** in this game.
4. Disorder stops science. Here only shields and taxes stop; science and food continue.
5. Production change penalty (50% loss switching category). **None** here.
6. Shield overflow carries over. **No** for the human: excess is lost.
7. Hit points, firepower, combat rounds. **No**: one random roll, loser dies; losing outside a city or
   fortress kills the whole stack.
8. Great Wall = City Walls everywhere. **No** combat effect here.
9. City Walls +100%. Here x3 (+200%), ignored by Artillery and Bombers; also stops population loss.
10. River gives +1 trade and movement along rivers. River is a terrain type: 2/1*/1, move 1, defence
    x1.5; road on River gives no trade.
11. Harbour, Sewer, Coastal Defense, Supermarket, Stock Exchange, Police Station, Airport, Marco Polo,
    King Richard's, Statue of Zeus, Pikemen, Alpine Troops, Engineers, Spy, Explorer: **do not exist**.
12. Celebration under Monarchy/Communism grows cities. **No**: rapture growth is Republic/Democracy
    only; under Monarchy/Communism celebration gives +1 trade per trade tile.
13. Military unhappiness only for "aggressive" units abroad. Here **any** homed attack-capable unit
    outside its own city tile counts, even inside your borders or another of your cities; air units
    always count.
14. Martial law only under Communism/Monarchy. Here also under Anarchy and Despotism.
15. Corruption reduces shields (waste). **No**: only trade.
16. Bank +30%, cumulative with Marketplace additively. Here +50% compounding (x2.25 together).
17. Newton's = +100% in one city. Here +33% per Library/University in **all** owner cities.
18. Michelangelo's = Cathedral in every city. Here it only strengthens existing Cathedrals.
19. A wonder stays active until its owner learns the obsoleting tech. Here it expires when **any**
    civ learns it.
20. Diplomats/Caravans cost upkeep. **No**, both are free.
21. Barracks are permanent. Gunpowder and Combustion delete all your Barracks; Barracks upkeep rises
    with difficulty and with those advances.
22. Settlers combine work on a tile. **No**, each works alone.
23. Despotism -1 applies only to food. It applies to food, shields and trade (any yield >= 3), and is
    lifted while the city celebrates.
24. Democracy collapses after N turns of disorder in many cities. Here a single city in disorder for
    two consecutive turns is enough. Republic never collapses.
25. Tech cost by a fixed table. Here cost = (advances gained x step), doubled in AD, scaled by
    difficulty, plus a catch-up penalty if any civ is far ahead.
26. Palace costs 5 gold upkeep. Listed but never charged.

---

## 17. Rules that differ for AI civilizations

- AI pays **no building maintenance** and creates no pollution (both coded for the human only).
- AI production and food box costs use a factor (16 - 2 x level) instead of 10 (cheaper than human
  from Prince up), minus 2 more in AD at King/Emperor when the human leads; AI keeps shield overflow.
- AI cities have 3 base content citizens regardless of level and empire size.
- AI research step = 14 - level (+ penalty).
- AI rush-buys at 2 gold per shield, and sets its own tax/science rates each turn.
- AI cities re-pick worked tiles every 4th turn; AI Republic/Democracy disbands far units when cities
  riot.

