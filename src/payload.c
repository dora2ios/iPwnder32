/*
 * Copyright 2013-2016, iH8sn0w. <iH8sn0w@iH8sn0w.com>
 *
 * This file is part of iBoot32Patcher.
 *
 * iBoot32Patcher is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * iBoot32Patcher is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with iBoot32Patcher.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <inttypes.h>
#include <strings.h>

// Based on iH8sn0w's iBoot32Patcher
#define GET_IBOOT_FILE_OFFSET(iboot_in, x) (x - (uintptr_t) iboot_in->buf)
#define GET_IBOOT_ADDR(iboot_in, x) (x - (uintptr_t) iboot_in->buf) + get_iboot_base_address(iboot_in->buf)

#define bswap32                     __builtin_bswap32
#define bswap16                     __builtin_bswap16

#define IMAGE3_MAGIC                'Img3'
#define IBOOT_VERS_STR_OFFSET       0x286
#define IBOOT32_RESET_VECTOR_BYTES  bswap32(0x0E0000EA)

#define ARM32_THUMB_MOV             0
#define ARM32_THUMB_CMP             1
#define ARM32_THUMB_ADD             2
#define ARM32_THUMB_SUB             3

#define ARM32_THUMB_IT_NE           __builtin_bswap16(0x18BF)
#define ARM32_THUMB_IT_EQ           __builtin_bswap16(0x08BF)

struct iboot_img {
    void* buf;
    size_t len;
    uint32_t VERS;
} __attribute__((packed));

size_t _strlen(str)
const char *str;
{
    register const char *s;
    
    for (s = str; *s; ++s);
    return(s - str);
}

struct arm32_thumb_LDR_T3 {
    uint8_t rn : 4;
    uint16_t pad : 12;
    uint16_t imm12 : 12;
    uint8_t rt : 4;
} __attribute__ ((packed));

struct arm32_thumb_MOVW {
    uint8_t imm4 : 4;
    uint8_t pad0 : 6;
    uint8_t i : 1;
    uint8_t pad1 : 5;
    uint8_t imm8;
    uint8_t rd : 4;
    uint8_t imm3 : 3;
    uint8_t bit31 : 1;
} __attribute__((packed));

struct arm32_thumb_BL {
    uint16_t offset : 11;
    uint8_t h : 1;
    uint8_t padd : 4;
    uint16_t offset2 : 11;
    uint8_t h2 : 1;
    uint8_t padd2 : 4;
} __attribute__((packed));

struct arm32_thumb_MOVT_W {
    uint8_t imm4 : 4;
    uint8_t pad0 : 6;
    uint8_t i : 1;
    uint8_t pad1 : 5;
    uint8_t imm8;
    uint8_t rd : 4;
    uint8_t imm3 : 3;
    uint8_t bit31 : 1;
} __attribute__((packed));

struct arm32_thumb_BW_T4 {
    uint16_t imm10 : 10;
    uint8_t s : 1;
    uint8_t pad0 : 5;
    uint16_t imm11 : 11;
    uint8_t j2 : 1;
    uint8_t bit12 : 1;
    uint8_t j1 : 1;
    uint8_t pad1 : 2;
} __attribute__((packed));

struct arm32_thumb_IT_T1 {
    uint8_t mask : 4;
    uint8_t cond : 4;
    uint8_t pad : 8;
}__attribute__((packed));

struct arm32_thumb_LDR {
    uint8_t imm8;
    uint8_t rd : 3;
    uint8_t padd : 5;
} __attribute__((packed));

struct arm32_thumb_hi_reg_op {
    uint8_t rd : 3;
    uint8_t rs : 3;
    uint8_t h2 : 1;
    uint8_t h1 : 1;
    uint8_t op : 2;
    uint8_t pad : 6;
} __attribute__((packed));

struct arm32_thumb {
    uint8_t offset : 8;
    uint8_t rd : 3;
    uint8_t op : 2;
    uint8_t one : 1;
    uint8_t z: 1;
    uint8_t zero : 1;
} __attribute__((packed));

void* pattern_search(const void* addr, int len, int pattern, int mask, int step) {
    char* caddr = (char*)addr;
    if (len <= 0)
        return NULL;
    if (step < 0) {
        len = -len;
        len &= ~-(step+1);
    } else {
        len &= ~(step-1);
    }
    for (int i = 0; i != len; i += step) {
        uint32_t x = *(uint32_t*)(caddr + i);
        if ((x & mask) == pattern)
            return (void*)(caddr + i);
    }
    return NULL;
}

void* push_r4_r7_lr_search_up(const void* start_addr, int len) {
    // F0 B5
    return pattern_search(start_addr, len, 0x0000B5F0, 0x0000FFFF, -2);
}

void* bl_search_down(const void* start_addr, int len) {
    return pattern_search(start_addr, len, 0xD000F000, 0xD000F800, 1);
}

uint16_t get_MOVW_val(struct arm32_thumb_MOVW* movw) {
    return (uint16_t) (((movw->imm4 << 4) + (movw->i << 3) + movw->imm3) << 8) + movw->imm8;
}

bool is_MOVW_insn(void* offset) {
    struct arm32_thumb_MOVW* test = (struct arm32_thumb_MOVW*) offset;
    if(test->pad0 == 0x24 && test->pad1 == 0x1E && test->bit31 == 0) {
        return true;
    }
    return false;
}

void* ldr_search_up(const void* start_addr, int len) {
    return pattern_search(start_addr, len, 0x00004800, 0x0000F800, -1);
}

void* ldr32_search_up(const void* start_addr, int len) {
    return pattern_search(start_addr, len, 0x0000F8DF, 0x0000FFFF, -1);
}

void* ldr_to(const void* loc) {
    uintptr_t xref_target = (uintptr_t)loc;
    uintptr_t i = xref_target;
    uintptr_t min_addr = xref_target - 0x420;
    for(; i > min_addr; i -= 2) {
        i = (uintptr_t)ldr_search_up((void*)i, i - min_addr);
        if (i == 0) {
            break;
        }
        
        uint32_t dw = *(uint32_t*)i;
        uintptr_t ldr_target = ((i + 4) & ~3) + ((dw & 0xff) << 2);
        if (ldr_target == xref_target) {
            return (void*)i;
        }
    }
    
    min_addr = xref_target - 0x1000;
    for(i = xref_target; i > min_addr; i -= 4) {
        i = (uintptr_t)ldr32_search_up((void*)i, i - min_addr);
        if (i == 0) {
            break;
        }
        uint32_t dw = *(uint32_t*)i;
        uintptr_t ldr_target = ((i + 4) & ~3) + ((dw >> 16) & 0xfff);
        if (ldr_target == xref_target) {
            return (void*)i;
        }
    }
    return NULL;
}

void* _memmem(const void* mem, int size, const void* pat, int size2) {
    char* cmem = (char*)mem;
    const char* cpat = (const char*)pat;
    for (int i = 0; i < size; i++) {
        for (int j = 0; j < size2; ++j) {
            if (cmem[i + j] != cpat[j])
                goto next;
        }
        return (void*)(cmem + i);
    next:
        continue;
    }
    return NULL;
}

void* memstr(const void* mem, size_t size, const char* str) {
    return (void*) _memmem(mem, size, str, _strlen(str));
}

/* Taken from saurik's substrate framework. (See Hooker.cpp) */
void* resolve_bl32(const void* bl) {
    union {
        uint16_t value;
        
        struct {
            uint16_t immediate : 10;
            uint16_t s : 1;
            uint16_t : 5;
        };
    } bits = {*(uint16_t*)bl};
    
    union {
        uint16_t value;
        
        struct {
            uint16_t immediate : 11;
            uint16_t j2 : 1;
            uint16_t x : 1;
            uint16_t j1 : 1;
            uint16_t : 2;
        };
    } exts = {((uint16_t*)bl)[1]};
    
    int32_t jump = 0;
    jump |= bits.s << 24;
    jump |= (~(bits.s ^ exts.j1) & 0x1) << 23;
    jump |= (~(bits.s ^ exts.j2) & 0x1) << 22;
    jump |= bits.immediate << 12;
    jump |= exts.immediate << 1;
    jump |= exts.x;
    jump <<= 7;
    jump >>= 7;
    
    return (void*) (bl + 4 + jump);
}


