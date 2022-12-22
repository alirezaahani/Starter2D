//
// Created by alireza on 8/23/22.
//

#include "goalie_formation.h"

int GoalieFormation::getSegment(rcsc::Vector2D focus_point)
{
    int segment = 0;

    for (double i = -52; i <= 0; i += X_STEP)
    {
        for (double j = -34; j <= 34; j += Y_STEP)
        {
            rcsc::Rect2D r(rcsc::Vector2D(i, j), rcsc::Vector2D(i + X_STEP, j + Y_STEP));
            if (r.contains(focus_point))
            {
                return segment;
            }
            segment++;
        }
    }

    return -1;
}

const struct SimpleVec2D goalie_formation[] = {
#include "formations/goalie-formation.conf.points"
};

const rcsc::Vector2D GoalieFormation::getPosition(const rcsc::Vector2D ball, const rcsc::WorldModel &wm)
{
    if (ball.x < 0)
    {
        int segment = getSegment(ball);

        // In situations where the ball has been shot or is outside
        // ground, try to position based on current ball position
        if (segment == -1)
        {
            segment = getSegment(wm.ball().pos());
        }

        const struct SimpleVec2D target = goalie_formation[segment];
        return rcsc::Vector2D(target.x, target.y);
    }
    else
    {
        return rcsc::Vector2D(-49, 0);
    }
}
