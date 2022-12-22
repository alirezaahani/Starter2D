//
// Created by alireza on 8/8/22.
//

#include "body_mark.h"
#include "bhv_basic_move.h"
#include <rcsc/action/body_go_to_point.h>
#include <rcsc/player/intercept_table.h>
#include <rcsc/player/player_agent.h>

#include <rcsc/common/logger.h>
#include <rcsc/soccer_math.h>

using namespace rcsc;

bool Body_Mark::execute(rcsc::PlayerAgent *agent)
{
    const WorldModel &wm = agent->world();
    const SelfObject &self = wm.self();
    const int unum = self.unum();

    if (wm.gameMode().type() == GameMode::AfterGoal_ || wm.gameMode().type() == GameMode::BeforeKickOff)
    {
        dlog.addText(Logger::MARK, "Can't mark opponents in after goal or before kick off");
        return false;
    }

    for (auto opponent : wm.opponentsFromSelf())
    {
        if (opponent == nullptr)
        {
            dlog.addText(Logger::MARK, "Invalid opponent");
            continue;
        }

        if (wm.gameMode().type() != GameMode::PlayOn)
        {
            if (opponent->isKickable())
            {
                dlog.addText(Logger::MARK, "Can't mark kickable opponent");
                continue;
            }
        }

        if (opponent->goalie())
        {
            dlog.addText(Logger::MARK, "Can't mark goalie opponent");
            continue;
        }

        double teammate_distance = 0;
        const PlayerObject *teammate = wm.getTeammateNearestTo(opponent->pos(), 5, &teammate_distance);

        if (teammate != nullptr)
        {
            if (teammate_distance < 7)
            {
                continue;
            }
        }

        Vector2D opponent_future_position = opponent->inertiaPoint(1);
        double distance_threshold = opponent_future_position.dist(wm.ball().pos()) * 0.1;

        if (distance_threshold > 1.0)
        {
            distance_threshold = 1.0;
        }

        if (Body_GoToPoint(opponent_future_position, distance_threshold, Bhv_BasicMove().get_normal_dash_power(wm))
                .execute(agent))
        {
            dlog.addText(Logger::MARK, "Marking [%d] at [%.2f, %.2f]", opponent->unum(), opponent_future_position.x,
                         opponent_future_position.y);
            return true;
        }
        else
        {
            dlog.addText(Logger::MARK, "Already marking [%d] at [%.2f, %.2f]", opponent->unum(),
                         opponent_future_position.x, opponent_future_position.y);
            return true;
        }
    }

    dlog.addText(Logger::MARK, "Failed to mark");
    return false;
}
