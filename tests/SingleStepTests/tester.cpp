#include "../../Cyclone.h"

#include <cinttypes>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <string>
#include <utility>

enum class TestStatus
{
    Passed,
    Failed,
    Skipped,
    Warning,
};

struct TestStats
{
    unsigned passes = 0;
    unsigned failures = 0;
    unsigned skips = 0;
    unsigned warnings = 0;
};

static constexpr unsigned int MEM_SIZE = 0x1000000;
static constexpr unsigned int DIRTY_PAGE_SIZE = 0x1000;

static struct Cyclone cpu;
static struct Cyclone final_cpu;

static uint8_t memory[MEM_SIZE];
static uint8_t final_memory[MEM_SIZE];
static uint8_t memory_dirty[MEM_SIZE / DIRTY_PAGE_SIZE];

// memhandlers externally defined with the default prefix in case MEMHANDLERS_DIRECT_PREFIX is used
extern "C" unsigned int cyclone_checkpc(unsigned int pc)
{
    pc -= cpu.membase;
    unsigned int pc_addr = (unsigned int)&memory[pc & (MEM_SIZE - 2)];
    // preserve the upper 8 bits of PC in the membase
    cpu.membase = pc_addr - pc;
    return pc_addr;
}

extern "C" unsigned int cyclone_read8(unsigned int a)
{
    a &= MEM_SIZE - 1;
    return memory[a ^ 1];
}

extern "C" unsigned int cyclone_read16(unsigned int a)
{
    a &= MEM_SIZE - 2;
    return *(const uint16_t*)(memory + a);
}

extern "C" unsigned int cyclone_read32(unsigned int a)
{
    unsigned int d = cyclone_read16(a) << 16;
    return d | cyclone_read16(a + 2);
}

extern "C" void cyclone_write8(unsigned int a, unsigned char d)
{
    a &= MEM_SIZE - 1;
    memory[a ^ 1] = d;
    memory_dirty[a / DIRTY_PAGE_SIZE] = 1;
}

extern "C" void cyclone_write16(unsigned int a, unsigned short d)
{
    a &= MEM_SIZE - 2;
    *(uint16_t*)(memory + a) = d;
    memory_dirty[a / DIRTY_PAGE_SIZE] = 1;
}

extern "C" void cyclone_write32(unsigned int a, unsigned int d)
{
    cyclone_write16(a, (unsigned short)(d >> 16));
    cyclone_write16(a + 2, (unsigned short)d);
}

extern "C" unsigned int cyclone_fetch8(unsigned int a)
{
    return cyclone_read8(a);
}

extern "C" unsigned int cyclone_fetch16(unsigned int a)
{
    return cyclone_read16(a);
}

extern "C" unsigned int cyclone_fetch32(unsigned int a)
{
    return cyclone_read32(a);
}

static bool read_failure()
{
    puts("Failed to read from test file");
    return false;
}

static bool read8(FILE* test_file, uint8_t& value)
{
    if (fread(&value, sizeof(value), 1, test_file) == 1) return true;

    return read_failure();
}

static bool read16(FILE* test_file, uint16_t& value)
{
    if (fread(&value, sizeof(value), 1, test_file) == 1) return true;

    return read_failure();
}

static bool read32(FILE* test_file, uint32_t& value)
{
    if (fread(&value, sizeof(value), 1, test_file) == 1) return true;

    return read_failure();
}

static bool read_name(FILE* test_file, std::string& name)
{
    uint32_t num_bytes, name_magic, name_len;
    if (!read32(test_file, num_bytes)) return false;
    if (!read32(test_file, name_magic)) return false;
    if (name_magic != UINT32_C(0x89ABCDEF))
    {
        puts("Test name magic is incorrect!");
        return false;
    }

    if (!read32(test_file, name_len)) return false;
    name.resize(name_len);

    if (fread(name.data(), name_len, 1, test_file) == 1) return true;

    return read_failure();
}

static bool read_transactions(FILE* test_file, uint32_t& cycles, bool& has_error)
{
    has_error = false;

    uint32_t num_bytes, transactions_magic, num_transactions;
    if (!read32(test_file, num_bytes)) return false;
    if (!read32(test_file, transactions_magic)) return false;
    if (transactions_magic != UINT32_C(0x456789AB))
    {
        puts("Test transactions magic is incorrect!");
        return false;
    }

    if (!read32(test_file, cycles)) return false;
    if (!read32(test_file, num_transactions)) return false;

    while (num_transactions--)
    {
        uint8_t tw;
        uint32_t tcyc;
        if (!read8(test_file, tw)) return false;
        if (!read32(test_file, tcyc)) return false;
        if (tw == 0) continue;

        uint32_t fc, addr_bus, data_bus, UDS, LDS;
        if (!read32(test_file, fc)) return false;
        if (!read32(test_file, addr_bus)) return false;
        if (!read32(test_file, data_bus)) return false;
        if (!read32(test_file, UDS)) return false;
        if (!read32(test_file, LDS)) return false;

        if (tw >= 4) has_error = true;
    }

    return true;
}

