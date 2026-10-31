/* Resident F1E31 pathfinding: complete 1E31:[000E,11C0) contribution.
 * Chooses goto moves with coarse connectivity and a cached local cost search.
 * /AM /Gs /Oas /Zl /c /W3 /Fc /NTF1E31; shared buffers remain external.
 * Local names reproduce MSC frame allocation; unused words are observed.
 */
#include <stdlib.h>
#include <string.h>
#include "CIV.H"
#include "GAME.H"
#include "GLOBALS.H"
#include "RESIDENT.H"
#include "MAP.H"
#include "PATH.H"
#include "CORE.H"
#include "UNITS.H"

/* Signed movement costs, as in UNITAI's local terrain declaration. */
struct path_terrain {
    char name[12], cost, defense, food, shields, rest[3];
};
extern struct unit armydata[8][128];
/* Path searches run on a probe unit: civilization 0's last unit slot. */
#define path_probe_x armydata[0][127].x
#define path_probe_y armydata[0][127].y
#define path_probe_domain armydata[0][127].type
extern struct unit_type utypes[28];
extern struct path_terrain terrains[];
extern int map_flags(int, int), terrain_at(int, int);
extern int map_stack(int, int);

int wrap_x(int);

extern char coarse_ocean_map[20][13], block_ground_way[20][13];

extern int map_owner(int, int);

int unit_goal(int player, int unit)
{
    int best_cost_j, tile_owner_g, ty, tx, blocked_origin, terrain_f, i, new_distance_e, best_direction_k, ny, nx, failed, on_road_c, old_distance_k, move_cost_d, turn_angle_c, remaining_y_o, remaining_x_p;
    if (player == civnow) {
        tx = (unsigned char) armydata[player][unit].goto_x - armydata[player][unit].x;
        ty = (unsigned char) armydata[player][unit].goto_y - armydata[player][unit].y;
        if (abs(tx) <= 1 && abs(ty) <= 1) {
            if (abs(tx) < 40)
                tx = map_random(tx);
            else
                tx = -map_random(tx);
            ty = map_random(ty);
            armydata[player][unit].goto_x = -1;
            for (i = 1; i <= 8; i++) {
                if (spiral_x[i] == tx && spiral_y[i] == ty)
                    return i;
            }
            return 0;
        }
    }
    if (utypes[armydata[player][unit].type].domain == 1) {
        path_target_x = (unsigned char) armydata[player][unit].goto_x;
        path_target_y = (unsigned char) armydata[player][unit].goto_y;
    } else {
        failed = 0;
        if ((abs((unsigned char) armydata[player][unit].goto_x - armydata[player][unit].x) < 7 || abs((unsigned char) armydata[player][unit].goto_x - armydata[player][unit].x) > 73) && abs((unsigned char) armydata[player][unit].goto_y - armydata[player][unit].y) < 7) {
            path_target_x = (unsigned char) armydata[player][unit].goto_x;
            path_target_y = (unsigned char) armydata[player][unit].goto_y;
            best_direction_k = path_search(player, unit, 999);
            if (best_direction_k != -1)
                return best_direction_k;
            failed = 1;
        }
        if (path_coarse(player, unit) || !failed) {
            best_direction_k = path_search(player, unit, 999);
            if (best_direction_k != -1)
                return best_direction_k;
        }
    }
    tx = path_target_x - armydata[player][unit].x;
    ty = path_target_y - armydata[player][unit].y;
    old_distance_k = abs(tx) + abs(ty) + (abs(tx) > abs(ty) ? abs(tx) : abs(ty));
    if (tx == 0 && ty == 0) {
        armydata[player][unit].goto_x = armydata[player][unit].f8 = -1;
        armydata[player][unit].moves = 0;
        return -1;
    }
    best_cost_j = 9999;
    best_direction_k = 0;
    on_road_c = map_flags(armydata[player][unit].x, armydata[player][unit].y) & 8;
    blocked_origin = if_open(player, armydata[player][unit].x, armydata[player][unit].y);
    for (i = 1; i < 9; i++) {
        nx = armydata[player][unit].x + spiral_x[i];
        ny = armydata[player][unit].y + spiral_y[i];
        remaining_x_p = tx - spiral_x[i];
        remaining_y_o = ty - spiral_y[i];
        new_distance_e = abs(remaining_x_p) + abs(remaining_y_o) + (abs(remaining_x_p) > abs(remaining_y_o) ? abs(remaining_x_p) : abs(remaining_y_o));
        if (player == civnow && new_distance_e > old_distance_k)
            continue;
        terrain_f = terrain_at(nx, ny);
        tile_owner_g = map_owner(nx, ny);
        if ((tile_owner_g != -1 && tile_owner_g != player) || (((terrain_f == 10) != (utypes[armydata[player][unit].type].domain == 2) || (blocked_origin && if_open(player, nx, ny))) && utypes[armydata[player][unit].type].domain != 1)) {
            if (!(map_flags(nx, ny) & 1) || map_city(nx, ny) != player)
                continue;
        }
        if (terrain_f == 10 && map_region(nx, ny) < 5)
            continue;
        if (!on_road_c || !(map_flags(nx, ny) & 8))
            move_cost_d = utypes[armydata[player][unit].type].moves > 1 ? terrains[terrain_f].cost * 3 : 3;
        else
            move_cost_d = 1;
        move_cost_d += new_distance_e * 4 + abs(remaining_x_p) + abs(remaining_y_o);
        if (armydata[player][unit].f8 != -1) {
            /* Prefer a smooth heading: squared turn angle breaks distance/cost ties. */
            turn_angle_c = abs(armydata[player][unit].f8 - i);
            if (turn_angle_c > 4)
                turn_angle_c = 8 - turn_angle_c;
            move_cost_d += turn_angle_c * turn_angle_c;
        }
        if (move_cost_d < best_cost_j) {
            best_direction_k = i;
            best_cost_j = move_cost_d;
        }
    }
    if (armydata[player][unit].f8 != -1 && (armydata[player][unit].f8 ^ 4) == best_direction_k)
        best_direction_k = armydata[player][unit].moves = 0;
    if (best_direction_k == 0) {
        armydata[player][unit].goto_x = armydata[player][unit].f8 = -1;
        best_direction_k = -1;
    } else
        armydata[player][unit].f8 = best_direction_k;
    return best_direction_k;
}

