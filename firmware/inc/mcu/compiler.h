#ifndef MCU_COMPILER_H
#define MCU_COMPILER_H

/* Memory-space qualifiers. Host builds have one flat address space, so the qualifiers vanish there. */
#if defined(__SDCC_mcs51)
#define MEM_XDATA __xdata
#define MEM_CODE __code
#else
#define MEM_XDATA
#define MEM_CODE
#endif

#endif /* MCU_COMPILER_H */
