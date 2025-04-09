/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2019 Western Digital Corporation or its affiliates.
 *
 * Authors:
 *   Anup Patel <anup.patel@wdc.com>
 */

#include <sbi/riscv_asm.h>
#include <sbi/riscv_barrier.h>
#include <sbi/riscv_encoding.h>
#include <sbi/sbi_atomic.h>
#include <sbi/sbi_bitops.h>
#include <sbi/sbi_emulate_csr.h>
#include <sbi/sbi_error.h>
#include <sbi/sbi_illegal_insn.h>
#include <sbi/sbi_pmu.h>
#include <sbi/sbi_trap.h>
#include <sbi/sbi_unpriv.h>
#include <sbi/sbi_console.h>

#define OPCODE_MASK 0x0000007f
#define AMO_OPCODE 0x0000002f
#define WD_MASK 0x00007000
#define WD_SHIFT 12
#define AQRL_MASK 0x06000000
#define AQRL_SHIFT 25

typedef int (*illegal_insn_func)(ulong insn, struct sbi_trap_regs *regs);

static int truly_illegal_insn(ulong insn, struct sbi_trap_regs *regs)
{
	struct sbi_trap_info trap;

	trap.cause = CAUSE_ILLEGAL_INSTRUCTION;
	trap.tval = insn;
	trap.tval2 = 0;
	trap.tinst = 0;
	trap.gva   = 0;

	return sbi_trap_redirect(regs, &trap);
}

static int misc_mem_opcode_insn(ulong insn, struct sbi_trap_regs *regs)
{
	/* Errata workaround: emulate `fence.tso` as `fence rw, rw`. */
	if ((insn & INSN_MASK_FENCE_TSO) == INSN_MATCH_FENCE_TSO) {
		smp_mb();
		regs->mepc += 4;
		return 0;
	}

	return truly_illegal_insn(insn, regs);
}

static int system_opcode_insn(ulong insn, struct sbi_trap_regs *regs)
{
	bool do_write	= false;
	int rs1_num	= GET_RS1_NUM(insn);
	ulong rs1_val	= GET_RS1(insn, regs);
	int csr_num	= GET_CSR_NUM((u32)insn);
	ulong prev_mode = sbi_mstatus_prev_mode(regs->mstatus);
	ulong csr_val, new_csr_val;

	if (prev_mode == PRV_M) {
		sbi_printf("%s: Failed to access CSR %#x from M-mode",
			__func__, csr_num);
		return SBI_EFAIL;
	}

	/* Ensure that we got CSR read/write instruction */
	int funct3 = GET_RM(insn);
	if (funct3 == 0 || funct3 == 4) {
		sbi_printf("%s: Invalid opcode for CSR read/write instruction",
			   __func__);
		return truly_illegal_insn(insn, regs);
	}

	if (sbi_emulate_csr_read(csr_num, regs, &csr_val))
		return truly_illegal_insn(insn, regs);

	switch (funct3) {
	case CSRRW:
		new_csr_val = rs1_val;
		do_write    = true;
		break;
	case CSRRS:
		new_csr_val = csr_val | rs1_val;
		do_write    = (rs1_num != 0);
		break;
	case CSRRC:
		new_csr_val = csr_val & ~rs1_val;
		do_write    = (rs1_num != 0);
		break;
	case CSRRWI:
		new_csr_val = rs1_num;
		do_write    = true;
		break;
	case CSRRSI:
		new_csr_val = csr_val | rs1_num;
		do_write    = (rs1_num != 0);
		break;
	case CSRRCI:
		new_csr_val = csr_val & ~rs1_num;
		do_write    = (rs1_num != 0);
		break;
	default:
		return truly_illegal_insn(insn, regs);
	}

	if (do_write && sbi_emulate_csr_write(csr_num, regs, new_csr_val))
		return truly_illegal_insn(insn, regs);

	SET_RD(insn, regs, csr_val);

	regs->mepc += 4;

	return 0;
}

