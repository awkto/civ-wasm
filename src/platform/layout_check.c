/* layout_check.c: compile-time checks that the game's records have the
 * sizes Microsoft C 5.1 gave them. Saved games copy these records byte for
 * byte, and the views civd declares inside them (building_names =
 * improvements + 1) rely on the same layout. Sizes from the original's save format and tables. */
#include "CIV.H"
#include "GRAPHICS.H"
#include "GAME.H"

_Static_assert(sizeof(int) == 2, "period int is 16 bits");
_Static_assert(sizeof(long) == 4, "period long is 32 bits");
_Static_assert(sizeof(struct unit) == 12, "unit record");
_Static_assert(sizeof(struct city) == 28, "city record");
_Static_assert(sizeof(struct unit_type) == 34, "unit type");
_Static_assert(sizeof(struct page) == 18, "page record");
