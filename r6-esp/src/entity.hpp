#pragma once
#include "memory.hpp"
#include "offsets.hpp"
#include "math.hpp"
#include <string>
#include <vector>

// A snapshot of one entity read out of the target each frame. Keeping a flat
// POD snapshot (rather than reading per-draw) means one RPM burst per entity
// and no torn reads while rendering.
struct Entity {
    uintptr_t addr    = 0;
    float     health  = 0.f;
    float     maxHp   = 0.f;
    int       team    = -1;
    bool      dead    = false;
    Vec3      root;                  // pelvis / root position
    Vec3      head;                  // head bone position
    std::string name;

    bool valid() const { return addr && !dead && health > 0.f; }
};

class EntityReader {
public:
    explicit EntityReader(const Memory& m) : mem(m) {}

    // Read the live entity list into `out`. Returns false if the roots are
    // not resolved yet.
    bool collect(std::vector<Entity>& out) {
        out.clear();
        if (!off::game_manager) return false;

        uintptr_t gm = mem.read<uintptr_t>(off::game_manager);
        if (!gm) return false;

        uintptr_t list = mem.read<uintptr_t>(gm + off::ENTITY_LIST);
        int count      = mem.read<int>(gm + off::ENTITY_COUNT);
        if (!list || count <= 0 || count > 1024) return false;

        out.reserve(count);
        for (int i = 0; i < count; ++i) {
            uintptr_t ePtr = mem.read<uintptr_t>(
                list + static_cast<uintptr_t>(i) * off::ENTITY_STRIDE);
            if (!ePtr) continue;

            Entity e;
            e.addr   = ePtr;
            e.health = mem.read<float>(ePtr + off::HEALTH);
            e.maxHp  = mem.read<float>(ePtr + off::MAX_HEALTH);
            e.team   = mem.read<int>(ePtr + off::TEAM_ID);
            e.dead   = mem.read<uint8_t>(ePtr + off::DEAD_FLAG) != 0;

            if (e.health <= 0.f || e.health > 1000.f) continue;

            e.head = readBone(ePtr, off::BONE_HEAD);
            e.root = readBone(ePtr, off::BONE_PELVIS);
            if (e.root.length() < 0.001f)
                e.root = mem.read<Vec3>(ePtr + off::ROOT_POS);

            e.name = readName(ePtr);
            out.push_back(std::move(e));
        }
        return true;
    }

private:
    const Memory& mem;

    Vec3 readBone(uintptr_t entity, int boneIdx) {
        uintptr_t boneArr = mem.read<uintptr_t>(entity + off::BONE_ARRAY);
        if (!boneArr) return {};
        uintptr_t transform = boneArr +
            static_cast<uintptr_t>(boneIdx) * off::BONE_STRIDE;
        return mem.read<Vec3>(transform + off::BONE_POS_OFF);
    }

    std::string readName(uintptr_t entity) {
        uintptr_t nPtr = mem.read<uintptr_t>(entity + off::NAME_PTR);
        if (!nPtr) return {};
        char buf[48] = {};
        mem.readRaw(nPtr, buf, sizeof(buf) - 1);
        return std::string(buf);
    }
};