void* find_next_LDR_insn_with_value(struct iboot_img* iboot_in, uint32_t value) {
    void* ldr_xref = (void*) _memmem(iboot_in->buf, iboot_in->len, &value, sizeof(value));
    if(!ldr_xref) {
        //printf("%s: Unable to find an LDR xref for 0x%X!\n", __FUNCTION__, value);
        return 0;
    }
    void* ldr_insn = ldr_to(ldr_xref);
    if(!ldr_insn) {
        //printf("%s: Unable to resolve to LDR insn from xref %p!\n", __FUNCTION__, ldr_xref);
        return 0;
    }
    return ldr_insn;
}

void* find_next_MOVW_insn_with_value(void* start, size_t len, const uint16_t val) {
    for(int i = 0; i < len; i += sizeof(uint16_t)) {
        struct arm32_thumb_MOVW* candidate = (struct arm32_thumb_MOVW*) (start + i);
        if(is_MOVW_insn(start + i) && get_MOVW_val(candidate) == val) {
            return (void*) candidate;
        }
    }
    return NULL;
}

void* find_next_bl_insn_to(struct iboot_img* iboot_in, uint32_t addr) {
    for(int i = 0; i < iboot_in->len - sizeof(uint32_t); i++) {
        void* possible_bl = resolve_bl32(iboot_in->buf + i);
        uint32_t resolved = (uintptr_t) GET_IBOOT_FILE_OFFSET(iboot_in, possible_bl);
        if(resolved == addr) {
            return (void*) (iboot_in->buf + i);
        }
    }
    return NULL;
}