static const illegal_insn_func illegal_insn_table[32] = {
	truly_illegal_insn, /* 0 */
	truly_illegal_insn, /* 1 */
	truly_illegal_insn, /* 2 */
	misc_mem_opcode_insn, /* 3 */
	truly_illegal_insn, /* 4 */
	truly_illegal_insn, /* 5 */
	truly_illegal_insn, /* 6 */
	truly_illegal_insn, /* 7 */
	truly_illegal_insn, /* 8 */
	truly_illegal_insn, /* 9 */
	truly_illegal_insn, /* 10 */
	truly_illegal_insn, /* 11 */
	truly_illegal_insn, /* 12 */
	truly_illegal_insn, /* 13 */
	truly_illegal_insn, /* 14 */
	truly_illegal_insn, /* 15 */
	truly_illegal_insn, /* 16 */
	truly_illegal_insn, /* 17 */
	truly_illegal_insn, /* 18 */
	truly_illegal_insn, /* 19 */
	truly_illegal_insn, /* 20 */
	truly_illegal_insn, /* 21 */
	truly_illegal_insn, /* 22 */
	truly_illegal_insn, /* 23 */
	truly_illegal_insn, /* 24 */
	truly_illegal_insn, /* 25 */
	truly_illegal_insn, /* 26 */
	truly_illegal_insn, /* 27 */
	system_opcode_insn, /* 28 */
	truly_illegal_insn, /* 29 */
	truly_illegal_insn, /* 30 */
	truly_illegal_insn  /* 31 */
};

static int other_illegal_insn(ulong insn, struct sbi_trap_regs *regs)
{
	return illegal_insn_table[(insn & 0x7c) >> 2](insn, regs);
}

static const illegal_insn_func amoadd_table[32] = {
	other_illegal_insn, /* 0 */
	other_illegal_insn, /* 1 */
	other_illegal_insn, /* 2 */
	other_illegal_insn, /* 3 */
	other_illegal_insn, /* 4 */
	other_illegal_insn, /* 5 */
	other_illegal_insn, /* 6 */
	other_illegal_insn, /* 7 */
	sbi_atomic_add_w, /* 8 */
	sbi_atomic_add_w_rl, /* 9 */
	sbi_atomic_add_w_aq, /* 10 */
	sbi_atomic_add_w_aqrl, /* 11 */
#if __riscv_xlen == 64
	sbi_atomic_add_d, /* 12 */
	sbi_atomic_add_d_rl, /* 13 */
	sbi_atomic_add_d_aq, /* 14 */
	sbi_atomic_add_d_aqrl, /* 15 */
#else
	other_illegal_insn, /* 12 */
	other_illegal_insn, /* 13 */
	other_illegal_insn, /* 14 */
	other_illegal_insn, /* 15 */
#endif
	other_illegal_insn, /* 16 */
	other_illegal_insn, /* 17 */
	other_illegal_insn, /* 18 */
	other_illegal_insn, /* 19 */
	other_illegal_insn, /* 20 */
	other_illegal_insn, /* 21 */
	other_illegal_insn, /* 22 */
	other_illegal_insn, /* 23 */
	other_illegal_insn, /* 24 */
	other_illegal_insn, /* 25 */
	other_illegal_insn, /* 26 */
	other_illegal_insn, /* 27 */
	other_illegal_insn, /* 28 */
	other_illegal_insn, /* 29 */
	other_illegal_insn, /* 30 */
	other_illegal_insn, /* 31 */
};

