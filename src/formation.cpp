//
// Created by alireza on 7/20/22.
//

#include "formation.h"

#include <rcsc/action/basic_actions.h>
#include <rcsc/action/body_go_to_point.h>
#include <rcsc/action/body_intercept.h>
#include <rcsc/common/server_param.h>
#include <rcsc/player/debug_client.h>
#include <rcsc/player/intercept_table.h>
#include <rcsc/player/player_agent.h>

using namespace rcsc;

static const struct SimpleVec2D normal_position[][11] = {
#include "formations/normal-formation.conf.points"
};

static const struct SimpleVec2D defensive_position[][11] = {
#include "formations/defense-formation.conf.points"
};

static const struct SimpleVec2D offensive_position[][11] = {
#include "formations/offense-formation.conf.points"
};

static const struct SimpleVec2D their_setplay_position[][11] = {
#include "formations/setplay-opp-formation.conf.points"
};

static const struct SimpleVec2D our_setplay_position[][11] = {
#include "formations/setplay-our-formation.conf.points"
};

static const struct SimpleVec2D kick_in_our_position[][11] = {
#include "formations/kickin-our-formation.conf.points"
};

static const struct SimpleVec2D indirect_freekick_opp[][11] = {
#include "formations/indirect-freekick-opp-formation.conf.points"
};

static const struct SimpleVec2D indirect_freekick_our[][11] = {
#include "formations/indirect-freekick-our-formation.conf.points"
};

int Formation::getSegment(rcsc::Vector2D focus_point)
{
    int segment = 0;

    for (int i = -60; i <= 60; i += X_STEP)
    {
        for (int j = -40; j <= 40; j += Y_STEP)
        {
            Rect2D r(Vector2D(i, j), Vector2D(i + X_STEP, j + Y_STEP));
            if (r.contains(focus_point))
            {
                return segment;
            }
            segment++;
        }
    }

    return -1;
}

Formation::Situation Formation::getCurrentSituation(const rcsc::WorldModel &wm)
{
    Situation current_situation = Situation::Normal;

    if (wm.gameMode().type() != GameMode::PlayOn)
    {
        if (wm.gameMode().isPenaltyKickMode())
        {
            current_situation = Situation::Penalty_Kick;
        }
        else if (wm.gameMode().isOurSetPlay(wm.ourSide()))
        {
            current_situation = Situation::OurSetPlay;
        }
        else
        {
            current_situation = Situation::OppSetPlay;
        }

        return current_situation;
    }

    const int self_min = wm.interceptTable()->selfReachCycle();
    const int mate_min = wm.interceptTable()->teammateReachCycle();
    const int opp_min = wm.interceptTable()->opponentReachCycle();
    const int our_min = std::min(self_min, mate_min);

    if (opp_min <= our_min - 3 || wm.existKickableOpponent())
    {
        current_situation = Situation::Defense;
        return current_situation;
    }

    if (our_min <= opp_min - 3 || wm.existKickableTeammate())
    {
        current_situation = Situation::Offense;
        return current_situation;
    }

    return current_situation;
}

