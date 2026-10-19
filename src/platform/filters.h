/* Host image filters. Pixels are packed 0x00RRGGBB, rows are contiguous.
 * These routines never see the period game headers or memory. */
#ifndef RECIV_FILTERS_H
#define RECIV_FILTERS_H
#include <stdint.h>

enum reciv_filter {
    RECIV_FILTER_NEAREST, RECIV_FILTER_SHARP, RECIV_FILTER_BILINEAR, RECIV_FILTER_HQ4X
};

#ifdef __cplusplus
extern "C" {
#endif
/* HQ4x enlarges the fixed source by four in each dimension. */
int reciv_filter_factor(enum reciv_filter filter);
/* Fixed 320x200 source; destination must hold (320*factor)*(200*factor).
 * Returns zero on an unsupported filter or allocation failure. */
int reciv_filter_scale(enum reciv_filter filter, const uint32_t *src, uint32_t *dst);
#ifdef __cplusplus
}
#endif
#endif