int path_coarse(int player, int unit)
{
    int cy_d, unused_k, cx_d, ny_g, unused_j, nx_g, i3, y2, unused_h, unused_g, unused_f, unused_v, unused_e, unused_u, unused_d, unused_t, unused_c, unused_s, sea_m, x39, dist, mask_h, best_f, found_g, cost_j, nearest_p, value_e, tail_g, head_o;
    cx_d = armydata[player][unit].x;
    cy_d = armydata[player][unit].y;
    path_target_x = (unsigned char) armydata[player][unit].goto_x;
    path_target_y = (unsigned char) armydata[player][unit].goto_y;
    if (path_select(cx_d, cy_d, utypes[armydata[player][unit].type].domain == 2)) {
        cx_d = path_target_x;
        cy_d = path_target_y;
    } else {
        path_target_x = (unsigned char) armydata[player][unit].goto_x;
        path_target_y = (unsigned char) armydata[player][unit].goto_y;
        /* Original bare return leaves the last coordinate in AX. */
        return RECIV_UB(path_target_y, "path_coarse: AX holds path_target_y");
    }
    path_select((unsigned char) armydata[player][unit].goto_x, (unsigned char) armydata[player][unit].goto_y, utypes[armydata[player][unit].type].domain == 2);
    memset(block_steps, 0, 260);
    head_o = 0;
    tail_g = 0;
    path_queue_x[head_o] = path_target_x;
    path_queue_y[head_o++] = path_target_y;
    block_steps[path_target_x][path_target_y] = 1;
    found_g = 0;
    sea_m = utypes[armydata[player][unit].type].domain == 2;
    do {
        x39 = path_queue_x[tail_g];
        y2 = path_queue_y[tail_g];
        if (x39 == cx_d && y2 == cy_d) {
            found_g = 1;
            break;
        }
        cost_j = block_steps[x39][y2];
        tail_g++;
        tail_g = (unsigned char) tail_g;
        mask_h = sea_m ? coarse_ocean_map[x39][y2] : block_ground_way[x39][y2];
        for (i3 = 1; i3 <= 8; i3++) {
            if ((1 << (i3 - 1)) & mask_h) {
                nx_g = x39 + spiral_x[i3];
                if (nx_g == 20)
                    nx_g = 0;
                if (nx_g == -1)
                    nx_g = 19;
                ny_g = y2 + spiral_y[i3];
                if (!block_steps[nx_g][ny_g]) {
                    block_steps[nx_g][ny_g] = cost_j + 1;
                    path_queue_x[head_o] = nx_g;
                    path_queue_y[head_o] = ny_g;
                    head_o++;
                    head_o = (unsigned char) head_o;
                }
            }
        }
    } while (!found_g && tail_g != head_o);
    path_target_x = -1;
    if (found_g) {
        nearest_p = 99;
        best_f = -1;
        mask_h = utypes[armydata[player][unit].type].domain == 2 ? coarse_ocean_map[cx_d][cy_d] : block_ground_way[cx_d][cy_d];
        for (i3 = 1; i3 <= 8; i3++) {
            if ((1 << (i3 - 1)) & mask_h) {
                nx_g = cx_d + spiral_x[i3];
                ny_g = cy_d + spiral_y[i3];
                if (nx_g == 20)
                    nx_g = 0;
                if (nx_g == -1)
                    nx_g = 19;
                value_e = block_steps[nx_g][ny_g];
                if (value_e) {
                    if (value_e < nearest_p) {
                        nearest_p = value_e;
                        best_f = i3;
                        cost_j = map_dist((unsigned char) armydata[player][unit].goto_x, nx_g * 4 + 1, (unsigned char) armydata[player][unit].goto_y, ny_g * 4 + 1);
                    } else if (value_e == nearest_p) {
                        dist = map_dist((unsigned char) armydata[player][unit].goto_x, nx_g * 4 + 1, (unsigned char) armydata[player][unit].goto_y, ny_g * 4 + 1);
                        if (dist < cost_j) {
                            best_f = i3;
                            cost_j = dist;
                        }
                    }
                }
            }
        }
        if (best_f != -1) {
            path_target_x = (cx_d + spiral_x[best_f]) * 4 + 1;
            path_target_x = wrap_x(path_target_x);
            path_target_y = (cy_d + spiral_y[best_f]) * 4 + 1;
            if ((terrain_at(path_target_x, path_target_y) == 10) != sea_m) {
                path_target_x++;
                if ((terrain_at(path_target_x, path_target_y) == 10) != sea_m)
                    path_target_y++;
            }
        }
    }
    if (path_target_x == -1) {
        path_target_x = (unsigned char) armydata[player][unit].goto_x;
        path_target_y = (unsigned char) armydata[player][unit].goto_y;
    }
    return found_g;
}

