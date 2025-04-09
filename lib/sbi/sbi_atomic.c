/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2025 MIPS
 *
 */

#include <sbi/riscv_asm.h>
#include <sbi/sbi_trap.h>
#include <sbi/sbi_hart.h>

#define DEFINE_UNPRIVILEGED_LR_FUNCTION(type, aqrl, insn)                     \
        type sbi_lr_##type##aqrl(const type *addr,                            \
                           struct sbi_trap_info *trap)                        \
        {                                                                     \
                register ulong tinfo asm("a3");                               \
                register ulong mstatus = 0;                                   \
                register ulong mtvec = sbi_hart_expected_trap_addr();         \
                type ret = 0;                                                 \
                trap->cause = 0;                                              \
                asm volatile(                                                 \
                        "add %[tinfo], %[taddr], zero\n"                      \
                        "csrrw %[mtvec], " STR(CSR_MTVEC) ", %[mtvec]\n"      \
                        "csrrs %[mstatus], " STR(CSR_MSTATUS) ", %[mprv]\n"   \
                        ".option push\n"                                      \
                        ".option norvc\n"                                     \
                        #insn " %[ret], %[addr]\n"                            \
                        ".option pop\n"                                       \
                        "csrw " STR(CSR_MSTATUS) ", %[mstatus]\n"             \
                        "csrw " STR(CSR_MTVEC) ", %[mtvec]"                   \
                    : [mstatus] "+&r"(mstatus), [mtvec] "+&r"(mtvec),         \
                      [tinfo] "+&r"(tinfo), [ret] "=&r"(ret)                  \
                    : [addr] "m"(*addr), [mprv] "r"(MSTATUS_MPRV),            \
                      [taddr] "r"((ulong)trap)                                \
                    : "a4", "memory");                                        \
                return ret;                                                   \
        }

#define DEFINE_UNPRIVILEGED_SC_FUNCTION(type, aqrl, insn)                     \
        type sbi_sc_##type##aqrl(type *addr, type val,                        \
                           struct sbi_trap_info *trap)                        \
        {                                                                     \
                register ulong tinfo asm("a3");                               \
                register ulong mstatus = 0;                                   \
                register ulong mtvec = sbi_hart_expected_trap_addr();         \
                type ret = 0;                                                 \
                trap->cause = 0;                                              \
                asm volatile(                                                 \
                        "add %[tinfo], %[taddr], zero\n"                      \
                        "csrrw %[mtvec], " STR(CSR_MTVEC) ", %[mtvec]\n"      \
                        "csrrs %[mstatus], " STR(CSR_MSTATUS) ", %[mprv]\n"   \
                        ".option push\n"                                      \
                        ".option norvc\n"                                     \
                        #insn " %[ret], %[val], %[addr]\n"                    \
                        ".option pop\n"                                       \
                        "csrw " STR(CSR_MSTATUS) ", %[mstatus]\n"             \
                        "csrw " STR(CSR_MTVEC) ", %[mtvec]"                   \
                    : [mstatus] "+&r"(mstatus), [mtvec] "+&r"(mtvec),         \
                      [tinfo] "+&r"(tinfo), [ret] "=&r"(ret)                  \
                    : [addr] "m"(*addr), [mprv] "r"(MSTATUS_MPRV),            \
                      [val] "r"(val), [taddr] "r"((ulong)trap)                \
                    : "a4", "memory");                                        \
                return ret;                                                   \
        }

DEFINE_UNPRIVILEGED_LR_FUNCTION(s32, , lr.w);
DEFINE_UNPRIVILEGED_LR_FUNCTION(s32, _aq, lr.w.aq);
DEFINE_UNPRIVILEGED_LR_FUNCTION(s32, _rl, lr.w.rl);
DEFINE_UNPRIVILEGED_LR_FUNCTION(s32, _aqrl, lr.w.aqrl);
DEFINE_UNPRIVILEGED_SC_FUNCTION(s32, , sc.w);
DEFINE_UNPRIVILEGED_SC_FUNCTION(s32, _aq, sc.w.aq);
DEFINE_UNPRIVILEGED_SC_FUNCTION(s32, _rl, sc.w.rl);
DEFINE_UNPRIVILEGED_SC_FUNCTION(s32, _aqrl, sc.w.aqrl);
#if __riscv_xlen == 64
DEFINE_UNPRIVILEGED_LR_FUNCTION(s64, , lr.d);
DEFINE_UNPRIVILEGED_LR_FUNCTION(s64, _aq, lr.d.aq);
DEFINE_UNPRIVILEGED_LR_FUNCTION(s64, _rl, lr.d.rl);
DEFINE_UNPRIVILEGED_LR_FUNCTION(s64, _aqrl, lr.d.aqrl);
DEFINE_UNPRIVILEGED_SC_FUNCTION(s64, , sc.d);
DEFINE_UNPRIVILEGED_SC_FUNCTION(s64, _aq, sc.d.aq);
DEFINE_UNPRIVILEGED_SC_FUNCTION(s64, _rl, sc.d.rl);
DEFINE_UNPRIVILEGED_SC_FUNCTION(s64, _aqrl, sc.d.aqrl);
#endif

