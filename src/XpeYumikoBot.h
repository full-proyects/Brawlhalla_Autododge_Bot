// XPE Bot — Yumiko module
// Copyright (c) 2026 xpe.nett. MIT License.
//
// Legend-specific offense layer for Yumiko (legend id 36, internal "Ninetails",
// weapons: Hammer = 4, Bow = 10). It builds on the generic OffenseController:
// every follow-up it proposes is re-validated by OffenseController::PickAttack
// (frame-hitbox simulation) before a single input is sent, so a wrong chain
// entry degrades to the generic scorer instead of causing blind inputs.
//
// Signature finishers use approximate range gates (the game-data signature
// entries carry no per-frame hitboxes) and are disabled by default, keeping
// the project's fail-closed philosophy.
#pragma once

#include "GameDataManager.h"
#include "OffenseController.h"
#include "attack_table.h"

#include <cstddef>
#include <vector>

namespace xpe {

inline constexpr int kYumikoLegendId = 36;
inline constexpr int kWeaponHammer = 4;
inline constexpr int kWeaponBow = 10;

// Attack-table power IDs used by the Yumiko chains.
enum : int {
    // Hammer
    kHammerNL = 118,          // nLight (auto-chain 119 -> 109 handled by the game)
    kHammerSL = 142,          // sLight
    kHammerDL = 145,          // dLight (146 internal hit)
    kHammerNair = 126,
    kHammerSair = 129,
    kHammerDair = 132,
    // Bow
    kBowNL = 327,             // nLight (328 internal hit)
    kBowSL = 318,
    kBowDL = 321,             // dLight (325 internal hit)
    kBowNair = 330,
    kBowSair = 333,
    kBowDair = 336,
    // Yumiko signatures (not present in the static C++ tables; data/game_data only)
    kSigBowNeutral = 1283,
    kSigBowSide = 1286,
    kSigBowDown = 1289,
    kSigHammerDown = 1296,
    kSigHammerNeutral = 1303,
    kSigHammerSide = 1321,
};

struct YumikoStep {
    int powerId = -1;
    float maxEnemyDamage = 999.0f;  // upper damage where the follow-up stays true
    bool requireYouAirborne = false;
};

struct YumikoChain {
    const char* name;
    int weaponTypeId;          // 4 hammer, 10 bow
    int starterId;
    YumikoStep steps[3];
    int stepCount = 0;
};

// True-string library. Windows assume the starter connected; every step is
// still re-validated against live positions before execution.
inline const YumikoChain* YumikoChains(std::size_t& count) {
    static const YumikoChain kChains[] = {
        // ---------------- Hammer ----------------
        {"dLight > dLight",             kWeaponHammer, kHammerDL, {{kHammerDL,  60.0f, false}}, 1},
        {"dLight > dLight > dair",      kWeaponHammer, kHammerDL, {{kHammerDL,  55.0f, false}, {kHammerDair, 85.0f, false}}, 2},
        {"dLight > sair",               kWeaponHammer, kHammerDL, {{kHammerSair, 100.0f, true}}, 1},
        {"dLight > nair",               kWeaponHammer, kHammerDL, {{kHammerNair,  75.0f, false}}, 1},
        {"sair > sair",                 kWeaponHammer, kHammerSair, {{kHammerSair,  60.0f, true}}, 1},
        {"sair > dair",                 kWeaponHammer, kHammerSair, {{kHammerDair, 110.0f, true}}, 1},
        {"sLight > dLight",             kWeaponHammer, kHammerSL, {{kHammerDL,  50.0f, false}}, 1},
        // ---------------- Bow ----------------
        {"dLight > sair",               kWeaponBow, kBowDL, {{kBowSair,  80.0f, true}}, 1},
        {"dLight > dair",               kWeaponBow, kBowDL, {{kBowDair,  60.0f, true}}, 1},
        {"dLight > nair",               kWeaponBow, kBowDL, {{kBowNair,  70.0f, false}}, 1},
        {"sair > sair",                 kWeaponBow, kBowSair, {{kBowSair,  50.0f, true}}, 1},
        {"sair > dair",                 kWeaponBow, kBowSair, {{kBowDair,  90.0f, true}}, 1},
        {"dLight > dLight",             kWeaponBow, kBowDL, {{kBowDL,  40.0f, false}}, 1},
    };
    count = sizeof(kChains) / sizeof(kChains[0]);
    return kChains;
}

class YumikoBot {
public:
    void Reset() {
        chainIndex_ = -1;
        stepIndex_ = 0;
        expiresMs_ = 0;
    }

    const char* ActiveChainName() const {
        if (chainIndex_ < 0) return "";
        std::size_t n = 0;
        const YumikoChain* chains = YumikoChains(n);
        return chains[chainIndex_].name;
    }

