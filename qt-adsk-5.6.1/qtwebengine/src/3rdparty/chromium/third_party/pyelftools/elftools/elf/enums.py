#-------------------------------------------------------------------------------
# elftools: elf/enums.py
#
# Mappings of enum names to values
#
# Eli Bendersky (eliben@gmail.com)
# This code is in the public domain
#-------------------------------------------------------------------------------
from ..construct import Pass


# e_ident[EI_CLASS] in the ELF header
ENUM_EI_CLASS = dict(
    ELFCLASSNONE=0,
    ELFCLASS32=1,
    ELFCLASS64=2
)

# e_ident[EI_DATA] in the ELF header
ENUM_EI_DATA = dict(
    ELFDATANONE=0,
    ELFDATA2LSB=1,
    ELFDATA2MSB=2
)

# e_version in the ELF header
ENUM_E_VERSION = dict(
    EV_NONE=0,
    EV_CURRENT=1,
    _default_=Pass,
)

# e_ident[EI_OSABI] in the ELF header
ENUM_EI_OSABI = dict(
    ELFOSABI_SYSV=0,
    ELFOSABI_HPUX=1,
    ELFOSABI_NETBSD=2,
    ELFOSABI_LINUX=3,
    ELFOSABI_HURD=4,
    ELFOSABI_SOLARIS=6,
    ELFOSABI_AIX=7,
    ELFOSABI_IRIX=8,
    ELFOSABI_FREEBSD=9,
    ELFOSABI_TRU64=10,
    ELFOSABI_MODESTO=11,
    ELFOSABI_OPENBSD=12,
    ELFOSABI_OPENVMS=13,
    ELFOSABI_NSK=14,
    ELFOSABI_AROS=15,
    ELFOSABI_ARM=97,
    ELFOSABI_STANDALONE=255,
    _default_=Pass,
)

# e_type in the ELF header
ENUM_E_TYPE = dict(
    ET_NONE=0,
    ET_REL=1,
    ET_EXEC=2,
    ET_DYN=3,
    ET_CORE=4,
    ET_LOPROC=0xff00,
    ET_HIPROC=0xffff,
    _default_=Pass,
)

# e_machine in the ELF header
# (this list is currently somewhat partial...)
ENUM_E_MACHINE = dict(
    EM_NONE=0,
    EM_M32=1,
    EM_SPARC=2,
    EM_386=3,
    EM_68K=4,
    EM_88K=5,
    EM_486=6,
    EM_860=7,
    EM_MIPS=8,
    EM_S370=9,
    EM_MIPS_RS4_BE=10,
    EM_IA_64=50,
    EM_X86_64=62,
    EM_AVR=83,
    EM_L10M=180,
    _default_=Pass,
)

# sh_type in the section header
ENUM_SH_TYPE = dict(
    SHT_NULL=0,
    SHT_PROGBITS=1,
    SHT_SYMTAB=2,
    SHT_STRTAB=3,
    SHT_RELA=4,
    SHT_HASH=5,
    SHT_DYNAMIC=6,
    SHT_NOTE=7,
    SHT_NOBITS=8,
    SHT_REL=9,
    SHT_SHLIB=10,
    SHT_DYNSYM=11,
    SHT_INIT_ARRAY=14,
    SHT_FINI_ARRAY=15,
    SHT_PREINIT_ARRAY=16,
    SHT_GROUP=17,
    SHT_SYMTAB_SHNDX=18,
    SHT_NUM=19,
    SHT_LOOS=0x60000000,
    SHT_GNU_HASH=0x6ffffff6,
    SHT_GNU_verdef=0x6ffffffd,
    SHT_GNU_verneed=0x6ffffffe,
    SHT_GNU_versym=0x6fffffff,
    SHT_LOPROC=0x70000000,
    SHT_HIPROC=0x7fffffff,
    SHT_LOUSER=0x80000000,
    SHT_HIUSER=0xffffffff,
    SHT_AMD64_UNWIND=0x70000001,
    _default_=Pass,
)