int path_select(int x, int y, int sea)
{
    int cy_c, cx_b, i2, best_l, ny_b, nx, py_l, nearest_b, dist_n, px_i;
    cx_b = x / 4;
    cy_c = y / 4;
    best_l = -1;
    if (sea) {
        if (coarse_ocean_map[cx_b][cy_c])
            best_l = 0;
    } else {
        if (block_ground_way[cx_b][cy_c])
            best_l = 0;
    }
    if (best_l == -1) {
        nearest_b = 99;
        for (i2 = 1; i2 <= 8; i2++) {
            nx = cx_b + spiral_x[i2];
            ny_b = cy_c + spiral_y[i2];
            if ((sea && coarse_ocean_map[nx][ny_b]) || (!sea && block_ground_way[nx][ny_b])) {
                dist_n = half_dist(x - nx * 4 - 1, y - ny_b * 4 - 1);
                if (dist_n < nearest_b) {
                    px_i = nx * 4 + 1;
                    py_l = ny_b * 4 + 1;
                    if ((terrain_at(px_i, py_l) == 10) == sea) {
                        if (path_probe(px_i, py_l, x, y, sea, 18) != -1) {
                            nearest_b = dist_n;
                            best_l = i2;
                        }
                        continue;
                    }
                    px_i++;
                    if ((terrain_at(px_i, py_l) == 10) == sea) {
                        if (path_probe(px_i, py_l, x, y, sea, 18) != -1) {
                            nearest_b = dist_n;
                            best_l = i2;
                        }
                        continue;
                    }
                    py_l++;
                    if ((terrain_at(px_i, py_l) == 10) == sea) {
                        if (path_probe(px_i, py_l, x, y, sea, 18) != -1) {
                            nearest_b = dist_n;
                            best_l = i2;
                        }
                        continue;
                    }
                    px_i--;
                    if ((terrain_at(px_i, py_l) == 10) == sea) {
                        if (path_probe(px_i, py_l, x, y, sea, 18) != -1) {
                            nearest_b = dist_n;
                            best_l = i2;
                        }
                    }

                }
            }
        }
    }
    if (best_l != -1) {
        path_target_x = cx_b + spiral_x[best_l];
        path_target_y = cy_c + spiral_y[best_l];
        return 1;
    }
    return 0;
}

