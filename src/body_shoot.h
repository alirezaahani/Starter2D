#pragma once

#include "body_shoot_generator.h"

#include <rcsc/game_time.h>
#include <rcsc/geom/line_2d.h>
#include <rcsc/geom/vector_2d.h>
#include <rcsc/player/player_agent.h>
#include <rcsc/player/soccer_action.h>

#include <rcsc/player/soccer_action.h>

class Body_Shoot : public rcsc::SoccerBehavior
{
  private:
    static ShootGenerator generator;

  public:
    Body_Shoot()
    {
    }

    bool execute(rcsc::PlayerAgent *agent);

    static ShootGenerator &generator_instance()
    {
        return generator;
    }
};
