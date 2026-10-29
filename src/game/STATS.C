/* civ-wasm: Debug game stats. With the page's setting on, the game hands
 * the page a JSON record of the player's civilization at the start of each
 * of the player's turns (after the cities have grown and built), and one
 * for each battle the player fights. The page keeps them as the game's
 * history (web/index.html) for the stats report and an advisor to read.
 *
 * Read-only: the only game call is city_process(city, -1), the advisors'
 * own recalculation of a city's yields. */
#include <string.h>
#include <stdlib.h>
#include "CIV.H"
#include "GAME.H"
#include "GLOBALS.H"
#include "ECONOMY.H"
#include "CITYMGT.H"
#include "RESIDENT.H"
#include "WIDE.H"
#include "UNITAI.H"

extern struct city townbuf[];
extern struct unit armydata[8][128];
extern struct unit_type utypes[];
extern int treaties[8][8];
extern char far place_name[][13];
extern int marvel_site[];
extern int city_process(int, int);
extern int game_flags;
extern struct terrain_type terrains[];
extern int terrain_at(int, int), wrap_x(int);
extern int spiral_x[], spiral_y[];
extern int far advance_owners[];

static void field(char *, int);

int civwasm_debug_stats;        /* --debug-stats (WIDE.H; set while playing too) */
int civwasm_ai_drive;           /* AI plays: the page's plan gives the orders (WIDE.H) */
char civwasm_ai_cmd[8][128];    /* the plan's next order key per unit; 0 none */
static int plan_turn = -1;      /* the turn the page's plan was applied for */

static char record[200000];
static char *out;
static char digits[12];

static void put(char *s)
{
    while (*s)
        *out++ = *s++;
}

static void num(int v)
{
    put(itoa(v, digits, 10));
}

static void text(char far *s)
{
    *out++ = '"';
    for (; *s; s++) {
        if (*s == '"' || *s == '\\')
            *out++ = '\\';
        if ((unsigned char) *s >= ' ')
            *out++ = *s;
    }
    *out++ = '"';
}

/* ,"name": */
static void key(char *name)
{
    if (out[-1] != '{' && out[-1] != '[')
        *out++ = ',';
    *out++ = '"';
    put(name);
    put("\":");
}

static void field(char *name, int v)
{
    key(name);
    num(v);
}

static void city_name(int city)
{
    if (city < 0 || city >= 128 || townbuf[city].status == -1)
        put("null");
    else
        text(place_name[(unsigned char) townbuf[city].rest[10]]);
}

static void open_record(char *kind)
{
    out = record;
    put("{");
    key("kind");
    text(kind);
    field("turn", turn_tick);
    field("year", year);
    key("civ");
    text(civ_names[civnow]);
    key("leader");
    text(leader_names[civnow]);
}

static void send_record(void)
{
    put("}");
    *out = 0;
    civwasm_stats_send(record);
}

/* What the city can build: the game's own change list (ECONOMY.C
 * find_slot), units as their type, improvements and wonders as minus
 * their number. */
static int can_build(int city, int what)
{
    int civ = townbuf[city].owner, i;
    long bld = townbuf[city].buildings;
    if (what >= 0) {
        if (what >= 28 || (what == 25 && (marvel_site[17] == -1 || !has_tech(civ, 27))))
            return 0;
        return has_tech(civ, utypes[what].prereq) && !has_tech(civ, utypes[what].obsolete_by) && ((townbuf[city].status & 2) || utypes[what].domain != 2);
    }
    i = -what;
    if (i <= 24) {
        if (!(rule_options & 4) || !has_tech(civ, improvements[i].prereq) || (bld & (1L << (i - 1))) || (i == 7 && (bld & 1)) || (i == 15 && (bld & 0x8000L)) ||
            (i == 10 && !(bld & 0x10L) && has_tech(civ, improvements[5].prereq)) || (i == 12 && !(bld & 0x20L) && has_tech(civ, improvements[6].prereq)) ||
            (i == 20 && !(townbuf[city].status & 8)) || (i == 19 && !(bld & 0x4000L)) || ((i == 19 || i == 20 || i == 21) && (bld & 0x1c0000L)))
            return 0;
        return !(i > 21 && (marvel_site[19] == -1 || (game_flags & (1 << civ))));
    }
    return i <= 45 && has_tech(civ, improvements[i].prereq) && (marvel_site - 24)[i] == -1;
}

