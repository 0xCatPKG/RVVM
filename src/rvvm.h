/*
rvvm.h - The RISC-V Virtual Machine
Copyright (C) 2021  LekKit <github.com/LekKit>
                    cerg2010cerg2010 <github.com/cerg2010cerg2010>
                    Mr0maks <mr.maks0443@gmail.com>
                    KotB <github.com/0xCatPKG>
                    X547 <github.com/X547>

This Source Code Form is subject to the terms of the Mozilla Public
License, v. 2.0. If a copy of the MPL was not distributed with this
file, You can obtain one at https://mozilla.org/MPL/2.0/.
*/

#ifndef RVVM_H
#define RVVM_H

#include "rvvmlib.h"
#include "rvvm_types.h"
#include "compiler.h"
#include "utils.h"
#include "vector.h"
#include "rvtimer.h"
#include "threading.h"
#include "blk_io.h"
#include "fdtlib.h"

#ifdef USE_JIT
#include "rvjit/rvjit.h"
#endif

#define TLB_SIZE 256  // Always nonzero, power of 2 (32, 64..)
BUILD_ASSERT(TLB_SIZE && !((TLB_SIZE - 1) & TLB_SIZE));

#define REGISTER_ZERO 0
#define REGISTER_X0   0
#define REGISTER_X1   1
#define REGISTER_X2   2
#define REGISTER_X3   3
#define REGISTER_X4   4
#define REGISTER_X5   5
#define REGISTER_X6   6
#define REGISTER_X7   7
#define REGISTER_X8   8
#define REGISTER_X9   9
#define REGISTER_X10  10
#define REGISTER_X11  11
#define REGISTER_X12  12
#define REGISTER_X13  13
#define REGISTER_X14  14
#define REGISTER_X15  15
#define REGISTER_X16  16
#define REGISTER_X17  17
#define REGISTER_X18  18
#define REGISTER_X19  19
#define REGISTER_X20  20
#define REGISTER_X21  21
#define REGISTER_X22  22
#define REGISTER_X23  23
#define REGISTER_X24  24
#define REGISTER_X25  25
#define REGISTER_X26  26
#define REGISTER_X27  27
#define REGISTER_X28  28
#define REGISTER_X29  29
#define REGISTER_X30  30
#define REGISTER_X31  31
#define REGISTER_PC   32

#define REGISTERS_MAX 33
#define FPU_REGISTERS_MAX 32

#define PRIVILEGE_USER       0
#define PRIVILEGE_SUPERVISOR 1 // VS-mode supervisor
#define PRIVILEGE_HYPERVISOR 2 // HS-mode supervisor
#define PRIVILEGE_MACHINE    3

#define PRIVILEGES_MAX       4

#define INTERRUPT_USOFTWARE    0x0
#define INTERRUPT_SSOFTWARE    0x1
#define INTERRUPT_MSOFTWARE    0x3
#define INTERRUPT_UTIMER       0x4
#define INTERRUPT_STIMER       0x5
#define INTERRUPT_MTIMER       0x7
#define INTERRUPT_UEXTERNAL    0x8
#define INTERRUPT_SEXTERNAL    0x9
#define INTERRUPT_MEXTERNAL    0xB

#define TRAP_INSTR_MISALIGN    0x0
#define TRAP_INSTR_FETCH       0x1
#define TRAP_ILL_INSTR         0x2
#define TRAP_BREAKPOINT        0x3
#define TRAP_LOAD_MISALIGN     0x4
#define TRAP_LOAD_FAULT        0x5
#define TRAP_STORE_MISALIGN    0x6
#define TRAP_STORE_FAULT       0x7
#define TRAP_ENVCALL_UMODE     0x8
#define TRAP_ENVCALL_SMODE     0x9
#define TRAP_ENVCALL_MMODE     0xB
#define TRAP_INSTR_PAGEFAULT   0xC
#define TRAP_LOAD_PAGEFAULT    0xD
#define TRAP_STORE_PAGEFAULT   0xF

/*
 * Address translation cache
 * In future, it would be nice to verify if cache-line alignment
 * gives any profit (entries scattered between cachelines waste L1)
 */
