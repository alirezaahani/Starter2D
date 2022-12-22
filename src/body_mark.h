//
// Created by alireza on 8/8/22.
//

#ifndef HELIOS_BASE_BODY_MARK_H
#define HELIOS_BASE_BODY_MARK_H

#include <rcsc/player/soccer_action.h>

class Body_Mark : public rcsc::SoccerBehavior
{
  public:
    Body_Mark()
    {
    }

    bool execute(rcsc::PlayerAgent *agent);
};

#endif // HELIOS_BASE_BODY_MARK_H