static char far *build_name(int what)
{
    return what >= 0 ? utypes[what].name : improvements[-what].name;
}

static void city_record(int city)
{
    int c = civnow, i, cost, net, settler_food;

    RECIV_SI(city * 28), city_process(city, -1);
    settler_food = civ_gov[c] > 1 ? 2 : 1;
    put("{");
    field("id", city);
    key("name");
    city_name(city);
    field("x", townbuf[city].x);
    field("y", townbuf[city].y);
    field("size", townbuf[city].size);
    key("disorder");
    put(townbuf[city].status & 1 ? "true" : "false");
    field("food", *(int *) &townbuf[city].rest[0]);
    field("food_box", (townbuf[city].size + 1) * 10);
    field("food_net", town_res_sums[0] - townbuf[city].size * 2 - colonist_homed * settler_food);
    field("settlers_homed", colonist_homed);
    net = (townbuf[city].status & 1) ? 0 : town_res_sums[1] - city_keep;
    field("shields", *(int *) &townbuf[city].rest[2]);
    field("shields_made", town_res_sums[1]);
    field("unit_support", city_keep);
    field("shields_net", net);
    key("building");
    if (townbuf[city].production >= 0) {
        text(utypes[townbuf[city].production].name);
        cost = utypes[townbuf[city].production].cost * 10;
    } else {
        text(improvements[-townbuf[city].production].name);
        cost = improvements[-townbuf[city].production].cost * 10;
    }
    field("building_cost", cost);
    if (net > 0)
        field("turns_left", clamp((cost - *(int *) &townbuf[city].rest[2] + net - 1) / net, 0, 999));
    key("change_due");
    put(city_change_due(city) ? "true" : "false");
    field("trade", town_res_sums[2]);
    field("corruption", city_leak);
    field("luxury", city_revel_sums);
    field("tax", taxes);
    field("science", learning_yield);
    field("happy", town_happy_ones_num);
    field("unhappy", town_unhappy_counts);
    field("specialists", city_experts_num);
    field("content", townbuf[city].size - town_happy_ones_num - town_unhappy_counts - city_experts_num);
    key("improvements");
    put("[");
    for (i = 0; i < 24; i++)
        if (townbuf[city].buildings & (1L << i)) {
            if (out[-1] != '[')
                put(",");
            text(improvements[i + 1].name);
        }
    put("]");
    key("wonders");
    put("[");
    for (i = 1; i <= 21; i++)
        if (marvel_site[i] == city) {
            if (out[-1] != '[')
                put(",");
            text(improvements[24 + i].name);
        }
    put("]");
    key("can_build");
    put("[");
    for (i = -45; i < 28; i++)
        if (i != 0 && can_build(city, i)) {
            if (out[-1] != '[')
                put(",");
            text(build_name(i));
        }
    put("]");
    key("trade_routes");
    put("[");
    for (i = 0; i < 3; i++)
        if (townbuf[city].rest[11 + i] != -1) {
            if (out[-1] != '[')
                put(",");
            city_name((unsigned char) townbuf[city].rest[11 + i]);
        }
    put("]");
    put("}");
}

static void unit_record(int u)
{
    struct unit *p = &armydata[civnow][u];

    put("{");
    field("id", u);
    key("type");
    text(utypes[p->type].name);
    field("x", p->x);
    field("y", p->y);
    key("terrain");
    text(terrains[terrain_at(p->x, p->y)].name);
    if (p->type == 0) {
        /* what a city here would have: the 8 tiles around, north first, clockwise */
        int d;
        key("around");
        put("[");
        for (d = 1; d <= 8; d++) {
            if (d > 1)
                put(",");
            text(terrains[terrain_at(wrap_x(p->x + spiral_x[d]), p->y + spiral_y[d])].name);
        }
        put("]");
    }
    if (civwasm_ai_cmd[civnow][u] == 'Z')
        field("ai_auto", 1);
    key("veteran");
    put(p->status & UNIT_VETERAN ? "true" : "false");
    key("fortified");
    put(p->status & UNIT_FORTIFIED ? "true" : "false");
    key("sentry");
    put(p->status & 1 ? "true" : "false");
    field("status", (unsigned char) p->status);
    field("moves_left", p->moves);
    if (p->goto_x != -1) {
        key("goto");
        put("[");
        num(p->goto_x);
        put(",");
        num(p->goto_y);
        put("]");
    }
    if (civwasm_road_to[civnow][u])
        field("road_to", civwasm_road_to[civnow][u]);
    if (civwasm_auto_work[civnow][u]) {
        key("auto_work");
        *out++ = '"';
        *out++ = civwasm_auto_work[civnow][u];
        *out++ = '"';
    }
    key("home");
    city_name(p->home_city);
    put("}");
}

