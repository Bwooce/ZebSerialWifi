#pragma once

#include <math.h>

/*
 * Millimetres and printer dots.
 *
 * The printer reports its resolution as a whole number of dots per mm
 * (^HH RESOLUTION "1280 12/MM FULL"). Zebra rounds there: a 300 dpi head
 * is "12/MM" and a 600 dpi head "24/MM", but 300 dpi is 11.81 dots per mm.
 * On a GX430t test label a 240 dot circle measured 20.57 mm across the head:
 * 300 dpi gives 20.32 mm plus ink spread, 12 dots per mm only 20.0 mm. The
 * 6 and 8 dots/mm heads (152 and 203 dpi) are exact.
 *
 * Zebra's own millimetre handling (^MUm, the mm figures in ^HH) uses the
 * rounded figure. The bridge converts with the true one so that a size typed
 * in mm prints at that size, and a 4 x 6 inch label shows as 101.6 x 152.4 mm.
 */

/* Dots per mm of a head the printer reports as dpmm dots/mm; 0 if unknown */
static inline double zpl_true_dpmm(int dpmm)
{
    if (dpmm == 12) {
        return 300.0 / 25.4;
    }
    if (dpmm == 24) {
        return 600.0 / 25.4;
    }
    return dpmm > 0 ? (double)dpmm : 0.0;
}

/* The printer's dpi for a dots/mm figure: 8 is 203 dpi, 12 is 300, 24 is 600 */
static inline int zpl_dpi(int dpmm)
{
    return dpmm == 8 ? 203 : dpmm == 12 ? 300 : dpmm == 24 ? 600 : (int)lround(dpmm * 25.4);
}

/* mm to the nearest dot, either side of zero */
static inline int zpl_mm_to_dots(double mm, int dpmm)
{
    return (int)lround(mm * zpl_true_dpmm(dpmm));
}

/* dots to mm; dpmm must be above 0 */
static inline double zpl_dots_to_mm(int dots, int dpmm)
{
    return dots / zpl_true_dpmm(dpmm);
}