static const illegal_insn_func amoswap_table[32] = {
	other_illegal_insn, /* 0 */
	other_illegal_insn, /* 1 */
	other_illegal_insn, /* 2 */
	other_illegal_insn, /* 3 */
	other_illegal_insn, /* 4 */
	other_illegal_insn, /* 5 */
	other_illegal_insn, /* 6 */
	other_illegal_insn, /* 7 */
	sbi_atomic_swap_w, /* 8 */
	sbi_atomic_swap_w_rl, /* 9 */
	sbi_atomic_swap_w_aq, /* 10 */
	sbi_atomic_swap_w_aqrl, /* 11 */
#if __riscv_xlen == 64
	sbi_atomic_swap_d, /* 12 */
	sbi_atomic_swap_d_rl, /* 13 */
	sbi_atomic_swap_d_aq, /* 14 */
	sbi_atomic_swap_d_aqrl, /* 15 */
#else
	other_illegal_insn, /* 12 */
	other_illegal_insn, /* 13 */
	other_illegal_insn, /* 14 */
	other_illegal_insn, /* 15 */
#endif
	other_illegal_insn, /* 16 */
	other_illegal_insn, /* 17 */
	other_illegal_insn, /* 18 */
	other_illegal_insn, /* 19 */
	other_illegal_insn, /* 20 */
	other_illegal_insn, /* 21 */
	other_illegal_insn, /* 22 */
	other_illegal_insn, /* 23 */
	other_illegal_insn, /* 24 */
	other_illegal_insn, /* 25 */
	other_illegal_insn, /* 26 */
	other_illegal_insn, /* 27 */
	other_illegal_insn, /* 28 */
	other_illegal_insn, /* 29 */
	other_illegal_insn, /* 30 */
	other_illegal_insn, /* 31 */
};

static const illegal_insn_func amoxor_table[32] = {
	other_illegal_insn, /* 0 */
	other_illegal_insn, /* 1 */
	other_illegal_insn, /* 2 */
	other_illegal_insn, /* 3 */
	other_illegal_insn, /* 4 */
	other_illegal_insn, /* 5 */
	other_illegal_insn, /* 6 */
	other_illegal_insn, /* 7 */
	sbi_atomic_xor_w, /* 8 */
	sbi_atomic_xor_w_rl, /* 9 */
	sbi_atomic_xor_w_aq, /* 10 */
	sbi_atomic_xor_w_aqrl, /* 11 */
#if __riscv_xlen == 64
	sbi_atomic_xor_d, /* 12 */
	sbi_atomic_xor_d_rl, /* 13 */
	sbi_atomic_xor_d_aq, /* 14 */
	sbi_atomic_xor_d_aqrl, /* 15 */
#else
	other_illegal_insn, /* 12 */
	other_illegal_insn, /* 13 */
	other_illegal_insn, /* 14 */
	other_illegal_insn, /* 15 */
#endif
	other_illegal_insn, /* 16 */
	other_illegal_insn, /* 17 */
	other_illegal_insn, /* 18 */
	other_illegal_insn, /* 19 */
	other_illegal_insn, /* 20 */
	other_illegal_insn, /* 21 */
	other_illegal_insn, /* 22 */
	other_illegal_insn, /* 23 */
	other_illegal_insn, /* 24 */
	other_illegal_insn, /* 25 */
	other_illegal_insn, /* 26 */
	other_illegal_insn, /* 27 */
	other_illegal_insn, /* 28 */
	other_illegal_insn, /* 29 */
	other_illegal_insn, /* 30 */
	other_illegal_insn, /* 31 */
};

