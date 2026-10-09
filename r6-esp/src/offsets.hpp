#pragma once
#include <cstdint>

// ---------------------------------------------------------------------------
// R6 Siege offsets.
//
// These move on nearly every content patch. Treat this file as the one place
// you update per build. Two ways to populate it:
//
//   1. Signature scan at runtime (see offsets.cpp / Offsets::resolve). The
//      patterns below survive most patches; when one breaks, re-dump it in a
//      disassembler against the current RainbowSix.exe and update the string.
//
//   2. Hardcode absolute RVAs for the current build if you already dumped
//      them. Set USE_SIGNATURES to 0 and fill the RVA_* values.
//
// The named fields describe the external data model this ESP relies on:
//   GameManager -> EntityList -> Entity[i] -> { health, team, bones, name }
//   plus a global ViewMatrix and local-player pointer.
// ---------------------------------------------------------------------------

#define USE_SIGNATURES 1

namespace sig {
    // mov rax, [rip+x] style refs to the global singletons. Update the
    // RIP-relative details (offsetToDisp / instrLen) if the instruction shape
    // changes.
    constexpr const char* GameManager  = "48 8B 0D ? ? ? ? E8 ? ? ? ? 48 8B C8 48 85 C0";
    constexpr const char* ViewMatrix   = "48 8D 0D ? ? ? ? 48 89 ? ? E8 ? ? ? ? 0F 10";
    constexpr const char* LocalPlayer  = "48 8B 05 ? ? ? ? 48 8B 88 ? ? ? ? 48 85 C9";
}

namespace off {
    // --- global roots (filled by Offsets::resolve or hardcoded RVAs) ---
    inline uintptr_t game_manager = 0;   // absolute addr of GameManager pointer
    inline uintptr_t view_matrix  = 0;   // absolute addr of float[16] VP matrix
    inline uintptr_t local_player = 0;   // absolute addr of local pawn pointer

    // --- GameManager -> entity list ---
    constexpr uintptr_t ENTITY_LIST   = 0x48;   // ptr to array of entity ptrs
    constexpr uintptr_t ENTITY_COUNT  = 0x50;   // int32 live entity count
    constexpr uintptr_t ENTITY_STRIDE = 0x08;   // array element size (ptr)

    // --- Entity fields ---
    constexpr uintptr_t HEALTH        = 0x0130; // float
    constexpr uintptr_t MAX_HEALTH    = 0x0134; // float
    constexpr uintptr_t TEAM_ID       = 0x0210; // int32
    constexpr uintptr_t DEAD_FLAG     = 0x0218; // uint8 (1 = down/dead)
    constexpr uintptr_t BONE_ARRAY    = 0x0540; // ptr to bone transform array
    constexpr uintptr_t NAME_PTR      = 0x05A0; // ptr to wide/utf8 name buffer
    constexpr uintptr_t ROOT_POS      = 0x0620; // Vec3 world position (fallback)

    // --- bone indices into the bone transform array ---
    // Each bone is a 4x4 transform (float[16]); translation is at +0x30.
    constexpr uintptr_t BONE_STRIDE   = 0x40;   // sizeof(float[16])
    constexpr uintptr_t BONE_POS_OFF  = 0x30;   // translation within transform
    enum Bone : int {
        BONE_HEAD   = 8,
        BONE_NECK   = 7,
        BONE_SPINE  = 4,
        BONE_PELVIS = 0,
        BONE_L_FOOT = 20,
        BONE_R_FOOT = 24,
    };
}
