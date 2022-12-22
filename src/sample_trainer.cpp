// -*-c++-*-

/*
 *Copyright:

 Copyright (C) Hidehisa AKIYAMA

 This code is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; either version 3, or (at your option)
 any later version.

 This code is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this code; see the file COPYING.  If not, write to
 the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.

 *EndCopyright:
 */

/////////////////////////////////////////////////////////////////////

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "sample_trainer.h"

#include <rcsc/coach/global_world_model.h>
#include <rcsc/common/basic_client.h>
#include <rcsc/common/player_param.h>
#include <rcsc/common/player_type.h>
#include <rcsc/common/server_param.h>
#include <rcsc/param/cmd_line_parser.h>
#include <rcsc/param/param_map.h>
#include <rcsc/random.h>
#include <rcsc/trainer/trainer_command.h>
#include <rcsc/trainer/trainer_config.h>

using namespace rcsc;

/*-------------------------------------------------------------------*/
/*!

 */
SampleTrainer::SampleTrainer() : TrainerAgent()
{
}

/*-------------------------------------------------------------------*/
/*!

 */
SampleTrainer::~SampleTrainer()
{
}

/*-------------------------------------------------------------------*/
/*!

 */
bool SampleTrainer::initImpl(CmdLineParser &cmd_parser)
{
    bool result = TrainerAgent::initImpl(cmd_parser);

#if 0
    ParamMap my_params;

    std::string formation_conf;
    my_map.add()
        ( &conf_path, "fconf" )
        ;

    cmd_parser.parse( my_params );
#endif

    if (cmd_parser.failed())
    {
        std::cerr << "coach: ***WARNING*** detected unsupported options: ";
        cmd_parser.print(std::cerr);
        std::cerr << std::endl;
    }

    if (!result)
    {
        return false;
    }

    //////////////////////////////////////////////////////////////////
    // Add your code here.
    //////////////////////////////////////////////////////////////////

    return true;
}

/*-------------------------------------------------------------------*/
/*!

 */
void SampleTrainer::actionImpl()
{
    if (world().teamNameLeft().empty())
    {
        doTeamNames();
        return;
    }

    //////////////////////////////////////////////////////////////////
    // Add your code here.

    sampleAction();
    // recoverForever();
    // doSubstitute();
    // doKeepaway();
}

/*-------------------------------------------------------------------*/
/*!

 */
void SampleTrainer::sampleAction()
{
    const GlobalBallObject &ball = world().ball();

    doRecover();
    doChangeMode(PM_PlayOn);

    if (world().time().cycle() % 30 == 0 || world().existKickablePlayer())
    {
        UniformReal y(-7, 7);

        AngleDeg angle = (Vector2D(-52, -6) - ball.pos()).th();
        Vector2D velocity = Vector2D::polar2vector(3, angle);
        doMovePlayer(config().teamName(), 1, Vector2D(-50, 0), Vector2D(-50, 0).th() - 180.0);
        doMoveBall(Vector2D(-15, 0.0), velocity);
    }
    return;
}

/*-------------------------------------------------------------------*/
/*!

 */
void SampleTrainer::recoverForever()
{
    if (world().playersLeft().empty())
    {
        return;
    }

    if (world().time().stopped() == 0 && world().time().cycle() % 50 == 0)
    {
        // recover stamina
        doRecover();
    }
}

/*-------------------------------------------------------------------*/
/*!

 */
void SampleTrainer::doSubstitute()
{
    static bool s_substitute = false;
    if (!s_substitute && world().time().cycle() == 0 && world().time().stopped() >= 10)
    {
        std::cerr << "trainer " << world().time() << " team name = " << world().teamNameLeft() << std::endl;

        if (!world().teamNameLeft().empty())
        {
            UniformSmallInt uni(0, PlayerParam::i().ptMax());
            doChangePlayerType(world().teamNameLeft(), 1, uni());

            s_substitute = true;
        }
    }

    if (world().time().stopped() == 0 && world().time().cycle() % 100 == 1 && !world().teamNameLeft().empty())
    {
        static int type = 0;
        doChangePlayerType(world().teamNameLeft(), 1, type);
        type = (type + 1) % PlayerParam::i().playerTypes();
    }
}

/*-------------------------------------------------------------------*/
/*!

 */
void SampleTrainer::doKeepaway()
{
    if (world().trainingTime() == world().time())
    {
        std::cerr << "trainer: " << world().time() << " keepaway training time." << std::endl;
    }
}
