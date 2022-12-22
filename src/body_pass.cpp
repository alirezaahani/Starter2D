//
// Created by alireza on 2022-07-11.
//
#include "body_pass.h"

#include <rcsc/action/body_kick_one_step.h>
#include <rcsc/action/body_smart_kick.h>
#include <rcsc/common/audio_memory.h>
#include <rcsc/common/logger.h>
#include <rcsc/common/server_param.h>
#include <rcsc/geom/ray_2d.h>
#include <rcsc/geom/sector_2d.h>
#include <rcsc/math_util.h>
#include <rcsc/player/audio_sensor.h>
#include <rcsc/player/debug_client.h>
#include <rcsc/player/intercept_table.h>
#include <rcsc/player/player_agent.h>
#include <rcsc/player/say_message_builder.h>

#include <algorithm>
#include <vector>

using namespace rcsc;

bool Body_Pass::execute(rcsc::PlayerAgent *agent)
{
    const WorldModel &wm = agent->world();

    PassInfo best;
    bool result = get_best_pass(wm, &best);

    if (!result)
    {
        return false;
    }

    const int kick_steps = wm.gameMode().type() == GameMode::PlayOn ? 2 : 1;

    if (!Body_SmartKick(best.destination, best.speed, best.speed * 0.8, kick_steps).execute(agent))
    {
        return false;
    }

    agent->addSayMessage(new PassMessage(best.player->unum(), best.destination, agent->effector().queuedNextBallPos(),
                                         agent->effector().queuedNextBallVel()));
    return true;
}

bool Body_Pass::forward_pass(PlayerAgent *agent)
{
    const WorldModel &wm = agent->world();

    PassInfo best;
    bool result = get_best_pass(wm, &best);

    if (!result)
    {
        return false;
    }

    if (best.destination.x <= wm.ball().pos().x + 5)
    {
        return false;
    }

    const int kick_steps = wm.gameMode().type() == GameMode::PlayOn ? 2 : 1;

    if (!Body_SmartKick(best.destination, best.speed, best.speed * 0.8, kick_steps).execute(agent))
    {
        return false;
    }

    agent->addSayMessage(new PassMessage(best.player->unum(), best.destination, agent->effector().queuedNextBallPos(),
                                         agent->effector().queuedNextBallVel()));
    return true;
}

#include <algorithm>

bool Body_Pass::get_best_pass(const rcsc::WorldModel &wm, PassInfo *best_pass)
{
    std::vector<PassInfo> targets;

    for (auto teammate : wm.teammatesFromBall())
    {
        if (teammate == nullptr)
        {
            continue;
        }
        if (teammate->unum() <= 0)
        {
            continue;
        }
        if (!teammate->posValid())
        {
            continue;
        }
        if (teammate->goalie() && teammate->pos().x < -22.0)
        {
            continue;
        }
        if (teammate->pos().x > wm.offsideLineX() + 1.0)
        {
            continue;
        }
        if (teammate->pos().x < wm.ball().pos().x - 25.0)
        {
            continue;
        }

        create_direct_pass(wm, teammate, targets);
        create_lead_pass(wm, teammate, targets);
        create_through_pass(wm, teammate, targets);
    }

    if (targets.size() == 0)
    {
        return false;
    }

    PassInfo target =
        *std::max_element(targets.begin(), targets.end(), [](PassInfo a, PassInfo b) { return a.score < b.score; });

    *best_pass = target;

    return true;
}

