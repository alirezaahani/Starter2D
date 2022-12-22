//
// Created by alireza on 7/20/22.
//

#ifndef ROBOROYAL_FORMATION_H
#define ROBOROYAL_FORMATION_H

#include <rcsc/geom/vector_2d.h>
#include <rcsc/player/player_agent.h>
#include <rcsc/player/soccer_action.h>

struct SimpleVec2D
{
    float x;
    float y;
};

class Formation
{
  public:
    enum Situation
    {
        Normal,
        Penalty_Kick,
        OurSetPlay,
        OppSetPlay,
        Defense,
        Offense
    };

    int getSegment(rcsc::Vector2D focus_point);
    Situation getCurrentSituation(const rcsc::WorldModel &wm);
    const struct SimpleVec2D *getCurrentFormation(const rcsc::WorldModel &wm, int focus_segment);
    const rcsc::Vector2D getPosition(const rcsc::WorldModel &wm, const int unum);

    const double X_STEP = 2;
    const double Y_STEP = 2;
};

#endif // ROBOROYAL_FORMATION_H
