// XPE Bot — Legend modules: Mordex, Nix, Artemis
// Copyright (c) 2026 xpe.nett. MIT License.
//
// Legend-specific offense layers for three additional legends, following the
// same fail-closed philosophy as XpeYumikoBot.h:
//  - every follow-up proposed here is re-validated by
//    OffenseController::PickAttack (frame-hitbox simulation) before a single
//    input is sent, so a wrong chain entry degrades to the generic scorer;
//  - signature finishers use approximate range gates (the game-data signature
//    entries carry no per-frame hitboxes) and are disabled by default;
//  - chain tables only fire when the local actor's legend id and weapon match.
//
// Data provenance (data/game_data):
//  - base move power IDs: attack_data/{scythe,fists,lance,pistol}_data.json
//  - signature power IDs: attack_data/legend_sigs/{mordex,nix,artemis}_sigs.json
//  - legend ids/weapons: legends.json (Mordex=35 "Werewolf", Nix=34 "Reaper",
//    Artemis=37 "Spacehunter")
//
// Weapon type ids (GameDataManager / OffenseController::WeaponPrefix):
//  Lance=5, Pistol(Blasters)=6, Fists(Gauntlets)=11, Scythe=12.
#pragma once

#include "GameDataManager.h"
#include "InputController.h"

#include <cstddef>

namespace xpe {

inline constexpr int kMordexLegendId = 35;   // internal "Werewolf"
inline constexpr int kNixLegendId = 34;      // internal "Reaper"
inline constexpr int kArtemisLegendId = 37;  // internal "Spacehunter"

inline constexpr int kWeaponLance = 5;
inline constexpr int kWeaponPistol = 6;   // Blasters
inline constexpr int kWeaponFists = 11;   // Gauntlets
inline constexpr int kWeaponScythe = 12;

// Attack-table power IDs used by the Mordex/Nix/Artemis chains.
enum : int {
    // ---------------- Scythe ----------------
    kScytheNL = 1251,      // nLight (1252 internal hit)
    kScytheSL = 1142,
    kScytheDL = 1146,      // dLight (1149 internal hit)
    kScytheNair = 1152,
    kScytheSair = 1159,
    kScytheDair = 1162,
    // ---------------- Fists (Gauntlets) ----------------
    kFistsNL = 364,        // nLight (366 internal hit)
    kFistsSL = 370,
    kFistsDL = 373,        // dLight (375 internal hit)
    kFistsNair = 376,
    kFistsSair = 380,
    kFistsDair = 383,
    // ---------------- Rocket Lance ----------------
    kLanceNL = 72,         // nLight (73 combo -> 74 hit handled by the game)
    kLanceSL = 90,
    kLanceDL = 97,
    kLanceNair = 76,
    kLanceSair = 102,
    kLanceDair = 79,
    // ---------------- Pistol (Blasters) ----------------
    kPistolNL = 166,       // nLight (167 combo -> 111 hit handled by the game)
    kPistolSL = 192,
    kPistolDL = 200,       // dLight (203 internal hit)
    kPistolNair = 207,
    kPistolSair = 179,
    kPistolDair = 3094,