static const illegal_insn_func amoor_table[32] = {
	other_illegal_insn, /* 0 */
	other_illegal_insn, /* 1 */
	other_illegal_insn, /* 2 */
	other_illegal_insn, /* 3 */
	other_illegal_insn, /* 4 */
	other_illegal_insn, /* 5 */
	other_illegal_insn, /* 6 */
	other_illegal_insn, /* 7 */
	sbi_atomic_or_w, /* 8 */
	sbi_atomic_or_w_rl, /* 9 */
	sbi_atomic_or_w_aq, /* 10 */
	sbi_atomic_or_w_aqrl, /* 11 */
#if __riscv_xlen == 64
	sbi_atomic_or_d, /* 12 */
	sbi_atomic_or_d_rl, /* 13 */
	sbi_atomic_or_d_aq, /* 14 */
	sbi_atomic_or_d_aqrl, /* 15 */
#else
	other_illegal_insn, /* 12 */
	other_illegal_insn, /* 13 */
	other_illegal_insn, /* 14 */
	other_illegal_insn, /* 15 */
#endif
	other_illegal_insn, /* 16 */
	other_illegal_insn, /* 17 */
	other_illegal_insn, /* 18 */
	other_illegal_insn, /* 19 */
	other_illegal_insn, /* 20 */
	other_illegal_insn, /* 21 */
	other_illegal_insn, /* 22 */
	other_illegal_insn, /* 23 */
	other_illegal_insn, /* 24 */
	other_illegal_insn, /* 25 */
	other_illegal_insn, /* 26 */
	other_illegal_insn, /* 27 */
	other_illegal_insn, /* 28 */
	other_illegal_insn, /* 29 */
	other_illegal_insn, /* 30 */
	other_illegal_insn, /* 31 */
};

static const illegal_insn_func amoand_table[32] = {
	other_illegal_insn, /* 0 */
	other_illegal_insn, /* 1 */
	other_illegal_insn, /* 2 */
	other_illegal_insn, /* 3 */
	other_illegal_insn, /* 4 */
	other_illegal_insn, /* 5 */
	other_illegal_insn, /* 6 */
	other_illegal_insn, /* 7 */
	sbi_atomic_and_w, /* 8 */
	sbi_atomic_and_w_rl, /* 9 */
	sbi_atomic_and_w_aq, /* 10 */
	sbi_atomic_and_w_aqrl, /* 11 */
#if __riscv_xlen == 64
	sbi_atomic_and_d, /* 12 */
	sbi_atomic_and_d_rl, /* 13 */
	sbi_atomic_and_d_aq, /* 14 */
	sbi_atomic_and_d_aqrl, /* 15 */
#else
	other_illegal_insn, /* 12 */
	other_illegal_insn, /* 13 */
	other_illegal_insn, /* 14 */
	other_illegal_insn, /* 15 */
#endif
	other_illegal_insn, /* 16 */
	other_illegal_insn, /* 17 */
	other_illegal_insn, /* 18 */
	other_illegal_insn, /* 19 */
	other_illegal_insn, /* 20 */
	other_illegal_insn, /* 21 */
	other_illegal_insn, /* 22 */
	other_illegal_insn, /* 23 */
	other_illegal_insn, /* 24 */
	other_illegal_insn, /* 25 */
	other_illegal_insn, /* 26 */
	other_illegal_insn, /* 27 */
	other_illegal_insn, /* 28 */
	other_illegal_insn, /* 29 */
	other_illegal_insn, /* 30 */
	other_illegal_insn, /* 31 */
};

static const illegal_insn_func amomin_table[32] = {
	other_illegal_insn, /* 0 */
	other_illegal_insn, /* 1 */
	other_illegal_insn, /* 2 */
	other_illegal_insn, /* 3 */
	other_illegal_insn, /* 4 */
	other_illegal_insn, /* 5 */
	other_illegal_insn, /* 6 */
	other_illegal_insn, /* 7 */
	sbi_atomic_min_w, /* 8 */
	sbi_atomic_min_w_rl, /* 9 */
	sbi_atomic_min_w_aq, /* 10 */
	sbi_atomic_min_w_aqrl, /* 11 */
#if __riscv_xlen == 64
	sbi_atomic_min_d, /* 12 */
	sbi_atomic_min_d_rl, /* 13 */
	sbi_atomic_min_d_aq, /* 14 */
	sbi_atomic_min_d_aqrl, /* 15 */
#else
	other_illegal_insn, /* 12 */
	other_illegal_insn, /* 13 */
	other_illegal_insn, /* 14 */
	other_illegal_insn, /* 15 */
#endif
	other_illegal_insn, /* 16 */
	other_illegal_insn, /* 17 */
	other_illegal_insn, /* 18 */
	other_illegal_insn, /* 19 */
	other_illegal_insn, /* 20 */
	other_illegal_insn, /* 21 */
	other_illegal_insn, /* 22 */
	other_illegal_insn, /* 23 */
	other_illegal_insn, /* 24 */
	other_illegal_insn, /* 25 */
	other_illegal_insn, /* 26 */
	other_illegal_insn, /* 27 */
	other_illegal_insn, /* 28 */
	other_illegal_insn, /* 29 */
	other_illegal_insn, /* 30 */
	other_illegal_insn, /* 31 */
};

