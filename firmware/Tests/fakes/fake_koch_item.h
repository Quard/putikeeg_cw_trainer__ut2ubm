#ifndef FAKE_KOCH_ITEM_H
#define FAKE_KOCH_ITEM_H

/*
 * Host fake of koch_make_item() (references/internal/training_spec.md D-T18 and section 15.2): the "fake training
 * source" of ui-ux.md section 9.4. It returns the pushed items in order, then the last one again. A call with no
 * item pushed, or a push past the FIFO, prints an error and fails the test program.
 */

#include <stddef.h>

#define FAKE_KOCH_ITEM_FIFO_SIZE 128U

void fake_koch_item_reset(void);
/** Adds @p text (1..KOCH_WORD_LEN_MAX characters) to the FIFO. */
void fake_koch_item_push(const char* text);
/** Returns the koch_make_item() calls so far. */
size_t fake_koch_item_count(void);

#endif /* FAKE_KOCH_ITEM_H */