void Body_Pass::create_direct_pass(const rcsc::WorldModel &wm, const rcsc::PlayerObject *receiver,
                                   std::vector<PassInfo> &targets)
{
    dlog.addText(Logger::PASS, "(create_direct_pass): to %d", receiver->unum());

    if (receiver->distFromSelf() < 4)
    {
        dlog.addText(Logger::PASS, "Too close: %.2f", receiver->distFromSelf());
        return;
    }

    if (receiver->distFromSelf() > 34)
    {
        dlog.addText(Logger::PASS, "Too far: %.2f", receiver->distFromSelf());
        return;
    }

    const Rect2D SOCCER_PITCH(Vector2D(-ServerParam::i().pitchHalfLength(), -ServerParam::i().pitchHalfWidth()),
                              Size2D(ServerParam::i().pitchLength(), ServerParam::i().pitchWidth()));

    if (!SOCCER_PITCH.contains(receiver->pos()))
    {
        dlog.addText(Logger::PASS, "Outside of pitch: [%.2f, %.2f]", receiver->pos().x, receiver->pos().y);
        return;
    }

    if (receiver->pos().x < wm.self().pos().x + 1 && receiver->pos().dist(Vector2D(-52.5, 0)) < 18)
    {
        dlog.addText(Logger::PASS, "Pass is dangerous");
    }

    const Vector2D receiver_position = receiver->inertiaPoint(1);
    const Vector2D receiver_relative = receiver_position - wm.ball().pos();
    const double receiver_distance = receiver_relative.r();
    const AngleDeg receiver_angle = receiver_relative.th();

    dlog.addText(Logger::PASS, "Estimated player: [%.2f, %.2f], distance: %.2f", receiver_position.x,
                 receiver_position.y, receiver_distance);

    double end_speed = 1.5;
    double first_speed = 100.0;
    do
    {
        first_speed = calc_first_term_geom_series_last(end_speed, receiver_distance, ServerParam::i().ballDecay());
        if (first_speed < ServerParam::i().ballSpeedMax())
        {
            break;
        }
        end_speed -= 0.1;
    } while (end_speed > 0.8);

    if (first_speed > ServerParam::i().ballSpeedMax())
    {
        dlog.addText(Logger::PASS, "First speed of [%.2f] is higher than max speed [%.2f]", first_speed,
                     ServerParam::i().ballSpeedMax());
        return;
    }

    dlog.addText(Logger::PASS, "First speed: [%.2f]", first_speed);

    PassInfo center_direct_pass = {.player = receiver,
                                   .destination = receiver_position,
                                   .score = -1,
                                   .speed = first_speed,
                                   .angle = receiver_angle,
                                   .distance = receiver_distance,
                                   .type = PassType::DIRECT};

    if (verify_direct_pass(wm, center_direct_pass))
    {
        center_direct_pass.score = score(wm, center_direct_pass);
        dlog.addText(Logger::PASS, "Center direct pass with score [%.2f] added", center_direct_pass.score);
        dlog.addLine(Logger::PASS, wm.ball().pos(), center_direct_pass.destination, 0, 255, 255);

        targets.push_back(center_direct_pass);
    }

    double kickable_angle = 360.0 * (ServerParam::i().defaultKickableArea() / (2.0 * M_PI * receiver_distance));
    first_speed *= ServerParam::i().ballDecay();

    const Vector2D left_position =
        Vector2D::polar2vector(receiver_distance, receiver_angle - kickable_angle) + wm.ball().pos();

    PassInfo left_direct_pass = {.player = receiver,
                                 .destination = left_position,
                                 .score = -1,
                                 .speed = first_speed,
                                 .angle = receiver_angle - kickable_angle,
                                 .distance = receiver_distance,
                                 .type = PassType::DIRECT};

    if (verify_direct_pass(wm, left_direct_pass))
    {
        left_direct_pass.score = score(wm, left_direct_pass);
        dlog.addText(Logger::PASS, "Left direct pass with score [%.2f] added", left_direct_pass.score);
        dlog.addLine(Logger::PASS, wm.ball().pos(), left_direct_pass.destination, 0, 255, 255);

        targets.push_back(left_direct_pass);
    }

    const Vector2D right_position =
        Vector2D::polar2vector(receiver_distance, receiver_angle + kickable_angle) + wm.ball().pos();

    PassInfo right_direct_pass = {.player = receiver,
                                  .destination = right_position,
                                  .score = -1,
                                  .speed = first_speed,
                                  .angle = receiver_angle + kickable_angle,
                                  .distance = receiver_distance,
                                  .type = PassType::DIRECT};

    if (verify_direct_pass(wm, right_direct_pass))
    {
        right_direct_pass.score = score(wm, right_direct_pass);
        dlog.addText(Logger::PASS, "Right direct pass with score [%.2f] added", right_direct_pass.score);
        dlog.addLine(Logger::PASS, wm.ball().pos(), right_direct_pass.destination, 0, 255, 255);

        targets.push_back(right_direct_pass);
    }
}

