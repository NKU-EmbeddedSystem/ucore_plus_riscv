#include <default_pmm.h>
#include <types.h>
#include <error.h>
#include <memlayout.h>
#include <mmu.h>
#include <pmm.h>
#include <sbi.h>
#include <stdio.h>
#include <string.h>
#include <sync.h>
#include <riscv.h>

// virtual address of physical page array
struct Page *pages;
// amount of physical memory (in pages)
size_t npage = 0;
// The kernel image is mapped at VA=KERNBASE and PA=info.base
uint_t va_pa_offset;
// memory starts at 0x80000000 in RISC-V
const size_t nbase = DRAM_BASE / PGSIZE;

// virtual address of boot-time page directory
pde_t *boot_pgdir = NULL;
// physical address of boot-time page directory
uintptr_t boot_cr3;

// physical memory management
const struct pmm_manager *pmm_manager;


static void check_alloc_page(void);
static void check_pgdir(void);
static void check_boot_pgdir(void);

// init_pmm_manager - initialize a pmm_manager instance
static void init_pmm_manager(void) {
    pmm_manager = &default_pmm_manager;
    cprintf("memory management: %s\n", pmm_manager->name);
    pmm_manager->init();
}

// init_memmap - call pmm->init_memmap to build Page struct for free memory
static void init_memmap(struct Page *base, size_t n) {
    pmm_manager->init_memmap(base, n);
}

// alloc_pages - call pmm->alloc_pages to allocate a continuous n*PAGESIZE
// memory
struct Page *alloc_pages(size_t n) {
    struct Page *page = NULL;
    bool intr_flag;

    
    local_intr_save(intr_flag);
    { page = pmm_manager->alloc_pages(n); }
    local_intr_restore(intr_flag);
    // cprintf("n %d,get page %x, No %d in alloc_pages\n",n,page,(page-pages));
    return page;
}

// free_pages - call pmm->free_pages to free a continuous n*PAGESIZE memory
void free_pages(struct Page *base, size_t n) {
    bool intr_flag;

    local_intr_save(intr_flag);
    { pmm_manager->free_pages(base, n); }
    local_intr_restore(intr_flag);
}

// nr_free_pages - call pmm->nr_free_pages to get the size (nr*PAGESIZE)
// of current free memory
size_t nr_free_pages(void) {
    size_t ret;
    bool intr_flag;
    local_intr_save(intr_flag);
    { ret = pmm_manager->nr_free_pages(); }
    local_intr_restore(intr_flag);
    return ret;
}

/* page_init - initialize the physical memory management */
static void page_init(void) {
    extern char kern_entry[];

    va_pa_offset = KERNBASE - 0x80200000;
    uint64_t mem_begin = KERNEL_BEGIN_PADDR;
    uint64_t mem_size = PHYSICAL_MEMORY_END - KERNEL_BEGIN_PADDR;
    uint64_t mem_end = PHYSICAL_MEMORY_END; //硬编码取代 sbi_query_memory()接口
    cprintf("membegin %llx memend %llx mem_size %llx\n",mem_begin, mem_end, mem_size);
    cprintf("physcial memory map:\n");
    cprintf("  memory: 0x%08lx, [0x%08lx, 0x%08lx].\n", mem_size, mem_begin,
            mem_end - 1);
    uint64_t maxpa = mem_end;

    if (maxpa > KERNTOP) {
        maxpa = KERNTOP;
    }

    extern char end[];

    npage = maxpa / PGSIZE;
    // BBL has put the initial page table at the first available page after the
    // kernel
    // so stay away from it by adding extra offset to end
    pages = (struct Page *)ROUNDUP((void *)end, PGSIZE);
    for (size_t i = 0; i < npage - nbase; i++) {
        SetPageReserved(pages + i);
    }

    uintptr_t freemem = PADDR((uintptr_t)pages + sizeof(struct Page) * (npage - nbase));
    mem_begin = ROUNDUP(freemem, PGSIZE);
    mem_end = ROUNDDOWN(mem_end, PGSIZE);
    if (freemem < mem_end) {
        init_memmap(pa2page(mem_begin), (mem_end - mem_begin) / PGSIZE);
    }
}

