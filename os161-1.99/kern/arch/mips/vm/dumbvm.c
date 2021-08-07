#include "opt-A3.h"
/*
 * Copyright (c) 2000, 2001, 2002, 2003, 2004, 2005, 2008, 2009
 *	The President and Fellows of Harvard College.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE UNIVERSITY AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE UNIVERSITY OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <types.h>
#include <kern/errno.h>
#include <lib.h>
#include <spl.h>
#include <spinlock.h>
#include <proc.h>
#include <current.h>
#include <mips/tlb.h>
#include <addrspace.h>
#include <vm.h>

/*
 * Dumb MIPS-only "VM system" that is intended to only be just barely
 * enough to struggle off the ground.
 */

/* under dumbvm, always have 48k of user stack */
#define DUMBVM_STACKPAGES    12


#ifdef OPT_A3

/*
 * Coremap structure for managing the allocation of memory space for general VM
 */
struct CME { // Core Map Entry
	paddr_t addr;
	int seq_index;
};

/*
 * Page table structure for managing the allocation of memory space for user space
 */
struct PTE { // Page Table Entry
	paddr_t addr;
};

struct CME* the_coremap = NULL;   
unsigned long max_pages = 0;           // number of maximum available pages
#endif // OPT_A3

/*
 * Wrap rma_stealmem in a spinlock.
 */
static struct spinlock stealmem_lock = SPINLOCK_INITIALIZER;

void
vm_bootstrap(void)
{
#ifdef OPT_A3
	paddr_t lo, hi, base;
	unsigned long map_size;

	// Get the range of available ram
	ram_getsize(&lo, &hi);

	// Round lo down to a multiple of PAGE_SIZE to do mem addr alignment
	lo = (lo / PAGE_SIZE) * PAGE_SIZE;

	// Figure out how many pages can fit it that range
	max_pages = (hi - lo) / PAGE_SIZE;

	// Calculate the size of core map 
	// (i.e. how many pages will the map take, if the map keeps track of max_pages pages, which is # of entries)
	map_size = DIVROUNDUP(max_pages * sizeof(struct CME), PAGE_SIZE);
	max_pages -= map_size; // we need map_size pages to manage the remaining pages

	// Set starting paddr for the mapped memory pages in the map
	base = lo + (map_size * PAGE_SIZE);

	// Initialize the_coremap to be the lo
	the_coremap = (struct CME*)PADDR_TO_KVADDR(lo);

	// Initialize the whole core map
	for (unsigned long i = 0; i < max_pages; ++i) {
		the_coremap[i].addr = base + (i * PAGE_SIZE);
		the_coremap[i].seq_index = 0;
	}
#else
	/* Do nothing. */
#endif // OPT_A3
}

static
paddr_t
getppages(unsigned long npages)
{
#ifdef OPT_A3
	paddr_t ret = 0;
	unsigned long seqlen, start, seqnum;
	seqlen = start = seqnum = 0;
	spinlock_acquire(&stealmem_lock);

	if (the_coremap == NULL) { 
		// Core map havn't been set up
		ret = ram_stealmem(npages);
		spinlock_release(&stealmem_lock);
		return ret;
	} 

	for (unsigned long i = 0; i < max_pages; ++i) {
		if (the_coremap[i].seq_index == 0) {
			// Current entry is free for allocation
			if (seqlen == 0) {
				// A new sequence starting from this entry
				start = i;
				seqlen = 1;
			} else {
				// Append current entry to previous sequence
				seqlen++;
			}

			if (seqlen == npages) {
				// Found continuous pages of length n
				// Set the increasing sequence number for each page's seq_index
				for (unsigned long j = 0; j < seqlen; ++j) { the_coremap[start+j].seq_index = ++seqnum; }
				ret = the_coremap[start].addr;
				spinlock_release(&stealmem_lock);
				return ret;
			}

		} else {
			// Current entry is not free
			seqlen = 0;

		}
	} // for loop

	// Unable to find a continuous pages of length n
	spinlock_release(&stealmem_lock);
	return ret;

#else
	paddr_t addr;

	spinlock_acquire(&stealmem_lock);

	addr = ram_stealmem(npages);
	
	spinlock_release(&stealmem_lock);
	return addr;
#endif // OPT_A3
}

