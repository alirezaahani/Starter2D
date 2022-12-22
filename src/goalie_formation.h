//
// Created by alireza on 8/23/22.
//

#ifndef STARTER2D_GOALIEFORMATION_H
#define STARTER2D_GOALIEFORMATION_H

#include "formation.h"

class GoalieFormation : public Formation
{
  public:
    int getSegment(rcsc::Vector2D focus_point);
    const rcsc::Vector2D getPosition(const rcsc::Vector2D ball, const rcsc::WorldModel &wm);

    const double X_STEP = 0.5;
    const double Y_STEP = 0.5;
};

#endif // STARTER2D_GOALIEFORMATION_H
