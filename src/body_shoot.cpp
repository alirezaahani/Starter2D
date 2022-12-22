#include "body_shoot.h"

#include <rcsc/common/logger.h>
#include <rcsc/player/debug_client.h>
#include <rcsc/player/player_agent.h>

#include <rcsc/action/neck_turn_to_goalie_or_scan.h>

#include <rcsc/action/body_smart_kick.h>

using namespace rcsc;

ShootGenerator Body_Shoot::generator;

/*-------------------------------------------------------------------*/
/*!

*/
bool Body_Shoot::execute(PlayerAgent *agent)
{
    const WorldModel &wm = agent->world();

    if (!wm.self().isKickable())
    {
        return false;
    }

    const ShootGenerator::ShotCont &shots = generator.getShots(agent);

    // update
    if (shots.empty())
    {
        return false;
    }

    ShootGenerator::ShotCont::const_iterator shot =
        std::min_element(shots.begin(), shots.end(), ShootGenerator::ScoreCmp());

    if (shot == shots.end())
    {
        return false;
    }

    Vector2D target_point = shot->point_;
    double first_speed = shot->speed_;

    Vector2D one_step_vel =
        KickTable::calc_max_velocity((target_point - wm.ball().pos()).th(), wm.self().kickRate(), wm.ball().vel());
    double one_step_speed = one_step_vel.r();

    if (one_step_speed > first_speed * 0.99)
    {
        if (Body_SmartKick(target_point, one_step_speed, one_step_speed * 0.99 - 0.0001, 1).execute(agent))
        {
            return true;
        }
    }

    if (Body_SmartKick(target_point, first_speed, first_speed * 0.99, 3).execute(agent))
    {
        return true;
    }

    return false;
}