/* Allocate/free some kernel-space virtual pages */
vaddr_t 
alloc_kpages(int npages)
{
	paddr_t pa;
	pa = getppages(npages);
	if (pa==0) {
		return 0;
	}
	return PADDR_TO_KVADDR(pa);
}

void 
free_kpages(vaddr_t addr)
{
#ifdef OPT_A3

	if (addr == 0) {
		// CASE 1: free on NULL
		// Do nothing
		return;
	} else if (the_coremap == NULL) {
		// CASE 2: core map hasn't been set up
		// Do nothing
		return;
	} else if (!KVADDR_IS_VALID(addr)) {
		// CASE 3: addr is invalid
		// The behaviour of free() on invalid address is undefined, because it is unpredictable.
		return;
	}

	// CASE 4: addr is valid, try to find corresponding pages
	paddr_t paddr = KVADDR_TO_PADDR(addr);
	spinlock_acquire(&stealmem_lock);

	for (unsigned long i = 0; i < max_pages; ++i) {
		if (the_coremap[i].addr == paddr && the_coremap[i].seq_index == 1) {
			// Found the sequence
			int index = 1;
			do {
				the_coremap[i].seq_index = 0;
			} while (the_coremap[++i].seq_index == ++index);
			break;
		}
	}

	spinlock_release(&stealmem_lock);

#else
	/* nothing - leak the memory. */

	(void)addr;
#endif // OPT_A3
}

void
vm_tlbshootdown_all(void)
{
	panic("dumbvm tried to do tlb shootdown?!\n");
}

void
vm_tlbshootdown(const struct tlbshootdown *ts)
{
	(void)ts;
	panic("dumbvm tried to do tlb shootdown?!\n");
}