static const illegal_insn_func amomax_table[32] = {
	other_illegal_insn, /* 0 */
	other_illegal_insn, /* 1 */
	other_illegal_insn, /* 2 */
	other_illegal_insn, /* 3 */
	other_illegal_insn, /* 4 */
	other_illegal_insn, /* 5 */
	other_illegal_insn, /* 6 */
	other_illegal_insn, /* 7 */
	sbi_atomic_max_w, /* 8 */
	sbi_atomic_max_w_rl, /* 9 */
	sbi_atomic_max_w_aq, /* 10 */
	sbi_atomic_max_w_aqrl, /* 11 */
#if __riscv_xlen == 64
	sbi_atomic_max_d, /* 12 */
	sbi_atomic_max_d_rl, /* 13 */
	sbi_atomic_max_d_aq, /* 14 */
	sbi_atomic_max_d_aqrl, /* 15 */
#else
	other_illegal_insn, /* 12 */
	other_illegal_insn, /* 13 */
	other_illegal_insn, /* 14 */
	other_illegal_insn, /* 15 */
#endif
	other_illegal_insn, /* 16 */
	other_illegal_insn, /* 17 */
	other_illegal_insn, /* 18 */
	other_illegal_insn, /* 19 */
	other_illegal_insn, /* 20 */
	other_illegal_insn, /* 21 */
	other_illegal_insn, /* 22 */
	other_illegal_insn, /* 23 */
	other_illegal_insn, /* 24 */
	other_illegal_insn, /* 25 */
	other_illegal_insn, /* 26 */
	other_illegal_insn, /* 27 */
	other_illegal_insn, /* 28 */
	other_illegal_insn, /* 29 */
	other_illegal_insn, /* 30 */
	other_illegal_insn, /* 31 */
};

static const illegal_insn_func amominu_table[32] = {
	other_illegal_insn, /* 0 */
	other_illegal_insn, /* 1 */
	other_illegal_insn, /* 2 */
	other_illegal_insn, /* 3 */
	other_illegal_insn, /* 4 */
	other_illegal_insn, /* 5 */
	other_illegal_insn, /* 6 */
	other_illegal_insn, /* 7 */
	sbi_atomic_minu_w, /* 8 */
	sbi_atomic_minu_w_rl, /* 9 */
	sbi_atomic_minu_w_aq, /* 10 */
	sbi_atomic_minu_w_aqrl, /* 11 */
#if __riscv_xlen == 64
	sbi_atomic_minu_d, /* 12 */
	sbi_atomic_minu_d_rl, /* 13 */
	sbi_atomic_minu_d_aq, /* 14 */
	sbi_atomic_minu_d_aqrl, /* 15 */
#else
	other_illegal_insn, /* 12 */
	other_illegal_insn, /* 13 */
	other_illegal_insn, /* 14 */
	other_illegal_insn, /* 15 */
#endif
	other_illegal_insn, /* 16 */
	other_illegal_insn, /* 17 */
	other_illegal_insn, /* 18 */
	other_illegal_insn, /* 19 */
	other_illegal_insn, /* 20 */
	other_illegal_insn, /* 21 */
	other_illegal_insn, /* 22 */
	other_illegal_insn, /* 23 */
	other_illegal_insn, /* 24 */
	other_illegal_insn, /* 25 */
	other_illegal_insn, /* 26 */
	other_illegal_insn, /* 27 */
	other_illegal_insn, /* 28 */
	other_illegal_insn, /* 29 */
	other_illegal_insn, /* 30 */
	other_illegal_insn, /* 31 */
};