int path_search(int player, int unit, int limit)
{
    int origin_y_p, unused_l, origin_x_p, next_y_h, dy, raw_x_m, terrain_h, i3, base_y_j, previous_o, y2, unused_w, unused_af, unused_ae, unused_au, unused_bd, unused_ad, unused_at, unused_d, unused_t, unused_c, base_x_e, sea_n, x49, new_cost_d, unused_a, best_g, best_cost_n, stack_n, road_n, current_cost_h, next_x_m, old_cost_k, region_n, nx_k, tail_g, head_o;
    origin_x_p = armydata[player][unit].x;
    origin_y_p = armydata[player][unit].y;
    base_x_e = path_target_x - 8;
    base_y_j = path_target_y - 8;
    path_limit = 0;
    if (path_target_x != path_cache_x || path_target_y != path_cache_y || !walk_cost[origin_x_p - base_x_e][origin_y_p - base_y_j]) {
        path_cache_x = path_target_x;
        path_cache_y = path_target_y;
        memset(walk_cost, 0, 256);
        head_o = 0;
        tail_g = 0;
        path_queue_x[head_o] = path_target_x;
        path_queue_y[head_o++] = path_target_y;
        walk_cost[path_target_x - base_x_e][path_target_y - base_y_j] = 1;
        best_cost_n = 0;
        path_limit = limit;
        sea_n = utypes[armydata[player][unit].type].domain == 2;
        road_n = utypes[armydata[player][unit].type].moves == 1;
        do {
            x49 = path_queue_x[tail_g];
            y2 = path_queue_y[tail_g];
            tail_g++;
            tail_g = (unsigned char) tail_g;
            current_cost_h = walk_cost[x49 - base_x_e][y2 - base_y_j];
            if (current_cost_h <= path_limit) {
                if (wrap_x(x49) == origin_x_p && y2 == origin_y_p)
                    path_limit = current_cost_h;
                else {
                    region_n = has_road(wrap_x(x49), y2);
                    for (i3 = 1; i3 <= 8; i3++) {
                        raw_x_m = x49 + spiral_x[i3];
                        if (abs(raw_x_m - path_target_x) >= 8)
                            continue;
                        next_x_m = wrap_x(raw_x_m);
                        next_y_h = y2 + spiral_y[i3];
                        if (abs(next_y_h - path_target_y) >= 8)
                            continue;
                        if (!map_in_bounds(next_x_m, next_y_h))
                            continue;
                        terrain_h = terrain_at(next_x_m, next_y_h);
                        if ((terrain_h == 10) != sea_n && !(map_flags(next_x_m, next_y_h) & 1))
                            continue;
                        new_cost_d = region_n && has_road(next_x_m, next_y_h) ? current_cost_h + 1 : road_n ? current_cost_h + 3 : current_cost_h + terrains[terrain_h].cost * 3;
                        previous_o = walk_cost[raw_x_m - base_x_e][next_y_h - base_y_j];
                        if (previous_o && previous_o <= new_cost_d)
                            continue;
                        walk_cost[raw_x_m - base_x_e][next_y_h - base_y_j] = new_cost_d;
                        path_queue_x[head_o] = raw_x_m;
                        path_queue_y[head_o] = next_y_h;
                        head_o++;
                        head_o = (unsigned char) head_o;
                    }
                }
            }
        } while (tail_g != head_o && tail_g < 225);
    }
    best_g = -1;
    if (limit > path_limit) {
        old_cost_k = 99;
        for (i3 = 1; i3 <= 8; i3++) {
            raw_x_m = origin_x_p + spiral_x[i3];
            if (abs(raw_x_m - path_target_x) >= 72) {
                if (raw_x_m > path_target_x)
                    raw_x_m -= 80;
                else
                    raw_x_m += 80;
            }
            if (abs(raw_x_m - path_target_x) >= 8)
                continue;
            next_x_m = wrap_x(raw_x_m);
            next_y_h = origin_y_p + spiral_y[i3];
            if (abs(next_y_h - path_target_y) >= 8)
                continue;
            if ((terrain_at(next_x_m, next_y_h) == 10) != (utypes[armydata[player][unit].type].domain == 2) && !(map_flags(next_x_m, next_y_h) & 1))
                continue;
            nx_k = walk_cost[raw_x_m - base_x_e][next_y_h - base_y_j];
            if (!nx_k)
                continue;
            if (nx_k < old_cost_k) {
                old_cost_k = nx_k;
                best_g = i3;
                stack_n = map_stack(next_x_m, next_y_h);
                if (stack_n != -1)
                    current_cost_h = stack_len(player, stack_n, 2) * 4;
                else
                    current_cost_h = 0;
                current_cost_h += map_dist(path_target_x, next_x_m, path_target_y, next_y_h);
            }
            if (nx_k == old_cost_k) {
                stack_n = map_stack(next_x_m, next_y_h);
                if (stack_n != -1)
                    new_cost_d = stack_len(player, stack_n, 2) * 4;
                else
                    new_cost_d = 0;
                new_cost_d += map_dist(path_target_x, next_x_m, path_target_y, next_y_h);
                if (new_cost_d < current_cost_h) {
                    best_g = i3;
                    current_cost_h = new_cost_d;
                }
            }
        }
        if (best_g != -1)
            return best_g;
    }
    if (best_g == -1) {
        path_target_x = (unsigned char) armydata[player][unit].goto_x;
        path_target_y = (unsigned char) armydata[player][unit].goto_y;
    }
    return -1;
}

int path_probe(int x, int y, int target_x, int target_y, int sea, int limit)
{
    int result;
    /* Original test accepts closeness in either coordinate. */
    if (abs(x - target_x) <= 7 || abs(y - target_y) <= 7) {
        path_probe_x = x;
        path_probe_y = y;
        path_probe_domain = sea ? 16 : 1;
        path_target_x = target_x;
        path_target_y = target_y;
        result = path_search(0, 127, limit);
        path_probe_domain = -1;
        if (result != -1)
            return path_limit;
    }
    return -1;
}

int wrap_x(int x)
{
    if (x < 0)
        return x + 80;
    else if (x >= 80)
        return x - 80;
    return x;
}

/* civ-wasm: Road To (Shift+T), a settlers' GoTo that builds road on every
 * tile it crosses: 'r' while its tile needs it, then unit_goal's step. With
 * Railroad known, the settlers then walk back to where they got the order
 * (civwasm_auto_x, civwasm_auto_y), laying railroad on the way (and road
 * first where the way back has none). It stops for orders (-1, its GoTo cleared) at the target, before a tile
 * that cannot take a road (a river before Bridge Building), and when
 * another civilization's unit stands next to it. The order is kept per
 * unit here, and in saves after the game's own data (SAVE.C). */