static bool read_state(FILE* test_file, struct Cyclone& state, uint8_t* mem)
{
    uint32_t num_bytes, state_magic;
    if (!read32(test_file, num_bytes)) return false;
    if (!read32(test_file, state_magic)) return false;
    if (state_magic != UINT32_C(0x01234567))
    {
        puts("Test state magic is incorrect!");
        return false;
    }

    uint32_t reg;
    for (size_t i = 0; i < 8; i++)
    {
        if (!read32(test_file, reg)) return false;
        state.d[i] = reg;
    }

    for (size_t i = 0; i < 8; i++)
    {
        if (!read32(test_file, reg)) return false;
        state.a[i] = reg;
    }

    if (!read32(test_file, reg)) return false;
    state.osp = reg;

    if (!read32(test_file, reg)) return false;
    CycloneSetSr(&state, reg);
    // Swap USP and SSP if applicable
    if (reg & 0x2000) std::swap(state.a[7], state.osp);

    if (!read32(test_file, reg)) return false;
    // Remove prefetch adjustment from PC
    state.pc = reg - 4;
    state.membase = 0;

    // Prefetch values, ignore
    if (!read32(test_file, reg)) return false;
    if (!read32(test_file, reg)) return false;

    uint32_t mem_count;
    if (!read32(test_file, mem_count)) return false;

    while (mem_count--)
    {
        uint32_t a;
        uint16_t d;
        if (!read32(test_file, a)) return false;
        if (!read16(test_file, d)) return false;
        a &= MEM_SIZE - 2;
        *(uint16_t*)(mem + a) = d;
        memory_dirty[a / DIRTY_PAGE_SIZE] = 1;
    }
    return true;
}

static void clean_memory()
{
    for (size_t i = 0; i < MEM_SIZE / DIRTY_PAGE_SIZE; i++)
    {
        if (memory_dirty[i])
        {
            memset(&memory[i * DIRTY_PAGE_SIZE], 0, DIRTY_PAGE_SIZE);
            memset(&final_memory[i * DIRTY_PAGE_SIZE], 0, DIRTY_PAGE_SIZE);
            memory_dirty[i] = 0;
        }
    }
}

static uint32_t state_get_pc(const struct Cyclone& state)
{
    uint32_t pc = state.pc - state.membase;
    // Adjust for stopped state, expects PC to point at stop instruction for some reason
    if (state.state_flags & 1)
    {
        pc -= 4;
    }
    return pc;
}

static bool states_equal()
{
    if (memcmp(&cpu.d, &final_cpu.d, sizeof(cpu.d)) != 0) return false;
    if (memcmp(&cpu.a, &final_cpu.a, sizeof(cpu.a)) != 0) return false;
    if (cpu.osp != final_cpu.osp) return false;
    if (CycloneGetSr(&cpu) != CycloneGetSr(&final_cpu)) return false;
    if (state_get_pc(cpu) != state_get_pc(final_cpu)) return false;

    for (size_t i = 0; i < MEM_SIZE / DIRTY_PAGE_SIZE; i++)
    {
        if (memory_dirty[i])
        {
            if (memcmp(&memory[i * DIRTY_PAGE_SIZE],
                       &final_memory[i * DIRTY_PAGE_SIZE],
                       DIRTY_PAGE_SIZE) != 0)
                return false;
        }
    }

    return true;
}

static void print_state(struct Cyclone& state, const uint8_t* mem)
{
    unsigned int sr = CycloneGetSr(&state);
    bool sys = sr & 0x2000;

    for (size_t i = 0; i < 8; i++)
    {
        printf("d%c=%08x\n", '0'+i, state.d[i]);
    }
    for (size_t i = 0; i < 7; i++)
    {
        printf("a%c=%08x\n", '0'+i, state.a[i]);
    }
    printf("usp=%08x\n", !sys ? state.a[7] : state.osp);
    printf("ssp=%08x\n", sys ? state.a[7] : state.osp);
    printf("sr=%08x\n", sr);
    printf("pc=%08x\n", state_get_pc(state));

    for (uint32_t addr = 0; addr < MEM_SIZE; addr += 2)
    {
        uint16_t data = *(const uint16_t*)(mem + addr);
        if (data)
        {
            printf("mem[%06" PRIx32 "]=%04" PRIx16 "\n", addr, data);
        }
    }
}