static const illegal_insn_func amomaxu_table[32] = {
	other_illegal_insn, /* 0 */
	other_illegal_insn, /* 1 */
	other_illegal_insn, /* 2 */
	other_illegal_insn, /* 3 */
	other_illegal_insn, /* 4 */
	other_illegal_insn, /* 5 */
	other_illegal_insn, /* 6 */
	other_illegal_insn, /* 7 */
	sbi_atomic_maxu_w, /* 8 */
	sbi_atomic_maxu_w_rl, /* 9 */
	sbi_atomic_maxu_w_aq, /* 10 */
	sbi_atomic_maxu_w_aqrl, /* 11 */
#if __riscv_xlen == 64
	sbi_atomic_maxu_d, /* 12 */
	sbi_atomic_maxu_d_rl, /* 13 */
	sbi_atomic_maxu_d_aq, /* 14 */
	sbi_atomic_maxu_d_aqrl, /* 15 */
#else
	other_illegal_insn, /* 12 */
	other_illegal_insn, /* 13 */
	other_illegal_insn, /* 14 */
	other_illegal_insn, /* 15 */
#endif
	other_illegal_insn, /* 16 */
	other_illegal_insn, /* 17 */
	other_illegal_insn, /* 18 */
	other_illegal_insn, /* 19 */
	other_illegal_insn, /* 20 */
	other_illegal_insn, /* 21 */
	other_illegal_insn, /* 22 */
	other_illegal_insn, /* 23 */
	other_illegal_insn, /* 24 */
	other_illegal_insn, /* 25 */
	other_illegal_insn, /* 26 */
	other_illegal_insn, /* 27 */
	other_illegal_insn, /* 28 */
	other_illegal_insn, /* 29 */
	other_illegal_insn, /* 30 */
	other_illegal_insn, /* 31 */
};

static int amoadd_insn(ulong insn, struct sbi_trap_regs *regs)
{
	int wd = ((insn & WD_MASK) >> WD_SHIFT) << 2;
	int aqrl = (insn & AQRL_MASK) >> AQRL_SHIFT;
	return amoadd_table[wd + aqrl](insn, regs);
}

static int amoswap_insn(ulong insn, struct sbi_trap_regs *regs)
{
	int wd = ((insn & WD_MASK) >> WD_SHIFT) << 2;
	int aqrl = (insn & AQRL_MASK) >> AQRL_SHIFT;
	return amoswap_table[wd + aqrl](insn, regs);
}

static int amoxor_insn(ulong insn, struct sbi_trap_regs *regs)
{
	int wd = ((insn & WD_MASK) >> WD_SHIFT) << 2;
	int aqrl = (insn & AQRL_MASK) >> AQRL_SHIFT;
	return amoxor_table[wd + aqrl](insn, regs);
}

static int amoor_insn(ulong insn, struct sbi_trap_regs *regs)
{
	int wd = ((insn & WD_MASK) >> WD_SHIFT) << 2;
	int aqrl = (insn & AQRL_MASK) >> AQRL_SHIFT;
	return amoor_table[wd + aqrl](insn, regs);
}

static int amoand_insn(ulong insn, struct sbi_trap_regs *regs)
{
	int wd = ((insn & WD_MASK) >> WD_SHIFT) << 2;
	int aqrl = (insn & AQRL_MASK) >> AQRL_SHIFT;
	return amoand_table[wd + aqrl](insn, regs);
}

