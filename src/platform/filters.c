/* Small C boundary around the bundled HQ4x scaler. */
#include "filters.h"
#include "hqx.h"

int reciv_filter_factor(enum reciv_filter filter)
{
    return filter == RECIV_FILTER_HQ4X ? 4 : 0;
}

int reciv_filter_scale(enum reciv_filter filter, const uint32_t *src, uint32_t *dst, int w, int h)
{
    static int initialized;
    if (filter != RECIV_FILTER_HQ4X)
        return 0;
    if (!initialized) {
        hqxInit();
        initialized = 1;
    }
    hq4x_32(src, dst, w, h);
    return 1;
}