void* find_next_next_bl_insn_to(struct iboot_img* iboot_in, uint32_t addr) {
    for(int i = 0; i < iboot_in->len - sizeof(uint32_t); i++) {
        void* possible_bl = resolve_bl32(iboot_in->buf + i);
        uint32_t resolved = (uintptr_t) GET_IBOOT_FILE_OFFSET(iboot_in, possible_bl);
        if(resolved == addr) {
            for(int j = i+1; j < iboot_in->len - sizeof(uint32_t); j++) {
                void* possible_bl = resolve_bl32(iboot_in->buf + j);
                uint32_t resolved = (uintptr_t) GET_IBOOT_FILE_OFFSET(iboot_in, possible_bl);
                if(resolved == addr) {
                    return (void*) (iboot_in->buf + j);
                }
            }
        }
    }
    return NULL;
}

int get_os_version(struct iboot_img* iboot_in) {
    
        if(iboot_in->VERS >= 1218 && iboot_in->VERS <= 1220) {
            return 5;
        }
        if(iboot_in->VERS == 1537) {
            return 6;
        }
        if(iboot_in->VERS == 1940) {
            return 7;
        }
        if(iboot_in->VERS == 2261) {
            return 8;
        }
        if(iboot_in->VERS == 2817) {
            return 9;
        }
        if(iboot_in->VERS == 3393) {
            return 10;
        }
    
    
    return 0;
}

void* find_verify_shsh_top(void* ptr) {
    void* top = push_r4_r7_lr_search_up(ptr, 0x500);
    if(!top) {
        return 0;
    }
    top++; // Thumb
    return top;
}

void* find_bl_verify_shsh_insn_next(struct iboot_img* iboot_in, void* pc) {
    /* Find the top of the function... */
    void* function_top = find_verify_shsh_top(pc);
    if(!function_top) {
        //printf("%s: Unable to find top of verify_shsh!\n", __FUNCTION__);
        return 0;
    }
    
    /* Find the BL insn resolving to this function... (BL verify_shsh seems to only happen once) */
    void* bl_verify_shsh = find_next_next_bl_insn_to(iboot_in, (uint32_t) ((uintptr_t)GET_IBOOT_FILE_OFFSET(iboot_in, function_top)));
    if(!bl_verify_shsh) {
        return 0;
    }
    
    return bl_verify_shsh;
}