void Body_Pass::create_lead_pass(const rcsc::WorldModel &wm, const rcsc::PlayerObject *receiver,
                                 std::vector<PassInfo> &targets)
{
    dlog.addText(Logger::PASS, "(create_lead_pass): to %d", receiver->unum());

    const Rect2D SOCCER_PITCH(Vector2D(-ServerParam::i().pitchHalfLength() - 3, -ServerParam::i().pitchHalfWidth() - 3),
                              Size2D(ServerParam::i().pitchLength() - 6, ServerParam::i().pitchWidth() - 6));

    if (receiver->distFromSelf() < 4)
    {
        dlog.addText(Logger::PASS, "Too close: %.2f", receiver->distFromSelf());
        return;
    }

    if (receiver->distFromSelf() > 34)
    {
        dlog.addText(Logger::PASS, "Too far: %.2f", receiver->distFromSelf());
        return;
    }

    if (!SOCCER_PITCH.contains(receiver->pos()))
    {
        dlog.addText(Logger::PASS, "Outside of pitch: [%.2f, %.2f]", receiver->pos().x, receiver->pos().y);
        return;
    }

    if (receiver->pos().x < wm.self().pos().x + 1 && receiver->pos().dist(Vector2D(-52.5, 0)) < 18)
    {
        dlog.addText(Logger::PASS, "Pass is dangerous");
    }

    const Vector2D base_positon = receiver->inertiaPoint(1);

    dlog.addText(Logger::PASS, "Estimated player: [%.2f, %.2f]", base_positon.x, base_positon.y);

    for (int angle = 0; angle < 360; angle += 30)
    {
        const Vector2D direction = Vector2D::polar2vector(1, angle);

        for (double radius = 0.1; radius <= 2; radius += 0.1)
        {
            const Vector2D receiver_position = (direction * radius) + base_positon;
            const Vector2D receiver_relative = receiver_position - wm.ball().pos();
            const double receiver_distance = receiver_relative.r();
            const AngleDeg receiver_angle = receiver_relative.th();

            if (receiver_distance < 4)
            {
                dlog.addText(Logger::PASS, "Too close: %.2f", receiver->distFromSelf());
                continue;
            }

            if (receiver_distance > 34)
            {
                dlog.addText(Logger::PASS, "Too far: %.2f", receiver->distFromSelf());
                continue;
            }

            if (!SOCCER_PITCH.contains(receiver_position))
            {
                dlog.addText(Logger::PASS, "Outside of pitch: [%.2f, %.2f]", receiver->pos().x, receiver->pos().y);
                continue;
            }

            if (receiver_position.x < wm.self().pos().x + 1 && receiver_position.dist(Vector2D(-52.5, 0)) < 18)
            {
                dlog.addText(Logger::PASS, "Pass is dangerous");
            }

            double end_speed = 1.5;
            double first_speed = 100.0;
            do
            {
                first_speed =
                    calc_first_term_geom_series_last(end_speed, receiver_distance, ServerParam::i().ballDecay());
                if (first_speed < ServerParam::i().ballSpeedMax())
                {
                    break;
                }
                end_speed -= 0.1;
            } while (end_speed > 0.8);

            if (first_speed > ServerParam::i().ballSpeedMax())
            {
                dlog.addText(Logger::PASS, "First speed of [%.2f] is higher than max speed [%.2f]", first_speed,
                             ServerParam::i().ballSpeedMax());
                return;
            }

            PassInfo lead_pass = {.player = receiver,
                                  .destination = receiver_position,
                                  .score = -1,
                                  .speed = 3,
                                  .angle = receiver_angle,
                                  .distance = receiver_distance,
                                  .type = PassType::LEAD};
            if (verify_lead_pass(wm, lead_pass))
            {
                lead_pass.score = score(wm, lead_pass);
                dlog.addText(Logger::PASS, "Lead pass pass with score [%.2f] added", lead_pass.score);
                dlog.addLine(Logger::PASS, wm.ball().pos(), lead_pass.destination, 0, 255, 255);

                targets.push_back(lead_pass);
            }
        }
    }
}