// pmm_init - setup a pmm to manage physical memory, build PDT&PT to setup
// paging mechanism
//         - check the correctness of pmm & paging mechanism, print PDT&PT
void pmm_init(void) {
    // We need to alloc/free the physical memory (granularity is 4KB or other
    // size).
    // So a framework of physical memory manager (struct pmm_manager)is defined
    // in pmm.h
    // First we should init a physical memory manager(pmm) based on the
    // framework.
    // Then pmm can alloc/free the physical memory.
    init_pmm_manager();

    // detect physical memory space, reserve already used memory,
    // then use pmm->init_memmap to create free page list
    page_init();

    // use pmm->check to verify the correctness of the alloc/free function in a pmm
    check_alloc_page();
    
    // create boot_pgdir, an initial page directory(Page Directory Table, PDT)
    extern char boot_page_table_sv39[];
    boot_pgdir = (pte_t*)boot_page_table_sv39;
    boot_cr3 = PADDR(boot_pgdir);
    cprintf("satp virtual address: 0x%016lx\nsatp physical address: 0x%016lx\n", boot_pgdir, boot_cr3);
}

void tlb_update(pde_t * pgdir, uintptr_t la) {
    tlb_invalidate(pgdir, la);
}

// invalidate a TLB entry, but only if the page tables being
// edited are the ones currently in use by the processor.
void tlb_invalidate(pde_t *pgdir, uintptr_t la) { flush_tlb(); }

static void check_alloc_page(void) {
    pmm_manager->check();
    cprintf("check_alloc_page() succeeded!\n");
}

static void check_pgdir(void) {
    // assert(npage <= KMEMSIZE / PGSIZE);
    // The memory starts at 2GB in RISC-V
    // so npage is always larger than KMEMSIZE / PGSIZE
    assert(npage <= KERNTOP / PGSIZE);
    assert(boot_pgdir != NULL && (uint32_t)PGOFF(boot_pgdir) == 0);
    assert(get_page(boot_pgdir, 0x0, NULL) == NULL);

    struct Page *p1, *p2;
    p1 = alloc_page();
    assert(page_insert(boot_pgdir, p1, 0x0, 0) == 0);
    pte_t *ptep;
    assert((ptep = get_pte(boot_pgdir, 0x0, 0)) != NULL);
    assert(pte2page(*ptep) == p1);
    assert(page_ref(p1) == 1);

    ptep = (pte_t *)KADDR(PDE_ADDR(boot_pgdir[0]));
    ptep = (pte_t *)KADDR(PDE_ADDR(ptep[0])) + 1;
    assert(get_pte(boot_pgdir, PGSIZE, 0) == ptep);

    p2 = alloc_page();
    assert(page_insert(boot_pgdir, p2, PGSIZE, PTE_U | PTE_W) == 0);
    assert((ptep = get_pte(boot_pgdir, PGSIZE, 0)) != NULL);
    assert(*ptep & PTE_U);
    assert(*ptep & PTE_W);
    assert(boot_pgdir[0] & PTE_U);
    assert(page_ref(p2) == 1);

    assert(page_insert(boot_pgdir, p1, PGSIZE, 0) == 0);
    assert(page_ref(p1) == 2);
    assert(page_ref(p2) == 0);
    assert((ptep = get_pte(boot_pgdir, PGSIZE, 0)) != NULL);
    assert(pte2page(*ptep) == p1);
    assert((*ptep & PTE_U) == 0);

    page_remove(boot_pgdir, 0x0);
    assert(page_ref(p1) == 1);
    assert(page_ref(p2) == 0);

    page_remove(boot_pgdir, PGSIZE);
    assert(page_ref(p1) == 0);
    assert(page_ref(p2) == 0);

    assert(page_ref(pde2page(boot_pgdir[0])) == 1);
    free_page(pde2page(boot_pgdir[0]));
    boot_pgdir[0] = 0;

    cprintf("check_pgdir() succeeded!\n");
}

static void check_boot_pgdir(void) {
    pte_t *ptep;
    int i;
    for (i = ROUNDDOWN(KERNBASE, PGSIZE); i < npage * PGSIZE; i += PGSIZE) {
        assert((ptep = get_pte(boot_pgdir, (uintptr_t)KADDR(i), 0)) != NULL);
        assert(PTE_ADDR(*ptep) == i);
    }


    assert(boot_pgdir[0] == 0);

    struct Page *p;
    p = alloc_page();
    assert(page_insert(boot_pgdir, p, 0x100, PTE_W | PTE_R) == 0);
    assert(page_ref(p) == 1);
    assert(page_insert(boot_pgdir, p, 0x100 + PGSIZE, PTE_W | PTE_R) == 0);
    assert(page_ref(p) == 2);

    const char *str = "ucore: Hello world!!";
    strcpy((void *)0x100, str);
    assert(strcmp((void *)0x100, (void *)(0x100 + PGSIZE)) == 0);

    *(char *)(page2kva(p) + 0x100) = '\0';
    assert(strlen((const char *)0x100) == 0);

    free_page(p);
    free_page(pde2page(boot_pgdir[0]));
    boot_pgdir[0] = 0;

    cprintf("check_boot_pgdir() succeeded!\n");
}