char civwasm_road_to[8][128];          /* 1 road there, 3 railroad back (2: Rail To of older saves, as 1) */
extern unsigned char civwasm_auto_x[8][128], civwasm_auto_y[8][128];

extern int has_tech(int, int);

static int road_ok(int player, int x, int y)
{
    int t = terrain_at(x, y);
    if (map_flags(x, y) & 9)    /* a road or a city already */
        return 1;
    return t != 10 && (t != 11 || has_tech(player, 19));
}

/* The tile still wants what 'r' would build there: road, or with `rail`
 * railroad (CONTROL.C's 'r' case: a city tile takes neither). */
static int lay_needed(int x, int y, int rail)
{
    int f = map_flags(x, y);
    if (f & 1)
        return 0;
    return rail ? !(f & 16) : !(f & 8);
}

int road_to_goal(int player, int unit)
{
    struct unit *u = &armydata[player][unit];
    int i, d, x, y, tx, ty, owner;
    int rail = civwasm_road_to[player][unit] == 3;
    for (i = 1; i < 9; i++) {
        y = u->y + spiral_y[i];
        if (y < 0 || y > 49)
            continue;
        owner = map_owner(wrap_x(u->x + spiral_x[i]), y);
        if (owner != -1 && owner != player)
            goto stop;
    }
    if (lay_needed(u->x, u->y, rail)) {
        if (!road_ok(player, u->x, u->y))
            goto stop;
        return 'r';
    }
    tx = (unsigned char) u->goto_x;
    ty = (unsigned char) u->goto_y;
    if (u->x == tx && u->y == ty) {
        if (rail || !has_tech(player, 49) ||
            (u->x == civwasm_auto_x[player][unit] && u->y == civwasm_auto_y[player][unit]))
            goto stop;
        civwasm_road_to[player][unit] = 3;      /* the road is there: railroad back */
        u->goto_x = (char) civwasm_auto_x[player][unit];
        u->goto_y = (char) civwasm_auto_y[player][unit];
        u->f8 = -1;
        return road_to_goal(player, unit);
    }
    d = unit_goal(player, unit);
    if (d < 1 || d > 8) {
        civwasm_road_to[player][unit] = 0;
        return d;
    }
    x = wrap_x(u->x + spiral_x[d]);
    y = u->y + spiral_y[d];
    if (!road_ok(player, x, y))
        goto stop;
    if (x == tx && y == ty) {
        /* unit_goal ends a GoTo on its last step; the target's road is still to build. */
        u->goto_x = (char) tx;
        u->goto_y = (char) ty;
    }
    return d;
stop:
    u->goto_x = u->f8 = -1;
    u->status &= ~2;
    u->fuel = 0;
    civwasm_road_to[player][unit] = 0;
    return -1;
}

/* civ-wasm: Auto Irrigate (Shift+I) and Auto Mine (Shift+M). Auto Mine
 * improves the tiles around where it was given, nearest that place first,
 * out to 6 tiles; Auto Irrigate the tiles of the player's cities. Both keep
 * to the landmass. The tiles are the game's own
 * choice for computer players' settlers (CONTROL.C work_tile): Hills are
 * mined, and Desert, Plains, Grassland and River are irrigated where water
 * reaches them; Forest, Mountains, Swamp, Jungle, Tundra and Arctic are
 * left alone, as are tiles already irrigated or mined and city tiles.
 * The choice is made again at every move and every finished tile, so an
 * irrigated tile opens the next ones. With nothing left to do, the
 * settlers ask for orders. Enemies do not interrupt them.
 *
 * Auto Mine given where nothing is left to mine is level 2 ('n'): it mines
 * the Mountains around, which the land's own rule leaves alone. */
char civwasm_auto_work[8][128];         /* 'i', 'm', 'n' (Mountains), 'a' (Auto Road; 'A', 'l', 'L' in older saves), 'e' (Auto Explore); 0 none */
unsigned char civwasm_auto_x[8][128], civwasm_auto_y[8][128];   /* where the order was given */

extern int work_tile(int, int), region_key(int, int);
extern char far map_visible_to[];

static int wrap_dist(int a, int b)
{
    int d = abs(a - b);
    return d > 40 ? 80 - d : d;
}

/* Another auto settler already works the tile or is on its way there. */
static int auto_taken(int player, int unit, int x, int y)
{
    int i;
    struct unit *o;
    for (i = 0; i < 128; i++) {
        o = &armydata[player][i];
        if (i == unit || o->type != 0 || !civwasm_auto_work[player][i])
            continue;
        if ((o->x == x && o->y == y) || ((unsigned char) o->goto_x == x && (unsigned char) o->goto_y == y))
            return 1;
    }
    return 0;
}

/* The tile is one this order works. */
static int auto_wanted(int order, int x, int y)
{
    if (order == 'n')
        return terrain_at(x, y) == 5 && !(map_flags(x, y) & 4);
    return work_tile(x, y) == (order == 'i' ? 1 : 2);
}

