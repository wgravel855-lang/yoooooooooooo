#include "memory.hpp"
#include "offsets.hpp"
#include "entity.hpp"
#include "overlay.hpp"
#include "math.hpp"
#include <vector>
#include <string>
#include <thread>
#include <chrono>

bool resolveOffsets(const Memory& mem); // offsets.cpp

// ESP feature toggles.
struct Config {
    bool boxes       = true;
    bool skeleton    = true;
    bool health      = true;
    bool names       = true;
    bool snaplines   = false;
    bool teamCheck   = true;   // only draw enemies
    int  localTeam   = -1;     // filled from local player; -1 = draw everyone
};

static const wchar_t* TARGET = L"RainbowSix.exe";

// Read the current view-projection matrix straight from the resolved global.
static Matrix4x4 readViewMatrix(const Memory& mem) {
    Matrix4x4 vm;
    if (off::view_matrix)
        mem.readRaw(off::view_matrix, vm.flat(), sizeof(float) * 16);
    return vm;
}

// Convert health fraction to a red->green gradient.
static D2D1::ColorF healthColor(float frac) {
    frac = frac < 0 ? 0 : (frac > 1 ? 1 : frac);
    return D2D1::ColorF(1.0f - frac, frac, 0.0f, 1.0f);
}

int main() {
    Memory mem;
    if (!mem.attach(TARGET)) {
        MessageBoxW(nullptr, L"RainbowSix.exe not found. Launch the game first.",
                    L"r6-esp", MB_OK | MB_ICONERROR);
        return 1;
    }

    if (!resolveOffsets(mem)) {
        MessageBoxW(nullptr, L"Signature scan failed. Update patterns in offsets.hpp.",
                    L"r6-esp", MB_OK | MB_ICONERROR);
        return 1;
    }

    Overlay ov;
    if (!ov.create()) {
        MessageBoxW(nullptr, L"Overlay creation failed.", L"r6-esp", MB_OK | MB_ICONERROR);
        return 1;
    }

    Config cfg;
    EntityReader reader(mem);
    std::vector<Entity> entities;

    const auto white  = D2D1::ColorF(D2D1::ColorF::White);
    const auto enemyC = D2D1::ColorF(1.0f, 0.2f, 0.2f, 1.0f);

    // Skeleton bone pairs to connect.
    const int skel[][2] = {
        {off::BONE_HEAD, off::BONE_NECK}, {off::BONE_NECK, off::BONE_SPINE},
        {off::BONE_SPINE, off::BONE_PELVIS},
        {off::BONE_PELVIS, off::BONE_L_FOOT}, {off::BONE_PELVIS, off::BONE_R_FOOT},
    };

    while (true) {
        ov.pumpMessages();
        if (GetAsyncKeyState(VK_END) & 1) break; // END exits

        // local team for the team check
        if (cfg.teamCheck && off::local_player) {
            uintptr_t lp = mem.read<uintptr_t>(off::local_player);
            if (lp) cfg.localTeam = mem.read<int>(lp + off::TEAM_ID);
        }

        Matrix4x4 vm = readViewMatrix(mem);
        reader.collect(entities);

        ov.begin();
        for (const Entity& e : entities) {
            if (!e.valid()) continue;
            if (cfg.teamCheck && cfg.localTeam != -1 && e.team == cfg.localTeam)
                continue;

            Vec2 headS, footS;
            bool okHead = WorldToScreen(e.head, vm, ov.width, ov.height, headS);
            bool okFoot = WorldToScreen(e.root, vm, ov.width, ov.height, footS);
            if (!okHead || !okFoot) continue;

            float h = footS.y - headS.y;
            if (h < 2.f) continue;
            float w = h * 0.45f;
            float boxX = headS.x - w * 0.5f;
            float boxY = headS.y;

            // 2D box
            if (cfg.boxes) {
                ov.box(boxX, boxY, w, h, enemyC, 1.5f);
                // thin dark outline for contrast
                ov.box(boxX - 1, boxY - 1, w + 2, h + 2,
                       D2D1::ColorF(0, 0, 0, 0.6f), 1.0f);
            }

            // snapline from bottom-center of screen to feet
            if (cfg.snaplines) {
                ov.line({ (float)ov.width * 0.5f, (float)ov.height }, footS,
                        enemyC, 1.0f);
            }

            // health bar on the left edge of the box
            if (cfg.health && e.maxHp > 0.f) {
                float frac = e.health / e.maxHp;
                float barX = boxX - 5.f;
                ov.fill(barX, boxY, 3.f, h, D2D1::ColorF(0, 0, 0, 0.7f));
                float fh = h * frac;
                ov.fill(barX, boxY + (h - fh), 3.f, fh, healthColor(frac));
            }

            // name above the box
            if (cfg.names && !e.name.empty()) {
                std::wstring wn(e.name.begin(), e.name.end());
                ov.text(wn, boxX - 175.f, boxY - 16.f, white);
            }

            // skeleton
            if (cfg.skeleton) {
                Vec2 pa, pb;
                for (auto& pair : skel) {
                    Vec3 a = mem.read<Vec3>(
                        mem.read<uintptr_t>(e.addr + off::BONE_ARRAY) +
                        (uintptr_t)pair[0] * off::BONE_STRIDE + off::BONE_POS_OFF);
                    Vec3 b = mem.read<Vec3>(
                        mem.read<uintptr_t>(e.addr + off::BONE_ARRAY) +
                        (uintptr_t)pair[1] * off::BONE_STRIDE + off::BONE_POS_OFF);
                    if (WorldToScreen(a, vm, ov.width, ov.height, pa) &&
                        WorldToScreen(b, vm, ov.width, ov.height, pb))
                        ov.line(pa, pb, white, 1.2f);
                }
            }
        }
        ov.end();

        // ~144 Hz cap; the overlay is cheap, this just avoids spinning a core.
        std::this_thread::sleep_for(std::chrono::milliseconds(6));
    }
    return 0;
}