void* find_bl_verify_shsh_insn(struct iboot_img* iboot_in, void* pc) {
    /* Find the top of the function... */
    void* function_top = find_verify_shsh_top(pc);
    if(!function_top) {
        //printf("%s: Unable to find top of verify_shsh!\n", __FUNCTION__);
        return 0;
    }
    
    /* Find the BL insn resolving to this function... (BL verify_shsh seems to only happen once) */
    void* bl_verify_shsh = find_next_bl_insn_to(iboot_in, (uint32_t) ((uintptr_t)GET_IBOOT_FILE_OFFSET(iboot_in, function_top)));
    if(!bl_verify_shsh) {
        return 0;
    }
    
    return bl_verify_shsh;
}

void* find_bl_verify_shsh_generic(struct iboot_img* iboot_in) {
    //printf("%s: Entering...\n", __FUNCTION__);
    
    /* Find the LDR Rx, ='CERT' instruction... */
    void* ldr_insn = find_next_LDR_insn_with_value(iboot_in, 'CERT');
    if(!ldr_insn) {
        //printf("%s: Unable to find LDR insn!\n", __FUNCTION__);
        return 0;
    }
    
    //printf("%s: Found LDR instruction at %p\n", __FUNCTION__, GET_IBOOT_FILE_OFFSET(iboot_in, ldr_insn));
    
    /* Resolve the BL verify_shsh routine from found instruction... */
    void* bl_verify_shsh = find_bl_verify_shsh_insn(iboot_in, ldr_insn);
    if(!bl_verify_shsh) {
        //printf("%s: Unable to find a BL verify_shsh! (Image may already be patched?)\n", __FUNCTION__);
        //return 0;
    }
    
    //printf("%s: Found BL verify_shsh at %p\n", __FUNCTION__, GET_IBOOT_FILE_OFFSET(iboot_in, bl_verify_shsh));
    
    //printf("%s: Leaving...\n", __FUNCTION__);
    
    return bl_verify_shsh;
}

void* find_bl_verify_shsh_5_6_7(struct iboot_img* iboot_in) {
    //printf("%s: Entering...\n", __FUNCTION__);
    
    /* Find the MOVW Rx, #'RT' instruction... */
    void* movw = find_next_MOVW_insn_with_value(iboot_in->buf, iboot_in->len, 0x5254); // 'RT'
    if(!movw) {
        //printf("%s: Unable to find MOVW instruction!\n", __FUNCTION__);
        return 0;
    }
    
    //printf("%s: Found MOVW instruction at %p\n", __FUNCTION__, GET_IBOOT_FILE_OFFSET(iboot_in, movw));
    
    /* Resolve the BL verify_shsh routine from found instruction... */
    void* bl_verify_shsh = find_bl_verify_shsh_insn(iboot_in, movw);
    if(!bl_verify_shsh) {
        //printf("%s: Unable to find a BL verify_shsh! (Image may already be patched?)\n", __FUNCTION__);
        return 0;
    }
    
    void* bl_verify_shsh_next = find_bl_verify_shsh_insn_next(iboot_in, movw);
    if(!bl_verify_shsh_next) {
        //printf("%s: Found BL verify_shsh at %p\n", __FUNCTION__, GET_IBOOT_FILE_OFFSET(iboot_in, bl_verify_shsh));
        
        //printf("%s: Leaving...\n", __FUNCTION__);
        
        return bl_verify_shsh;
    }
    
    //printf("%s: Found BL verify_shsh_next at %p\n", __FUNCTION__, GET_IBOOT_FILE_OFFSET(iboot_in, bl_verify_shsh_next));
    
    //printf("%s: Leaving...\n", __FUNCTION__);
    
    return bl_verify_shsh_next;
}

