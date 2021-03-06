#include <elf32.h>

#include <stddef.h>
#include <kstdio.h>

#include <mm_common.h>
#include <paging.h>

#define SHN_UNDEF   0

enum elf_ident {
    EI_MAG0 = 0,    // 0x7f
    EI_MAG1,        // 'E'
    EI_MAG2,        // 'L'
    EI_MAG3,        // 'F'
    EI_CLASS,       // arch: 1 => 32 bit, 2 => 64 bit
    EI_DATA,        // endianness: 1 => little, 2 => big
    EI_VERSION,     // ELF version
    EI_OSABI,       // OS specific
    EI_ABIVERSION,  // OS specific
    EI_PAD          // padding
};

#define ELF_MAG0            0x7f
#define ELF_MAG1            'E'
#define ELF_MAG2            'L'
#define ELF_MAG3            'F'
#define ELF_DATA_LITTLE     1   // little endian
#define ELF_CLASS_32        1   // 32-bit arch
#define ELF_TYPE_REL        1   // relocatable
#define ELF_TYPE_EXEC       2   // executable
#define ELF_MACHINE_386     3   // x86 machine type
#define ELF_VERSION_CURRENT 1   // ELF current version


// ELF32 section header
typedef struct {
    elf32_word_t    sh_name;    // relative to e_shstrndx in elf32_ehdr_t
    elf32_word_t    sh_type;
    elf32_word_t    sh_flags; 
    elf32_addr_t    sh_addr;
    elf32_off_t     sh_offset;
    elf32_word_t    sh_size;
    elf32_word_t    sh_link;
    elf32_word_t    sh_info;
    elf32_word_t    sh_addralign;
    elf32_word_t    sh_entsize;  
} __attribute__((packed)) elf32_shdr_t;

enum sh_types {
    SHT_NULL    = 0,    // null section
    SHT_PROGBITS,       // program information
    SHT_SYMTAB,         // symbol table
    SHT_STRTAB,         // string table
    SHT_RELA,           // relocation (with addend)
    SHT_NOBITS  = 8,    // not present in file
    SHT_REL             // relocation (no addend)
};

enum sh_attr {
    SHF_WRITE   = 1,    // write section
    SHF_ALLOC   = 2     // exists in memory
};

// ELF32 program segement header
typedef struct {
    elf32_word_t    p_type;
    elf32_off_t     p_offset;
    elf32_addr_t    p_vaddr;
    elf32_addr_t    p_paddr;
    elf32_word_t    p_filesz;
    elf32_word_t    p_memsz;
    elf32_word_t    p_flags;
    elf32_word_t    p_align;    
} __attribute__((packed)) elf32_phdr_t;

enum p_type {
    PT_NULL = 0,
    PT_LOAD,
    // ...
};

enum p_flags { // rwx, just like in Unix
    PF_X = 1,
    PF_W = 2,
    PF_R = 4
};
        
static char _dyn_str_error[128];

// get the first entry of the program header table
static inline
const elf32_phdr_t *_get_progtab(const elf32_ehdr_t *ehdr)
{
    return (const elf32_phdr_t *) ((uint8_t *) ehdr + ehdr->e_phoff);
}

static inline
uintptr_t _get_progseg_offset(const elf32_phdr_t *phdr)
{
    return phdr->p_offset;
}

static inline
bool _progseg_is_loadable(const elf32_phdr_t *phdr)
{
    return PT_LOAD == phdr->p_type;
}

static inline
bool _progseg_is_executable(const elf32_phdr_t *phdr)
{
    return PF_X & phdr->p_flags;
}

static inline
bool _progseg_is_writable(const elf32_phdr_t *phdr)
{
    return PF_W & phdr->p_flags;
}

static inline
bool _progseg_is_readable(const elf32_phdr_t *phdr)
{
    return PF_R & phdr->p_flags;
}