int
vm_fault(int faulttype, vaddr_t faultaddress)
{
	vaddr_t vbase1, vtop1, vbase2, vtop2, stackbase, stacktop;
	paddr_t paddr;
	int i;
	uint32_t ehi, elo; // ehi for TLB's high word (32 bits) - where page# lives
					   // elo for TLB's low  word (32 bits) - where frame#, 
					   //                                           write permission (TLBLO_DIRTY), and
					   //                                           valid bit (TLBLO_VALID) lives
	struct addrspace *as;
	int spl;

#ifdef OPT_A3
	bool is_read_only = false;     // flag for indicating whether the fault happens in Read-only area
#endif // OPT_A3

	faultaddress &= PAGE_FRAME; // getting page number

	DEBUG(DB_VM, "dumbvm: fault: 0x%x\n", faultaddress);

	switch (faulttype) {
	    case VM_FAULT_READONLY:
#ifdef OPT_A3
		return EFAULT;
#else
		/* We always create pages read-write, so we can't get this */
		panic("dumbvm: got VM_FAULT_READONLY\n");
#endif // OPT_A3
	    case VM_FAULT_READ:
	    case VM_FAULT_WRITE:
		break;
	    default:
		return EINVAL;
	}

	if (curproc == NULL) {
		/*
		 * No process. This is probably a kernel fault early
		 * in boot. Return EFAULT so as to panic instead of
		 * getting into an infinite faulting loop.
		 */
		return EFAULT;
	}

	as = curproc_getas();
	if (as == NULL) {
		/*
		 * No address space set up. This is probably also a
		 * kernel fault early in boot.
		 */
		return EFAULT;
	}

	/* Assert that the address space has been set up properly. */

	KASSERT(as->as_vbase1 != 0);
	KASSERT(as->as_npages1 != 0);
	KASSERT(as->as_vbase2 != 0);
	KASSERT(as->as_npages2 != 0);
	KASSERT((as->as_vbase1 & PAGE_FRAME) == as->as_vbase1);
	KASSERT((as->as_vbase2 & PAGE_FRAME) == as->as_vbase2);

#ifdef OPT_A3
	// Do not check for as_pbase1, as_pbase2, as_stackpbase which are 
	// legacy of simple continuous segementation implementation of VM
	KASSERT(as->as_table1 != 0);
	KASSERT(as->as_table2 != 0);
	KASSERT(as->as_stacktable != 0);

#else
	KASSERT(as->as_pbase1 != 0);
	KASSERT(as->as_pbase2 != 0);
	KASSERT(as->as_stackpbase != 0);
	KASSERT((as->as_pbase1 & PAGE_FRAME) == as->as_pbase1);
	KASSERT((as->as_pbase2 & PAGE_FRAME) == as->as_pbase2);
	KASSERT((as->as_stackpbase & PAGE_FRAME) == as->as_stackpbase);
#endif // OPT_A3

	vbase1 = as->as_vbase1;
	vtop1 = vbase1 + as->as_npages1 * PAGE_SIZE;
	vbase2 = as->as_vbase2;
	vtop2 = vbase2 + as->as_npages2 * PAGE_SIZE;
	stackbase = USERSTACK - DUMBVM_STACKPAGES * PAGE_SIZE;
	stacktop = USERSTACK;

#ifdef OPT_A3
	if (faultaddress >= vbase1 && faultaddress < vtop1) {
		paddr = as->as_table1[(faultaddress - vbase1) / PAGE_SIZE].addr;
		is_read_only = true;
	} 
	else if (faultaddress >= vbase2 && faultaddress < vtop2) {
		paddr = as->as_table2[(faultaddress - vbase2) / PAGE_SIZE].addr;
	} 
	else if (faultaddress >= stackbase && faultaddress < stacktop) {
		paddr = as->as_stacktable[(faultaddress - stackbase) / PAGE_SIZE].addr;
	} 
	else {
		return EFAULT;
	}
#else
	if (faultaddress >= vbase1 && faultaddress < vtop1) {
		paddr = (faultaddress - vbase1) + as->as_pbase1;
	}
	else if (faultaddress >= vbase2 && faultaddress < vtop2) {
		paddr = (faultaddress - vbase2) + as->as_pbase2;
	}
	else if (faultaddress >= stackbase && faultaddress < stacktop) {
		paddr = (faultaddress - stackbase) + as->as_stackpbase;
	}
	else {
		return EFAULT;
	}
#endif
	// now paddr is the physical addr of faultaddress

	/* make sure it's page-aligned */
	KASSERT((paddr & PAGE_FRAME) == paddr);

	/* Disable interrupts on this CPU while frobbing the TLB. */
	spl = splhigh();

	for (i=0; i<NUM_TLB; i++) {
		tlb_read(&ehi, &elo, i);
		if (elo & TLBLO_VALID) {
			continue;
		}
		ehi = faultaddress;
		elo = paddr | TLBLO_DIRTY | TLBLO_VALID; // set dirty bit and valid bit to 1 at paddr and pass it to elo
#ifdef OPT_A3
		if (is_read_only && as->is_elf_loaded) {
			// Set dirty bit to 0 if addr is in read only area and elf is loaded
			elo &= ~TLBLO_DIRTY; 
		}
#endif // OPT_A3
		DEBUG(DB_VM, "dumbvm: 0x%x -> 0x%x\n", faultaddress, paddr);
		tlb_write(ehi, elo, i);
		splx(spl);
		return 0;
	}
#ifdef OPT_A3
	ehi = faultaddress;
	elo = paddr | TLBLO_DIRTY | TLBLO_VALID; // set dirty bit and valid bit to 1 at paddr and pass it to elo
	if (is_read_only && as->is_elf_loaded) {
		// Set dirty bit to 0 if addr is in read only area and elf is loaded
		elo &= ~TLBLO_DIRTY;
	}

	tlb_random(ehi, elo); // randomly evict a TLB
	splx(spl);
	return 0;
#else
	kprintf("dumbvm: Ran out of TLB entries - cannot handle page fault\n");
	splx(spl);
	return EFAULT;
#endif // OPT_A3
}

struct addrspace *
as_create(void)
{
	struct addrspace *as = kmalloc(sizeof(struct addrspace));
	if (as==NULL) {
		return NULL;
	}

	as->as_vbase1 = 0;
	as->as_npages1 = 0;
	as->as_vbase2 = 0;
	as->as_npages2 = 0;

#ifdef OPT_A3
	as->as_table1 = 0;
	as->as_table2 = 0;
	as->as_stacktable = 0;
	as->is_elf_loaded = false;
#else 
	as->as_pbase1 = 0;
	as->as_pbase2 = 0;
	as->as_stackpbase = 0;
#endif // OPT_A3

	return as;
}

