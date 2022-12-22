// -*-c++-*-

/*
 *Copyright:

 Copyright (C) Hidehisa AKIYAMA

 This code is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; either version 3, or (at your option)
 any later version.

 This code is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this code; see the file COPYING.  If not, write to
 the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.

 *EndCopyright:
 */

// Student Soccer 2D Simulation Base , STDAGENT2D
// Simplified the Agent2D Base for HighSchool Students.
// Technical Committee of Soccer 2D Simulation League, IranOpen
// Nader Zare
// Mostafa Sayahi
// Pooria Kaviani
/////////////////////////////////////////////////////////////////////

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "bhv_basic_move.h"

#include "bhv_basic_tackle.h"

#include <rcsc/action/basic_actions.h>
#include <rcsc/action/body_go_to_point.h>
#include <rcsc/action/body_intercept.h>

#include <rcsc/player/debug_client.h>
#include <rcsc/player/intercept_table.h>
#include <rcsc/player/player_agent.h>

#include <rcsc/common/logger.h>
#include <rcsc/common/server_param.h>

#include "formation.h"

#include <cstdio>
#include <rcsc/action/arm_point_to_point.h>
#include <rcsc/player/say_message_builder.h>
#include <vector>

#include "body_mark.h"

#include <rcsc/geom/delaunay_triangulation.h>

using namespace rcsc;

/*-------------------------------------------------------------------*/
/*!

 */
bool Bhv_BasicMove::execute(PlayerAgent *agent)
{
    const WorldModel &wm = agent->world();

    dlog.addText(Logger::TEAM, __FILE__ ": Bhv_BasicMove");

    /*--------------------------------------------------------*/
    // chase ball
    const int self_min = wm.interceptTable()->selfReachCycle();
    const int mate_min = wm.interceptTable()->teammateReachCycle();
    const int opp_min = wm.interceptTable()->opponentReachCycle();

    if (!wm.existKickableTeammate() && (self_min <= 3 || (self_min <= mate_min)))
    {
        dlog.addText(Logger::TEAM, __FILE__ ": intercept");
        Body_Intercept().execute(agent);

        return true;
    }

    //-----------------------------------------------
    // tackle
    if (Bhv_BasicTackle(0.8, 80.0).execute(agent))
    {
        return true;
    }

    const Vector2D target_point = getPosition(wm, wm.self().unum());
    const double dash_power = get_normal_dash_power(wm);

    double dist_thr = wm.ball().distFromSelf() * 0.1;
    if (dist_thr < 1.0)
        dist_thr = 1.0;

    dlog.addText(Logger::TEAM, __FILE__ ": Bhv_BasicMove target=(%.1f %.1f) dist_thr=%.2f", target_point.x,
                 target_point.y, dist_thr);

    agent->debugClient().addMessage("BasicMove%.0f", dash_power);
    agent->debugClient().setTarget(target_point);
    agent->debugClient().addCircle(target_point, dist_thr);

    if (!Body_GoToPoint(target_point, dist_thr, dash_power).execute(agent))
    {
        Body_TurnToBall().execute(agent);
    }

    return true;
}

rcsc::Vector2D Bhv_BasicMove::getPosition(const rcsc::WorldModel &wm, int self_unum)
{
    return Formation().getPosition(wm, self_unum);
}

double Bhv_BasicMove::get_normal_dash_power(const WorldModel &wm)
{
    static bool s_recover_mode = false;

    // role
    int role = wm.self().unum();

    if (wm.self().staminaModel().capacityIsEmpty())
    {
        return std::min(ServerParam::i().maxDashPower(), wm.self().stamina() + wm.self().playerType().extraStamina());
    }

    const int self_min = wm.interceptTable()->selfReachCycle();
    const int mate_min = wm.interceptTable()->teammateReachCycle();
    const int opp_min = wm.interceptTable()->opponentReachCycle();

    // check recover
    if (wm.self().staminaModel().capacityIsEmpty())
    {
        s_recover_mode = false;
    }
    else if (wm.self().stamina() < ServerParam::i().staminaMax() * 0.5)
    {
        s_recover_mode = true;
    }
    else if (wm.self().stamina() > ServerParam::i().staminaMax() * 0.7)
    {
        s_recover_mode = false;
    }

    /*--------------------------------------------------------*/
    double dash_power = ServerParam::i().maxDashPower();
    const double my_inc = wm.self().playerType().staminaIncMax() * wm.self().recovery();

    if (wm.ourDefenseLineX() > wm.self().pos().x && wm.ball().pos().x < wm.ourDefenseLineX() + 20.0)
    {
        dlog.addText(Logger::TEAM, __FILE__ ": (get_normal_dash_power) correct DF "
                                            "line. keep max power");
        // keep max power
        dash_power = ServerParam::i().maxDashPower();
    }
    else if (s_recover_mode)
    {
        dash_power = my_inc - 25.0; // preffered recover value
        if (dash_power < 0.0)
            dash_power = 0.0;

        dlog.addText(Logger::TEAM, __FILE__ ": (get_normal_dash_power) recovering");
    }

    // run to offside line
    else if (wm.ball().pos().x > 0.0 && wm.self().pos().x < wm.offsideLineX() &&
             fabs(wm.ball().pos().x - wm.self().pos().x) < 25.0)
        dash_power = ServerParam::i().maxDashPower();

    // defenders
    else if (wm.ball().pos().x < 10.0 && (role == 4 || role == 5 || role == 2 || role == 3))
        dash_power = ServerParam::i().maxDashPower();

    // midfielders
    else if (wm.ball().pos().x < -10.0 && (role == 6 || role == 7 || role == 8))
        dash_power = ServerParam::i().maxDashPower();

    // run in opp penalty area
    else if (wm.ball().pos().x > 36.0 && wm.self().pos().x > 36.0 && mate_min < opp_min - 4)
        dash_power = ServerParam::i().maxDashPower();

    // exist kickable teammate
    else if (wm.existKickableTeammate() && wm.ball().distFromSelf() < 20.0)
    {
        dash_power = std::min(my_inc * 1.1, ServerParam::i().maxDashPower());
        dlog.addText(Logger::TEAM,
                     __FILE__ ": (get_normal_dash_power) exist kickable "
                              "teammate. dash_power=%.1f",
                     dash_power);
    }
    // in offside area
    else if (wm.self().pos().x > wm.offsideLineX())
    {
        dash_power = ServerParam::i().maxDashPower();
        dlog.addText(Logger::TEAM, __FILE__ ": in offside area. dash_power=%.1f", dash_power);
    }
    else if (wm.ball().pos().x > 25.0 && wm.ball().pos().x > wm.self().pos().x + 10.0 && self_min < opp_min - 6 &&
             mate_min < opp_min - 6)
    {
        dash_power = bound(ServerParam::i().maxDashPower() * 0.1, my_inc * 0.5, ServerParam::i().maxDashPower());
        dlog.addText(Logger::TEAM, __FILE__ ": (get_normal_dash_power) opponent ball dash_power=%.1f", dash_power);
    }
    // normal
    else
    {
        dash_power = std::min(my_inc * 1.7, ServerParam::i().maxDashPower());
        dlog.addText(Logger::TEAM, __FILE__ ": (get_normal_dash_power) normal mode dash_power=%.1f", dash_power);
    }

    return dash_power;
}