# p_type in the program header
# some values scavenged from the ELF headers in binutils-2.21
ENUM_P_TYPE = dict(
    PT_NULL=0,
    PT_LOAD=1,
    PT_DYNAMIC=2,
    PT_INTERP=3,
    PT_NOTE=4,
    PT_SHLIB=5,
    PT_PHDR=6,
    PT_TLS=7,
    PT_LOPROC=0x70000000,
    PT_HIPROC=0x7fffffff,
    PT_GNU_EH_FRAME=0x6474e550,
    PT_GNU_STACK=0x6474e551,
    PT_GNU_RELRO=0x6474e552,
    _default_=Pass,
)

# st_info bindings in the symbol header
ENUM_ST_INFO_BIND = dict(
    STB_LOCAL=0,
    STB_GLOBAL=1,
    STB_WEAK=2,
    STB_NUM=3,
    STB_LOOS=10,
    STB_HIOS=12,
    STB_LOPROC=13,
    STB_HIPROC=15,
    _default_=Pass,
)

# st_info type in the symbol header
ENUM_ST_INFO_TYPE = dict(
    STT_NOTYPE=0,
    STT_OBJECT=1,
    STT_FUNC=2,
    STT_SECTION=3,
    STT_FILE=4,
    STT_COMMON=5,
    STT_TLS=6,
    STT_NUM=7,
    STT_RELC=8,
    STT_SRELC=9,
    STT_LOOS=10,
    STT_HIOS=12,
    STT_LOPROC=13,
    STT_HIPROC=15,
    _default_=Pass,
)

# visibility from st_other
ENUM_ST_VISIBILITY = dict(
    STV_DEFAULT=0,
    STV_INTERNAL=1,
    STV_HIDDEN=2,
    STV_PROTECTED=3,
    _default_=Pass,
)

# st_shndx
ENUM_ST_SHNDX = dict(
    SHN_UNDEF=0,
    SHN_ABS=0xfff1,
    SHN_COMMON=0xfff2,
    _default_=Pass,
)

# d_tag
ENUM_D_TAG = dict(
    DT_NULL=0,
    DT_NEEDED=1,
    DT_PLTRELSZ=2,
    DT_PLTGOT=3,
    DT_HASH=4,
    DT_STRTAB=5,
    DT_SYMTAB=6,
    DT_RELA=7,
    DT_RELASZ=8,
    DT_RELAENT=9,
    DT_STRSZ=10,
    DT_SYMENT=11,
    DT_INIT=12,
    DT_FINI=13,
    DT_SONAME=14,
    DT_RPATH=15,
    DT_SYMBOLIC=16,
    DT_REL=17,
    DT_RELSZ=18,
    DT_RELENT=19,
    DT_PLTREL=20,
    DT_DEBUG=21,
    DT_TEXTREL=22,
    DT_JMPREL=23,
    DT_BIND_NOW=24,
    DT_INIT_ARRAY=25,
    DT_FINI_ARRAY=26,
    DT_INIT_ARRAYSZ=27,
    DT_FINI_ARRAYSZ=28,
    DT_RUNPATH=29,
    DT_FLAGS=30,
    DT_ENCODING=32,
    DT_PREINIT_ARRAY=32,
    DT_PREINIT_ARRAYSZ=33,
    DT_NUM=34,
    DT_LOOS=0x6000000d,
    DT_HIOS=0x6ffff000,
    DT_LOPROC=0x70000000,
    DT_HIPROC=0x7fffffff,
    DT_PROCNUM=0x35,
    DT_VALRNGLO=0x6ffffd00,
    DT_GNU_PRELINKED=0x6ffffdf5,
    DT_GNU_CONFLICTSZ=0x6ffffdf6,
    DT_GNU_LIBLISTSZ=0x6ffffdf7,
    DT_CHECKSUM=0x6ffffdf8,
    DT_PLTPADSZ=0x6ffffdf9,
    DT_MOVEENT=0x6ffffdfa,
    DT_MOVESZ=0x6ffffdfb,
    DT_SYMINSZ=0x6ffffdfe,
    DT_SYMINENT=0x6ffffdff,
    DT_GNU_HASH=0x6ffffef5,
    DT_TLSDESC_PLT=0x6ffffef6,
    DT_TLSDESC_GOT=0x6ffffef7,
    DT_GNU_CONFLICT=0x6ffffef8,
    DT_GNU_LIBLIST=0x6ffffef9,
    DT_CONFIG=0x6ffffefa,
    DT_DEPAUDIT=0x6ffffefb,
    DT_AUDIT=0x6ffffefc,
    DT_PLTPAD=0x6ffffefd,
    DT_MOVETAB=0x6ffffefe,
    DT_SYMINFO=0x6ffffeff,
    DT_VERSYM=0x6ffffff0,
    DT_RELACOUNT=0x6ffffff9,
    DT_RELCOUNT=0x6ffffffa,
    DT_FLAGS_1=0x6ffffffb,
    DT_VERDEF=0x6ffffffc,
    DT_VERDEFNUM=0x6ffffffd,
    DT_VERNEED=0x6ffffffe,
    DT_VERNEEDNUM=0x6fffffff,
    DT_AUXILIARY=0x7ffffffd,
    DT_FILTER=0x7fffffff,
    _default_=Pass,
)

