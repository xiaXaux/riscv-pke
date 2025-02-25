/*
 * Utility functions for process management. 
 *
 * Note: in Lab1, only one process (i.e., our user application) exists. Therefore, 
 * PKE OS at this stage will set "current" to the loaded user application, and also
 * switch to the old "current" process after trap handling.
 */

#include "riscv.h"
#include "strap.h"
#include "config.h"
#include "process.h"
#include "elf.h"
#include "string.h"
#include "vmm.h"
#include "pmm.h"
#include "memlayout.h"
#include "spike_interface/spike_utils.h"

//Two functions defined in kernel/usertrap.S
extern char smode_trap_vector[];
extern void return_to_user(trapframe *, uint64 satp);

// current points to the currently running user-mode application.
process* current = NULL;

// points to the first free page in our simple heap. added @lab2_2
uint64 g_ufree_page = USER_FREE_ADDRESS_START;

//
// switch to a user-mode process
//
void switch_to(process* proc) {
  assert(proc);
  current = proc;

  // write the smode_trap_vector (64-bit func. address) defined in kernel/strap_vector.S
  // to the stvec privilege register, such that trap handler pointed by smode_trap_vector
  // will be triggered when an interrupt occurs in S mode.
  write_csr(stvec, (uint64)smode_trap_vector);

  // set up trapframe values (in process structure) that smode_trap_vector will need when
  // the process next re-enters the kernel.
  proc->trapframe->kernel_sp = proc->kstack;      // process's kernel stack
  proc->trapframe->kernel_satp = read_csr(satp);  // kernel page table
  proc->trapframe->kernel_trap = (uint64)smode_trap_handler;

  // SSTATUS_SPP and SSTATUS_SPIE are defined in kernel/riscv.h
  // set S Previous Privilege mode (the SSTATUS_SPP bit in sstatus register) to User mode.
  unsigned long x = read_csr(sstatus);
  x &= ~SSTATUS_SPP;  // clear SPP to 0 for user mode
  x |= SSTATUS_SPIE;  // enable interrupts in user mode

  // write x back to 'sstatus' register to enable interrupts, and sret destination mode.
  write_csr(sstatus, x);

  // set S Exception Program Counter (sepc register) to the elf entry pc.
  write_csr(sepc, proc->trapframe->epc);

  // make user page table. macro MAKE_SATP is defined in kernel/riscv.h. added @lab2_1
  uint64 user_satp = MAKE_SATP(proc->pagetable);

  // return_to_user() is defined in kernel/strap_vector.S. switch to user mode with sret.
  // note, return_to_user takes two parameters @ and after lab2_1.
  return_to_user(proc->trapframe, user_satp);
}

uint64 alloc_va(uint64 n){
    int pt = 0;
    for(pt = 0;pt < current->unalloc_pt;pt++)
        if(current->unalloc_list[pt].sz >= n)
            break;
    if(pt == current->unalloc_pt)
        return 0;
    uint64 va = current->unalloc_list[pt].va;
    if(current->unalloc_list[pt].sz > n){
        current->unalloc_list[pt].sz -= n;
        current->unalloc_list[pt].va += n;
    }else if(current->unalloc_list[pt].sz == n){
        for(int i = pt;i < current->unalloc_pt - 1;i++)
            current->unalloc_list[i] = current->unalloc_list[i + 1];
        current->unalloc_pt--;
    }
    current->alloc_list[current->alloc_pt].va = va;
    current->alloc_list[current->alloc_pt].sz = n;
    current->alloc_pt++;
    return va;
}

void alloc_and_map_page(){
    uint64 pa = (uint64)alloc_page();
    user_vm_map(current->pagetable,g_ufree_page,PGSIZE,pa,prot_to_type(PROT_READ | PROT_WRITE,1));
    current->unalloc_list[current->unalloc_pt].va = g_ufree_page;
    current->unalloc_list[current->unalloc_pt].sz = PGSIZE;
    current->unalloc_pt++;
    g_ufree_page += PGSIZE;

    if(current->unalloc_pt >= 2){
        if(current->unalloc_list[current->unalloc_pt - 2].va + current->unalloc_list[current->unalloc_pt - 2].sz ==
            current->unalloc_list[current->unalloc_pt - 1].va){
            current->unalloc_list[current->unalloc_pt - 2].sz += current->unalloc_list[current->unalloc_pt - 1].sz;
            current->unalloc_pt--;
        }
    }
}

uint64 better_alloc(uint64 n){
    uint64 va = alloc_va(n);
    if(va == 0) {
        alloc_and_map_page();
        va = alloc_va(n);
    }
    return va;
}

void better_free(uint64 va){
    int pt = 0;
    for(pt = 0;pt < current->alloc_pt;pt++)
        if(current->alloc_list[pt].va == va)
            break;
    uint64 n = current->alloc_list[pt].sz;
    for(int i = pt;i < current->alloc_pt - 1;i++)
        current->alloc_list[i] = current->alloc_list[i + 1];
    current->alloc_pt--;
    for(int i = 0;i <= current->unalloc_pt;i++){
        if(i == current->unalloc_pt){
            current->unalloc_list[i].va = va;
            current->unalloc_list[i].sz = n;
            current->unalloc_pt++;
            break;
        }else if(va < current->unalloc_list[i].va){
            if(va + n == current->unalloc_list[i].va){
                current->unalloc_list[i].va = va;
                current->unalloc_list[i].sz += n;
            }else{
                for(int j = current->unalloc_pt;j > i;j--)
                    current->unalloc_list[j] = current->unalloc_list[j - 1];
                current->unalloc_pt++;
                current->unalloc_list[i].va = va;
                current->unalloc_list[i].sz = n;
            }
            break;
        }
    }
}