/* not need
void* find_rsa_check_4(struct iboot_img* iboot_in) {
    //printf("%s: Entering...\n", __FUNCTION__);
    
    // Find the RSA check
    void* rsa_check_4 = memstr(iboot_in->buf, iboot_in->len, "\x4F\xF0\xFF\x30\xDD\xF8\x40\x24\xDB\xF8\x00\x30\x9A\x42\x01\xD0");
    if(!rsa_check_4) {
        //printf("%s: Unable to find RSA check!\n", __FUNCTION__);
        return 0;
    }
    
    //printf("%s: Found RSA check at %p\n", __FUNCTION__, GET_IBOOT_FILE_OFFSET(iboot_in, rsa_check_4));
    
    //printf("%s: Leaving...\n", __FUNCTION__);
    
    return rsa_check_4;
}
 
void* find_ldr_ecid(struct iboot_img* iboot_in) {
    //printf("%s: Entering...\n", __FUNCTION__);
    
    // Find the LDR Rx, ='ECID' instruction...
    void* ldr_insn = find_next_LDR_insn_with_value(iboot_in, 'ECID');
    if(!ldr_insn) {
        //printf("%s: Unable to find LDR ECID!\n", __FUNCTION__);
        return 0;
    }
    
    //printf("%s: Found LDR instruction at %p\n", __FUNCTION__, GET_IBOOT_FILE_OFFSET(iboot_in, ldr_insn));
    
    // Resolve the BL verify_shsh routine from found instruction...
    char *ldr_ecid = bl_search_down(ldr_insn,0x100);
    if(!ldr_ecid) {
        //printf("%s: Unable to find a BL ECID! (Image may already be patched?)\n", __FUNCTION__);
        return 0;
    }
    
    //printf("%s: Found BL ECID at %p\n", __FUNCTION__, GET_IBOOT_FILE_OFFSET(iboot_in, ldr_ecid));
    
    //printf("%s: Leaving...\n", __FUNCTION__);
    
    return ldr_ecid;
}

void* find_ldr_bord(struct iboot_img* iboot_in) {
    //printf("%s: Entering...\n", __FUNCTION__);
    
    // Find the LDR Rx, ='BORD' instruction...
    void* ldr_insn = find_next_LDR_insn_with_value(iboot_in, 'BORD');
    if(!ldr_insn) {
        //printf("%s: Unable to find LDR insn!\n", __FUNCTION__);
        return 0;
    }
    
    //printf("%s: Found LDR BORD instruction at %p\n", __FUNCTION__, GET_IBOOT_FILE_OFFSET(iboot_in, ldr_insn));
    
    // Resolve the BL verify_shsh routine from found instruction...
    char *ldr_bord = bl_search_down(ldr_insn,0x100);
    if(!ldr_bord) {
        //printf("%s: Unable to find a BL BORD! (Image may already be patched?)\n", __FUNCTION__);
        return 0;
    }
    
    //printf("%s: Found BL BORD at %p\n", __FUNCTION__, GET_IBOOT_FILE_OFFSET(iboot_in, ldr_bord));
    
    //printf("%s: Leaving...\n", __FUNCTION__);
    
    return ldr_bord;
}

void* find_ldr_prod(struct iboot_img* iboot_in) {
    //printf("%s: Entering...\n", __FUNCTION__);
    
    // Find the LDR Rx, ='PROD' instruction...
    void* ldr_insn = find_next_LDR_insn_with_value(iboot_in, 'PROD');
    if(!ldr_insn) {
        //printf("%s: Unable to find LDR insn!\n", __FUNCTION__);
        return 0;
    }
    
    //printf("%s: Found LDR PROD instruction at %p\n", __FUNCTION__, GET_IBOOT_FILE_OFFSET(iboot_in, ldr_insn));
    
    // Resolve the BL verify_shsh routine from found instruction...
    char *ldr_prod = bl_search_down(ldr_insn,0x100);
    if(!ldr_prod) {
        //printf("%s: Unable to find a BL PROD! (Image may already be patched?)\n", __FUNCTION__);
        return 0;
    }
    
    //printf("%s: Found BL PROD at %p\n", __FUNCTION__, GET_IBOOT_FILE_OFFSET(iboot_in, ldr_prod));
    
    //printf("%s: Leaving...\n", __FUNCTION__);
    
    return ldr_prod;
}

void* find_ldr_sepo(struct iboot_img* iboot_in) {
    //printf("%s: Entering...\n", __FUNCTION__);
    
    // Find the LDR Rx, ='SEPO' instruction...
    void* ldr_insn = find_next_LDR_insn_with_value(iboot_in, 'SEPO');
    if(!ldr_insn) {
        //printf("%s: Unable to find LDR insn!\n", __FUNCTION__);
        return 0;
    }
    
    //printf("%s: Found LDR SEPO instruction at %p\n", __FUNCTION__, GET_IBOOT_FILE_OFFSET(iboot_in, ldr_insn));
    
    // Resolve the BL verify_shsh routine from found instruction...
    char *ldr_sepo = bl_search_down(ldr_insn,0x100);
    if(!ldr_sepo) {
        //printf("%s: Unable to find a BL SEPO! (Image may already be patched?)\n", __FUNCTION__);
        return 0;
    }
    
    //printf("%s: Found BL SEPO at %p\n", __FUNCTION__, GET_IBOOT_FILE_OFFSET(iboot_in, ldr_sepo));
    
    //printf("%s: Leaving...\n", __FUNCTION__);
    
    return ldr_sepo;
}
*/

