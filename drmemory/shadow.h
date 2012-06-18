/* **********************************************************
 * Copyright (c) 2010-2012 Google, Inc.  All rights reserved.
 * Copyright (c) 2007-2010 VMware, Inc.  All rights reserved.
 * **********************************************************/

/* Dr. Memory: the memory debugger
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; 
 * version 2.1 of the License, and no later version.

 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Library General Public License for more details.

 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef _SHADOW_H_
#define _SHADOW_H_ 1

/* are we using 4Bto1B, or the default 1Bto2b? */
#define MAP_4B_TO_1B (!options.check_uninitialized)

/***************************************************************************
 * We track both addressability and definedness for each byte of memory.
 * We plan to extend to per-bit definedness, but as an escape-mechanism.
 * Thus we have 2 shadow bits per byte of application memory, with further
 * shadow bits only for memory that needs per-bit shadowing.
 * This ends up being similar to Valgrind Memcheck, which has 1 A
 * (addressability) bit and 8 V (validity, or definedness) bits for
 * bit-level definedness.  We use different bit sequences so that we
 * can directly copy from shadow registers yet still use simple
 * bitwise operations:
 *
 *   01 = unaddressable
 *   11 = addressable, undefined
 *   00 = addressable, defined
 *   10 = addressable, definedness is at bit level
 *
 * If we used Memcheck's scheme, for an aligned 4-byte read we could say:
 * cmp to 0b10101010 (0xaa), je good, jg bitlevel, jl bad
 * Here we'll need a second compare for bitlevel.
 */
#define SHADOW_UNADDRESSABLE    0x1
#define SHADOW_UNDEFINED        0x3
#define SHADOW_DEFINED          0x0
#define SHADOW_DEFINED_BITLEVEL 0x2

#define SHADOW_MIXED 4 /* only used for shadow_check_range and leak reporting */
#define SHADOW_UNKNOWN 5 /* only used for leak reporting */

/* Entire-word patterns */
#define SHADOW_WORD_UNADDRESSABLE (MAP_4B_TO_1B ? SHADOW_UNADDRESSABLE    : 0x5)
#define SHADOW_WORD_UNDEFINED     (MAP_4B_TO_1B ? SHADOW_UNDEFINED        : 0xf)
#define SHADOW_WORD_DEFINED       (MAP_4B_TO_1B ? SHADOW_DEFINED          : 0x0)
#define SHADOW_WORD_BITLEVEL      (MAP_4B_TO_1B ? SHADOW_DEFINED_BITLEVEL : 0xa)

#define SHADOW_DWORD_UNADDRESSABLE (MAP_4B_TO_1B ? SHADOW_UNADDRESSABLE    : 0x55)
#define SHADOW_DWORD_UNDEFINED     (MAP_4B_TO_1B ? SHADOW_UNDEFINED        : 0xff)
#define SHADOW_DWORD_DEFINED       (MAP_4B_TO_1B ? SHADOW_DEFINED          : 0x00)
#define SHADOW_DWORD_BITLEVEL      (MAP_4B_TO_1B ? SHADOW_DEFINED_BITLEVEL : 0xaa)

#define SHADOW_QWORD_UNADDRESSABLE (MAP_4B_TO_1B ? 0x0101 : 0x5555)
#define SHADOW_QWORD_UNDEFINED     (MAP_4B_TO_1B ? 0x0303 : 0xffff)
#define SHADOW_QWORD_DEFINED       (MAP_4B_TO_1B ? 0x0000 : 0x0000)
#define SHADOW_QWORD_BITLEVEL      (MAP_4B_TO_1B ? 0x0202 : 0xaaaa)

#define SHADOW_DQWORD_UNADDRESSABLE (MAP_4B_TO_1B ? 0x01010101 : 0x55555555)
#define SHADOW_DQWORD_UNDEFINED     (MAP_4B_TO_1B ? 0x03030303 : 0xffffffff)
#define SHADOW_DQWORD_DEFINED       (MAP_4B_TO_1B ? 0x00000000 : 0x00000000)
#define SHADOW_DQWORD_BITLEVEL      (MAP_4B_TO_1B ? 0x02020202 : 0xaaaaaaaa)

/* extracts the 2 bits for byte#n from the dword-representing byte v */
#define SHADOW_DWORD2BYTE(v, n) (((v) & (0x3 << 2*(n))) >> 2*(n))

extern uint val_to_dword[];
extern uint val_to_qword[];
extern uint val_to_dqword[];

/* To check 4 bytes for addressability we need to determine whether any
 * one 2-bit sequence in a byte is SHADOW_UNADDRESSABLE.
 *
 * for 32 bits, determining whether any one byte is zero, borrowing from strlen:
 * (((x + 0x7efefeff) ^ (~x)) & 0x81010100) == 0
 *
 * To distinguish addressable or defined from unaddressable or bitlevel,
 * can use parity, which can be computed from low bit of popcnt: but
 * only on SSE4 machines.
 * Should probably just use table lookup: and in fact we do so
 * now with -loads_use_table and -stores_use_table.
 */