ENUM_RELOC_TYPE_i386 = dict(
    R_386_NONE=0,
    R_386_32=1,
    R_386_PC32=2,
    R_386_GOT32=3,
    R_386_PLT32=4,
    R_386_COPY=5,
    R_386_GLOB_DAT=6,
    R_386_JUMP_SLOT=7,
    R_386_RELATIVE=8,
    R_386_GOTOFF=9,
    R_386_GOTPC=10,
    R_386_32PLT=11,
    R_386_TLS_TPOFF=14,
    R_386_TLS_IE=15,
    R_386_TLS_GOTIE=16,
    R_386_TLS_LE=17,
    R_386_TLS_GD=18,
    R_386_TLS_LDM=19,
    R_386_16=20,
    R_386_PC16=21,
    R_386_8=22,
    R_386_PC8=23,
    R_386_TLS_GD_32=24,
    R_386_TLS_GD_PUSH=25,
    R_386_TLS_GD_CALL=26,
    R_386_TLS_GD_POP=27,
    R_386_TLS_LDM_32=28,
    R_386_TLS_LDM_PUSH=29,
    R_386_TLS_LDM_CALL=30,
    R_386_TLS_LDM_POP=31,
    R_386_TLS_LDO_32=32,
    R_386_TLS_IE_32=33,
    R_386_TLS_LE_32=34,
    R_386_TLS_DTPMOD32=35,
    R_386_TLS_DTPOFF32=36,
    R_386_TLS_TPOFF32=37,
    R_386_TLS_GOTDESC=39,
    R_386_TLS_DESC_CALL=40,
    R_386_TLS_DESC=41,
    R_386_IRELATIVE=42,
    R_386_USED_BY_INTEL_200=200,
    R_386_GNU_VTINHERIT=250,
    R_386_GNU_VTENTRY=251,
    _default_=Pass,
)

ENUM_RELOC_TYPE_x64 = dict(
    R_X86_64_NONE=0,
    R_X86_64_64=1,
    R_X86_64_PC32=2,
    R_X86_64_GOT32=3,
    R_X86_64_PLT32=4,
    R_X86_64_COPY=5,
    R_X86_64_GLOB_DAT=6,
    R_X86_64_JUMP_SLOT=7,
    R_X86_64_RELATIVE=8,
    R_X86_64_GOTPCREL=9,
    R_X86_64_32=10,
    R_X86_64_32S=11,
    R_X86_64_16=12,
    R_X86_64_PC16=13,
    R_X86_64_8=14,
    R_X86_64_PC8=15,
    R_X86_64_DTPMOD64=16,
    R_X86_64_DTPOFF64=17,
    R_X86_64_TPOFF64=18,
    R_X86_64_TLSGD=19,
    R_X86_64_TLSLD=20,
    R_X86_64_DTPOFF32=21,
    R_X86_64_GOTTPOFF=22,
    R_X86_64_TPOFF32=23,
    R_X86_64_PC64=24,
    R_X86_64_GOTOFF64=25,
    R_X86_64_GOTPC32=26,
    R_X86_64_GOT64=27,
    R_X86_64_GOTPCREL64=28,
    R_X86_64_GOTPC64=29,
    R_X86_64_GOTPLT64=30,
    R_X86_64_PLTOFF64=31,
    R_X86_64_GOTPC32_TLSDESC=34,
    R_X86_64_TLSDESC_CALL=35,
    R_X86_64_TLSDESC=36,
    R_X86_64_IRELATIVE=37,
    R_X86_64_GNU_VTINHERIT=250,
    R_X86_64_GNU_VTENTRY=251,
    _default_=Pass,
)

