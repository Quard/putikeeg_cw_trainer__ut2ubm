#ifndef PROGRESS_FIXTURE_H
#define PROGRESS_FIXTURE_H

/* Test fixture of the progress record (references/internal/training_spec.md sections 10.1 and 15.2). */

#include <stdint.h>

/** Writes a valid progress record to the fake IAP: all levels 0, then @p level for (@p mode, @p wpm). */
void progress_fixture_write(uint8_t mode, uint8_t wpm, uint16_t level);

#endif /* PROGRESS_FIXTURE_H */
