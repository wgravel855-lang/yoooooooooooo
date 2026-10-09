#include "offsets.hpp"
#include "memory.hpp"
#include <cstdio>

// Resolve the three global roots via signature scan. Each pattern lands on an
// instruction with a RIP-relative displacement pointing at the singleton; we
// decode that displacement to the absolute address in the target.
bool resolveOffsets(const Memory& mem) {
#if USE_SIGNATURES
    // GameManager: 48 8B 0D <disp32>  -> disp at +3, instr length 7
    uintptr_t gm = mem.findPattern(sig::GameManager);
    if (gm) off::game_manager = mem.resolveRip(gm, 3, 7);

    // ViewMatrix: 48 8D 0D <disp32>   -> disp at +3, instr length 7
    uintptr_t vm = mem.findPattern(sig::ViewMatrix);
    if (vm) off::view_matrix = mem.resolveRip(vm, 3, 7);

    // LocalPlayer: 48 8B 05 <disp32>  -> disp at +3, instr length 7
    uintptr_t lp = mem.findPattern(sig::LocalPlayer);
    if (lp) off::local_player = mem.resolveRip(lp, 3, 7);

    printf("[offsets] game_manager = %p\n", (void*)off::game_manager);
    printf("[offsets] view_matrix  = %p\n", (void*)off::view_matrix);
    printf("[offsets] local_player = %p\n", (void*)off::local_player);

    return off::game_manager && off::view_matrix;
#else
    // Hardcoded RVAs for a known build. Fill these after dumping.
    constexpr uintptr_t RVA_GAME_MANAGER = 0x0;
    constexpr uintptr_t RVA_VIEW_MATRIX  = 0x0;
    constexpr uintptr_t RVA_LOCAL_PLAYER = 0x0;

    off::game_manager = mem.base + RVA_GAME_MANAGER;
    off::view_matrix  = mem.base + RVA_VIEW_MATRIX;
    off::local_player = mem.base + RVA_LOCAL_PLAYER;
    return RVA_GAME_MANAGER && RVA_VIEW_MATRIX;
#endif
}