#define DEFINE_ATOMIC_FUNCTION(name, type, func)                              \
	int sbi_atomic_##name(ulong insn, struct sbi_trap_regs *regs)         \
	{                                                                     \
		struct sbi_trap_info uptrap;                                  \
		ulong addr = GET_RS1(insn, regs);                             \
		ulong val = GET_RS2(insn, regs);                              \
		ulong rd_val = 0;                                             \
		ulong fail = 1;                                               \
		while (fail) {                                                \
			rd_val = sbi_lr_##type((void *)addr, &uptrap);        \
			if (uptrap.cause) {                                   \
				return sbi_trap_redirect(regs, &uptrap);      \
			}                                                     \
			fail = sbi_sc_##type((void *)addr, func, &uptrap);    \
			if (uptrap.cause) {                                   \
				return sbi_trap_redirect(regs, &uptrap);      \
			}                                                     \
		}                                                             \
		SET_RD(insn, regs, rd_val);                                   \
		regs->mepc += 4;                                              \
		return 0;                                                     \
	}

DEFINE_ATOMIC_FUNCTION(add_w, s32, rd_val + val);
DEFINE_ATOMIC_FUNCTION(add_w_aq, s32_aq, rd_val + val);
DEFINE_ATOMIC_FUNCTION(add_w_rl, s32_rl, rd_val + val);
DEFINE_ATOMIC_FUNCTION(add_w_aqrl, s32_aqrl, rd_val + val);
DEFINE_ATOMIC_FUNCTION(and_w, s32, rd_val & val);
DEFINE_ATOMIC_FUNCTION(and_w_aq, s32_aq, rd_val & val);
DEFINE_ATOMIC_FUNCTION(and_w_rl, s32_rl, rd_val & val);
DEFINE_ATOMIC_FUNCTION(and_w_aqrl, s32_aqrl, rd_val & val);
DEFINE_ATOMIC_FUNCTION(or_w, s32, rd_val | val);
DEFINE_ATOMIC_FUNCTION(or_w_aq, s32_aq, rd_val | val);
DEFINE_ATOMIC_FUNCTION(or_w_rl, s32_rl, rd_val | val);
DEFINE_ATOMIC_FUNCTION(or_w_aqrl, s32_aqrl, rd_val | val);
DEFINE_ATOMIC_FUNCTION(xor_w, s32, rd_val ^ val);
DEFINE_ATOMIC_FUNCTION(xor_w_aq, s32_aq, rd_val ^ val);
DEFINE_ATOMIC_FUNCTION(xor_w_rl, s32_rl, rd_val ^ val);
DEFINE_ATOMIC_FUNCTION(xor_w_aqrl, s32_aqrl, rd_val ^ val);
DEFINE_ATOMIC_FUNCTION(swap_w, s32, val);
DEFINE_ATOMIC_FUNCTION(swap_w_aq, s32_aq, val);
DEFINE_ATOMIC_FUNCTION(swap_w_rl, s32_rl, val);
DEFINE_ATOMIC_FUNCTION(swap_w_aqrl, s32_aqrl, val);
DEFINE_ATOMIC_FUNCTION(max_w, s32, (s32)rd_val > (s32)val ? rd_val : val);
DEFINE_ATOMIC_FUNCTION(max_w_aq, s32_aq, (s32)rd_val > (s32)val ? rd_val : val);
DEFINE_ATOMIC_FUNCTION(max_w_rl, s32_rl, (s32)rd_val > (s32)val ? rd_val : val);
DEFINE_ATOMIC_FUNCTION(max_w_aqrl, s32_aqrl, (s32)rd_val > (s32)val ? rd_val : val);
DEFINE_ATOMIC_FUNCTION(maxu_w, s32, (u32)rd_val > (u32)val ? rd_val : val);
DEFINE_ATOMIC_FUNCTION(maxu_w_aq, s32_aq, (u32)rd_val > (u32)val ? rd_val : val);
DEFINE_ATOMIC_FUNCTION(maxu_w_rl, s32_rl, (u32)rd_val > (u32)val ? rd_val : val);
DEFINE_ATOMIC_FUNCTION(maxu_w_aqrl, s32_aqrl, (u32)rd_val > (u32)val ? rd_val : val);
DEFINE_ATOMIC_FUNCTION(min_w, s32, (s32)rd_val < (s32)val ? rd_val : val);
DEFINE_ATOMIC_FUNCTION(min_w_aq, s32_aq, (s32)rd_val < (s32)val ? rd_val : val);
DEFINE_ATOMIC_FUNCTION(min_w_rl, s32_rl, (s32)rd_val < (s32)val ? rd_val : val);
DEFINE_ATOMIC_FUNCTION(min_w_aqrl, s32_aqrl, (s32)rd_val < (s32)val ? rd_val : val);
DEFINE_ATOMIC_FUNCTION(minu_w, s32, (u32)rd_val < (u32)val ? rd_val : val);
DEFINE_ATOMIC_FUNCTION(minu_w_aq, s32_aq, (u32)rd_val < (u32)val ? rd_val : val);
DEFINE_ATOMIC_FUNCTION(minu_w_rl, s32_rl, (u32)rd_val < (u32)val ? rd_val : val);
DEFINE_ATOMIC_FUNCTION(minu_w_aqrl, s32_aqrl, (u32)rd_val < (u32)val ? rd_val : val);

