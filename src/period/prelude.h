/* prelude.h: force-included before every game module (src/game).
 *
 * The game is Microsoft C 5.1 medium-model code. Here it gets that
 * compiler's integer sizes on a modern one: int is 16 bits, long 32 bits
 * (native long on MSVC; C23 _BitInt(32) on GCC), and pointers are
 * native. The MSC keywords for segmented memory mean nothing here.
 *
 * Game modules must not see host headers: src/period/include provides the
 * MSC 5.1 headers they include, declaring the period C run time (src/crt)
 * under crt_* names. */
#ifndef RECIV_PRELUDE_H
#define RECIV_PRELUDE_H

#define far
#define near
#define huge
#define interrupt
#define cdecl
#define pascal

#define int short
#ifdef _MSC_VER
/* MSVC predeclares native size_t; the period CRT needs a separate word. */
#define size_t reciv_period_size_t
#else
#define long _BitInt(32)
#endif

#define main game_main

#include "crtnames.h"

/* Supply an explicit compatibility value at legacy undefined-behaviour sites. */
#define RECIV_UB(value, site) (value)

/* The SI register at a call, for a callee that reads the word its
 * prologue saved SI in (CITYMGT.C's city_process passes it to adj_grp as a
 * missing argument). Call sites set it where the listing shows SI; calls
 * that pass SI through unchanged leave it as their caller set it. */
extern short reciv_si;
#define RECIV_SI(value) (reciv_si = (value))

/* Preserve a compatibility value shared across selected legacy calls. */
extern short reciv_stack;
#define RECIV_STACK(value, site) (reciv_stack = (value))

#endif