    // Called after the bot actually fired a move via SendInput.
    // Advances the active chain, or opens a chain when the move matches a starter.
    void OnExecuted(int powerId, long long nowMs) {
        std::size_t n = 0;
        const YumikoChain* chains = YumikoChains(n);
        if (chainIndex_ >= 0) {
            const YumikoChain& chain = chains[chainIndex_];
            if (stepIndex_ < static_cast<std::size_t>(chain.stepCount) &&
                chain.steps[stepIndex_].powerId == powerId) {
                ++stepIndex_;
                expiresMs_ = nowMs + kStepWindowMs;
                return;
            }
            Reset();  // executed something outside the chain: restart matching below
        }
        for (std::size_t i = 0; i < n; ++i) {
            if (chains[i].starterId == powerId) {
                chainIndex_ = static_cast<int>(i);
                stepIndex_ = 0;
                expiresMs_ = nowMs + kStepWindowMs;
                return;
            }
        }
    }

    // Returns the power ID the offense loop should force next, or -1.
    // All returned light moves exist in the static attack table, so the generic
    // PickAttack(forced) path validates them before input.
    int PickForced(const Actor& you, const Actor& eny, long long nowMs) const {
        if (you.legend_id != kYumikoLegendId) return -1;
        const int weapon = you.weapon_type_id;
        if (weapon != kWeaponHammer && weapon != kWeaponBow) return -1;

        if (chainIndex_ >= 0 && nowMs < expiresMs_) {
            std::size_t n = 0;
            const YumikoChain* chains = YumikoChains(n);
            const YumikoChain& chain = chains[chainIndex_];
            if (chain.weaponTypeId != weapon) return -1;
            if (stepIndex_ < static_cast<std::size_t>(chain.stepCount)) {
                const YumikoStep& step = chain.steps[stepIndex_];
                if (eny.damage > step.maxEnemyDamage) return -1;  // string would stop being true
                if (step.requireYouAirborne && you.airborne == 0) return -1;
                if (!step.requireYouAirborne && you.airborne != 0) return -1;
                return step.powerId;
            }
            return -1;  // chain finished
        }
        return -1;
    }

    // Approximate signature finisher. Disabled by default because the game-data
    // signatures carry no frame hitboxes; the gates below are heuristic boxes
    // around the documented Yumiko sig reach, not confirmed hitbox data.
    int PickSignature(const Actor& you, const Actor& eny, bool useSignatures,
                      int killDamage) const {
        if (!useSignatures || you.legend_id != kYumikoLegendId) return -1;
        if (eny.damage < killDamage) return -1;
        if (you.is_attacking) return -1;
        if (you.airborne != 0) return -1;  // ground sigs only (Yumiko sig set is grounded)
        if (!eny.valid_position) return -1;

        const bool youFacingRight = (you.facing == 0);
        const double dx = (eny.pos_x - you.pos_x) * (youFacingRight ? 1.0 : -1.0);
        const double dy = eny.pos_y - you.pos_y;

        if (you.weapon_type_id == kWeaponHammer) {
            if (eny.airborne == 0) {
                // Hammer side sig: wide horizontal swipe.
                if (dx > 30.0 && dx < 190.0 && dy > -80.0 && dy < 80.0) return kSigHammerSide;
            } else if (dx > -60.0 && dx < 160.0 && dy > -160.0 && dy < -20.0) {
                return kSigHammerNeutral;  // anti-air overhead
            }
        } else if (you.weapon_type_id == kWeaponBow) {
            if (eny.airborne == 0) {
                if (dx > 40.0 && dx < 210.0 && dy > -70.0 && dy < 70.0) return kSigBowSide;
            }
        }
        return -1;
    }

private:
    static constexpr long long kStepWindowMs = 700;  // hitstun + input travel margin
    int chainIndex_ = -1;
    std::size_t stepIndex_ = 0;
    long long expiresMs_ = 0;
};

// Input translation for signature finishers. Mirrors OffenseController::Execute
// but for sig power IDs that are not in the static move table.
inline void ExecuteYumikoSignature(InputController& input, int powerId, bool towardRight) {
    bool isSide = false;
    bool isDown = false;
    switch (powerId) {
        case kSigBowSide:
        case kSigHammerSide: isSide = true; break;
        case kSigBowDown:
        case kSigHammerDown: isDown = true; break;
        default: break;  // neutral sigs need no direction key
    }
    if (isSide) input.Down(towardRight ? VK_RIGHT : VK_LEFT);
    else if (isDown) input.Down(VK_DOWN);
    input.Press('K');  // signatures are heavy inputs
    if (isSide) input.Release(towardRight ? VK_RIGHT : VK_LEFT);
    else if (isDown) input.Release(VK_DOWN);
}

}  // namespace xpe
