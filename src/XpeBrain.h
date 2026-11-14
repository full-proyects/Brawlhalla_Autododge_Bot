// XPE Bot — decision brain (playstyles, target selection, team awareness)
// Copyright (c) 2026 xpe.nett. MIT License.
//
// Sits on top of the dodge layer and the Yumiko offense module:
//  - picks the engagement target out of every valid actor (nearest, alive,
//    non-local), so 2v2 works instead of always fighting actor #0;
//  - excludes teammates when a verified team id offset is configured
//    (config.xpe_team_offset). Fail-closed: with no offset the brain falls
//    back to "only engage whoever is attacking us" in doubles;
//  - gates offense by playstyle: Passive only punishes (enemy stunned or an
//    open chain), Balanced keeps the generic hit-window behavior, Aggressive
//    adds approach movement and lowers the signature kill threshold.
#pragma once

#include "GameDataManager.h"

#include <cmath>
#include <string>
#include <vector>

namespace xpe {

enum class Playstyle { Passive, Balanced, Aggressive };
enum class TeamMode { Auto, Solo, Doubles };

inline Playstyle ParsePlaystyle(const std::string& value) {
    if (value == "passive") return Playstyle::Passive;
    if (value == "aggressive") return Playstyle::Aggressive;
    return Playstyle::Balanced;
}

inline const char* PlaystyleName(Playstyle style) {
    switch (style) {
        case Playstyle::Passive: return "passive";
        case Playstyle::Aggressive: return "aggressive";
        default: return "balanced";
    }
}

inline TeamMode ParseTeamMode(const std::string& value) {
    if (value == "1v1") return TeamMode::Solo;
    if (value == "2v2") return TeamMode::Doubles;
    return TeamMode::Auto;
}

inline const char* TeamModeName(TeamMode mode) {
    switch (mode) {
        case TeamMode::Solo: return "1v1";
        case TeamMode::Doubles: return "2v2";
        default: return "auto";
    }
}

struct TargetSelection {
    const Actor* target = nullptr;      // engagement target (never the local player)
    int validOpponents = 0;             // valid, alive, non-local actors before filtering
    int opponentsAfterFilter = 0;       // candidates left after teammate filtering
    TeamMode detected = TeamMode::Solo; // resolved mode for this frame
    bool teammateFilterApplied = false; // team ids were usable this frame
};

class XpeBrain {
public:
    // Resolve the current mode. Auto counts live non-local actors:
    // 0-1 opponents -> Solo, otherwise Doubles.
    static TeamMode ResolveMode(TeamMode configured, int validOpponents) {
        if (configured != TeamMode::Auto) return configured;
        return validOpponents <= 1 ? TeamMode::Solo : TeamMode::Doubles;
    }

    // A team id is only trusted when it reads as a small non-negative slot id.
    static bool UsableTeamId(int teamId) {
        return teamId >= 0 && teamId <= 3;
    }

    // Pick the engagement target for this frame. `localTeamId` < 0 means team
    // info is unavailable and no teammate can be excluded.
    static TargetSelection SelectTarget(const std::vector<Actor>& actors,
                                        const Actor& local, TeamMode configured) {
        TargetSelection out;
        double bestDist = 1e18;
        double bestAttackerDist = 1e18;
        const Actor* nearest = nullptr;
        const Actor* nearestAttacker = nullptr;
        bool anyUsableTeamIds = local.team_id >= 0;

        for (const Actor& a : actors) {
            if (a.dp == local.dp || !a.valid || a.is_dead) continue;
            if (!a.valid_position) continue;
            ++out.validOpponents;
            const bool isTeammate = anyUsableTeamIds && UsableTeamId(local.team_id) &&
                                    UsableTeamId(a.team_id) && a.team_id == local.team_id;
            if (isTeammate) continue;
            ++out.opponentsAfterFilter;
            const double dist = std::hypot(a.pos_x - local.pos_x, a.pos_y - local.pos_y);
            if (dist < bestDist) {
                bestDist = dist;
                nearest = &a;
            }
            if (a.is_attacking && dist < bestAttackerDist) {
                bestAttackerDist = dist;
                nearestAttacker = &a;
            }
        }

        out.detected = ResolveMode(configured, out.validOpponents);
        out.teammateFilterApplied = anyUsableTeamIds;

        if (out.detected == TeamMode::Doubles && !out.teammateFilterApplied) {
            // Fail-closed doubles: without team ids the only actor we can be
            // sure about is whoever is attacking us right now.
            out.target = nearestAttacker;
            return out;
        }
        out.target = nearest;
        return out;
    }

    // Should the offense layer open an action this frame?
    //  - forcedOpen: an active chain/queued follow-up (always allowed)
    //  - enemyStunned: free punish window
    static bool OffenseAllowed(Playstyle style, bool forcedOpen, bool enemyStunned,
                               bool enemyAttacking) {
        switch (style) {
            case Playstyle::Passive:
                return forcedOpen || enemyStunned;
            case Playstyle::Aggressive:
                (void)enemyAttacking;
                return true;
            case Playstyle::Balanced:
            default:
                return true;
        }
    }

    // Signature threshold multiplier per playstyle: aggressive finishes kills
    // earlier, passive waits for a bigger window.
    static float SigThresholdMultiplier(Playstyle style) {
        switch (style) {
            case Playstyle::Passive: return 1.15f;
            case Playstyle::Aggressive: return 0.80f;
            default: return 1.0f;
        }
    }

    // Aggressive approach: tap toward the target when nothing better to do.
    // Returns the key to press, or 0 when no approach is wanted.
    static WORD ApproachKey(const Actor& local, const Actor& target,
                            Playstyle style, double minRangePx, long long nowMs) {
        if (style != Playstyle::Aggressive) return 0;
        if (nowMs - lastApproachMs_ < kApproachCooldownMs) return 0;
        if (local.is_stunned || local.is_attacking || local.airborne) return 0;
        const double dx = target.pos_x - local.pos_x;
        const double dy = target.pos_y - local.pos_y;
        const double dist = std::hypot(dx, dy);
        if (dist <= minRangePx || dist > 1600.0) return 0;
        lastApproachMs_ = nowMs;
        return dx > 0 ? VK_RIGHT : VK_LEFT;
    }

    static void Reset() { lastApproachMs_ = -kApproachCooldownMs; }

private:
    static constexpr long long kApproachCooldownMs = 220;
    static inline long long lastApproachMs_ = -kApproachCooldownMs;
};

}  // namespace xpe
