#include "body_shoot_generator.h"

#include <rcsc/action/kick_table.h>

#include <rcsc/common/logger.h>
#include <rcsc/common/server_param.h>
#include <rcsc/math_util.h>
#include <rcsc/player/player_agent.h>
#include <rcsc/timer.h>

#include <rcsc/action/basic_actions.h>
#include <rcsc/action/body_go_to_point.h>
#include <rcsc/action/body_intercept.h>
#include <rcsc/common/server_param.h>
#include <rcsc/player/debug_client.h>
#include <rcsc/player/intercept_table.h>
#include <rcsc/player/player_agent.h>

using namespace rcsc;

/*-------------------------------------------------------------------*/
/*!

 */
void ShootGenerator::search(const PlayerAgent *agent)
{
    const WorldModel &wm = agent->world();

    total_shots = 0;
    M_shots.clear();

    static const Vector2D goal_c(ServerParam::i().pitchHalfLength(), 0.0);

    if (!wm.self().isKickable())
    {
        return;
    }

    if (wm.self().pos().dist2(goal_c) > 900)
    {
        return;
    }

    Vector2D goal_l(ServerParam::i().pitchHalfLength(), -ServerParam::i().goalHalfWidth());
    Vector2D goal_r(ServerParam::i().pitchHalfLength(), ServerParam::i().goalHalfWidth());

    goal_l.y += std::min(1.5, 0.6 + goal_l.dist(wm.ball().pos()) * 0.042);
    goal_r.y -= std::min(1.5, 0.6 + goal_r.dist(wm.ball().pos()) * 0.042);

    if (wm.self().pos().x > ServerParam::i().pitchHalfLength() - 1.0 &&
        wm.self().pos().absY() < ServerParam::i().goalHalfWidth())
    {
        goal_l.x = wm.self().pos().x + 1.5;
        goal_r.x = wm.self().pos().x + 1.5;
    }

    const int DIST_DIVS = 90;
    const double dist_step = std::fabs(goal_l.y - goal_r.y) / (DIST_DIVS - 1);

    const PlayerObject *goalie = wm.getOpponentGoalie();

    Vector2D shot_point = goal_l;

    for (int i = 0; i < DIST_DIVS; i++, shot_point.y += dist_step)
    {
        total_shots++;
        calculateShotPoint(wm, shot_point, goalie);
    }
}

/*-------------------------------------------------------------------*/
/*!

 */
void ShootGenerator::calculateShotPoint(const WorldModel &wm, const Vector2D &shot_point, const PlayerObject *goalie)
{
    const Vector2D shot_rel = shot_point - wm.ball().pos();
    const AngleDeg shot_angle = shot_rel.th();

    const double shot_dist = shot_rel.r();

    const Vector2D one_step_vel = KickTable::calc_max_velocity(shot_angle, wm.self().kickRate(), wm.ball().vel());
    const double max_one_step_speed = one_step_vel.r();

    double shot_first_speed = (shot_dist + 5.0) * (1.0 - ServerParam::i().ballDecay());
    shot_first_speed = std::max(max_one_step_speed, shot_first_speed);
    shot_first_speed = std::max(1.5, shot_first_speed);

    const double y_dist = std::max(0.0, shot_point.absY() - 4.0);
    const double y_rate = std::exp(-std::pow(y_dist, 2.0) / (2.0 * ServerParam::i().goalHalfWidth()));

    bool over_max = false;
    while (!over_max)
    {
        if (shot_first_speed > ServerParam::i().ballSpeedMax() - 0.001)
        {
            over_max = true;
            shot_first_speed = ServerParam::i().ballSpeedMax();
        }

        Shot shot(shot_point, shot_first_speed, shot_angle);
        shot.score_ = 0;

        const bool one_step = (shot_first_speed <= max_one_step_speed);
        if (canScore(wm, one_step, &shot))
        {
            shot.score_ += 100;
            if (one_step)
            {
                shot.score_ += 100;
            }

            double goalie_rate = -1.0;
            if (shot.goalie_never_reach_)
            {
                shot.score_ += 100;
            }

            if (goalie)
            {
                AngleDeg goalie_angle = (goalie->pos() - wm.ball().pos()).th();
                double angle_diff = (shot.angle_ - goalie_angle).abs();
                goalie_rate = 1.0 - std::exp(-std::pow(angle_diff * 0.1, 2) / (2.0 * 90.0 * 0.1));
                shot.score_ = static_cast<int>(shot.score_ * goalie_rate);
            }

            shot.score_ = static_cast<int>(shot.score_ * y_rate);

            M_shots.push_back(shot);
        }
        else
        {
        }

        shot_first_speed += 0.5;
    }
}