#define SHT_UNDEF   0
// ELF32 symbol table entry
typedef struct {
    elf32_word_t    st_name;
    elf32_addr_t    st_value;
    elf32_word_t    st_size;
    uint8_t         st_info;
    uint8_t         st_other;
    elf32_half_t    st_shndx;
} __attribute__((packed)) elf32_sym_t;


static const char *_str_error = "unknown error";

static bool _is_valid(const elf32_ehdr_t *hdr)
{
    if (hdr->e_ident[EI_MAG0] != ELF_MAG0)
        return false;

    if (hdr->e_ident[EI_MAG1] != ELF_MAG1)
        return false;

    if (hdr->e_ident[EI_MAG2] != ELF_MAG2)
        return false;

    if (hdr->e_ident[EI_MAG3] != ELF_MAG3)
        return false;

    if (hdr->e_phentsize != sizeof(elf32_phdr_t))
        return false;

    if (hdr->e_shentsize != sizeof(elf32_shdr_t))
        return false;

    return true;
}

bool elf32_is_supported(const elf32_ehdr_t *ehdr)
{
    if (!ehdr)
        return false;

    if (!_is_valid(ehdr)) {
        _str_error = "invalid ELF magin number";
        return false;
    }

    if (ehdr->e_ident[EI_CLASS] != ELF_CLASS_32) {
        _str_error = "not 32-bit";
        return false;
    }

    if (ehdr->e_ident[EI_DATA] != ELF_DATA_LITTLE) {
        _str_error = "not little-endian";
        return false;
    }

    if (ehdr->e_machine != ELF_MACHINE_386) {
        _str_error = "not 386";
        return false;
    }

    if (ehdr->e_type != ELF_TYPE_EXEC) {
        _str_error = "not executable";
        return false;
    }

    return true;
}

static inline
size_t _progseg_get_memsz(const elf32_phdr_t *phdr)
{
    return phdr->p_memsz;
}

static
size_t _progsegs_get_memsz(const elf32_ehdr_t *ehdr)
{
    unsigned int i;
    size_t sum_memsz;
    const elf32_phdr_t *phdr;
    
    if (!(phdr = _get_progtab(ehdr)))
        return 0;

    for (i = 0, sum_memsz = 0; i < ehdr->e_phnum; i++, phdr++)
        if (_progseg_is_loadable(phdr))
            sum_memsz += _progseg_get_memsz(phdr);

    return sum_memsz;
}

static bool _is_paddr_in_valid_range(uintptr_t paddr, size_t memsz)
{
    if (paddr + memsz > KERNEL_PADDR_MAX)
        return false;

    return true;
}

static bool _is_vaddr_in_valid_range(uintptr_t vaddr, size_t memsz)
{
    if (vaddr + memsz > KERNEL_VADDR_MAX)
        return false;

    return true;
}

static bool _valid_progseg(uintptr_t paddr, uintptr_t vaddr, size_t memsz)
{
    if (!_is_addr_page_aligned(paddr)) {
        ksprintf(_dyn_str_error,
                "starting physical address not page-aligned: %x", paddr);
        _str_error = _dyn_str_error;
        return false;
    }

    if (!_is_addr_page_aligned(vaddr)) {
        ksprintf(_dyn_str_error,
                "starting virtual address not page-aligned: %x", vaddr);
        _str_error = _dyn_str_error;
        return false;
    }

    if (!_is_paddr_in_valid_range(paddr, memsz)) {
        // TODO more expressive, display actual limits
        ksprintf(_dyn_str_error, "physical address out of range: %x", paddr);
        _str_error = _dyn_str_error;
        return false;
    }

    if (!_is_vaddr_in_valid_range(vaddr, memsz)) {
        // TODO more expressive, display actual limits
        ksprintf(_dyn_str_error, "virtual address out of range: %x", vaddr);
        _str_error = _dyn_str_error;
        return false;
    }

    return true;
}