    // Mordex signatures (data/game_data only, not in the static C++ tables)
    kSigMordexScytheNeutral = 2042,
    kSigMordexScytheSide = 2016,
    kSigMordexScytheDown = 2116,
    kSigMordexFistsNeutral = 2058,
    kSigMordexFistsSide = 2051,
    kSigMordexFistsDown = 2028,
    // Nix signatures
    kSigNixScytheNeutral = 1228,
    kSigNixScytheSide = 1233,
    kSigNixScytheDown = 1238,
    kSigNixPistolNeutral = 1240,
    kSigNixPistolSide = 1245,
    kSigNixPistolDown = 1248,
    // Artemis signatures
    kSigArtemisLanceNeutral = 1331,
    kSigArtemisLanceSide = 1336,
    kSigArtemisLanceDown = 1340,
    kSigArtemisScytheNeutral = 1226,
    kSigArtemisScytheSide = 1276,
    kSigArtemisScytheDown = 1324,
};

struct XpeStep {
    int powerId = -1;
    float maxEnemyDamage = 999.0f;  // upper damage where the follow-up stays true
    bool requireYouAirborne = false;
};

struct XpeChain {
    const char* name;
    int weaponTypeId;
    int starterId;
    XpeStep steps[3];
    int stepCount = 0;
};

// ---------------- Scythe (Mordex + Nix share the weapon moveset) ----------------
inline const XpeChain* ScytheChains(std::size_t& count) {
    static const XpeChain kChains[] = {
        {"dLight > dLight",  kWeaponScythe, kScytheDL, {{kScytheDL,   60.0f, false}}, 1},
        {"dLight > nair",    kWeaponScythe, kScytheDL, {{kScytheNair, 70.0f, false}}, 1},
        {"dLight > sair",    kWeaponScythe, kScytheDL, {{kScytheSair, 80.0f, true}},  1},
        {"dLight > dair",    kWeaponScythe, kScytheDL, {{kScytheDair, 60.0f, true}},  1},
        {"sLight > dair",    kWeaponScythe, kScytheSL, {{kScytheDair, 55.0f, true}},  1},
        {"nair > dair",      kWeaponScythe, kScytheNair, {{kScytheDair, 70.0f, true}}, 1},
        {"sair > dair",      kWeaponScythe, kScytheSair, {{kScytheDair, 50.0f, true}}, 1},
    };
    count = sizeof(kChains) / sizeof(kChains[0]);
    return kChains;
}

// ---------------- Fists / Gauntlets (Mordex) ----------------
inline const XpeChain* FistsChains(std::size_t& count) {
    static const XpeChain kChains[] = {
        {"dLight > dLight",  kWeaponFists, kFistsDL, {{kFistsDL,   55.0f, false}}, 1},
        {"dLight > sair",    kWeaponFists, kFistsDL, {{kFistsSair, 70.0f, true}},  1},
        {"dLight > nair",    kWeaponFists, kFistsDL, {{kFistsNair, 60.0f, false}}, 1},
        {"sLight > dLight",  kWeaponFists, kFistsSL, {{kFistsDL,   50.0f, false}}, 1},
        {"nair > dLight",    kWeaponFists, kFistsNair, {{kFistsDL, 60.0f, true}},   1},
        {"sair > dair",      kWeaponFists, kFistsSair, {{kFistsDair, 55.0f, true}}, 1},
    };
    count = sizeof(kChains) / sizeof(kChains[0]);
    return kChains;
}

// ---------------- Rocket Lance (Artemis) ----------------
inline const XpeChain* LanceChains(std::size_t& count) {
    static const XpeChain kChains[] = {
        {"dLight > nair",    kWeaponLance, kLanceDL,   {{kLanceNair, 70.0f, false}}, 1},
        {"nair > nair",      kWeaponLance, kLanceNair, {{kLanceNair, 50.0f, true}},  1},
        {"sair > sair",      kWeaponLance, kLanceSair, {{kLanceSair, 55.0f, true}},  1},
        {"dair > nair",      kWeaponLance, kLanceDair, {{kLanceNair, 65.0f, true}},  1},
    };
    count = sizeof(kChains) / sizeof(kChains[0]);
    return kChains;
}

// ---------------- Pistol / Blasters (Nix) ----------------
inline const XpeChain* PistolChains(std::size_t& count) {
    static const XpeChain kChains[] = {
        {"dLight > dLight > dLight", kWeaponPistol, kPistolDL,
            {{kPistolDL, 55.0f, false}, {kPistolDL, 70.0f, false}}, 2},
        {"dLight > sair",    kWeaponPistol, kPistolDL, {{kPistolSair, 70.0f, true}},  1},
        {"dLight > nair",    kWeaponPistol, kPistolDL, {{kPistolNair, 50.0f, false}}, 1},
        {"sair > dair",      kWeaponPistol, kPistolSair, {{kPistolDair, 60.0f, true}}, 1},
    };
    count = sizeof(kChains) / sizeof(kChains[0]);
    return kChains;
}

// A legend may own several weapons; each weapon contributes one chain set.
struct XpeChainSet {
    using ChainFn = const XpeChain* (*)(std::size_t&);
    const XpeChain* chains = nullptr;
    std::size_t count = 0;
    explicit XpeChainSet(ChainFn fn) { chains = fn(count); }
};

// Signature descriptor: one per (weapon, direction) for each legend. Range
// gates are heuristic boxes around the weapon's documented sig reach, not
// confirmed hitbox data (game-data signatures carry no per-frame hitboxes).
struct XpeLegendSig {
    int weaponTypeId;
    int powerId;
    int direction;       // 0 neutral, 1 side, 2 down
    float sideMinX;      // facing-normalized reach box for the side sig
    float sideMaxX;
};

inline const XpeLegendSig* MordexSigs(std::size_t& count) {
    static const XpeLegendSig kSigs[] = {
        {kWeaponScythe, kSigMordexScytheNeutral, 0, 30.0f, 200.0f},
        {kWeaponScythe, kSigMordexScytheSide,    1, 30.0f, 200.0f},
        {kWeaponScythe, kSigMordexScytheDown,    2, 30.0f, 200.0f},
        {kWeaponFists,  kSigMordexFistsNeutral,  0, 20.0f, 140.0f},
        {kWeaponFists,  kSigMordexFistsSide,     1, 20.0f, 140.0f},
        {kWeaponFists,  kSigMordexFistsDown,     2, 20.0f, 140.0f},
    };
    count = sizeof(kSigs) / sizeof(kSigs[0]);
    return kSigs;
}

inline const XpeLegendSig* NixSigs(std::size_t& count) {
    static const XpeLegendSig kSigs[] = {
        {kWeaponScythe, kSigNixScytheNeutral,  0, 30.0f, 200.0f},
        {kWeaponScythe, kSigNixScytheSide,     1, 30.0f, 200.0f},
        {kWeaponScythe, kSigNixScytheDown,     2, 30.0f, 200.0f},
        {kWeaponPistol, kSigNixPistolNeutral,  0, 40.0f, 230.0f},
        {kWeaponPistol, kSigNixPistolSide,     1, 40.0f, 230.0f},
        {kWeaponPistol, kSigNixPistolDown,     2, 40.0f, 230.0f},
    };
    count = sizeof(kSigs) / sizeof(kSigs[0]);
    return kSigs;
}

inline const XpeLegendSig* ArtemisSigs(std::size_t& count) {
    static const XpeLegendSig kSigs[] = {
        {kWeaponLance,  kSigArtemisLanceNeutral,  0, 40.0f, 260.0f},
        {kWeaponLance,  kSigArtemisLanceSide,     1, 40.0f, 260.0f},
        {kWeaponLance,  kSigArtemisLanceDown,     2, 40.0f, 260.0f},
        {kWeaponScythe, kSigArtemisScytheNeutral, 0, 30.0f, 200.0f},
        {kWeaponScythe, kSigArtemisScytheSide,    1, 30.0f, 200.0f},
        {kWeaponScythe, kSigArtemisScytheDown,    2, 30.0f, 200.0f},
    };
    count = sizeof(kSigs) / sizeof(kSigs[0]);
    return kSigs;
}

// Generic legend chain engine. Same state machine as YumikoBot, parameterized
// by legend id and one chain set per weapon.
class XpeChainBot {
public:
    XpeChainBot(int legendId, const XpeChainSet* sets, std::size_t setCount)
        : legendId_(legendId), sets_(sets), setCount_(setCount) {}

