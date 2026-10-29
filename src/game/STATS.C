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

extern struct city townbuf[];
extern struct unit armydata[8][128];
extern struct unit_type utypes[];
extern int treaties[8][8];
extern char far place_name[][13];
extern int marvel_site[];
extern int city_process(int, int);

int civwasm_debug_stats;        /* --debug-stats (WIDE.H; set while playing too) */

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

/* The start of the player's turn (TURN.C game_turn). */
void civwasm_stats_turn(void)
{
    int c = civnow, i, diff;

    if (!civwasm_debug_stats || c < 0)
        return;
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