typedef struct {
    // Pointer to page (with vaddr subtracted? faster tlb translation)
    size_t ptr;
    // Make entry size a power of 2 (32 or 16 bytes)
#if !defined(HOST_64BIT) && defined(USE_RV64)
    size_t align;
#endif
    // Virtual page number per each op type (vaddr >> 12)
    virt_addr_t r;
    virt_addr_t w;
    virt_addr_t e;
#if defined(HOST_64BIT) && !defined(USE_RV64)
    virt_addr_t align[3];
#endif
} rvvm_tlb_entry_t;

#ifdef USE_JIT
typedef struct {
    // Pointer to code block
    rvjit_func_t block;
#if !defined(HOST_64BIT) && defined(USE_RV64)
    size_t align;
#endif
    // Virtual PC of this entry
    virt_addr_t pc;
#if defined(HOST_64BIT) && !defined(USE_RV64)
    virt_addr_t align;
#endif
} rvvm_jtlb_entry_t;
#endif

typedef struct {
    phys_addr_t begin; // First usable address in physical memory
    phys_addr_t size;  // Memory amount (since the region may be empty)
    vmptr_t data;      // Pointer to memory data
} rvvm_ram_t;

struct rvvm_hart_t {
    uint32_t wait_event;
    maxlen_t registers[REGISTERS_MAX];
#ifdef USE_FPU
    double fpu_registers[FPU_REGISTERS_MAX];
#endif

    // We want short offsets from vmptr to tlb
    rvvm_tlb_entry_t tlb[TLB_SIZE];
#ifdef USE_JIT
    rvvm_jtlb_entry_t jtlb[TLB_SIZE];
#endif
    rvvm_ram_t mem;
    rvvm_machine_t* machine;
    phys_addr_t root_page_table;
    uint8_t mmu_mode;
    uint8_t priv_mode;
    bool rv64;
    bool trap;
    maxlen_t trap_pc;

    bool userland;

    uint32_t lrsc;
    maxlen_t lrsc_cas;

    struct {
        maxlen_t hartid;
        maxlen_t isa;
        maxlen_t status;
        maxlen_t fcsr;

        maxlen_t ie;
        maxlen_t ip;

        maxlen_t edeleg[PRIVILEGES_MAX];
        maxlen_t ideleg[PRIVILEGES_MAX];
        maxlen_t tvec[PRIVILEGES_MAX];
        maxlen_t scratch[PRIVILEGES_MAX];
        maxlen_t epc[PRIVILEGES_MAX];
        maxlen_t cause[PRIVILEGES_MAX];
        maxlen_t tval[PRIVILEGES_MAX];
        maxlen_t counteren[PRIVILEGES_MAX];
        uint64_t envcfg[PRIVILEGES_MAX];
        uint64_t mseccfg;
    } csr;

#ifdef USE_JIT
    rvjit_block_t jit;
    bool jit_enabled;
    bool jit_compiling;
    bool block_ends;
    bool ldst_trace;
#endif

    thread_ctx_t* thread;
    cond_var_t* wfi_cond;

    rvtimecmp_t mtimecmp;
    rvtimecmp_t stimecmp;

    uint64_t pending_irqs;
    uint32_t pending_events;
    uint32_t preempt_ms;

    // Cacheline alignment
    uint8_t align[64];
};

struct rvvm_machine_t {
    rvvm_ram_t mem;
    vector_t(rvvm_hart_t*) harts;
    vector_t(rvvm_mmio_dev_t*) mmio_devs;
    rvtimer_t timer;

    uint32_t running;
    uint32_t power_state;
    bool rv64;

    rvfile_t* bootrom_file;
    rvfile_t* kernel_file;
    rvfile_t* dtb_file;

    plic_ctx_t* plic;
    pci_bus_t*  pci_bus;
    i2c_bus_t*  i2c_bus;

    rvvm_addr_t opts[RVVM_OPTS_ARR_SIZE];
#ifdef USE_FDT
    // FDT nodes for device tree generation
    struct fdt_node* fdt;
    struct fdt_node* fdt_soc;
    // Kernel cmdline
    char* cmdline;
#endif
};

#endif