    void Reset() {
        chainIndex_ = -1;
        setIndex_ = 0;
        stepIndex_ = 0;
        expiresMs_ = 0;
    }

    const char* ActiveChainName() const {
        if (chainIndex_ < 0) return "";
        return sets_[setIndex_].chains[chainIndex_].name;
    }

    // A chain step is usable when the string would still be true and the
    // required state (grounded/airborne) matches.
    static bool StepValid(const Actor& you, const Actor& eny, const XpeStep& step) {
        if (eny.damage > step.maxEnemyDamage) return false;
        if (step.requireYouAirborne && you.airborne == 0) return false;
        if (!step.requireYouAirborne && you.airborne != 0) return false;
        return true;
    }

    // Called after the bot actually fired a move via SendInput.
    // Advances the active chain, or opens a chain when the move matches a
    // starter. State-aware chain opening: among chains sharing the starter,
    // prefers one whose first step is currently valid.
    void OnExecuted(const Actor& you, const Actor& eny, int powerId, long long nowMs) {
        if (chainIndex_ >= 0) {
            const XpeChainSet& set = sets_[setIndex_];
            const XpeChain& chain = set.chains[chainIndex_];
            if (stepIndex_ < static_cast<std::size_t>(chain.stepCount) &&
                chain.steps[stepIndex_].powerId == powerId) {
                ++stepIndex_;
                expiresMs_ = nowMs + kStepWindowMs;
                return;
            }
            Reset();  // executed something outside the chain: restart matching below
        }
        int fallbackSet = -1, fallbackChain = -1;
        for (std::size_t s = 0; s < setCount_; ++s) {
            const XpeChainSet& set = sets_[s];
            for (std::size_t i = 0; i < set.count; ++i) {
                const XpeChain& chain = set.chains[i];
                if (chain.starterId != powerId) continue;
                if (fallbackChain < 0) {
                    fallbackSet = static_cast<int>(s);
                    fallbackChain = static_cast<int>(i);
                }
                if (chain.stepCount > 0 && StepValid(you, eny, chain.steps[0])) {
                    setIndex_ = static_cast<int>(s);
                    chainIndex_ = static_cast<int>(i);
                    stepIndex_ = 0;
                    expiresMs_ = nowMs + kStepWindowMs;
                    return;
                }
            }
        }
        if (fallbackChain >= 0) {
            setIndex_ = fallbackSet;
            chainIndex_ = fallbackChain;
            stepIndex_ = 0;
            expiresMs_ = nowMs + kStepWindowMs;
        }
    }