/* Auto Irrigate works for the player's cities on this landmass: the tiles
 * of their radius, the nearest first, a tile the city works before one it
 * does not, and a city with as many irrigated tiles as citizens only once
 * the others have theirs. */
extern struct city townbuf[];
extern int radius_dx[21], radius_dy[21];

static int irrigate_target(int player, int unit, int region, int *bx, int *by)
{
    struct unit *u = &armydata[player][unit];
    int best = 32767, c, i, x, y, irrigated, d, score;
    long worked;
    for (c = 0; c < 128; c++) {
        if (townbuf[c].status == -1 || townbuf[c].owner != player || region_key(townbuf[c].x, townbuf[c].y) != region)
            continue;
        irrigated = 0;
        for (i = 0; i < 20; i++) {
            x = wrap_x(townbuf[c].x + radius_dx[i]);
            y = townbuf[c].y + radius_dy[i];
            if (y >= 0 && y < 50 && (map_flags(x, y) & 2))
                irrigated++;
        }
        worked = *(long *) &townbuf[c].rest[4];
        for (i = 0; i < 20; i++) {
            x = wrap_x(townbuf[c].x + radius_dx[i]);
            y = townbuf[c].y + radius_dy[i];
            if (y < 1 || y > 48 || !(map_visible_to[x * 50 + y] & (1 << player)))
                continue;
            if (terrain_at(x, y) == 10 || region_key(x, y) != region || !auto_wanted('i', x, y))
                continue;
            d = map_owner(x, y);
            if ((d != -1 && d != player) || auto_taken(player, unit, x, y))
                continue;
            d = wrap_dist(u->x, x) > abs(u->y - y) ? wrap_dist(u->x, x) : abs(u->y - y);
            score = 4 * d - (((worked >> i) & 1) ? 3 : 0) + (irrigated >= townbuf[c].size ? 60 : 0);
            if (score < best) {
                best = score;
                *bx = x;
                *by = y;
            }
        }
    }
    return best != 32767;
}

int auto_work_goal(int player, int unit)
{
    struct unit *u = &armydata[player][unit];
    int order = civwasm_auto_work[player][unit], kind = order == 'i' ? 1 : 2, key = kind == 1 ? 'i' : 'm';
    int ax = civwasm_auto_x[player][unit], ay = civwasm_auto_y[player][unit];
    int region = region_key(ax, ay), best = 9999, bx = -1, by = 0;
    int dx, dy, x, y, d, owner;
    /* Work under way goes on. */
    if ((u->status & (kind == 1 ? 0x40 : 0x80)) && auto_wanted(order, u->x, u->y))
        return key;
    if (order == 'i') {
        if (!irrigate_target(player, unit, region_key(u->x, u->y), &bx, &by))
            goto stop;
        goto chosen;
    }
    for (dy = -6; dy <= 6; dy++)
        for (dx = -6; dx <= 6; dx++) {
            x = wrap_x(ax + dx);
            y = ay + dy;
            if (y < 1 || y > 48 || !(map_visible_to[x * 50 + y] & (1 << player)))
                continue;
            if (terrain_at(x, y) == 10 || region_key(x, y) != region || !auto_wanted(order, x, y))
                continue;
            owner = map_owner(x, y);
            if ((owner != -1 && owner != player) || auto_taken(player, unit, x, y))
                continue;
            d = 16 * (abs(dx) > abs(dy) ? abs(dx) : abs(dy));
            d += wrap_dist(u->x, x) > abs(u->y - y) ? wrap_dist(u->x, x) : abs(u->y - y);
            if (d < best) {
                best = d;
                bx = x;
                by = y;
            }
        }
    if (bx == -1)
        goto stop;
chosen:
    if (bx == u->x && by == u->y)
        return key;
    u->goto_x = (char) bx;
    u->goto_y = (char) by;
    d = unit_goal(player, unit);
    if (d < 1 || d > 8)
        goto stop;
    return d;
stop:
    civwasm_auto_work[player][unit] = 0;
    u->goto_x = u->f8 = -1;
    return -1;
}

/* civ-wasm: Auto Road (Shift+R): settlers lay road, and with Railroad
 * known railroad, over the landmass where they got the order, in three
 * stages, each road first and then railroad:
 * 1. Links between the player's cities there: a shortest network (Kruskal
 *    over each city's six nearest cities), then every other link to those
 *    six within 12 tiles, for more ways between the cities. Each link is a
 *    straight-ish line of steps that always come closer to the far city,
 *    over land that can take a road and past no foreign city or unit; a
 *    link that cannot be walked so is left out.
 * 2. The cities' own tiles, the nearest first, a tile the city works or
 *    that is irrigated before others.
 * 3. Every other tile of the landmass that they have seen and no one else
 *    holds, the nearest first.
 * Within a stage they work the tile nearest to them that no other auto
 * settler has taken, chosen again at every move; when nothing is left
 * they ask for orders. */
extern int map_city(int, int);
extern struct city townbuf[128];

static int cheb(int x1, int y1, int x2, int y2)
{
    int dx = wrap_dist(x1, x2), dy = abs(y1 - y2);
    return dx > dy ? dx : dy;
}

