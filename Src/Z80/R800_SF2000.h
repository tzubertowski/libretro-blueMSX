/*****************************************************************************
** SF2000-Optimized Z80/R800 Processor Header
**
** Description: MIPS-optimized Z80/R800 emulation for DataFrog SF2000
**
** Performance Focus:
** - MIPS32 register allocation optimization
** - Computed goto instruction dispatch
** - Inlined hot-path functions
** - Fixed-point arithmetic optimizations
**
** Maintains cycle-accurate timing for MSX compatibility
*******************************************************************************/

#ifndef R800_SF2000_H
#define R800_SF2000_H

#include "R800.h"

#ifdef SF2000

// MIPS-specific optimizations
#define MIPS_LIKELY(x)    __builtin_expect(!!(x), 1)
#define MIPS_UNLIKELY(x)  __builtin_expect(!!(x), 0)
#define MIPS_INLINE       static __inline__ __attribute__((always_inline))
#define MIPS_HOT          __attribute__((hot))
#define MIPS_COLD         __attribute__((cold))

// SF2000-specific Z80 optimizations
#define SF2000_Z80_OPTIMIZATIONS

// Fast flag computation macros for MIPS
#define FAST_FLAG_ZSXY(val) (ZSXYTable[(val) & 0xFF])
#define FAST_FLAG_ZSPXY(val) (ZSPXYTable[(val) & 0xFF])
#define FAST_FLAG_ZSPH(val) (ZSPHTable[(val) & 0xFF])

// MIPS-optimized arithmetic operations
MIPS_INLINE void sf2000_add8(R800* r800, UInt8 reg) MIPS_HOT;
MIPS_INLINE void sf2000_sub8(R800* r800, UInt8 reg) MIPS_HOT;
MIPS_INLINE void sf2000_cp8(R800* r800, UInt8 reg) MIPS_HOT;
MIPS_INLINE void sf2000_and8(R800* r800, UInt8 reg) MIPS_HOT;
MIPS_INLINE void sf2000_or8(R800* r800, UInt8 reg) MIPS_HOT;
MIPS_INLINE void sf2000_xor8(R800* r800, UInt8 reg) MIPS_HOT;

// MIPS-optimized memory access
MIPS_INLINE UInt8 sf2000_readMem(R800* r800, UInt16 address) MIPS_HOT;
MIPS_INLINE void sf2000_writeMem(R800* r800, UInt16 address, UInt8 value) MIPS_HOT;
MIPS_INLINE UInt8 sf2000_readOpcode(R800* r800, UInt16 address) MIPS_HOT;

// Computed goto instruction dispatch
typedef enum {
    Z80_NOP = 0x00, Z80_LD_BC_NN = 0x01, Z80_LD_BC_A = 0x02, Z80_INC_BC = 0x03,
    Z80_INC_B = 0x04, Z80_DEC_B = 0x05, Z80_LD_B_N = 0x06, Z80_RLCA = 0x07,
    Z80_EX_AF_AFP = 0x08, Z80_ADD_HL_BC = 0x09, Z80_LD_A_BC = 0x0A, Z80_DEC_BC = 0x0B,
    Z80_INC_C = 0x0C, Z80_DEC_C = 0x0D, Z80_LD_C_N = 0x0E, Z80_RRCA = 0x0F,
    // ... (all 256 opcodes)
} Z80Opcode;

// SF2000-optimized execution functions
void sf2000_r800Execute(R800* r800, UInt32 systemTime) MIPS_HOT;
void sf2000_r800ExecuteUntil(R800* r800, UInt32 systemTime) MIPS_HOT;
void sf2000_executeInstruction(R800* r800) MIPS_HOT;

// MIPS register allocation hints for hot Z80 registers
register UInt8 mips_z80_a asm("s0");     // Z80 A register -> MIPS s0
register UInt8 mips_z80_f asm("s1");     // Z80 F register -> MIPS s1  
register UInt16 mips_z80_hl asm("s2");   // Z80 HL register -> MIPS s2
register UInt16 mips_z80_pc asm("s3");   // Z80 PC register -> MIPS s3

// Fast instruction decode for common opcodes
#define SF2000_FAST_DECODE_ENABLED

#ifdef SF2000_FAST_DECODE_ENABLED
// Optimized decode for most common MSX instructions
MIPS_INLINE void sf2000_fastDecode_LD_r_r(R800* r800, UInt8 opcode) MIPS_HOT;
MIPS_INLINE void sf2000_fastDecode_ALU_r(R800* r800, UInt8 opcode) MIPS_HOT;
MIPS_INLINE void sf2000_fastDecode_INC_DEC_r(R800* r800, UInt8 opcode) MIPS_HOT;
#endif

// SF2000-specific timing optimizations
#define SF2000_FAST_TIMING
#ifdef SF2000_FAST_TIMING
MIPS_INLINE void sf2000_updateTiming(R800* r800, UInt8 cycles) MIPS_HOT;
#endif

#endif /* SF2000 */

#endif /* R800_SF2000_H */