static void rival_record(int r)
{
    int c = civnow, embassy = (treaties[c][r] & 0x40) != 0;

    put("{");
    key("civ");
    text(civ_names[r]);
    key("leader");
    text(leader_names[r]);
    key("contact");
    put(treaties[c][r] & 1 ? "true" : "false");
    key("peace");
    put(treaties[c][r] & 2 ? "true" : "false");
    key("at_war");
    put((treaties[c][r] & 3) == 1 ? "true" : "false");
    key("they_hold_grudge");
    put(treaties[r][c] & 8 ? "true" : "false");
    key("embassy");
    put(embassy ? "true" : "false");
    field("treaty_bits", treaties[c][r]);
    field("their_treaty_bits", treaties[r][c]);
    field("cities", city_count[r]);
    if (embassy) {
        key("government");
        text(government_names[civ_gov[r]]);
        field("gold", money[r]);
        field("advances", tech_count[r]);
    }
    put("}");
}

static void turn_record(void)
{
    int c = civnow, i, diff;

    open_record("turn");
    key("government");
    text(government_names[civ_gov[c]]);
    field("gold", money[c]);
    field("tax_rate", civ_toll[c] * 10);
    field("science_rate", sci[c] * 10);
    field("luxury_rate", (10 - civ_toll[c] - sci[c]) * 10);
    field("difficulty", cur_tier);
    field("citizens", citizens[c]);
    key("researching");
    if (selected_index >= 0)
        text(advances[selected_index].name);
    else
        put("null");
    diff = cur_tier * 2 + cost_penalty + 6;
    if (diff < 11 - tech_count[c])
        diff = 11 - tech_count[c];
    field("research_progress", civ_research_tally[c]);
    field("research_cost", tech_count[c] * (year < 0 ? 1 : 2) * diff);
    key("advances");
    put("[");
    for (i = 0; i < 71; i++)
        if (has_tech(c, i)) {
            if (out[-1] != '[')
                put(",");
            text(advances[i].name);
        }
    put("]");
    key("can_research");
    put("[");
    for (i = 0; i < 71; i++)
        if (!has_tech(c, i) && has_tech(c, advances[i].prereq1) && has_tech(c, advances[i].prereq2)) {
            if (out[-1] != '[')
                put(",");
            text(advances[i].name);
        }
    put("]");
    key("wonders_elsewhere");
    put("[");
    for (i = 1; i <= 21; i++)
        if (marvel_site[i] != -1 && townbuf[marvel_site[i]].owner != c) {
            if (out[-1] != '[')
                put(",");
            text(improvements[24 + i].name);
        }
    put("]");
    key("cities");
    put("[");
    for (i = 0; i < 128; i++)
        if (townbuf[i].status != -1 && townbuf[i].owner == c) {
            if (out[-1] != '[')
                put(",");
            city_record(i);
        }
    put("]");
    key("units");
    put("[");
    for (i = 0; i < 128; i++)
        if (armydata[c][i].type != -1) {
            if (out[-1] != '[')
                put(",");
            unit_record(i);
        }
    put("]");
    key("rivals");
    put("[");
    for (i = 1; i < 8; i++)
        if (i != c && (players & (1 << i))) {
            if (out[-1] != '[')
                put(",");
            rival_record(i);
        }
    put("]");
    send_record();
}

/* The start of the player's turn (TURN.C game_turn): the record, and
 * with AI plays on, its plan. */
void civwasm_stats_turn(void)
{
    if (!civwasm_debug_stats || civnow < 0)
        return;
    turn_record();
    if (civwasm_ai_drive)
        civwasm_ai_wait_plan();
}