static bool run_test(FILE* test_file, TestStatus& status, bool& show_failure)
{
    status = TestStatus::Passed;

    uint32_t num_bytes, test_magic;
    if (!read32(test_file, num_bytes)) return false;
    if (!read32(test_file, test_magic)) return false;
    if (test_magic != UINT32_C(0xABC12367))
    {
        puts("Test magic is incorrect!");
        return false;
    }

    std::string test_name;
    if (!read_name(test_file, test_name)) return false;
    clean_memory();
    if (!read_state(test_file, cpu, memory)) return false;
    if (!read_state(test_file, final_cpu, final_memory)) return false;
    uint32_t cycles;
    bool has_error;
    if (!read_transactions(test_file, cycles, has_error)) return false;

    if (has_error)
    {
        status = TestStatus::Skipped;
        return true;
    }

    cpu.state_flags = 0;
    cpu.cycles = 0;
    cpu.pc = cpu.checkpc(cpu.pc);
    CycloneRun(&cpu);

    if ((uint32_t)-cpu.cycles != cycles)
    {
        status = TestStatus::Warning;
        printf("Warning: %s expected %" PRIu32 " cycles, got %d\n",
            test_name.c_str(), cycles, -cpu.cycles);
    }

    if (!states_equal())
    {
        status = TestStatus::Failed;
        if (show_failure)
        {
            show_failure = false;
            printf("Test failed: %s\n", test_name.c_str());
            printf("Expected state:\n");
            print_state(final_cpu, final_memory);
            printf("Actual state:\n");
            print_state(cpu, memory);
        }
        return true;
    }

    return true;
}

static bool run_tests(FILE* test_file, TestStats& total_stats)
{
    uint32_t file_magic, num_tests;
    if (!read32(test_file, file_magic)) return false;
    if (file_magic != UINT32_C(0x1A3F5D71))
    {
        puts("Test file magic is incorrect!");
        return false;
    }

    if (!read32(test_file, num_tests)) return false;
    printf("Running %" PRIu32 " tests\n", num_tests);

    struct TestStats stats;
    bool show_failure = true;
    bool result = true;
    for (; num_tests; num_tests--)
    {
        enum TestStatus status;
        result = run_test(test_file, status, show_failure);
        if (!result) break;
        switch (status)
        {
        case TestStatus::Passed:
            stats.passes++;
            break;
        case TestStatus::Failed:
            stats.failures++;
            break;
        case TestStatus::Skipped:
            stats.skips++;
            break;
        case TestStatus::Warning:
            stats.warnings++;
            stats.passes++;
            break;
	}
    }

    stats.skips += num_tests;
    printf("%u passed, %u skipped, %u failed, %u warnings\n",
           stats.passes, stats.skips, stats.failures, stats.warnings);

    total_stats.passes += stats.passes;
    total_stats.skips += stats.skips;
    total_stats.failures += stats.failures;
    total_stats.warnings += stats.warnings;

    return result;
}

int main()
{
    cpu.checkpc = cyclone_checkpc;
    cpu.read8 = cyclone_read8;
    cpu.read16 = cyclone_read16;
    cpu.read32 = cyclone_read32;
    cpu.write8 = cyclone_write8;
    cpu.write16 = cyclone_write16;
    cpu.write32 = cyclone_write32;
    cpu.fetch8 = cyclone_fetch8;
    cpu.fetch16 = cyclone_fetch16;
    cpu.fetch32 = cyclone_fetch32;

    CycloneInit();
    CycloneReset(&cpu);

    TestStats stats;
    unsigned errored = 0;
    for (const auto& dir_entry : std::filesystem::directory_iterator("m68000/v1"))
    {
        const char* test_path = dir_entry.path().c_str();
        printf("Running tests from %s\n", test_path);
        FILE* test_file = fopen(test_path, "rb");
        if (!test_file)
        {
            perror("Failed to open test file");
            return EXIT_FAILURE;
        }
        if (!run_tests(test_file, stats))
        {
            errored++;
        }
    }
    puts("Test summary:");
    if (errored != 0)
    {
        printf("%u test files had parsing errors\n", errored);
    }

    printf("%u passed, %u skipped, %u failed, %u warnings\n",
           stats.passes, stats.skips, stats.failures, stats.warnings);

    return EXIT_SUCCESS;
}
