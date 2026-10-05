#ifndef MCU_COMPILER_H
#define MCU_COMPILER_H

/* Memory-space qualifiers. Host builds have one flat address space, so the qualifiers vanish there. */
#if defined(__SDCC_mcs51)
#define MEM_XDATA __xdata
#define MEM_CODE __code
#define MEM_DATA __data /* direct internal RAM */
#define MEM_BIT __bit   /* one bit in the bit area 0x20-0x2F; used as a type: static volatile MEM_BIT is_x; */
#else
#include <stdbool.h>

#define MEM_XDATA
#define MEM_CODE
#define MEM_DATA
#define MEM_BIT bool
#endif

#endif /* MCU_COMPILER_H */
