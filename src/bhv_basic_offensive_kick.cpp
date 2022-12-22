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

#include "bhv_basic_offensive_kick.h"

#include <rcsc/action/body_hold_ball.h>
#include <rcsc/action/body_smart_kick.h>
#include <rcsc/action/body_stop_ball.h>
#include <rcsc/player/debug_client.h>
#include <rcsc/player/player_agent.h>

#include <rcsc/common/logger.h>
#include <rcsc/common/server_param.h>

#include <rcsc/geom/sector_2d.h>

#include <vector>

#include "body_dribble.h"
#include "body_pass.h"
#include "body_shoot.h"

#include <rcsc/player/say_message_builder.h>

using namespace rcsc;

/*-------------------------------------------------------------------*/
/*!

 */
bool Bhv_BasicOffensiveKick::execute(PlayerAgent *agent)
{

    dlog.addText(Logger::TEAM, __FILE__ ": Bhv_BasicOffensiveKick");

    const WorldModel &wm = agent->world();

    if (Body_Shoot().execute(agent))
    {
        return true;
    }
    
    if (Body_Pass().forward_pass(agent))
    {
        return true;
    }

    const PlayerPtrCont &opps = wm.opponentsFromSelf();
    const PlayerObject *nearest_opp = (opps.empty() ? nullptr : opps.front());
    const double nearest_opp_dist = (nearest_opp ? nearest_opp->distFromSelf() : 1000.0);
    //    const Vector2D nearest_opp_pos = ( nearest_opp
    //                                       ? nearest_opp->pos()
    //                                       : Vector2D( -1000.0, 0.0
    //                                       ) );
    
    if (Body_Dribble().execute(agent))
    {
        return true;
    }
    
    if (Body_Pass().execute(agent))
    {
        return true;
    }

    Body_HoldBall().execute(agent);

    //    clearball(agent);

    return true;
}

bool Bhv_BasicOffensiveKick::shoot(rcsc::PlayerAgent *agent)
{
    return Body_Shoot().execute(agent);
}

bool Bhv_BasicOffensiveKick::clearball(PlayerAgent *agent)
{

    std::cerr << "Warning: Using clear ball" << std::endl;

    const WorldModel &wm = agent->world();
    if (!wm.self().isKickable())
        return false;
    Vector2D ball_pos = wm.ball().pos();
    Vector2D target = Vector2D(52.5, 0);
    if (ball_pos.x < 0)
    {
        if (ball_pos.x > -25)
        {
            if (ball_pos.dist(Vector2D(0, -34)) < ball_pos.dist(Vector2D(0, +34)))
            {
                target = Vector2D(0, -34);
            }
            else
            {
                target = Vector2D(0, +34);
            }
        }
        else
        {
            if (ball_pos.absY() < 10 && ball_pos.x < -10)
            {
                if (ball_pos.y > 0)
                {
                    target = Vector2D(-52, 20);
                }
                else
                {
                    target = Vector2D(-52, -20);
                }
            }
            else
            {
                if (ball_pos.y > 0)
                {
                    target = Vector2D(ball_pos.x, 34);
                }
                else
                {
                    target = Vector2D(ball_pos.x, -34);
                }
            }
        }
    }
    if (Body_SmartKick(target, 2.7, 2.7, 2).execute(agent))
    {
        return true;
    }
    Body_StopBall().execute(agent);
    return true;
}