void* find_bl_verify_shsh(struct iboot_img* iboot_in) {
    int os_vers = get_os_version(iboot_in);
    
    /* Use the os-specific method for finding BL verify_shsh... */
    if(os_vers >= 5 && os_vers <= 7) {
        return find_bl_verify_shsh_5_6_7(iboot_in);
    }
    
    return find_bl_verify_shsh_generic(iboot_in);
}



int patch_rsa_check(struct iboot_img* iboot_in) {
    //printf("%s: Entering...\n", __FUNCTION__);
    
    /* Find the BL verify_shsh instruction... */
    int os_vers = get_os_version(iboot_in);
    /* not need
    if(os_vers == 4 || os_vers == 3) {
        
        void* rsa_check_4 = find_rsa_check_4(iboot_in);
        if(!&find_rsa_check_4) {
            //printf("%s: Unable to find BL ECID!\n", __FUNCTION__);
            return 0;
        }
        // BL --> MOVS R0, #0; MOVS R0, #0
        //printf("%s: Patching RSA at %p...\n", __FUNCTION__, GET_IBOOT_FILE_OFFSET(iboot_in, rsa_check_4));
        *(uint32_t*)rsa_check_4 = bswap32(0x00200020);
        
        void* ldr_ecid = find_ldr_ecid(iboot_in);
        if(!ldr_ecid) {
            //printf("%s: Unable to find RSA check!\n", __FUNCTION__);
            return 0;
        }
        // BL --> MOVS R0, #0; MOVS R0, #0
        //printf("%s: Patching BL ECID at %p...\n", __FUNCTION__, GET_IBOOT_FILE_OFFSET(iboot_in, ldr_ecid));
        *(uint32_t*)ldr_ecid = bswap32(0x00200020);
        
        void* ldr_bord = find_ldr_bord(iboot_in);
        if(!ldr_bord) {
            //printf("%s: Unable to find BL BORD!\n", __FUNCTION__);
            return 0;
        }
        // BL --> MOVS R0, #0; MOVS R0, #0
        //printf("%s: Patching BL BORD at %p...\n", __FUNCTION__, GET_IBOOT_FILE_OFFSET(iboot_in, ldr_bord));
        *(uint32_t*)ldr_bord = bswap32(0x00200020);
        
        void* ldr_prod = find_ldr_prod(iboot_in);
        if(!ldr_prod) {
            //printf("%s: Unable to find BL PROD!\n", __FUNCTION__);
            return 0;
        }
        // BL --> MOVS R0, #0; MOVS R0, #0
        //printf("%s: Patching BL PROD at %p...\n", __FUNCTION__, GET_IBOOT_FILE_OFFSET(iboot_in, ldr_prod));
        *(uint32_t*)ldr_prod = bswap32(0x00200020);
        
        void* ldr_sepo = find_ldr_sepo(iboot_in);
        if(!ldr_sepo) {
            //printf("%s: Unable to find BL SEPO!\n", __FUNCTION__);
            return 0;
        }
        // BL --> MOVS R0, #0; MOVS R0, #0
        //printf("%s: Patching BL SEPO at %p...\n", __FUNCTION__, GET_IBOOT_FILE_OFFSET(iboot_in, ldr_sepo));
        *(uint32_t*)ldr_sepo = bswap32(0x00200020);
        
        return 1;
    }
    */
    void* bl_verify_shsh = find_bl_verify_shsh(iboot_in);
    if(!bl_verify_shsh) {
        //printf("%s: Unable to find BL verify_shsh!\n", __FUNCTION__);
        return 0;
    }
    
    //printf("%s: Patching BL verify_shsh at %p...\n", __FUNCTION__, GET_IBOOT_FILE_OFFSET(iboot_in, bl_verify_shsh));
    
    /* BL verify_shsh --> MOVS R0, #0; STR R0, [R3] */
    *(uint32_t*)bl_verify_shsh = bswap32(0x00201860);
    
    //printf("%s: Leaving...\n", __FUNCTION__);
    return 1;
}