void
as_destroy(struct addrspace *as)
{
#ifdef OPT_A3
	// To completely destroy kernel space part of the address space 
	// allocated to a user program, we need to:
	
	// STEP 1: Free all pages pointed by the PTEs
	for (unsigned int i = 0; i < as->as_npages1; ++i) {
		free_kpages(PADDR_TO_KVADDR(as->as_table1[i].addr));
	}
	for (unsigned int i = 0; i < as->as_npages2; ++i) {
		free_kpages(PADDR_TO_KVADDR(as->as_table2[i].addr));
	}
	for (unsigned int i = 0; i < DUMBVM_STACKPAGES; ++i) {
		free_kpages(PADDR_TO_KVADDR(as->as_stacktable[i].addr));
	}

	// STEP 2: Free the PTEs in kernel space
	kfree(as->as_table1);
	kfree(as->as_table2);
	kfree(as->as_stacktable);

	// STEP 3: Free as address structure
#endif // OPT_A3
	kfree(as);
}

void
as_activate(void)
{
	int i, spl;
	struct addrspace *as;

	as = curproc_getas();
#ifdef UW
        /* Kernel threads don't have an address spaces to activate */
#endif // OPT_A3
	if (as == NULL) {
		return;
	}

	/* Disable interrupts on this CPU while frobbing the TLB. */
	spl = splhigh();

	for (i=0; i<NUM_TLB; i++) {
		tlb_write(TLBHI_INVALID(i), TLBLO_INVALID(), i);
	}

	splx(spl);
}

void
as_deactivate(void)
{
	/* nothing */
}

int
as_define_region(struct addrspace *as, vaddr_t vaddr, size_t sz,
		 int readable, int writeable, int executable)
{
	size_t npages; 

	/* Align the region. First, the base... */
	sz += vaddr & ~(vaddr_t)PAGE_FRAME;
	vaddr &= PAGE_FRAME;

	/* ...and now the length. */
	sz = (sz + PAGE_SIZE - 1) & PAGE_FRAME;

	npages = sz / PAGE_SIZE;

	/* We don't use these - all pages are read-write */
	(void)readable;
	(void)writeable;
	(void)executable;

	if (as->as_vbase1 == 0) {
		as->as_vbase1 = vaddr;
		as->as_npages1 = npages;
		return 0;
	}

	if (as->as_vbase2 == 0) {
		as->as_vbase2 = vaddr;
		as->as_npages2 = npages;
		return 0;
	}

	/*
	 * Support for more than two regions is not available.
	 */
	kprintf("dumbvm: Warning: too many regions\n");
	return EUNIMP;
}

static
void
as_zero_region(paddr_t paddr, unsigned npages)
{
	bzero((void *)PADDR_TO_KVADDR(paddr), npages * PAGE_SIZE);
}

int
as_prepare_load(struct addrspace *as)
{
#ifdef OPT_A3

	// Allocate page tables
	struct PTE *table1, *table2, *stacktable;  // quick aliases for allocated tables
	size_t npages1, npages2;                   // quick aliases for page sizes
	paddr_t paddr;                             // temp var

	// Set up three page tables in kernel addrspace for managing the three 
	// segments (code, data, stack) in user addrspace of the program
	table1 = as->as_table1 = kmalloc(sizeof(struct PTE) * (npages1 = as->as_npages1));
	if (table1 == NULL) {
		return ENOMEM;
	}
	table2 = as->as_table2 = kmalloc(sizeof(struct PTE) * (npages2 = as->as_npages2));
	if (table2 == NULL) {
		kfree(table1);
		return ENOMEM;
	}
	stacktable = as->as_stacktable = kmalloc(sizeof(struct PTE) * DUMBVM_STACKPAGES);
	if (stacktable == NULL) {
		kfree(table1); kfree(table2);
		return ENOMEM;
	}

	// Build up three page tables by allocating physical frames from the coremap
	for (unsigned int i = 0; i < npages1; ++i) {
		paddr =  getppages(1);
		if (paddr == 0) {
			// TODO: FREE PREVIOUSLY ALLOCATED PAGES
			kfree(table1); kfree(table2); kfree(stacktable);
			return ENOMEM;
		}
		as_zero_region(paddr, 1);
		table1[i].addr = paddr;
	}
	for (unsigned int i = 0; i < npages2; ++i) {
		paddr =  getppages(1);
		if (paddr == 0) {
			// TODO: FREE PREVIOUSLY ALLOCATED PAGES
			kfree(table1); kfree(table2); kfree(stacktable);
			return ENOMEM;
		}
		as_zero_region(paddr, 1);
		table2[i].addr = paddr;
	}
	for (unsigned int i = 0; i < DUMBVM_STACKPAGES; ++i) {
		paddr =  getppages(1);
		if (paddr == 0) {
			// TODO: FREE PREVIOUSLY ALLOCATED PAGES
			kfree(table1); kfree(table2); kfree(stacktable);
			return ENOMEM;
		}
		as_zero_region(paddr, 1);
		stacktable[i].addr = paddr;
	}

#else
	KASSERT(as->as_pbase1 == 0);
	KASSERT(as->as_pbase2 == 0);
	KASSERT(as->as_stackpbase == 0);

	as->as_pbase1 = getppages(as->as_npages1);
	if (as->as_pbase1 == 0) {
		return ENOMEM;
	}

	as->as_pbase2 = getppages(as->as_npages2);
	if (as->as_pbase2 == 0) {
		return ENOMEM;
	}

	as->as_stackpbase = getppages(DUMBVM_STACKPAGES);
	if (as->as_stackpbase == 0) {
		return ENOMEM;
	}
	
	as_zero_region(as->as_pbase1, as->as_npages1);
	as_zero_region(as->as_pbase2, as->as_npages2);
	as_zero_region(as->as_stackpbase, DUMBVM_STACKPAGES);
#endif // OPT_A3

	return 0;
}