extern const char * const shadow_name[];

const char *shadow_dqword_name(uint dqword);

/* PR 493257: share shadow translation across multiple instrs */
#define SHADOW_REDZONE_SIZE 512

#ifdef STATISTICS
extern uint shadow_block_alloc;
extern uint shadow_block_free;
extern uint num_special_unaddressable;
extern uint num_special_undefined;
extern uint num_special_defined;
#endif

uint
set_2bits(uint orig, uint val, uint shift);

void
shadow_init(void);

void
shadow_exit(void);

void
shadow_thread_init(void *drcontext);

void
shadow_thread_exit(void *drcontext);

size_t
get_shadow_block_size(void);

/* Returns whether pc is a pointer into a special shadow block */
bool
is_in_special_shadow_block(byte *pc);

bool
shadow_get_special(app_pc addr, uint *val);

/* Returns the two bits for the byte at the passed-in address */
uint
shadow_get_byte(app_pc addr);

/* Returns the byte that shadows the 4-byte-aligned address */
uint
shadow_get_dword(app_pc addr);

/* Sets the two bits for the byte at the passed-in address */
void
shadow_set_byte(app_pc addr, uint val);

/* Converts the special shadow block for addr to non-special
 * and returns a pointer to the same offset within the new
 * non-special block.
 */
byte *
shadow_replace_special(app_pc addr);

byte *
shadow_translation_addr(app_pc addr);

byte *
shadow_translation_addr_using_offset(app_pc addr, byte *target);

/* Returns a pointer to an always-bitlevel shadow block */
byte *
shadow_bitlevel_addr(void);

/* Sets the two bits for each byte in the range [start, end) */
void
shadow_set_range(app_pc start, app_pc end, uint val);

/* Copies the values for each byte in the range [old_start, old_start+end) to
 * [new_start, new_start+size).  The two ranges can overlap.
 */
void
shadow_copy_range(app_pc old_start, app_pc new_start, size_t size);

/* Compares every byte in [start, start+size) to expect.
 * start must be 16-byte aligned.
 * Stops and returns the pc of the first non-matching value.
 * If all bytes match, returns start_size.
 * bad_state is a dqword value.
 */
bool
shadow_check_range(app_pc start, size_t size, uint expect,
                   app_pc *bad_start, app_pc *bad_end, uint *bad_state);

/* Walks backward from start comparing each byte to expect.
 * If a non-matching value is reached, stops and returns false with the
 * non-matching addr in bad_addr.
 * If all bytes match when it reaches start-size, returns true.
 * N.B.: not highly performant!
 */
bool
shadow_check_range_backward(app_pc start, size_t size, uint expect, app_pc *bad_addr);

/* Finds the next aligned dword, starting at start and stopping at
 * end, whose shadow equals expect expanded to a dword.
 */
app_pc
shadow_next_dword(app_pc start, app_pc end, uint expect);

/* Finds the previous aligned dword, starting at start and stopping at
 * end (end < start), whose shadow equals expect expanded to a dword.
 */
app_pc
shadow_prev_dword(app_pc start, app_pc end, uint expect);

/* Caller should place application address in addr_reg.
 * drutil_insert_get_mem_addr() can be used to obtain the address
 * from an application memory operand.
 * On return, the shadow address will be in addr_reg.
 *
 * The returned address is the address of the shadow byte containing
 * the shadow value for the application address.  It is up to the
 * caller to locate the value within that byte when using sub-byte
 * mappings, and to handle further bytes referenced in a memory
 * reference larger than one byte.
 */
void
shadow_gen_translation_addr(void *drcontext, instrlist_t *bb, instr_t *inst,
                            reg_id_t addr_reg, reg_id_t scratch_reg);

/***************************************************************************
 * SHADOWING THE GPR REGISTERS
 */

void
print_shadow_registers(void);

opnd_t
opnd_create_shadow_reg_slot(reg_id_t reg);

opnd_t
opnd_create_shadow_eflags_slot(void);

opnd_t
opnd_create_shadow_inheap_slot(void);

#ifdef LINUX
opnd_t
opnd_create_seg_base_slot(reg_id_t seg, opnd_size_t opsz);
#endif

byte
get_shadow_register(reg_id_t reg);

byte
get_thread_shadow_register(void *drcontext, reg_id_t reg);

void
register_shadow_set_byte(reg_id_t reg, uint bytenum, uint val);

void
register_shadow_set_dword(reg_id_t reg, uint val);

byte
get_shadow_eflags(void);

void
set_shadow_eflags(uint val);

byte
get_shadow_inheap(void);

void
set_shadow_inheap(uint val);

bool
is_shadow_register_defined(byte val);

#endif /* _SHADOW_H_ */