static int wrap_dx(int from, int to)    /* to - from, the short way round */
{
    return (to - from + 120) % 80 - 40;
}

/* The next step from (x, y) on the link (x0, y0) to (tx, ty), or -1. */
static int link_step(int player, int x0, int y0, int x, int y, int tx, int ty, int rail)
{
    int i, x2, y2, d0 = cheb(x, y, tx, ty), s, best = -1, best_s = 32767, owner, cross;
    for (i = 1; i < 9; i++) {
        x2 = wrap_x(x + spiral_x[i]);
        y2 = y + spiral_y[i];
        if (y2 < 1 || y2 > 48 || cheb(x2, y2, tx, ty) >= d0)
            continue;
        if (!(x2 == tx && y2 == ty)) {
            if (!road_ok(player, x2, y2))
                continue;
            if ((map_flags(x2, y2) & 1) && map_city(x2, y2) != player)
                continue;
            owner = map_owner(x2, y2);
            if (owner != -1 && owner != player)
                continue;
        }
        cross = wrap_dx(x0, tx) * (y2 - y0) - (ty - y0) * wrap_dx(x0, x2);
        s = (lay_needed(x2, y2, rail) ? 8 * terrains[terrain_at(x2, y2)].cost : 0) + abs(cross);
        if (s < best_s) {
            best_s = s;
            best = i;
        }
    }
    return best;
}

/* A tile of stage 2 or 3: on the landmass, seen, on no one else's ground,
 * and still wanting what `rail` asks for. */
static int road_wanted(int player, int region, int x, int y, int rail)
{
    int owner;
    if (y < 1 || y > 48 || !(map_visible_to[x * 50 + y] & (1 << player)))
        return 0;
    if (region_key(x, y) != region || !lay_needed(x, y, rail) || !road_ok(player, x, y))
        return 0;
    owner = map_owner(x, y);
    return owner == -1 || owner == player;
}

static int link_root(int *parent, int i)
{
    while (parent[i] != i)
        i = parent[i] = parent[parent[i]];
    return i;
}

int auto_road_goal(int player, int unit)
{
    static int cx[128], cy[128], cc[128], parent[128], ea[768], eb[768], el[768];
    struct unit *u = &armydata[player][unit];
    int rail, rails = has_tech(player, 49), stage, score;
    long worked;
    int region = region_key(civwasm_auto_x[player][unit], civwasm_auto_y[player][unit]);
    int n = 0, edges = 0, i, j, k, a, b, d, x, y, steps, bx = -1, by = 0, best = 32767, pass;
    int near_j[6], near_d[6];
    if ((u->status & 2) && lay_needed(u->x, u->y, rails) && road_ok(player, u->x, u->y))
        return 'r';             /* work under way goes on */
    for (i = 0; i < 128; i++)
        if (townbuf[i].status != -1 && townbuf[i].owner == player && region_key(townbuf[i].x, townbuf[i].y) == region) {
            cx[n] = townbuf[i].x;
            cy[n] = townbuf[i].y;
            cc[n] = i;
            parent[n] = n;
            n++;
        }
    /* Each city's six nearest, by length, shortest first. */
    for (i = 0; i < n; i++) {
        for (k = 0; k < 6; k++)
            near_j[k] = -1, near_d[k] = 32767;
        for (j = 0; j < n; j++) {
            if (j == i)
                continue;
            d = cheb(cx[i], cy[i], cx[j], cy[j]);
            for (k = 0; k < 6 && d >= near_d[k]; k++)
                ;
            if (k == 6)
                continue;
            for (a = 5; a > k; a--)
                near_j[a] = near_j[a - 1], near_d[a] = near_d[a - 1];
            near_j[k] = j;
            near_d[k] = d;
        }
        for (k = 0; k < 6 && near_j[k] != -1 && edges < 768; k++) {
            for (a = edges; a > 0 && el[a - 1] > near_d[k]; a--)
                ea[a] = ea[a - 1], eb[a] = eb[a - 1], el[a] = el[a - 1];
            ea[a] = i;
            eb[a] = near_j[k];
            el[a] = near_d[k];
            edges++;
        }
    }
    for (stage = 1; stage <= 3 && bx == -1; stage++)
    for (rail = 0; rail <= rails && bx == -1; rail++) {
    if (stage == 2) {
        for (j = 0; j < n; j++) {
            worked = *(long *) &townbuf[cc[j]].rest[4];
            for (i = 0; i < 20; i++) {
                x = wrap_x(cx[j] + radius_dx[i]);
                y = cy[j] + radius_dy[i];
                if (!road_wanted(player, region, x, y, rail))
                    continue;
                score = 4 * cheb(u->x, u->y, x, y) - (((worked >> i) & 1) ? 3 : 0) - ((map_flags(x, y) & 2) ? 2 : 0);
                if (score < best && !auto_taken(player, unit, x, y)) {
                    best = score;
                    bx = x;
                    by = y;
                }
            }
        }
        continue;
    }
    if (stage == 3) {
        for (x = 0; x < 80; x++)
            for (y = 1; y < 49; y++) {
                if (!road_wanted(player, region, x, y, rail))
                    continue;
                d = cheb(u->x, u->y, x, y);
                if (d < best && !auto_taken(player, unit, x, y)) {
                    best = d;
                    bx = x;
                    by = y;
                }
            }
        continue;
    }
    /* Stage 1: pass 0 the shortest network, pass 1 (when that is built) the rest. */
    for (pass = 0; pass < 2 && bx == -1; pass++) {
    if (pass == 0)
        for (i = 0; i < n; i++)
            parent[i] = i;
    for (k = 0; k < edges; k++) {
        int lbx = -1, lby = 0, lbest = 32767;
        a = ea[k];
        b = eb[k];
        if (pass == 0 ? link_root(parent, a) == link_root(parent, b) : el[k] > 12)
            continue;
        x = cx[a];
        y = cy[a];
        for (steps = 0; steps < 80 && !(x == cx[b] && y == cy[b]); steps++) {
            d = link_step(player, cx[a], cy[a], x, y, cx[b], cy[b], rail);
            if (d == -1)
                break;
            x = wrap_x(x + spiral_x[d]);
            y += spiral_y[d];
            if (lay_needed(x, y, rail) && road_ok(player, x, y)) {
                j = cheb(u->x, u->y, x, y);
                if (j < lbest && !auto_taken(player, unit, x, y)) {
                    lbest = j;
                    lbx = x;
                    lby = y;
                }
            }
        }
        if (!(x == cx[b] && y == cy[b]))
            continue;           /* not walkable: left out */
        if (pass == 0)
            parent[link_root(parent, a)] = link_root(parent, b);
        if (lbx != -1 && lbest < best) {
            best = lbest;
            bx = lbx;
            by = lby;
        }
    }
    }
    }
    if (bx == -1)
        goto stop;
    if (bx == u->x && by == u->y)
        return 'r';
    u->goto_x = (char) bx;
    u->goto_y = (char) by;
    d = unit_goal(player, unit);
    if (d < 1 || d > 8)
        goto stop;
    return d;
stop:
    civwasm_auto_work[player][unit] = 0;
    u->goto_x = u->f8 = -1;
    return -1;
}