int
_atoi(const char *nptr)
{
    int val = 0;
    int neg = 0;
    if (*nptr == '-') {
        neg = 1;
        nptr++;
    }
    for (;;) {
        int ch = *nptr++;
        if (ch < '0' || ch > '9') {
            break;
        }
        val = (val << 3) + (val << 1) + (ch - '0');
    }
    return neg ? -val : val;
}


void *_memset(void *dst, int ch, size_t len)
{
    long tmp = 0x01010101 * (ch & 0x000000FF);
    char *dest = dst;
    
    if (len < 32) while (len--) *dest++ = ch;
    else {
        /* do the front chunk as chars */
        while ((long)dest & 3) {
            len--;
            *dest++ = ch;
        }
        
        /* do the middle chunk as longs */
        while (len > 3) {
            len -= 4;
            *(long *)dest = tmp;
            dest += 4;
        }
        
        /* do the last chunk as chars */
        while (len--) *dest++ = ch;
    }
    
    return dst;
}

int iBoot32Patcher(void *buf, size_t sz){
    
    struct iboot_img iboot_in;
    _memset(&iboot_in, 0, sizeof(iboot_in));
    
    iboot_in.len = sz;
    iboot_in.buf = buf;
    
    uint32_t image_magic = *(uint32_t*)iboot_in.buf;
    
    if(image_magic == IMAGE3_MAGIC) {
        //printf("%s: The supplied image appears to be in an img3 container. Please ensure that the image is decrypted and that the img3 header is stripped.\n", __FUNCTION__);
        //free(iboot_in.buf);
        return -1;
    }
    
    if(image_magic != IBOOT32_RESET_VECTOR_BYTES) {
        //printf("%s: The supplied image is not a valid 32-bit iBoot.\n", __FUNCTION__);
        //free(iboot_in.buf);
        return -1;
    }
    
    const char* iboot_vers_str = (iboot_in.buf + IBOOT_VERS_STR_OFFSET);
    
    iboot_in.VERS = _atoi(iboot_vers_str);
    if(!iboot_in.VERS) {
        //printf("%s: No iBoot version found!\n", __FUNCTION__);
        //free(iboot_in.buf);
        return -1;
    }
    
    //printf("%s: iBoot-%d inputted.\n", __FUNCTION__, iboot_in.VERS);
    
    if(1){
        int ret = patch_rsa_check(&iboot_in);
        if(!ret) {
            //printf("%s: Error doing patch_rsa_check()!\n", __FUNCTION__);
            //free(iboot_in.buf);
            return -1;
        }
    }
    
    return 0;
}

//int _exit(){
//    return 0;
//}

//int main(){
//    return 0;
//}
