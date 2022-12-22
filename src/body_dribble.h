//
// Created by alireza on Wed 14 Sep 2022
//

#ifndef STARTER2D_BODY_DRIBBLE_H
#define STARTER2D_BODY_DRIBBLE_H

#include <rcsc/action/body_go_to_point.h>
#include <rcsc/action/body_intercept.h>
#include <rcsc/geom.h>
#include <rcsc/player/player_agent.h>
#include <rcsc/player/soccer_action.h>
#include <rcsc/player/soccer_intention.h>

struct DribbleInfo
{
    rcsc::Sector2D path_sector;
    double mean_angle;
    double score;
};

class Body_Dribble : public rcsc::SoccerBehavior
{
  public:
    Body_Dribble()
    {
    }

    bool execute(rcsc::PlayerAgent *agent);
    bool verify_dribble(const rcsc::WorldModel &wm, DribbleInfo target);
    double score_dribble(const rcsc::WorldModel &wm, DribbleInfo target);
};

#endif // STARTER2D_BODY_DRIBBLE_H
