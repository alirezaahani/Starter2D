//
// Created by alireza on 2022-07-11.
//

#ifndef SRC_BASIC_PASS_H
#define SRC_BASIC_PASS_H
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <rcsc/action/body_hold_ball.h>
#include <rcsc/action/body_smart_kick.h>
#include <rcsc/action/body_stop_ball.h>
#include <rcsc/common/logger.h>
#include <rcsc/common/server_param.h>
#include <rcsc/geom/convex_hull.h>
#include <rcsc/geom/sector_2d.h>
#include <rcsc/player/debug_client.h>
#include <rcsc/player/player_agent.h>

#include <cmath>
#include <vector>

#include "bhv_basic_offensive_kick.h"

enum class PassType
{
    DIRECT,
    LEAD,
    THROUGH
};

typedef struct PassInfo
{
    const rcsc::PlayerObject *player;
    rcsc::Vector2D destination;
    double score;
    double speed;
    rcsc::AngleDeg angle;
    double distance;
    PassType type;
} PassInfo;

class Body_Pass : public rcsc::SoccerBehavior
{
  public:
    Body_Pass()
    {
    }

    bool execute(rcsc::PlayerAgent *agent);
    bool forward_pass(rcsc::PlayerAgent *agent);

    bool get_best_pass(const rcsc::WorldModel &wm, PassInfo *best_pass);

    double score(const rcsc::WorldModel &wm, PassInfo &pass);

    void create_direct_pass(const rcsc::WorldModel &wm, const rcsc::PlayerObject *receiver,
                            std::vector<PassInfo> &targets);
    void create_lead_pass(const rcsc::WorldModel &wm, const rcsc::PlayerObject *receiver,
                          std::vector<PassInfo> &targets);
    void create_through_pass(const rcsc::WorldModel &wm, const rcsc::PlayerObject *receiver,
                             std::vector<PassInfo> &targets);

    bool verify_direct_pass(const rcsc::WorldModel &wm, PassInfo &pass);
    bool verify_lead_pass(const rcsc::WorldModel &wm, PassInfo &pass);
};
#endif // SRC_BASIC_PASS_H