/* civ-wasm: Auto Explore (Shift+E): a land or sea unit heads for the
 * nearest tile it knows that borders the unknown, on its own landmass or
 * sea, and looks again at every move. Other explorers' goals are left to
 * them, and tiles held by others are passed over. With nothing left to
 * explore within reach, or another civilization's unit next to it, the
 * unit asks for orders. */
static int explore_frontier(int player, int x, int y)
{
    int d, nx, ny;
    for (d = 1; d <= 8; d++) {
        nx = wrap_x(x + spiral_x[d]);
        ny = y + spiral_y[d];
        if (ny >= 0 && ny < 50 && !(map_visible_to[nx * 50 + ny] & (1 << player)))
            return 1;
    }
    return 0;
}

int auto_explore_goal(int player, int unit)
{
    struct unit *u = &armydata[player][unit], *o;
    int sea = utypes[u->type].domain == 2, region = region_key(u->x, u->y);
    int best = 9999, bx = -1, by = 0, dx, dy, x, y, d, i, r;
    for (d = 1; d <= 8; d++) {
        y = u->y + spiral_y[d];
        if (y >= 0 && y < 50 && map_stack(wrap_x(u->x + spiral_x[d]), y) != -1 && stk_plr != player)
            goto stop;
    }
    for (r = 1; r <= 24 && bx == -1; r++)       /* rings out from the unit: the nearest ring wins */
        for (dy = -r; dy <= r; dy++)
            for (dx = -r; dx <= r; dx++) {
                if (abs(dx) != r && abs(dy) != r)
                    continue;
                x = wrap_x(u->x + dx);
                y = u->y + dy;
                if (y < 1 || y > 48 || !(map_visible_to[x * 50 + y] & (1 << player)))
                    continue;
                if ((terrain_at(x, y) == 10) != sea || region_key(x, y) != region || !explore_frontier(player, x, y))
                    continue;
                if ((map_stack(x, y) != -1 && stk_plr != player) || ((map_flags(x, y) & 1) && map_owner(x, y) != player))
                    continue;
                for (i = 0; i < 128; i++) {
                    o = &armydata[player][i];
                    if (i != unit && o->type != -1 && civwasm_auto_work[player][i] == 'e' &&
                        (unsigned char) o->goto_x == x && (unsigned char) o->goto_y == y)
                        break;
                }
                if (i < 128)
                    continue;
                d = abs(dx) + abs(dy);
                if (d < best) {
                    best = d;
                    bx = x;
                    by = y;
                }
            }
    if (bx == -1)
        goto stop;
    u->goto_x = (char) bx;
    u->goto_y = (char) by;
    d = unit_goal(player, unit);
    if (d < 1 || d > 8)
        goto stop;
    return d;
stop:
    civwasm_auto_work[player][unit] = 0;
    u->goto_x = u->f8 = -1;
    return -1;
}
