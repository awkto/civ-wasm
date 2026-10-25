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
/* A w x h source (civ-wasm: was fixed at 320x200); destination must hold
 * (w*factor)*(h*factor). Returns zero on an unsupported filter or
 * allocation failure. */
int reciv_filter_scale(enum reciv_filter filter, const uint32_t *src, uint32_t *dst, int w, int h);
#ifdef __cplusplus
}
#endif
#endif