static int amomin_insn(ulong insn, struct sbi_trap_regs *regs)
{
	int wd = ((insn & WD_MASK) >> WD_SHIFT) << 2;
	int aqrl = (insn & AQRL_MASK) >> AQRL_SHIFT;
	return amomin_table[wd + aqrl](insn, regs);
}

static int amomax_insn(ulong insn, struct sbi_trap_regs *regs)
{
	int wd = ((insn & WD_MASK) >> WD_SHIFT) << 2;
	int aqrl = (insn & AQRL_MASK) >> AQRL_SHIFT;
	return amomax_table[wd + aqrl](insn, regs);
}

static int amominu_insn(ulong insn, struct sbi_trap_regs *regs)
{
	int wd = ((insn & WD_MASK) >> WD_SHIFT) << 2;
	int aqrl = (insn & AQRL_MASK) >> AQRL_SHIFT;
	return amominu_table[wd + aqrl](insn, regs);
}

static int amomaxu_insn(ulong insn, struct sbi_trap_regs *regs)
{
	int wd = ((insn & WD_MASK) >> WD_SHIFT) << 2;
	int aqrl = (insn & AQRL_MASK) >> AQRL_SHIFT;
	return amomaxu_table[wd + aqrl](insn, regs);
}

static const illegal_insn_func amo_insn_table[32] = {
	amoadd_insn, /* 0 */
	amoswap_insn, /* 1 */
	other_illegal_insn, /* 2 */
	other_illegal_insn, /* 3 */
	amoxor_insn, /* 4 */
	other_illegal_insn, /* 5 */
	other_illegal_insn, /* 6 */
	other_illegal_insn, /* 7 */
	amoor_insn, /* 8 */
	other_illegal_insn, /* 9 */
	other_illegal_insn, /* 10 */
	other_illegal_insn, /* 11 */
	amoand_insn, /* 12 */
	other_illegal_insn, /* 13 */
	other_illegal_insn, /* 14 */
	other_illegal_insn, /* 15 */
	amomin_insn, /* 16 */
	other_illegal_insn, /* 17 */
	other_illegal_insn, /* 18 */
	other_illegal_insn, /* 19 */
	amomax_insn, /* 20 */
	other_illegal_insn, /* 21 */
	other_illegal_insn, /* 22 */
	other_illegal_insn, /* 23 */
	amominu_insn, /* 24 */
	other_illegal_insn, /* 25 */
	other_illegal_insn, /* 26 */
	other_illegal_insn, /* 27 */
	amomaxu_insn, /* 28 */
	other_illegal_insn, /* 29 */
	other_illegal_insn, /* 30 */
	other_illegal_insn  /* 31 */
};

int sbi_illegal_insn_handler(struct sbi_trap_context *tcntx)
{
	struct sbi_trap_regs *regs = &tcntx->regs;
	ulong insn = tcntx->trap.tval;
	struct sbi_trap_info uptrap;

	/*
	 * We only deal with 32-bit (or longer) illegal instructions. If we
	 * see instruction is zero OR instruction is 16-bit then we fetch and
	 * check the instruction encoding using unprivilege access.
	 *
	 * The program counter (PC) in RISC-V world is always 2-byte aligned
	 * so handling only 32-bit (or longer) illegal instructions also help
	 * the case where MTVAL CSR contains instruction address for illegal
	 * instruction trap.
	 */

	sbi_pmu_ctr_incr_fw(SBI_PMU_FW_ILLEGAL_INSN);
	if (unlikely((insn & 3) != 3)) {
		insn = sbi_get_insn(regs->mepc, &uptrap);
		if (uptrap.cause)
			return sbi_trap_redirect(regs, &uptrap);
		if ((insn & 3) != 3)
			return truly_illegal_insn(insn, regs);
	}

	if ((insn & OPCODE_MASK) == AMO_OPCODE)
		return amo_insn_table[(insn >> 27) & 0x1f](insn, regs);

	return illegal_insn_table[(insn & 0x7c) >> 2](insn, regs);
}
