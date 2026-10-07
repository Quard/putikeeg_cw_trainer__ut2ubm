#ifndef STATS_STATS_H
#define STATS_STATS_H

/* Training statistics: the training levels of the progress record. */

#include <stdbool.h>

/** Sets every training level back to level 1 and stores it. Returns true when the store succeeded. */
bool stats_reset(void);

#endif /* STATS_STATS_H */