static inline
int _map_progseg(uintptr_t ehdr_addr, const elf32_phdr_t *phdr)
{
    uintptr_t paddr, vaddr;
    uint8_t *dst, *src;
    size_t filesz, memsz;
    bool is_writable;

    paddr = phdr->p_paddr;  // LMA
    vaddr = phdr->p_vaddr;  // VMA

    dst = (uint8_t *) vaddr;
    src = (uint8_t *) (ehdr_addr + phdr->p_offset);

    filesz = phdr->p_filesz;
    memsz  = _progseg_get_memsz(phdr);
    is_writable = _progseg_is_writable(phdr);

    if (!_valid_progseg(paddr, vaddr, memsz))
        return 1; 

    // copy in memory the contents of the ELF that were in the file
    for (; filesz; filesz--, memsz--, paddr++) {
        if (!((uintptr_t) dst % PAGE_SIZE))
            if (paging_map((uintptr_t) dst, paddr, is_writable)) {
                _str_error = "mapping a in-image-present failed";
                return 1;
            }

        // copy the contents
        *dst++ = *src++;
    }

    // set to zero the rest of the bytes not present in the ELF image
    for (; memsz; memsz--, paddr++) {
        if (!((uintptr_t) dst % PAGE_SIZE))
            if (paging_map((uintptr_t) dst, paddr, is_writable)) {
                _str_error = "mapping of a to-be-zeroed page failed";
                return 1;
            }

        *dst++ = 0;
    }

    return 0;
}


// map the elf image and returns the address of the entry point
uintptr_t elf32_map(const void *elf_image)
{
    const elf32_ehdr_t *ehdr;
    const elf32_phdr_t *phdr;
    size_t i;

    ehdr = (const elf32_ehdr_t *) elf_image;
    if (!elf32_is_supported(ehdr))
        return 0;

    if (!(phdr = _get_progtab(ehdr)))
        return 0;

    {
        size_t total_memsz;

        if ((total_memsz = _progsegs_get_memsz(ehdr)) > KERNEL_MEMSZ_MAX) {
            ksprintf(_dyn_str_error,
                        "required %d bytes, only %d supported",
                        total_memsz, KERNEL_MEMSZ_MAX);
            _str_error = _dyn_str_error;
            return 0;
        }
    }

    for (i = 0; i < ehdr->e_phnum; i++, phdr++) {
        if (!_progseg_is_loadable(phdr))
            continue;

        if (_map_progseg((uintptr_t) ehdr, phdr))
            return 0;
    }

    return ehdr->e_entry;
}

const char *elf32_strerror(void)
{
    return _str_error;
}


static const elf32_phdr_t *_get_phdr(const elf32_ehdr_t *ehdr, size_t idx)
{
    if (!ehdr)
        return NULL;
        
    if (idx >= ehdr->e_phnum)
        return NULL; // requestes program header out of boundaries

    return &idx[_get_progtab(ehdr)];
}


static void _display_pentry(const elf32_phdr_t *phdr)
{
    if (_progseg_is_loadable(phdr))
        kprintf("load\n");
    kprintf("vaddr: %x\n", phdr->p_vaddr);        
    kprintf("paddr: %x\n", phdr->p_paddr);
    kprintf("filesz: %d\n", phdr->p_filesz);
    kprintf("memsz: %d\n", phdr->p_memsz);
    if (_progseg_is_executable(phdr))
        kprintf(" execute\n");
    if (_progseg_is_writable(phdr))
        kprintf(" write\n");
    if (_progseg_is_readable(phdr))
        kprintf(" read\n");  
}

void elf32_display_pentries(const void *elf_image)
{
    const elf32_ehdr_t *ehdr;
    const elf32_phdr_t *phdr;
    size_t i; 

    ehdr = (elf32_ehdr_t *) elf_image;

    for (i = 0; i < ehdr->e_phnum; i++) {
        phdr = _get_phdr(ehdr, i);
        _display_pentry(phdr);
        kprintf("----------------------\n");
    }
}

void elf32_display_symtable(const void *elf_image)
{
    // TODO
    (void) elf_image;
}

void elf32_display(const void *elf_image)
{
    elf32_display_pentries(elf_image);
    elf32_display_symtable(elf_image);
}
