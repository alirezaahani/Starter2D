#include "body_dribble.h"

#include <rcsc/action/body_smart_kick.h>
#include <rcsc/common/logger.h>
#include <rcsc/common/server_param.h>
#include <rcsc/player/debug_client.h>

using namespace rcsc;

class DribbleIntention : public SoccerIntention
{
  public:
    Vector2D M_receive_point = Vector2D::INVALIDATED;
    bool M_finished = false;

    DribbleIntention(Vector2D _receive_point) : M_receive_point(_receive_point), M_finished(false)
    {
    }

    bool finished(const rcsc::PlayerAgent *agent)
    {
        return M_finished;
    }

    bool execute(rcsc::PlayerAgent *agent)
    {
        if (M_finished)
        {
            return false;
        }

        if (Body_GoToPoint(M_receive_point, 0.1, ServerParam::i().maxDashPower()).execute(agent))
        {
            M_finished = true;
            return true;
        }

        M_finished = false;
        return false;
    }
};

bool Body_Dribble::execute(rcsc::PlayerAgent *agent)
{
    const WorldModel &wm = agent->world();
    const BallObject &ball = wm.ball();

    std::vector<DribbleInfo> targets;

    for (int i = 0; i < 360; i += 10)
    {
        Sector2D sector(ball.pos(), 0.0, 7.0, i - 5, i + 5);
        const double mean_angle = (sector.angleLeftStart().degree() + sector.angleRightEnd().degree()) / 2.0;

        DribbleInfo dribble = {.path_sector = sector, .mean_angle = mean_angle, .score = -1};

        if (!verify_dribble(wm, dribble))
        {
            continue;
        }

        dribble.score = score_dribble(wm, dribble);

        targets.push_back(dribble);
        
        dlog.addSector(Logger::DRIBBLE, sector, 255, 0, 0);
    }

    if (targets.size() == 0)
    {
        return false;
    }

    DribbleInfo target = *std::max_element(targets.begin(), targets.end(),
                                           [](DribbleInfo a, DribbleInfo b) { return a.score < b.score; });

    dlog.addSector(Logger::DRIBBLE, target.path_sector, 255, 255, 255, true);
    
    const Vector2D sector_rel = Vector2D::polar2vector(1, target.mean_angle);
    const Vector2D sector_end = sector_rel + wm.ball().pos();

    dlog.addCircle(Logger::DRIBBLE, sector_end, 0.3, 0, 0, 255, true);

    if (Body_SmartKick(sector_end, 0.7, 0.6, 1).execute(agent))
    {
        agent->setIntention(new DribbleIntention(sector_end));
        return true;
    }

    return false;
}

double Body_Dribble::score_dribble(const rcsc::WorldModel &wm, DribbleInfo target)
{
    double score = 1000;

    const Vector2D sector_rel = Vector2D::polar2vector(3, target.mean_angle);
    const Vector2D sector_end = sector_rel + wm.ball().pos();
    
    score += sector_end.x;
    score -= sector_end.dist(Vector2D(52, 0));
    score += 2 * wm.getDistOpponentNearestTo(sector_end, 5);

    return score;
}

bool Body_Dribble::verify_dribble(const rcsc::WorldModel &wm, DribbleInfo target)
{
        const Rect2D SOCCER_PITCH(Vector2D(-ServerParam::i().pitchHalfLength() - 3, -ServerParam::i().pitchHalfWidth() - 3),
                              Size2D(ServerParam::i().pitchLength() - 6, ServerParam::i().pitchWidth() - 6));

            const Vector2D end =  Vector2D::polar2vector(5, target.mean_angle) + wm.ball().pos();
            
    if(!SOCCER_PITCH.contains(end)) {
        return false;
    }
        
        
    for (auto opponent : wm.opponentsFromBall())
    {
        if (opponent == nullptr)
        {
            continue;
        }

        if (target.path_sector.contains(opponent->pos()))
        {
            return false;
        }
        
        if (target.path_sector.contains(opponent->inertiaPoint(1)))
        {
            return false;
        }
        
        for(int i = 1; i <= 5; i++) {
            const Vector2D sector_rel = Vector2D::polar2vector(i, target.mean_angle);
            const Vector2D sector_end = sector_rel + wm.ball().pos();
            
            if(sector_end.dist(opponent->pos()) < 3) {
                return false;
            }
        }
    }
    
    return true;
}