    // Returns the power ID the offense loop should force next, or -1.
    // All returned light moves exist in the static attack table, so the generic
    // PickAttack(forced) path validates them before input.
    int PickForced(const Actor& you, const Actor& eny, long long nowMs) const {
        if (you.legend_id != legendId_) return -1;
        if (chainIndex_ < 0 || nowMs >= expiresMs_) return -1;
        const XpeChain& chain = sets_[setIndex_].chains[chainIndex_];
        if (chain.weaponTypeId != you.weapon_type_id) return -1;
        if (stepIndex_ >= static_cast<std::size_t>(chain.stepCount)) return -1;  // chain finished
        const XpeStep& step = chain.steps[stepIndex_];
        if (!StepValid(you, eny, step)) return -1;
        return step.powerId;
    }

private:
    static constexpr long long kStepWindowMs = 700;  // hitstun + input travel margin
    int legendId_ = -1;
    const XpeChainSet* sets_ = nullptr;
    std::size_t setCount_ = 0;
    int setIndex_ = 0;
    int chainIndex_ = -1;
    std::size_t stepIndex_ = 0;
    long long expiresMs_ = 0;
};

// Approximate signature finisher shared by the three legends. Same gates as
// the Yumiko picker: only while grounded, only past the kill threshold
// (checked by the caller). Direction choice: side sig against a grounded
// target in front, neutral sig as anti-air, down sig against a target below.
inline int PickLegendSignature(const Actor& you, const Actor& eny,
                               const XpeLegendSig* sigs, std::size_t sigCount) {
    if (you.is_attacking) return -1;
    if (you.airborne != 0) return -1;  // ground sigs only (all three sig sets are grounded)
    if (!eny.valid_position) return -1;

    const bool youFacingRight = (you.facing == 0);
    const double dx = (eny.pos_x - you.pos_x) * (youFacingRight ? 1.0 : -1.0);
    const double dy = eny.pos_y - you.pos_y;

    for (std::size_t i = 0; i < sigCount; ++i) {
        const XpeLegendSig& sig = sigs[i];
        if (sig.weaponTypeId != you.weapon_type_id) continue;
        if (sig.direction == 1 && eny.airborne == 0) {
            if (dx > sig.sideMinX && dx < sig.sideMaxX && dy > -90.0 && dy < 90.0)
                return sig.powerId;
        } else if (sig.direction == 0 && eny.airborne != 0) {
            // Anti-air overhead: target above and slightly in front.
            if (dx > -70.0 && dx < sig.sideMaxX - 40.0 && dy > -190.0 && dy < -30.0)
                return sig.powerId;
        } else if (sig.direction == 2) {
            // Spike downward: target well below.
            if (dx > -60.0 && dx < 60.0 && dy > 60.0 && dy < 220.0)
                return sig.powerId;
        }
    }
    return -1;
}

// Input translation for signature finishers of the three legends.
// Mirrors OffenseController::Execute / ExecuteYumikoSignature but for sig
// power IDs that are not in the static move table.
inline void ExecuteLegendSignature(InputController& input, int powerId, bool towardRight) {
    bool isSide = false;
    bool isDown = false;
    std::size_t n = 0;
    const XpeLegendSig* tables[] = { MordexSigs(n), NixSigs(n), ArtemisSigs(n) };
    for (const XpeLegendSig* table : tables) {
        for (std::size_t i = 0; i < n; ++i) {
            if (table[i].powerId == powerId) {
                if (table[i].direction == 1) isSide = true;
                else if (table[i].direction == 2) isDown = true;
            }
        }
    }
    if (isSide) input.Down(towardRight ? VK_RIGHT : VK_LEFT);
    else if (isDown) input.Down(VK_DOWN);
    input.Press('K');  // signatures are heavy inputs
    if (isSide) input.Release(towardRight ? VK_RIGHT : VK_LEFT);
    else if (isDown) input.Release(VK_DOWN);
}

// Convenience wrapper bundling one legend's chain engine + signature table.
class XpeLegendBot {
public:
    XpeLegendBot(int legendId, const XpeChainSet* sets, std::size_t setCount,
                 const XpeLegendSig* sigs, std::size_t sigCount)
        : bot_(legendId, sets, setCount), sigs_(sigs), sigCount_(sigCount) {}