#if __riscv_xlen == 64
DEFINE_ATOMIC_FUNCTION(add_d, s64, rd_val + val);
DEFINE_ATOMIC_FUNCTION(add_d_aq, s64_aq, rd_val + val);
DEFINE_ATOMIC_FUNCTION(add_d_rl, s64_rl, rd_val + val);
DEFINE_ATOMIC_FUNCTION(add_d_aqrl, s64_aqrl, rd_val + val);
DEFINE_ATOMIC_FUNCTION(and_d, s64, rd_val & val);
DEFINE_ATOMIC_FUNCTION(and_d_aq, s64_aq, rd_val & val);
DEFINE_ATOMIC_FUNCTION(and_d_rl, s64_rl, rd_val & val);
DEFINE_ATOMIC_FUNCTION(and_d_aqrl, s64_aqrl, rd_val & val);
DEFINE_ATOMIC_FUNCTION(or_d, s64, rd_val | val);
DEFINE_ATOMIC_FUNCTION(or_d_aq, s64_aq, rd_val | val);
DEFINE_ATOMIC_FUNCTION(or_d_rl, s64_rl, rd_val | val);
DEFINE_ATOMIC_FUNCTION(or_d_aqrl, s64_aqrl, rd_val | val);
DEFINE_ATOMIC_FUNCTION(xor_d, s64, rd_val ^ val);
DEFINE_ATOMIC_FUNCTION(xor_d_aq, s64_aq, rd_val ^ val);
DEFINE_ATOMIC_FUNCTION(xor_d_rl, s64_rl, rd_val ^ val);
DEFINE_ATOMIC_FUNCTION(xor_d_aqrl, s64_aqrl, rd_val ^ val);
DEFINE_ATOMIC_FUNCTION(swap_d, s64, val);
DEFINE_ATOMIC_FUNCTION(swap_d_aq, s64_aq, val);
DEFINE_ATOMIC_FUNCTION(swap_d_rl, s64_rl, val);
DEFINE_ATOMIC_FUNCTION(swap_d_aqrl, s64_aqrl, val);
DEFINE_ATOMIC_FUNCTION(max_d, s64, (s64)rd_val > (s64)val ? rd_val : val);
DEFINE_ATOMIC_FUNCTION(max_d_aq, s64_aq, (s64)rd_val > (s64)val ? rd_val : val);
DEFINE_ATOMIC_FUNCTION(max_d_rl, s64_rl, (s64)rd_val > (s64)val ? rd_val : val);
DEFINE_ATOMIC_FUNCTION(max_d_aqrl, s64_aqrl, (s64)rd_val > (s64)val ? rd_val : val);
DEFINE_ATOMIC_FUNCTION(maxu_d, s64, (u64)rd_val > (u64)val ? rd_val : val);
DEFINE_ATOMIC_FUNCTION(maxu_d_aq, s64_aq, (u64)rd_val > (u64)val ? rd_val : val);
DEFINE_ATOMIC_FUNCTION(maxu_d_rl, s64_rl, (u64)rd_val > (u64)val ? rd_val : val);
DEFINE_ATOMIC_FUNCTION(maxu_d_aqrl, s64_aqrl, (u64)rd_val > (u64)val ? rd_val : val);
DEFINE_ATOMIC_FUNCTION(min_d, s64, (s64)rd_val < (s64)val ? rd_val : val);
DEFINE_ATOMIC_FUNCTION(min_d_aq, s64_aq, (s64)rd_val < (s64)val ? rd_val : val);
DEFINE_ATOMIC_FUNCTION(min_d_rl, s64_rl, (s64)rd_val < (s64)val ? rd_val : val);
DEFINE_ATOMIC_FUNCTION(min_d_aqrl, s64_aqrl, (s64)rd_val < (s64)val ? rd_val : val);
DEFINE_ATOMIC_FUNCTION(minu_d, s64, (u64)rd_val < (u64)val ? rd_val : val);
DEFINE_ATOMIC_FUNCTION(minu_d_aq, s64_aq, (u64)rd_val < (u64)val ? rd_val : val);
DEFINE_ATOMIC_FUNCTION(minu_d_rl, s64_rl, (u64)rd_val < (u64)val ? rd_val : val);
DEFINE_ATOMIC_FUNCTION(minu_d_aqrl, s64_aqrl, (u64)rd_val < (u64)val ? rd_val : val);
#endif
