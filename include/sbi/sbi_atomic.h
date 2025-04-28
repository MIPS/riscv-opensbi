/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2025 MIPS
 *
 */

#ifndef __SBI_ATOMIC_H__
#define __SBI_ATOMIC_H__

#include <sbi/sbi_types.h>

struct sbi_trap_regs;

int sbi_atomic_add_w(ulong insn, struct sbi_trap_regs *regs);
int sbi_atomic_add_d(ulong insn, struct sbi_trap_regs *regs);
int sbi_atomic_and_w(ulong insn, struct sbi_trap_regs *regs);
int sbi_atomic_and_d(ulong insn, struct sbi_trap_regs *regs);
int sbi_atomic_or_w(ulong insn, struct sbi_trap_regs *regs);
int sbi_atomic_or_d(ulong insn, struct sbi_trap_regs *regs);
int sbi_atomic_xor_w(ulong insn, struct sbi_trap_regs *regs);
int sbi_atomic_xor_d(ulong insn, struct sbi_trap_regs *regs);
int sbi_atomic_max_w(ulong insn, struct sbi_trap_regs *regs);
int sbi_atomic_max_d(ulong insn, struct sbi_trap_regs *regs);
int sbi_atomic_min_w(ulong insn, struct sbi_trap_regs *regs);
int sbi_atomic_min_d(ulong insn, struct sbi_trap_regs *regs);
int sbi_atomic_maxu_w(ulong insn, struct sbi_trap_regs *regs);
int sbi_atomic_maxu_d(ulong insn, struct sbi_trap_regs *regs);
int sbi_atomic_minu_w(ulong insn, struct sbi_trap_regs *regs);
int sbi_atomic_minu_d(ulong insn, struct sbi_trap_regs *regs);
int sbi_atomic_swap_w(ulong insn, struct sbi_trap_regs *regs);
int sbi_atomic_swap_d(ulong insn, struct sbi_trap_regs *regs);

int sbi_atomic_add_w_aq(ulong insn, struct sbi_trap_regs *regs);
int sbi_atomic_add_d_aq(ulong insn, struct sbi_trap_regs *regs);
int sbi_atomic_and_w_aq(ulong insn, struct sbi_trap_regs *regs);
int sbi_atomic_and_d_aq(ulong insn, struct sbi_trap_regs *regs);
int sbi_atomic_or_w_aq(ulong insn, struct sbi_trap_regs *regs);
int sbi_atomic_or_d_aq(ulong insn, struct sbi_trap_regs *regs);
int sbi_atomic_xor_w_aq(ulong insn, struct sbi_trap_regs *regs);
int sbi_atomic_xor_d_aq(ulong insn, struct sbi_trap_regs *regs);
int sbi_atomic_max_w_aq(ulong insn, struct sbi_trap_regs *regs);
int sbi_atomic_max_d_aq(ulong insn, struct sbi_trap_regs *regs);
int sbi_atomic_min_w_aq(ulong insn, struct sbi_trap_regs *regs);
int sbi_atomic_min_d_aq(ulong insn, struct sbi_trap_regs *regs);
int sbi_atomic_maxu_w_aq(ulong insn, struct sbi_trap_regs *regs);
int sbi_atomic_maxu_d_aq(ulong insn, struct sbi_trap_regs *regs);
int sbi_atomic_minu_w_aq(ulong insn, struct sbi_trap_regs *regs);
int sbi_atomic_minu_d_aq(ulong insn, struct sbi_trap_regs *regs);
int sbi_atomic_swap_w_aq(ulong insn, struct sbi_trap_regs *regs);
int sbi_atomic_swap_d_aq(ulong insn, struct sbi_trap_regs *regs);

int sbi_atomic_add_w_rl(ulong insn, struct sbi_trap_regs *regs);
int sbi_atomic_add_d_rl(ulong insn, struct sbi_trap_regs *regs);
int sbi_atomic_and_w_rl(ulong insn, struct sbi_trap_regs *regs);
int sbi_atomic_and_d_rl(ulong insn, struct sbi_trap_regs *regs);
int sbi_atomic_or_w_rl(ulong insn, struct sbi_trap_regs *regs);
int sbi_atomic_or_d_rl(ulong insn, struct sbi_trap_regs *regs);
int sbi_atomic_xor_w_rl(ulong insn, struct sbi_trap_regs *regs);
int sbi_atomic_xor_d_rl(ulong insn, struct sbi_trap_regs *regs);
int sbi_atomic_max_w_rl(ulong insn, struct sbi_trap_regs *regs);
int sbi_atomic_max_d_rl(ulong insn, struct sbi_trap_regs *regs);
int sbi_atomic_min_w_rl(ulong insn, struct sbi_trap_regs *regs);
int sbi_atomic_min_d_rl(ulong insn, struct sbi_trap_regs *regs);
int sbi_atomic_maxu_w_rl(ulong insn, struct sbi_trap_regs *regs);
int sbi_atomic_maxu_d_rl(ulong insn, struct sbi_trap_regs *regs);
int sbi_atomic_minu_w_rl(ulong insn, struct sbi_trap_regs *regs);
int sbi_atomic_minu_d_rl(ulong insn, struct sbi_trap_regs *regs);
int sbi_atomic_swap_w_rl(ulong insn, struct sbi_trap_regs *regs);
int sbi_atomic_swap_d_rl(ulong insn, struct sbi_trap_regs *regs);

int sbi_atomic_add_w_aqrl(ulong insn, struct sbi_trap_regs *regs);
int sbi_atomic_add_d_aqrl(ulong insn, struct sbi_trap_regs *regs);
int sbi_atomic_and_w_aqrl(ulong insn, struct sbi_trap_regs *regs);
int sbi_atomic_and_d_aqrl(ulong insn, struct sbi_trap_regs *regs);
int sbi_atomic_or_w_aqrl(ulong insn, struct sbi_trap_regs *regs);
int sbi_atomic_or_d_aqrl(ulong insn, struct sbi_trap_regs *regs);
int sbi_atomic_xor_w_aqrl(ulong insn, struct sbi_trap_regs *regs);
int sbi_atomic_xor_d_aqrl(ulong insn, struct sbi_trap_regs *regs);
int sbi_atomic_max_w_aqrl(ulong insn, struct sbi_trap_regs *regs);
int sbi_atomic_max_d_aqrl(ulong insn, struct sbi_trap_regs *regs);
int sbi_atomic_min_w_aqrl(ulong insn, struct sbi_trap_regs *regs);
int sbi_atomic_min_d_aqrl(ulong insn, struct sbi_trap_regs *regs);
int sbi_atomic_maxu_w_aqrl(ulong insn, struct sbi_trap_regs *regs);
int sbi_atomic_maxu_d_aqrl(ulong insn, struct sbi_trap_regs *regs);
int sbi_atomic_minu_w_aqrl(ulong insn, struct sbi_trap_regs *regs);
int sbi_atomic_minu_d_aqrl(ulong insn, struct sbi_trap_regs *regs);
int sbi_atomic_swap_w_aqrl(ulong insn, struct sbi_trap_regs *regs);
int sbi_atomic_swap_d_aqrl(ulong insn, struct sbi_trap_regs *regs);
#endif