/*-------------------------------------------------------------------*/
/*!

 */
bool ShootGenerator::canScore(const WorldModel &wm, const bool one_step_kick, Shot *shot)
{
    static const double opp_x_thr = ServerParam::i().theirPenaltyAreaLineX() - 5.0;
    static const double opp_y_thr = ServerParam::i().penaltyAreaHalfWidth();

    // estimate required ball travel step
    const double ball_reach_step =
        calc_length_geom_series(shot->speed_, wm.ball().pos().dist(shot->point_), ServerParam::i().ballDecay());

    if (ball_reach_step < 1.0)
    {
        shot->score_ += 100;
        return true;
    }

    const int ball_reach_step_i = static_cast<int>(std::ceil(ball_reach_step));

    // estimate opponent interception

    for (auto opp : wm.opponentsFromSelf())
    {
        // outside of penalty
        if (opp->pos().x < opp_x_thr)
            continue;
        if (opp->pos().absY() > opp_y_thr)
            continue;
        if (opp->isTackling())
            continue;

        // behind of shoot course
        if ((shot->angle_ - opp->angleFromSelf()).abs() > 90.0)
        {
            continue;
        }

        if (opp->goalie())
        {
            if (maybeGoalieCatch(wm, opp, shot))
            {
                return false;
            }
        }
        else
        {
            int cycle = predictOpponentReachStep(wm, shot->point_, opp, wm.ball().pos(), shot->vel_, one_step_kick,
                                                 ball_reach_step_i);

            if (cycle == 1 || cycle < ball_reach_step_i - 1)
            {
                return false;
            }
        }
    }

    return true;
}

/*-------------------------------------------------------------------*/
/*!

 */
bool ShootGenerator::maybeGoalieCatch(const WorldModel &wm, const PlayerObject *goalie, Shot *shot)
{
    static const Rect2D penalty_area(Vector2D(ServerParam::i().theirPenaltyAreaLineX(),  // left
                                              -ServerParam::i().penaltyAreaHalfWidth()), // top
                                     Size2D(ServerParam::i().penaltyAreaLength(),        // length
                                            ServerParam::i().penaltyAreaWidth()));       // width
    static const double catchable_area = ServerParam::i().catchableArea();

    const ServerParam &param = ServerParam::i();

    const double dash_accel_mag = (param.maxDashPower() * param.defaultDashPowerRate() * param.defaultEffortMax());

    int min_cycle = 1;
    {
        Line2D shot_line(wm.ball().pos(), shot->point_);
        double goalie_line_dist = shot_line.dist(goalie->pos());
        goalie_line_dist -= catchable_area;
        min_cycle = static_cast<int>(std::ceil(goalie_line_dist / param.defaultRealSpeedMax()));
        min_cycle -= std::min(5, goalie->posCount());
        min_cycle = std::max(1, min_cycle);
    }
    Vector2D ball_pos = inertia_n_step_point(wm.ball().pos(), shot->vel_, min_cycle, param.ballDecay());
    Vector2D ball_vel = (shot->vel_ * std::pow(param.ballDecay(), min_cycle));

    int cycle = min_cycle;
    while (ball_pos.x < param.pitchHalfLength() + 0.085 && cycle <= 50)
    {
        // estimate the required turn angle
        Vector2D goalie_pos = goalie->inertiaPoint(cycle);
        Vector2D ball_relative = ball_pos - goalie_pos;
        double ball_dist = ball_relative.r();

        if (ball_dist < catchable_area)
        {
            return true;
        }

        if (ball_dist < catchable_area + 1.7)
        {
            shot->goalie_never_reach_ = false;
        }

        AngleDeg ball_angle = ball_relative.th();
        AngleDeg goalie_body = (goalie->bodyCount() <= 5 ? goalie->body() : ball_angle);

        int n_turn = 0;
        double angle_diff = (ball_angle - goalie_body).abs();
        if (angle_diff > 90.0)
        {
            angle_diff = 180.0 - angle_diff; // back dash
            goalie_body -= 180.0;
        }

        double turn_margin = std::max(AngleDeg::asin_deg(catchable_area / ball_dist), 15.0);

        Vector2D goalie_vel = goalie->vel();

        while (angle_diff > turn_margin)
        {
            double max_turn = effective_turn(180.0, goalie_vel.r(), param.defaultInertiaMoment());
            angle_diff -= max_turn;
            goalie_vel *= param.defaultPlayerDecay();
            ++n_turn;
        }

        // simulate dash
        goalie_pos = goalie->inertiaPoint(n_turn);

        const Vector2D dash_accel = Vector2D::polar2vector(dash_accel_mag, ball_angle);
        const int max_dash = (cycle - 1 - n_turn + bound(0, goalie->posCount() - 1, 5));
        double goalie_travel = 0.0;
        for (int i = 0; i < max_dash; ++i)
        {
            goalie_vel += dash_accel;
            goalie_pos += goalie_vel;
            goalie_travel += goalie_vel.r();
            goalie_vel *= param.defaultPlayerDecay();

            double d = goalie_pos.dist(ball_pos);
            if (d < catchable_area + 1.0 + (goalie_travel * 0.04))
            {
                shot->goalie_never_reach_ = false;
            }
        }

        // check distance
        if (goalie->pos().dist(goalie_pos) * 1.05 > goalie->pos().dist(ball_pos) - catchable_area)
        {
            return true;
        }

        // update ball position & velocity
        ++cycle;
        ball_pos += ball_vel;
        ball_vel *= param.ballDecay();
    }

    return false;
}