/* A record of the turn as it stands (the page, when AI plays starts). */
void civwasm_stats_now(void)
{
    if (civnow >= 0 && turn_tick > 0)
        turn_record();
}

/* A battle the player fights (BATTLE.C battle), once it is decided. */
void civwasm_stats_battle(int attacker, int attacker_type, int defender, int defender_type, int x, int y, int attacker_won)
{
    int city;

    if (!civwasm_debug_stats || (attacker != civnow && defender != civnow))
        return;
    open_record("battle");
    key("attacker");
    text(attacker == 0 ? "Barbarians" : civ_names[attacker]);
    key("attacker_unit");
    text(utypes[attacker_type].name);
    key("defender");
    text(defender == 0 ? "Barbarians" : civ_names[defender]);
    key("defender_unit");
    text(utypes[defender_type].name);
    field("x", x);
    field("y", y);
    key("attacker_won");
    put(attacker_won ? "true" : "false");
    key("city");
    for (city = 0; city < 128; city++)
        if (townbuf[city].status != -1 && townbuf[city].x == x && townbuf[city].y == y)
            break;
    city_name(city);
    send_record();
}

/* ------------------------------------------------------------ AI plays
 * The page's plan, applied through these: each checks the game's rules
 * and returns 1 when done. Units get standing orders: a GoTo now, or a
 * key their next turn takes as the player's (civwasm_ai_key). */
int civwasm_ai_build(int city, int what)
{
    if (city < 0 || city >= 128 || townbuf[city].status == -1 || townbuf[city].owner != civnow || !can_build(city, what))
        return 0;
    townbuf[city].production = (char) what;
    return 1;
}

/* Tax and science in tenths; luxury is the rest. */
int civwasm_ai_rates(int tax, int science)
{
    if (tax < 0 || science < 0 || tax + science > 10)
        return 0;
    civ_toll[civnow] = tax;
    sci[civnow] = science;
    return 1;
}

int civwasm_ai_research(int adv)
{
    int c = civnow;
    if (adv < 0 || adv >= 71 || has_tech(c, adv) || !has_tech(c, advances[adv].prereq1) || !has_tech(c, advances[adv].prereq2))
        return 0;
    selected_index = adv;
    return 1;
}

/* order: 'g' GoTo (x, y), or a key the unit takes on its next move: 1-8
 * a step (north, then clockwise), and the game's own keys (f fortify,
 * s sentry, b found or join a city, r road, i irrigate, m mine, I M A L E
 * the Auto orders, w wait, space no orders, D disband, h home city). */
int civwasm_ai_unit(int unit, int order, int x, int y)
{
    struct unit *u;
    if (unit < 0 || unit >= 128 || armydata[civnow][unit].type == -1)
        return 0;
    u = &armydata[civnow][unit];
    if (order == 'g') {
        if (x < 0 || x >= 80 || y < 1 || y > 48)
            return 0;
        u->status &= 0x30;
        civwasm_auto_work[civnow][unit] = civwasm_road_to[civnow][unit] = 0;
        civwasm_ai_cmd[civnow][unit] = 0;
        u->goto_x = (char) x;
        u->goto_y = (char) y;
        return 1;
    }
    u->status &= 0x30;          /* woken from sentry or fortified to take it */
    u->goto_x = -1;
    civwasm_auto_work[civnow][unit] = civwasm_road_to[civnow][unit] = 0;
    civwasm_ai_cmd[civnow][unit] = (char) order;
    return 1;
}

void civwasm_ai_planned(void)
{
    plan_turn = turn_tick;
}

/* The key for a unit waiting for orders (CONTROL.C), or Enter at the end
 * of the turn. Waits for the turn's plan first. */
int civwasm_ai_key(int civ, int unit)
{
    int k;
    if (plan_turn != turn_tick)
        civwasm_ai_wait_plan();
    if (unit >= 128)
        return 13;
    k = civwasm_ai_cmd[civ][unit];
    if (k == 'Z') {
        /* the game's own computer-player orders, every turn until replaced */
        k = ai_order(civ, unit);
        if (k)
            armydata[civ][unit].goto_x = -1;
        return k;
    }
    civwasm_ai_cmd[civ][unit] = 0;
    return k ? k : ' ';
}