void Body_Pass::create_through_pass(const rcsc::WorldModel &wm, const rcsc::PlayerObject *receiver,
                                    std::vector<PassInfo> &targets)
{
    dlog.addText(Logger::PASS, "(create_through_pass): to %d", receiver->unum());

    static const double MAX_THROUGH_PASS_DIST =
        0.9 * inertia_final_distance(ServerParam::i().ballSpeedMax(), ServerParam::i().ballDecay());

    static const Rect2D shrinked_pitch(
        Vector2D(-ServerParam::i().pitchHalfLength() + 3.0, -ServerParam::i().pitchHalfWidth() + 3.0),
        Size2D(ServerParam::i().pitchLength() - 6.0, ServerParam::i().pitchWidth() - 6.0));

    static const double receiver_dash_speed = 1.0;

    static const double min_dash = 5.0;
    static const double max_dash = 25.0;
    static const double dash_range = max_dash - min_dash;
    static const double dash_inc = 4.0;
    static const int dash_loop = static_cast<int>(std::ceil(dash_range / dash_inc)) + 1;

    static const AngleDeg min_angle = -20.0;
    static const AngleDeg max_angle = 20.0;
    static const double angle_range = (min_angle - max_angle).abs();
    static const double angle_inc = 10.0;
    static const int angle_loop = static_cast<int>(std::ceil(angle_range / angle_inc)) + 1;

    /////////////////////////////////////////////////////////////////
    // check receiver position
    if (receiver->pos().x > wm.offsideLineX() - 0.5)
    {
        return;
    }
    if (receiver->pos().x < wm.self().pos().x - 10.0)
    {
        return;
    }
    if (std::fabs(receiver->pos().y - wm.self().pos().y) > 35.0)
    {
        return;
    }
    if (wm.ourDefenseLineX() < 0.0 && receiver->pos().x < wm.ourDefenseLineX() - 15.0)
    {
        return;
    }
    if (wm.offsideLineX() < 30.0 && receiver->pos().x < wm.offsideLineX() - 15.0)
    {
        return;
    }
    if (receiver->angleFromSelf().abs() > 135.0)
    {
        return;
    }

    // angle loop
    AngleDeg dash_angle = min_angle;
    for (int i = 0; i < angle_loop; ++i, dash_angle += angle_inc)
    {
        const Vector2D base_dash = Vector2D::polar2vector(1.0, dash_angle);

        // dash dist loop
        double dash_dist = min_dash;
        for (int j = 0; j < dash_loop; ++j, dash_dist += dash_inc)
        {
            Vector2D target_point = base_dash;
            target_point *= dash_dist;
            target_point += receiver->pos();

            if (!shrinked_pitch.contains(target_point))
            {
                // out of pitch
                continue;
            }

            if (target_point.x < wm.self().pos().x + 3.0)
            {
                continue;
            }

            const Vector2D target_rel = target_point - wm.ball().pos();
            const double target_dist = target_rel.r();
            const AngleDeg target_angle = target_rel.th();

            if (target_dist > MAX_THROUGH_PASS_DIST) // dist range over
            {
                continue;
            }

            if (target_dist < dash_dist) // I am closer than receiver
            {
                continue;
            }

            const double dash_step = dash_dist / receiver_dash_speed; // + 5.0;//+2.0

            double end_speed = 0.81; // 0.65
            double first_speed = 100.0;
            double ball_steps_to_target = 100.0;
            do
            {
                first_speed = calc_first_term_geom_series_last(end_speed, target_dist, ServerParam::i().ballDecay());
                if (first_speed > ServerParam::i().ballSpeedMax())
                {
                    end_speed -= 0.1;
                    continue;
                }

                ball_steps_to_target = calc_length_geom_series(first_speed, target_dist, ServerParam::i().ballDecay());
                if (dash_step < ball_steps_to_target)
                {
                    break;
                }

                end_speed -= 0.1;
            } while (end_speed > 0.5);

            if (first_speed > ServerParam::i().ballSpeedMax() || dash_step > ball_steps_to_target)
            {
                continue;
            }

            PassInfo through = {.player = receiver,
                                .destination = target_point,
                                .score = -1,
                                .speed = first_speed,
                                .angle = target_angle,
                                .distance = target_dist,
                                .type = PassType::THROUGH};

            if (verify_lead_pass(wm, through))
            {
                through.score = score(wm, through);
                dlog.addLine(Logger::PASS, wm.ball().pos(), through.destination, 0, 255, 255);

                targets.push_back(through);
            }
        }
    }
}