    void Reset() { bot_.Reset(); }
    const char* ActiveChainName() const { return bot_.ActiveChainName(); }
    void OnExecuted(const Actor& you, const Actor& eny, int powerId, long long nowMs) {
        bot_.OnExecuted(you, eny, powerId, nowMs);
    }
    int PickForced(const Actor& you, const Actor& eny, long long nowMs) const {
        return bot_.PickForced(you, eny, nowMs);
    }
    int PickSignature(const Actor& you, const Actor& eny) const {
        return PickLegendSignature(you, eny, sigs_, sigCount_);
    }

private:
    XpeChainBot bot_;
    const XpeLegendSig* sigs_ = nullptr;
    std::size_t sigCount_ = 0;
};

namespace detail {

inline XpeLegendBot MakeMordexBot() {
    static const XpeChainSet kSets[] = { XpeChainSet(ScytheChains), XpeChainSet(FistsChains) };
    std::size_t sn = 0;
    const XpeLegendSig* sigs = MordexSigs(sn);
    return XpeLegendBot(kMordexLegendId, kSets, 2, sigs, sn);
}

inline XpeLegendBot MakeNixBot() {
    static const XpeChainSet kSets[] = { XpeChainSet(ScytheChains), XpeChainSet(PistolChains) };
    std::size_t sn = 0;
    const XpeLegendSig* sigs = NixSigs(sn);
    return XpeLegendBot(kNixLegendId, kSets, 2, sigs, sn);
}

inline XpeLegendBot MakeArtemisBot() {
    static const XpeChainSet kSets[] = { XpeChainSet(LanceChains), XpeChainSet(ScytheChains) };
    std::size_t sn = 0;
    const XpeLegendSig* sigs = ArtemisSigs(sn);
    return XpeLegendBot(kArtemisLegendId, kSets, 2, sigs, sn);
}

}  // namespace detail

// Accessors: lazily created per-translation-unit instances (the bot loop runs
// on a single thread, so no synchronization is needed).
inline XpeLegendBot& MordexBot() {
    static XpeLegendBot bot = detail::MakeMordexBot();
    return bot;
}

inline XpeLegendBot& NixBot() {
    static XpeLegendBot bot = detail::MakeNixBot();
    return bot;
}

inline XpeLegendBot& ArtemisBot() {
    static XpeLegendBot bot = detail::MakeArtemisBot();
    return bot;
}

}  // namespace xpe