/*-------------------------------------------------------------------*/
/*!

 */
int ShootGenerator::predictOpponentReachStep(const WorldModel &, const Vector2D &target_point,
                                             const PlayerObject *opponent, const Vector2D &first_ball_pos,
                                             const Vector2D &first_ball_vel, const bool one_step_kick,
                                             const int max_step)
{
    const ServerParam &param = ServerParam::i();
    const PlayerType *player_type = opponent->playerTypePtr();
    const double control_area = player_type->kickableArea();

    int min_cycle = 1;
    {
        Line2D shot_line(first_ball_pos, target_point);
        double line_dist = shot_line.dist(opponent->pos());
        line_dist -= control_area;
        min_cycle = static_cast<int>(std::ceil(line_dist / player_type->realSpeedMax()));
        min_cycle -= std::min(5, opponent->posCount());
        min_cycle = std::max(1, min_cycle);
    }

    Vector2D ball_pos = inertia_n_step_point(first_ball_pos, first_ball_vel, min_cycle, param.ballDecay());
    Vector2D ball_vel = first_ball_vel * std::pow(param.ballDecay(), min_cycle);

    int cycle = min_cycle;

    while (cycle <= max_step)
    {
        Vector2D opp_pos = opponent->inertiaPoint(cycle);
        Vector2D opp_to_ball = ball_pos - opp_pos;
        double opp_to_ball_dist = opp_to_ball.r();

        int n_turn = 0;
        if (opponent->bodyCount() <= 1 || opponent->velCount() <= 1)
        {
            double angle_diff = (opponent->bodyCount() <= 1 ? (opp_to_ball.th() - opponent->body()).abs()
                                                            : (opp_to_ball.th() - opponent->vel().th()).abs());

            double turn_margin = 180.0;
            if (control_area < opp_to_ball_dist)
            {
                turn_margin = AngleDeg::asin_deg(control_area / opp_to_ball_dist);
            }
            turn_margin = std::max(turn_margin, 12.0);

            double opp_speed = opponent->vel().r();
            while (angle_diff > turn_margin)
            {
                angle_diff -= player_type->effectiveTurn(param.maxMoment(), opp_speed);
                opp_speed *= player_type->playerDecay();
                ++n_turn;
            }
        }

        opp_to_ball_dist -= control_area;
        opp_to_ball_dist -= opponent->distFromSelf() * 0.03;

        if (opp_to_ball_dist < 0.0)
        {
            return cycle;
        }

        int n_step = player_type->cyclesToReachDistance(opp_to_ball_dist);
        n_step += n_turn;
        n_step -= bound(0, opponent->posCount(), 2);

        if (n_step < cycle - (one_step_kick ? 1 : 0))
        {
            return cycle;
        }

        // update ball position & velocity
        ++cycle;
        ball_pos += ball_vel;
        ball_vel *= param.ballDecay();
    }

    return cycle;
}