bool Body_Pass::verify_lead_pass(const rcsc::WorldModel &wm, PassInfo &pass)
{
    static const double player_dash_speed = 1.0;

    const Vector2D first_vel = Vector2D::polar2vector(pass.speed, pass.angle);
    const AngleDeg minus_target_angle = -pass.angle;
    const double next_speed = pass.speed * ServerParam::i().ballDecay();

    const double receiver_to_target = pass.player->pos().dist(pass.destination);

    bool very_aggressive = false;
    if (pass.destination.x > 28.0 && pass.destination.x > wm.ball().pos().x + 20.0)
    {
        very_aggressive = true;
    }
    else if (pass.destination.x > wm.offsideLineX() + 15.0 && pass.destination.x > wm.ball().pos().x + 15.0)
    {
        very_aggressive = true;
    }
    else if (pass.destination.x > 38.0 && pass.destination.x > wm.offsideLineX() && pass.destination.absY() < 14.0)
    {
        very_aggressive = true;
    }

    for (const PlayerObject *o : wm.opponentsFromSelf())
    {
        if (o->goalie())
        {
            if (pass.destination.absY() > ServerParam::i().penaltyAreaWidth() - 3.0 ||
                pass.destination.x < ServerParam::i().theirPenaltyAreaLineX() + 2.0)
            {
                continue;
            }
        }

        const double virtual_dash = player_dash_speed * std::min(2, o->posCount());
        const double opp_to_target = o->pos().dist(pass.destination);
        double dist_rate = (very_aggressive ? 0.8 : 1.0);
        double dist_buf = (very_aggressive ? 0.5 : 2.0);

        if (opp_to_target - virtual_dash < receiver_to_target * dist_rate + dist_buf)
        {
            return false;
        }

        Vector2D ball_to_opp = o->pos();
        ball_to_opp -= wm.ball().pos();
        ball_to_opp -= first_vel;
        ball_to_opp.rotate(minus_target_angle);

        if (0.0 < ball_to_opp.x && ball_to_opp.x < pass.distance)
        {
            double opp2line_dist = ball_to_opp.absY();
            opp2line_dist -= virtual_dash;
            opp2line_dist -= ServerParam::i().defaultKickableArea();
            opp2line_dist -= 0.1;
            if (opp2line_dist < 0.0)
            {
                return false;
            }

            const double ball_steps_to_project =
                calc_length_geom_series(next_speed, ball_to_opp.x, ServerParam::i().ballDecay());

            if (ball_steps_to_project < 0.0 || opp2line_dist / player_dash_speed < ball_steps_to_project)
            {
                return false;
            }
        }
    }

    return true;
}

bool Body_Pass::verify_direct_pass(const rcsc::WorldModel &wm, PassInfo &pass)
{
    dlog.addText(Logger::PASS, "(verify_direct_pass)");

    for (auto opponent : wm.opponentsFromSelf())
    {
        if (opponent->pos().dist(pass.destination) < 5)
        {
            dlog.addText(Logger::PASS, "Exists opponent at destination, distance: %.2f",
                         opponent->pos().dist(pass.destination));
            return false;
        }
    }

    const Vector2D direction = Vector2D::polar2vector(1, pass.angle);

    for (int i = 0; i <= pass.distance; i++)
    {
        const Vector2D path = direction * i + wm.ball().pos();

        dlog.addCircle(Logger::PASS, path, 0.3, 255, 0, 0);

        if (wm.getDistOpponentNearestTo(path, 11) < i / 2)
        {
            dlog.addText(Logger::PASS, "Opponent can catch in path, distance: %.2f, cycle: %d",
                         wm.getDistOpponentNearestTo(path, 11), i);
            return false;
        }
    }

    return true;
}

double Body_Pass::score(const rcsc::WorldModel &wm, PassInfo &pass)
{
    double score = 1000;

    score += pass.destination.x - wm.ball().pos().x;
    score += wm.getDistOpponentNearestTo(pass.destination, 11);
    score -= pass.destination.dist2(pass.player->pos());

    return score;
}