const SimpleVec2D *Formation::getCurrentFormation(const rcsc::WorldModel &wm, int focus_segment)
{
    // play on
    if (wm.gameMode().type() == GameMode::PlayOn)
    {
        switch (getCurrentSituation(wm))
        {
        case Situation::Defense:
            return defensive_position[focus_segment];
        case Situation::Offense:
            return offensive_position[focus_segment];
        default:
            return normal_position[focus_segment];
        }
    }

    // kick in, corner kick
    if (wm.gameMode().type() == GameMode::KickIn_ || wm.gameMode().type() == GameMode::CornerKick_)
    {
        if (wm.ourSide() == wm.gameMode().side())
        {
            // our kick-in or corner-kick
            return kick_in_our_position[focus_segment];
        }
        else
        {
            return their_setplay_position[focus_segment];
        }
    }

    // our indirect free kick
    if ((wm.gameMode().type() == GameMode::BackPass_ && wm.gameMode().side() == wm.theirSide()) ||
        (wm.gameMode().type() == GameMode::IndFreeKick_ && wm.gameMode().side() == wm.ourSide()))
    {
        return indirect_freekick_our[focus_segment];
    }

    // opponent indirect free kick
    if ((wm.gameMode().type() == GameMode::BackPass_ && wm.gameMode().side() == wm.ourSide()) ||
        (wm.gameMode().type() == GameMode::IndFreeKick_ && wm.gameMode().side() == wm.theirSide()))
    {
        return indirect_freekick_opp[focus_segment];
    }

    // after foul
    if (wm.gameMode().type() == GameMode::FoulCharge_ || wm.gameMode().type() == GameMode::FoulPush_)
    {
        if (wm.gameMode().side() == wm.ourSide())
        {
            // opponent (indirect) free kick
            if (wm.ball().pos().x < ServerParam::i().ourPenaltyAreaLineX() + 1.0 &&
                wm.ball().pos().absY() < ServerParam::i().penaltyAreaHalfWidth() + 1.0)
            {
                return indirect_freekick_opp[focus_segment];
            }
            else
            {
                return their_setplay_position[focus_segment];
            }
        }
        else
        {
            // our (indirect) free kick
            if (wm.ball().pos().x > ServerParam::i().theirPenaltyAreaLineX() &&
                wm.ball().pos().absY() < ServerParam::i().penaltyAreaHalfWidth())
            {
                return indirect_freekick_our[focus_segment];
            }
            else
            {
                return our_setplay_position[focus_segment];
            }
        }
    }

    // other set play
    if (wm.gameMode().isOurSetPlay(wm.ourSide()))
    {
        return our_setplay_position[focus_segment];
    }

    if (wm.gameMode().type() != GameMode::PlayOn)
    {
        return their_setplay_position[focus_segment];
    }

    // unknown
    switch (getCurrentSituation(wm))
    {
    case Situation::Defense:
        return defensive_position[focus_segment];
    case Situation::Offense:
        return offensive_position[focus_segment];
    default:
        break;
    }

    return normal_position[focus_segment];
}

const rcsc::Vector2D Formation::getPosition(const rcsc::WorldModel &wm, const int unum)
{
    int ball_step = 0;
    if (wm.gameMode().type() == GameMode::PlayOn || wm.gameMode().type() == GameMode::GoalKick_)
    {
        ball_step = std::min(1000, wm.interceptTable()->teammateReachCycle());
        ball_step = std::min(ball_step, wm.interceptTable()->opponentReachCycle());
        ball_step = std::min(ball_step, wm.interceptTable()->selfReachCycle());
    }

    Vector2D ball_pos = wm.ball().inertiaPoint(ball_step);

    int segment = Formation().getSegment(ball_pos);
    // In situations where the ball has been shot or is outside
    // ground, try to position based on current ball position
    if (segment == -1)
    {
        segment = Formation().getSegment(wm.ball().pos());
    }

    const SimpleVec2D *positions = Formation().getCurrentFormation(wm, segment);
    Vector2D target(positions[unum - 1].x, positions[unum - 1].y);

    // Fix Kick Off bug
    if (wm.gameMode().type() == GameMode::KickOff_)
    {
        if (target.x > -1)
        {
            target.x = -1;
        }
    }

    if (ServerParam::i().useOffside())
    {
        double max_x = wm.offsideLineX();
        if (ServerParam::i().kickoffOffside() &&
            (wm.gameMode().type() == GameMode::BeforeKickOff || wm.gameMode().type() == GameMode::AfterGoal_))
        {
            max_x = 0.0;
        }
        else
        {
            int mate_step = wm.interceptTable()->teammateReachCycle();
            if (mate_step < 50)
            {
                Vector2D trap_pos = wm.ball().inertiaPoint(mate_step);
                if (trap_pos.x > max_x)
                    max_x = trap_pos.x;
            }

            max_x -= 1.0;
        }

        if (target.x > max_x)
        {
            target.x = max_x;
        }
    }

    return target;
}