int
as_complete_load(struct addrspace *as)
{
#ifdef OPT_A3
	as->is_elf_loaded = true;
	as_activate();
#endif // OPT_A3
	(void)as;
	return 0;
}

int
as_define_stack(struct addrspace *as, vaddr_t *stackptr)
{
#ifdef OPT_A3
	KASSERT(as->as_stacktable != 0);
#else
	KASSERT(as->as_stackpbase != 0);
#endif // OPT_A3

	*stackptr = USERSTACK;
	return 0;
}

int
as_copy(struct addrspace *old, struct addrspace **ret)
{
	struct addrspace *new;

	new = as_create();
	if (new==NULL) {
		return ENOMEM;
	}

	new->as_vbase1 = old->as_vbase1;
	new->as_npages1 = old->as_npages1;
	new->as_vbase2 = old->as_vbase2;
	new->as_npages2 = old->as_npages2;

	/* (Mis)use as_prepare_load to allocate some physical memory. */
	if (as_prepare_load(new)) {
		as_destroy(new);
		return ENOMEM;
	}

#ifdef OPT_A3
	KASSERT(new->as_table1 != 0);
	KASSERT(new->as_table2 != 0);
	KASSERT(new->as_stacktable != 0);

	// Do memmove() page by page
	for (unsigned int i = 0; i < new->as_npages1; ++i) {
		memmove((void *)PADDR_TO_KVADDR(new->as_table1[i].addr),         // destination
				(const void *)PADDR_TO_KVADDR(old->as_table1[i].addr),   // source
				PAGE_SIZE);											     // move size
	}
	for (unsigned int i = 0; i < new->as_npages2; ++i) {
		memmove((void *)PADDR_TO_KVADDR(new->as_table2[i].addr),         // destination
				(const void *)PADDR_TO_KVADDR(old->as_table2[i].addr),   // source
				PAGE_SIZE);											     // move size
	}
	for (unsigned int i = 0; i < DUMBVM_STACKPAGES; ++i) {
		memmove((void *)PADDR_TO_KVADDR(new->as_stacktable[i].addr),         // destination
				(const void *)PADDR_TO_KVADDR(old->as_stacktable[i].addr),   // source
				PAGE_SIZE);											         // move size
	}
	
#else

	KASSERT(new->as_pbase1 != 0);
	KASSERT(new->as_pbase2 != 0);
	KASSERT(new->as_stackpbase != 0);

	memmove((void *)PADDR_TO_KVADDR(new->as_pbase1),
		(const void *)PADDR_TO_KVADDR(old->as_pbase1),
		old->as_npages1*PAGE_SIZE);

	memmove((void *)PADDR_TO_KVADDR(new->as_pbase2),
		(const void *)PADDR_TO_KVADDR(old->as_pbase2),
		old->as_npages2*PAGE_SIZE);

	memmove((void *)PADDR_TO_KVADDR(new->as_stackpbase),
		(const void *)PADDR_TO_KVADDR(old->as_stackpbase),
		DUMBVM_STACKPAGES*PAGE_SIZE);
#endif
	*ret = new;
	return 0;
}
