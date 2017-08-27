#include "function_hook.h"
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <algorithm>  /* max, min */
#include <unistd.h> /* getpagesize */
#include <sys/mman.h> /* mmap */
#include "ZydisInstructionDecoder.hpp"
#include "debug.h"
#if defined __APPLE__ && defined __MACH__ /* Mac OS X */
#include <mach/mach.h>
#endif

#define DEBUG_FUNCTION_HOOK 1
#if DEBUG_FUNCTION_HOOK
#include "ZydisInstructionFormatter.hpp"
#endif

template <typename T>
T round_up(T n, T size) {
    return ((n + size - 1) / size) * size;
}

template <typename T>
T round_down(T n, T size) {
    return n / size * size;
}

template <typename T>
T round_down_ptr(T ptr, uintptr_t size) {
    return reinterpret_cast<T>(round_down(reinterpret_cast<uintptr_t>(ptr), size));
}

template <typename T>
T round_up_ptr(T ptr, uintptr_t size) {
    return reinterpret_cast<T>(round_up(reinterpret_cast<uintptr_t>(ptr), size));
}

template <typename T>
T offset_ptr(T ptr, ptrdiff_t diff) {
    return reinterpret_cast<T>(reinterpret_cast<uintptr_t>(ptr) + diff);
}

#if defined __APPLE__ && defined __MACH__ /* Mac OS X */
void* vm_mmap(uint8_t* lower_limit, uint8_t* upper_limit, size_t map_size)
{
    size_t const page_size = static_cast<size_t>(getpagesize());
    task_t task = mach_task_self();
    vm_address_t address = VM_MIN_ADDRESS;
    vm_size_t size;

    for (address = VM_MIN_ADDRESS;; address += size) {
        mach_port_t object_name;
        struct vm_region_basic_info_64 info;
        mach_msg_type_number_t info_count = VM_REGION_BASIC_INFO_COUNT_64;

        vm_address_t prev_bottom = address;
        int more = vm_region_64(task, &address, &size, VM_REGION_BASIC_INFO_64,
                             (vm_region_info_t)&info, &info_count, &object_name);
        if (object_name != MACH_PORT_NULL)
            mach_port_deallocate(task, object_name);
        if (more != KERN_SUCCESS)
            return nullptr;

        uint8_t* lower = reinterpret_cast<uint8_t*>(round_up(std::max(reinterpret_cast<vm_address_t>(lower_limit + 1), prev_bottom), page_size));
        uint8_t* upper = reinterpret_cast<uint8_t*>(round_down(std::min(reinterpret_cast<vm_address_t>(upper_limit - 1), address), page_size));
        for ( ; lower < upper; lower += page_size) {
            void *p = mmap(lower, map_size, PROT_READ | PROT_WRITE | PROT_EXEC, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
            if (p != MAP_FAILED) {
                if (lower_limit < static_cast<uint8_t *>(p) && static_cast<uint8_t *>(p) < upper_limit)
                    return p;
                else
                    munmap(p, map_size);
            }
        }
    }
    return nullptr;
}

void vm_dump()
{
    task_t task = mach_task_self();
    vm_address_t address;
    vm_size_t size;

    memtuner_debug_println_hex("VM_MIN_ADDRESS: ", reinterpret_cast<uintptr_t>(VM_MIN_ADDRESS));

    for (address = VM_MIN_ADDRESS;; address += size) {
      mach_port_t object_name;
      struct vm_region_basic_info_64 info;
      mach_msg_type_number_t info_count = VM_REGION_BASIC_INFO_COUNT_64;

      int more = vm_region_64(task, &address, &size, VM_REGION_BASIC_INFO_64,
                             (vm_region_info_t)&info, &info_count, &object_name);
      if (object_name != MACH_PORT_NULL)
        mach_port_deallocate(task, object_name);
      if (more != KERN_SUCCESS)
        break;
      char flags[4];
      flags[0] = (info.protection & VM_PROT_READ) ? 'R' : '-';
      flags[1] = (info.protection & VM_PROT_WRITE) ? 'W' : '-';
      flags[2] = (info.protection & VM_PROT_EXECUTE) ? 'X' : '-';
      flags[3] = '\0';
      memtuner_debug_print_hex("", address);
      memtuner_debug_print_hex(" - ", address + size);
      memtuner_debug_print(" ");
      memtuner_debug_println(flags);
    }
}
#else
void vm_dump() {}
#endif
const size_t MAX_CODE_BYTES = 32;
const size_t JMP_CODE_BYTES = 5;
const size_t RIP_PATCH_MAX = 4;

struct rip_patch_t{
    size_t offset;
    int32_t displacement;
};

struct patch_data_t{
    ptrdiff_t top;
    ptrdiff_t bottom;
    uint32_t rip_count;
    rip_patch_t rip_patch[RIP_PATCH_MAX];
    size_t jmp_code_offset;
    uintptr_t jmp_address;
};

struct trampoline_t {
    void *original_func;
    void *hook_func;
    size_t code_length;
    uint8_t original_code[MAX_CODE_BYTES];
    uint8_t jump_to_hook_code[MAX_CODE_BYTES];
    uint8_t entry_code[MAX_CODE_BYTES];
};

static void* jump_address(uint8_t* code) {
    if (code[0] == 0xff && code[1] == 0x25) {
        int32_t offset;
        memcpy(&offset, code + 2, sizeof(offset));
        void* ptr;
        memcpy(&ptr, code + 6 + offset, sizeof(ptr));
        return ptr;
    } else if (code[0] == 0x48 && code[1] == 0xff && code[2] == 0x25) {
        int32_t offset;
        memcpy(&offset, code + 3, sizeof(offset));
        void* ptr;
        memcpy(&ptr, code + 7 + offset, sizeof(ptr));
        return ptr;
    } else if (code[0] == 0xe9) {
        int32_t offset;
        memcpy(&offset, code + 1, sizeof(offset));
        return code + 5 + offset;
    } else if (code[0] == 0xeb) {
        int8_t offset;
        memcpy(&offset, code + 1, sizeof(offset));
        return code + 2 + offset;
    }
    return nullptr;
}

static void* skip_jumps(void* func) {
    uint8_t* const code = static_cast<uint8_t*>(func);
    void* ptr = jump_address(code);
    if (ptr)
        return skip_jumps(ptr);
    return func;
}

size_t disassemble_and_skip(void *func, patch_data_t& patch_data)
{
    Zydis::InstructionInfo info;
    Zydis::InstructionDecoder decoder;
    Zydis::MemoryInput input64(func, MAX_CODE_BYTES);

    decoder.setDisassemblerMode(Zydis::DisassemblerMode::M64BIT);
    decoder.setDataSource(&input64);
    decoder.setInstructionPointer(reinterpret_cast<uintptr_t>(func));

    patch_data.top = 0;
    patch_data.bottom = 0;
    patch_data.rip_count = 0;
    patch_data.jmp_code_offset = 0;
    patch_data.jmp_address = 0;

    size_t size = 0;
    while (size < JMP_CODE_BYTES && decoder.decodeInstruction(info)) {
        if (info.flags & Zydis::IF_ERROR_MASK){
            return 0;
        }
        switch(info.mnemonic) {
        case Zydis::InstructionMnemonic::JMP:
            {
                void* address = jump_address(reinterpret_cast<uint8_t*>(func) + size);
                if (info.length >= JMP_CODE_BYTES && address) {
                    patch_data.jmp_code_offset = size;
                    patch_data.jmp_address = reinterpret_cast<uintptr_t>(address);
                    ptrdiff_t const adjusted_displacement = patch_data.jmp_address - reinterpret_cast<uintptr_t>(func);
                    if (adjusted_displacement < patch_data.bottom)
                        patch_data.bottom = adjusted_displacement;
                    if (patch_data.top < adjusted_displacement)
                        patch_data.top = adjusted_displacement;
                    return size + info.length;
                }
                return size;
            }
        case Zydis::InstructionMnemonic::RET:
        case Zydis::InstructionMnemonic::RETF:
        case Zydis::InstructionMnemonic::JA:
        case Zydis::InstructionMnemonic::JB:
        case Zydis::InstructionMnemonic::JBE:
        case Zydis::InstructionMnemonic::JCXZ:
        case Zydis::InstructionMnemonic::JE:
        case Zydis::InstructionMnemonic::JECXZ:
        case Zydis::InstructionMnemonic::JG:
        case Zydis::InstructionMnemonic::JGE:
        case Zydis::InstructionMnemonic::JL:
        case Zydis::InstructionMnemonic::JLE:
        case Zydis::InstructionMnemonic::JNB:
        case Zydis::InstructionMnemonic::JNE:
        case Zydis::InstructionMnemonic::JNO:
        case Zydis::InstructionMnemonic::JNP:
        case Zydis::InstructionMnemonic::JNS:
        case Zydis::InstructionMnemonic::JO:
        case Zydis::InstructionMnemonic::JP:
        case Zydis::InstructionMnemonic::JRCXZ:
        case Zydis::InstructionMnemonic::JS:
        case Zydis::InstructionMnemonic::CALL:
            return size;
        case Zydis::InstructionMnemonic::MOV:
        case Zydis::InstructionMnemonic::MOVAPD:
        case Zydis::InstructionMnemonic::MOVAPS:
        case Zydis::InstructionMnemonic::MOVBE:
        case Zydis::InstructionMnemonic::MOVD:
        case Zydis::InstructionMnemonic::MOVDDUP:
        case Zydis::InstructionMnemonic::MOVDQ2Q:
        case Zydis::InstructionMnemonic::MOVDQA:
        case Zydis::InstructionMnemonic::MOVDQU:
        case Zydis::InstructionMnemonic::MOVHLPS:
        case Zydis::InstructionMnemonic::MOVHPD:
        case Zydis::InstructionMnemonic::MOVHPS:
        case Zydis::InstructionMnemonic::MOVLHPS:
        case Zydis::InstructionMnemonic::MOVLPD:
        case Zydis::InstructionMnemonic::MOVLPS:
        case Zydis::InstructionMnemonic::MOVMSKPD:
        case Zydis::InstructionMnemonic::MOVMSKPS:
        case Zydis::InstructionMnemonic::MOVNTDQ:
        case Zydis::InstructionMnemonic::MOVNTDQA:
        case Zydis::InstructionMnemonic::MOVNTI:
        case Zydis::InstructionMnemonic::MOVNTPD:
        case Zydis::InstructionMnemonic::MOVNTPS:
        case Zydis::InstructionMnemonic::MOVNTQ:
        case Zydis::InstructionMnemonic::MOVQ:
        case Zydis::InstructionMnemonic::MOVQ2DQ:
        case Zydis::InstructionMnemonic::MOVSB:
        case Zydis::InstructionMnemonic::MOVSD:
        case Zydis::InstructionMnemonic::MOVSHDUP:
        case Zydis::InstructionMnemonic::MOVSLDUP:
        case Zydis::InstructionMnemonic::MOVSQ:
        case Zydis::InstructionMnemonic::MOVSS:
        case Zydis::InstructionMnemonic::MOVSW:
        case Zydis::InstructionMnemonic::MOVSX:
        case Zydis::InstructionMnemonic::MOVSXD:
        case Zydis::InstructionMnemonic::MOVUPD:
        case Zydis::InstructionMnemonic::MOVUPS:
        case Zydis::InstructionMnemonic::MOVZX:
        case Zydis::InstructionMnemonic::LEA:
            for(size_t i = 0; i < sizeof(info.operand)/sizeof(info.operand[0]); ++i) {
                Zydis::OperandInfo& operand = info.operand[i];
                if (operand.type == Zydis::OperandType::MEMORY && operand.base == Zydis::Register::RIP) {
                    if (patch_data.rip_count < RIP_PATCH_MAX) {
                        rip_patch_t& patch = patch_data.rip_patch[patch_data.rip_count];
                        patch.offset = size + 3;
                        patch.displacement = operand.lval.sdword;

                        ptrdiff_t const adjusted_displacement = patch.displacement + static_cast<ptrdiff_t>(info.instrAddress - reinterpret_cast<uintptr_t>(func));
                        if (adjusted_displacement < patch_data.bottom)
                            patch_data.bottom = adjusted_displacement;
                        if (patch_data.top < adjusted_displacement)
                            patch_data.top = adjusted_displacement;

                        ++patch_data.rip_count;
                    }
                }
            }
            break;
        default:
            break;
        }

#if DEBUG_FUNCTION_HOOK
        uint8_t *bytes = (uint8_t *)func + size;
        printf("%p: ", bytes);
        for(size_t i = 0; i < info.length; ++i) {
            printf("%02x ", bytes[i]);
        }
        Zydis::IntelInstructionFormatter formatter;
        printf(" %s", formatter.formatInstruction(info));
        printf("\n");
#endif
        size += info.length;
    }
#if DEBUG_FUNCTION_HOOK    
    for(size_t i = 0; i < patch_data.rip_count; ++i) {
        printf("RIP[%zu] offset=%zu displacement=%04x\n", i, patch_data.rip_patch[i].offset, patch_data.rip_patch[i].displacement);
    }
    printf("top=%zd botttom=%zd\n", patch_data.top, patch_data.bottom);
#endif
    return size;
}

uint64_t const TWO_GIGA = 0x80000000;
uint64_t const MINUS_TWO_GIGA = 0xffffffff80000000;
namespace {
    size_t trampoline_map_size = 0;
}

void free_trampoline(void* trampoline) {
    munmap(trampoline, trampoline_map_size);
}

trampoline_t* alloc_trampoline(void* func, int64_t bottom, int64_t top) {
    size_t const page_size = static_cast<size_t>(getpagesize());

    uint8_t* lower_limit = static_cast<uint8_t *>(func) + top;
    uint8_t* upper_limit = static_cast<uint8_t *>(func) + bottom;
    lower_limit = lower_limit < reinterpret_cast<uint8_t*>(TWO_GIGA) ?  reinterpret_cast<uint8_t*>(1) : lower_limit - 0x7fff0000;
    upper_limit = reinterpret_cast<uint8_t*>(MINUS_TWO_GIGA) < upper_limit ? reinterpret_cast<uint8_t*>(0xfffffffffff80000) : upper_limit + 0x7ff80000;

    trampoline_map_size = round_up(sizeof(trampoline_t), page_size);
#if defined __APPLE__ && defined __MACH__ /* Mac OS X */
    void* p = vm_mmap(lower_limit, upper_limit, trampoline_map_size);
#else
    void* p = mmap(round_down_ptr(func, page_size), trampoline_map_size, PROT_READ | PROT_WRITE | PROT_EXEC, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
#endif
    if (p == MAP_FAILED) {
        memtuner_debug_println_signed("memtuner: map failed: ", errno);
        return nullptr;
    }
    if (lower_limit < static_cast<uint8_t *>(p) && static_cast<uint8_t *>(p) < upper_limit) {
        memtuner_debug_print_hex("memtuner: map ok: p=", reinterpret_cast<uintptr_t>(p));
        memtuner_debug_println_hex(", func=", reinterpret_cast<uintptr_t>(func));
        return static_cast<trampoline_t *>(p);
    } else {
        memtuner_debug_print_hex("memtuner: map out of range: p=", reinterpret_cast<uintptr_t>(p));
        memtuner_debug_println_hex(", func=", reinterpret_cast<uintptr_t>(func));
        // printf("map out of range: %p (func=%p)\n", p, func);
        free_trampoline(p);
        return nullptr;
    }
}

void protect_trampoline(trampoline_t* trampoline) {
    mprotect(trampoline, trampoline_map_size, PROT_EXEC | PROT_READ);
}

uint8_t* emit_jump(uint8_t* code, uint8_t* jump_to) {
    uint8_t* const jump_from = code + 5;
    size_t diff = jump_from > jump_to ? jump_from - jump_to : jump_to - jump_from;
    if (diff < 0x7fff0000) {
        // 5bytes
        *code = 0xe9;
        ++code;
        uint32_t const displacement = static_cast<uint32_t>(jump_to - jump_from);
        memcpy(code, &displacement, sizeof(displacement));
        code += sizeof(displacement);
    } else {
        // todo: bug?
        // 14bytes
        code[0] = 0xff;
        code[1] = 0x25;
        code += 2;
        memset(code, 0, 4);
        code += 4;
        memcpy(code, &jump_to, sizeof(jump_to));
        code += sizeof(jump_to);
    }
    return code;
}

void fixup_ip_relative(uint8_t *code, uint8_t *original_code, patch_data_t const& patch_data) {
    ptrdiff_t const diff = code - original_code;
    for (uint32_t i = 0; i < patch_data.rip_count; ++i) {
        rip_patch_t const& rip_patch = patch_data.rip_patch[i];
        int32_t const new_displacement = static_cast<int32_t>(rip_patch.displacement - diff);
        memcpy(code + rip_patch.offset, &new_displacement, sizeof(new_displacement));
    }
    if (patch_data.jmp_address != 0) {
        emit_jump(code + patch_data.jmp_code_offset, reinterpret_cast<uint8_t*>(patch_data.jmp_address));
    }
}

void flush_icache(void* lower, void* upper) {
    // x64: no need to flush icache
    // see https://github.com/LuaJIT/LuaJIT/blob/f50bf7585a32738c4fb719cb8fc59d02231fc8c3/src/lj_mcode.c#L36
    (void)lower;
    (void)upper;
}

int mprotect_code(void* func, size_t size, int prot) {
    size_t const page_size = static_cast<size_t>(getpagesize());
    uint8_t* const aligned_code_begin = round_down_ptr(static_cast<uint8_t*>(func), page_size);
    size_t const aligned_code_length = round_up_ptr(static_cast<uint8_t*>(func) + size, page_size) - aligned_code_begin;
    return mprotect(aligned_code_begin, aligned_code_length, prot);
}

void* hook_function(void* func, void* hook_func) {
    patch_data_t patch_data;
    memset(&patch_data, 0, sizeof(patch_data));
    func = skip_jumps(func);
    hook_func = skip_jumps(hook_func);
    size_t const size = disassemble_and_skip(func, patch_data);
    if (size >= JMP_CODE_BYTES) {
        trampoline_t* trampoline = alloc_trampoline(func, patch_data.bottom, patch_data.top);
        if (trampoline) {
            if (mprotect_code(func, size, PROT_EXEC | PROT_READ | PROT_WRITE) != 0) {
                memtuner_debug_print("memtuner: mprotect failed\n");
                free_trampoline(trampoline);
                return nullptr;
            }

            memcpy(trampoline->original_code, func, size);
            memcpy(trampoline->entry_code, func, size);
            fixup_ip_relative(trampoline->entry_code, static_cast<uint8_t *>(func), patch_data);
            uint8_t* const end_entry_code = emit_jump(trampoline->entry_code + size, static_cast<uint8_t *>(func) + size);
            flush_icache(trampoline->entry_code, end_entry_code);

            uintptr_t const func_addr = reinterpret_cast<uintptr_t>(func);
            uintptr_t const hook_func_addr = reinterpret_cast<uintptr_t>(hook_func);
            uintptr_t const distance = func_addr > hook_func_addr ? func_addr - hook_func_addr : hook_func_addr - func_addr;
            if (distance > 0x7fff0000) {
                uint8_t* const end = emit_jump(trampoline->jump_to_hook_code, static_cast<uint8_t*>(hook_func));
                flush_icache(trampoline->jump_to_hook_code, end);
                emit_jump(static_cast<uint8_t*>(func), trampoline->jump_to_hook_code);
            } else {
                emit_jump(static_cast<uint8_t*>(func), static_cast<uint8_t*>(hook_func));
            }
            flush_icache(func, static_cast<uint8_t*>(func) + size);

            trampoline->original_func = func;
            trampoline->hook_func = hook_func;
            trampoline->code_length = size;

            mprotect_code(func, size, PROT_EXEC | PROT_READ);
            protect_trampoline(trampoline);
            return trampoline->entry_code;
        }
    }

    return nullptr;
}
