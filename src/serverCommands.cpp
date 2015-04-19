#include "app.h"
#include "message.h"
#include "utils.h"
#include "serialize.h"
#include "mob.h"
#include "sync.h"
#include "settings.h"
#include "scene.h"
#include <Qt>
#include <QDir>

using namespace Settings;

// Prints a very basic help message
void App::printBasicHelp()
{
    logMessage(tr("== Basic Server Commands"));
    logMessage(tr("> start/stop login  :  Starts or stops login server"));
    logMessage(tr("> start/stop game   :  Starts or stops game server"));
    logMessage(tr("> status            :  Views login and game server status"));
    logMessage(tr("> exit              :  Shutdown and exit"));
    logMessage(tr("> help              :  View help and all commands"));
    logMessage("");
    logMessage(tr("To send a server command, simply type it and hit enter"));
}

// Processes the commands entered directly in the server, not the chat messages
void App::sendCmdLine()
{
    if (!enableGameServer)
    {
        logMessage(QObject::tr("This is not a game server, commands are disabled"));
        return;
    }

#ifdef USE_GUI
    QString str = ui->cmdLine->text();
    ui->cmdLine->clear();
    ui->cmdLine->setFocus();
#else
    QString str = cin.readLine();
#endif

    if (str.startsWith("help", Qt::CaseInsensitive))
    {
        QString indent = "  -";

        logMessage(QObject::tr("== Server Commands"));
#ifdef USE_GUI
        logMessage("clear");
        logMessage(QObject::tr("%1 Clears the log screen").arg(indent));
#endif
        logMessage("exit");
        logMessage(QObject::tr("%1 Shuts down all operations and exits").arg(indent));
        logMessage("start login");
        logMessage("stop login");
        logMessage(QObject::tr("%1 Starts and stops the login server").arg(indent));
        logMessage("start game");
        logMessage("stop game");
        logMessage(QObject::tr("%1 Starts and stops the game server").arg(indent));
        logMessage("status");
        logMessage(QObject::tr("%1 Shows the status of the login and game servers").arg(indent));
        logMessage("listTcpPlayers");
        logMessage(QObject::tr("%1 Lists players currently logged into the game server").arg(indent));
        logMessage("listPeers [scene]");
        logMessage(QObject::tr("%1 Lists the peers currently connected to the server").arg(indent));
        logMessage(QObject::tr("%1 If scene name is defined, return only peers in that scene").arg(indent));
        logMessage("setPeer <ponyid>");
        logMessage("setPeer <ip:port>");
        logMessage(QObject::tr("%1 Sets the 'currently selected' peer that will be used for other commands").arg(indent));
        logMessage("listVortexes");
        logMessage(QObject::tr("%1 List all the vortexrs currently in the game").arg(indent));
        logMessage("sync");
        logMessage(QObject::tr("%1 Syncs the positions of all clients now").arg(indent));
        logMessage("tele [sourceponyid] [destponyid]");
        logMessage(QObject::tr("%1 Move sourcepony to destpony's location").arg(indent));
        logMessage("dbgStressLoad [scene]");
        logMessage(QObject::tr("%1 Move all players to either given scene, or Gem Mines at once").arg(indent));
        logMessage(QObject::tr("%1 This is a DEBUG COMMAND. Please do not use it in production").arg(indent));
        logMessage("");
        logMessage(QObject::tr("== Commands requiring a selected peer"));
        logMessage("disconnect");
        logMessage(QObject::tr("%1 Disconnects the selected player").arg(indent));
        logMessage("load <scene>");
        logMessage(QObject::tr("%1 Send player to the given scene").arg(indent));
        logMessage("getPos");
        logMessage(QObject::tr("%1 Get selected player's position").arg(indent));
        logMessage(QObject::tr("%1 Often used with the 'move' command").arg(indent));
        logMessage("getRot");
        logMessage(QObject::tr("%1 Get selected player's rotation").arg(indent));
        logMessage("move <x> <y> <z>");
        logMessage(QObject::tr("%1 Moves the selected player to the given location").arg(indent));
        logMessage("error <string>");
        logMessage(QObject::tr("%1 Sends a message box to the client, with the title").arg(indent));
        logMessage(QObject::tr("%1   'Error', and the given message").arg(indent));
        logMessage(QObject::tr("%1 Does not disconnect the client").arg(indent));
        logMessage("getPonyData");
        logMessage(QObject::tr("%1 Return Base64 pony data for selected player").arg(indent));
        logMessage("sendPonies");
        logMessage(QObject::tr("%1 Resends ponies to the given player. Debug command").arg(indent));
        logMessage("sendUtils3");
        logMessage(QObject::tr("%1 Sends a UDP sendUtils3 request").arg(indent));
        logMessage("setPlayerId <playerid>");
        logMessage(QObject::tr("%1 Set player's playerid to the given number").arg(indent));
        logMessage("reloadNpc <npcname>");
        logMessage(QObject::tr("%1 Reloads the given NPC from our database").arg(indent));
        logMessage("removekill <killcode>");
        logMessage(QObject::tr("%1 Kills the selected player, sending the given killcode to the client").arg(indent));
        logMessage("remove <viewid>");
        logMessage(QObject::tr("%1 Removes the selected view id").arg(indent));
        logMessage("sendPonyData <data>");
        logMessage(QObject::tr("%1 Sends the selected pony data to the player").arg(indent));
        logMessage("setStat <statid> <value>");
        logMessage(QObject::tr("%1 Sets the selected stat to <value>").arg(indent));
        logMessage("setMaxStat <statid> <value>");
        logMessage(QObject::tr("%1 Sets the maximum value of the selected stat to <value>").arg(indent));
        logMessage("instantiate");
        logMessage(QObject::tr("%1 DEBUG: Spawns the player's body. Will clone the body").arg(indent));
        logMessage(QObject::tr("%1   if used repeatedly. May lag").arg(indent));
        logMessage("instantiate <key> <viewId> <ownerId>");
        logMessage(QObject::tr("%1 Will spawn key (PlayerBase for a pony). If the Ids are already").arg(indent));
        logMessage(QObject::tr("%1   taken, bad things will happen").arg(indent));
        logMessage("instantiate <key> <viewId> <ownerId> <x1> <y1> <z1>");
        logMessage(QObject::tr("%1 Same than above but spawn at the given position").arg(indent));
        logMessage("instantiate <key> <viewId> <ownerId> <x1> <y1> <z1> <x2> <y2> <z2> <w2>");
        logMessage(QObject::tr("%1 Same than above but spawn at the given position and rotation").arg(indent));
        logMessage("beginDialog");
        logMessage(QObject::tr("%1 DEBUG: Used when talking to NPCs").arg(indent));
        logMessage("endDialog");
        logMessage(QObject::tr("%1 DEBUG: Used when talking to NPCs").arg(indent));
        logMessage("setDialogMsg <dialogue> <npcname>");
        logMessage(QObject::tr("%1 DEBUG: Used when talking to NPCs").arg(indent));
        logMessage("setDialogOptions <arguments separated by spaces>");
        logMessage(QObject::tr("%1 DEBUG: Used when talking to NPCs").arg(indent));
        logMessage("listQuests");
        logMessage(QObject::tr("%1 Lists the quests currently on the server").arg(indent));
        logMessage("listNpcs");
        logMessage(QObject::tr("%1 Lists the NPCs currently on the server").arg(indent));
        logMessage("listMobs");
        logMessage(QObject::tr("%1 Lists the mobs currently on the server").arg(indent));
        logMessage("listInventory");
        logMessage(QObject::tr("%1 Lists the inventory of the selected player").arg(indent));
        logMessage("listWorn");
        logMessage(QObject::tr("%1 Lists all items the player is wearing").arg(indent));
        logMessage("");
        app.printBasicHelp();
        return;
    }
    else if (str == "clear")
    {
#ifdef USE_GUI
        ui->log->clear();
#endif
        return;
    }
    else if (str.startsWith("start", Qt::CaseInsensitive))
    {
        QString target = str.right(str.length() - QString("start ").length());

        if (target.startsWith("login"))
        {
            startLoginServer();
        }
        else if (target.startsWith("game"))
        {
            startGameServer();
        }
        else
        {
            logMessage(tr("What do you want to start?"));
            logMessage(tr("Usage:  start login  or  start game"));
        }

        return;
    }
    else if (str.startsWith("stop", Qt::CaseInsensitive))
    {
        QString target = str.right(str.length() - QString("stop ").length());

        if (target.startsWith("login"))
        {
            stopLoginServer();
        }
        else if (target.startsWith("game"))
        {
            stopGameServer();
        }
        else
        {
            logMessage(tr("What do you want to stop?"));
            logMessage(tr("Usage:  stop login  or  stop game"));
        }

        return;
    }
    else if (str.startsWith("status", Qt::CaseInsensitive))
    {
        // Login server
        if (app.loginServerUp)
        {
            logMessage(tr("Login Server Status: ONLINE"));
        }
        else
        {
            logMessage(tr("Login Server Status: OFFLINE"));
        }

        // Game server
        if (app.gameServerUp)
        {
            logMessage(tr("Game Server Status: ONLINE"));
            int connectedPlayers = Player::udpPlayers.length();
            logMessage(tr("  %1 / %2 Players Connected").arg(connectedPlayers).arg(maxConnected));
        }
        else
        {
            logMessage(tr("Game Server Status: OFFLINE"));
        }

        return;
    }
    else if (str == "exit")
    {
        delete this;
        return;
    }
    else if (str == "listTcpPlayers")
    {
        for (int i=0; i<Player::tcpPlayers.size(); i++)
        {
            Player* p = Player::tcpPlayers[i];
            logMessage(p->name+" "+p->IP+":"+QString().setNum(p->port));
        }
        return;
    }
    else if (str.startsWith("setPeer", Qt::CaseInsensitive))
    {
        if (Player::udpPlayers.size() == 1)
        {
            cmdPeer = Player::udpPlayers[0];
            QString peerName = cmdPeer->IP + " " + QString().setNum(cmdPeer->port);
            logMessage(QObject::tr("UDP: Peer set to %1").arg(peerName));
            return;
        }

        str = str.right(str.size()-8);
        QStringList args = str.split(':');
        bool ok;
        if (args.size() != 2)
        {
            if (args.size() != 1)
            {
                logMessage(QObject::tr("UDP: setPeer takes a pony id or ip:port combination"));
                return;
            }
            quint16 id = args[0].toUInt(&ok);
            if (!ok)
            {
                logMessage(QObject::tr("UDP: setPeer takes a pony id as argument"));
                return;
            }
            for (int i=0; i<Player::udpPlayers.size();i++)
            {
                if (Player::udpPlayers[i]->pony.id == id)
                {
                    cmdPeer = Player::findPlayer(Player::udpPlayers,Player::udpPlayers[i]->IP, Player::udpPlayers[i]->port);
                    logMessage(QObject::tr("UDP: Peer set to %1").arg(Player::udpPlayers[i]->pony.name));
                    return;
                }
            }
            logMessage(QObject::tr("UDP: Peer not found (id %1)").arg(args[0]));
            return;
        }

        quint16 port = args[1].toUInt(&ok);
        if (!ok)
        {
            logMessage(QObject::tr("UDP: setPeer takes a port as argument"));
            return;
        }

        cmdPeer = Player::findPlayer(Player::udpPlayers,args[0], port);
        if (cmdPeer->IP!="")
            logMessage(QObject::tr("UDP: Peer set to %1").arg(str));
        else
            logMessage(QObject::tr("UDP: Peer not found (%1)").arg(str));
        return;
    }
    else if (str.startsWith("listPeers", Qt::CaseInsensitive))
    {
        if (str.size()<=10)
        {
            for (int i=0; i<Player::udpPlayers.size();i++)
                app.logMessage(QString().setNum(Player::udpPlayers[i]->pony.id)
                               +" ("+QString().setNum(Player::udpPlayers[i]->pony.netviewId)+")"
                               +"   "+Player::udpPlayers[i]->name
                               +" / "+Player::udpPlayers[i]->pony.name
                               +"   "+Player::udpPlayers[i]->IP
                               +":"+QString().setNum(Player::udpPlayers[i]->port)
                               +"   "+QString().number(timestampNow()-Player::udpPlayers[i]->lastPingTime, 'f', 3)+"s");
            return;
        }
        str = str.right(str.size()-10);
        Scene* scene = findScene(str);
        if (scene->name.isEmpty())
            logMessage(QObject::tr("Can't find scene"));
        else
            for (int i=0; i<scene->players.size();i++)
                logMessage(Player::udpPlayers[i]->IP
                               +":"+QString().setNum(Player::udpPlayers[i]->port)
                               +" "+QString().setNum((int)timestampNow()-Player::udpPlayers[i]->lastPingTime)+"s");
        return;
    }
    else if (str.startsWith("listVortexes", Qt::CaseInsensitive))
    {
        for (int i=0; i<Scene::scenes.size(); i++)
        {
            logMessage("Scene "+Scene::scenes[i].name);
            for (int j=0; j<Scene::scenes[i].vortexes.size(); j++)
                logMessage("Vortex "+QString().setNum(Scene::scenes[i].vortexes[j].id)
                               +" to "+Scene::scenes[i].vortexes[j].destName+" "
                               +QString().setNum(Scene::scenes[i].vortexes[j].destPos.x)+" "
                               +QString().setNum(Scene::scenes[i].vortexes[j].destPos.y)+" "
                               +QString().setNum(Scene::scenes[i].vortexes[j].destPos.z));
        }
        return;
    }
    else if (str.startsWith("sync", Qt::CaseInsensitive))
    {
        logMessage(QObject::tr("UDP: Syncing manually"));
        sync->doSync();
        return;
    }
    // DEBUG global commands from now on
    else if (str==("dbgStressLoad"))
    {
        // Send all the players to the GemMines at the same time
        for (int i=0; i<Player::udpPlayers.size(); i++)
            sendLoadSceneRPC(Player::udpPlayers[i], "GemMines");
        return;
    }
    else if (str.startsWith("dbgStressLoad", Qt::CaseInsensitive))
    {
        str = str.mid(14);
        // Send all the players to the given scene at the same time
        for (int i=0; i<Player::udpPlayers.size(); i++)
            sendLoadSceneRPC(Player::udpPlayers[i], str);
        return;
    }
    else if (str.startsWith("tele", Qt::CaseInsensitive))
    {
        str = str.right(str.size()-5);
        QStringList args = str.split(' ');
        if (args.size() != 2)
        {
            logError(QObject::tr("Error: Usage is tele ponyIdToMove destinationPonyId"));
            return;
        }
        bool ok;
        bool ok1;
        bool ok2 = false;
        quint16 sourceID = args[0].toUInt(&ok);
        quint16 destID = args[1].toUInt(&ok1);
        Player* sourcePeer;
        if (!ok && !ok1)
        {
            logError(QObject::tr("Error: Usage is tele ponyIdToMove destinationPonyId"));
            return;
        }
        for (int i=0; i<Player::udpPlayers.size();i++)
        {
            if (Player::udpPlayers[i]->pony.id == sourceID)
            {
                sourcePeer = Player::udpPlayers[i];
                ok2 = true;
                break;
            }
        }
        if (!ok2)
        {
            logError(QObject::tr("Error: Source peer does not exist!"));
            return;
        }
        for (int i=0; i<Player::udpPlayers.size();i++)
        {
            if (Player::udpPlayers[i]->pony.id == destID)
            {
                logMessage(QObject::tr("UDP: Teleported %1 to %2").arg(sourcePeer->pony.name,Player::udpPlayers[i]->pony.name));
                if (Player::udpPlayers[i]->pony.sceneName.toLower() != sourcePeer->pony.sceneName.toLower())
                    sendLoadSceneRPC(sourcePeer, Player::udpPlayers[i]->pony.sceneName, Player::udpPlayers[i]->pony.pos);
                else
                    sendMove(sourcePeer, Player::udpPlayers[i]->pony.pos.x, Player::udpPlayers[i]->pony.pos.y, Player::udpPlayers[i]->pony.pos.z);
                return;
            }
        }
        logError(QObject::tr("Error: Destination peer does not exist!"));
    }
    if (cmdPeer->IP=="")
    {
        logError(QObject::tr("Select a peer first with setPeer/listPeers"));
        return;
    }
    else // Refresh peer info
    {
        cmdPeer = Player::findPlayer(Player::udpPlayers,cmdPeer->IP, cmdPeer->port);
        if (cmdPeer->IP=="")
        {
            logMessage(QObject::tr("UDP: Peer not found"));
            return;
        }
    }

    // User commands from now on (requires setPeer)
    if (str.startsWith("disconnect", Qt::CaseInsensitive))
    {
        logMessage(QObject::tr("UDP: Disconnecting"));
        sendMessage(cmdPeer,MsgDisconnect, "You were kicked by the server admin");
        Player::disconnectPlayerCleanup(cmdPeer); // Save game and remove the player
    }
    else if (str.startsWith("load", Qt::CaseInsensitive))
    {
        str = str.mid(5);
        sendLoadSceneRPC(cmdPeer, str);
    }
    else if (str.startsWith("getPos", Qt::CaseInsensitive))
    {
        logMessage(QObject::tr("pos ","Short for position") + QString().setNum(cmdPeer->pony.pos.x)
                   + " " + QString().setNum(cmdPeer->pony.pos.y)
                   + " " + QString().setNum(cmdPeer->pony.pos.z));
    }
    else if (str.startsWith("getRot", Qt::CaseInsensitive))
    {
        logMessage(QObject::tr("rot ","Short for rotation") + QString().setNum(cmdPeer->pony.rot.x)
                   + " " + QString().setNum(cmdPeer->pony.rot.y)
                   + " " + QString().setNum(cmdPeer->pony.rot.z)
                   + " " + QString().setNum(cmdPeer->pony.rot.w));
    }
    else if (str.startsWith("getPonyData", Qt::CaseInsensitive))
    {
        logMessage("ponyData : "+cmdPeer->pony.ponyData.toBase64());
    }
    else if (str.startsWith("sendPonies", Qt::CaseInsensitive))
    {
        sendPonies(cmdPeer);
    }
    else if (str.startsWith("sendUtils3", Qt::CaseInsensitive))
    {
        logMessage(QObject::tr("UDP: Sending Utils3 request"));
        QByteArray data(1,3);
        sendMessage(cmdPeer,MsgUserReliableOrdered6,data);
    }
    else if (str.startsWith("setPlayerId", Qt::CaseInsensitive))
    {
        str = str.right(str.size()-12);
        QByteArray data(3,4);
        bool ok;
        unsigned id = str.toUInt(&ok);
        if (ok)
        {
            logMessage(QObject::tr("UDP: Sending setPlayerId request"));
            data[1]=(quint8)(id&0xFF);
            data[2]=(quint8)((id >> 8)&0xFF);
            sendMessage(cmdPeer,MsgUserReliableOrdered6,data);
        }
        else
            logError(tr("Error : Player ID must be a number"));
    }
    else if (str.startsWith("reloadNpc", Qt::CaseInsensitive))
    {
        str = str.mid(10);
        Pony* npc = NULL;
        for (int i=0; i<Quest::npcs.size(); i++)
            if (Quest::npcs[i]->name == str)
            {
                npc = Quest::npcs[i];
                break;
            }
        if (npc != NULL)
        {
            // Reload the NPCs from the DB
            Quest::npcs.clear();
            Quest::quests.clear();
            unsigned nQuests = 0;
            QDir npcsDir("data/npcs/");
            QStringList files = npcsDir.entryList(QDir::Files);
            for (int i=0; i<files.size(); i++, nQuests++) // For each vortex file
            {
                Quest *quest = new Quest("data/npcs/"+files[i], NULL);
                Quest::quests << *quest;
                Quest::npcs << quest->npc;
            }
            logMessage(tr("Loaded %1 quests/npcs").arg(nQuests));

            // Resend the NPC if needed
            if (npc->sceneName.toLower() == cmdPeer->pony.sceneName.toLower())
            {
                sendNetviewRemove(cmdPeer, npc->netviewId);
                sendNetviewInstantiate(npc, cmdPeer);
            }
        }
        else
            logMessage(tr("NPC not found"));
    }
    else if (str.startsWith("removekill", Qt::CaseInsensitive))
    {
        str = str.right(str.size()-11);
        QByteArray data(4,2);
        bool ok;
        unsigned id = str.toUInt(&ok);
        if (ok)
        {
            logMessage(tr("UDP: Sending remove request with kill reason code"));
            data[1]=id;
            data[2]=id >> 8;
            data[3]=NetviewRemoveReasonKill;
            sendMessage(cmdPeer,MsgUserReliableOrdered6,data);
        }
        else
            logError(tr("Error : Removekill needs the id of the view to remove"));
    }
    else if (str.startsWith("remove", Qt::CaseInsensitive))
    {
        str = str.right(str.size()-7);
        QByteArray data(3,2);
        bool ok;
        unsigned id = str.toUInt(&ok);
        if (ok)
        {
            logMessage(tr("UDP: Sending remove request"));
            data[1]=id;
            data[2]=id >> 8;
            sendMessage(cmdPeer,MsgUserReliableOrdered6,data);
        }
        else
            logError(tr("Error : Remove needs the id of the view to remove"));
    }
    else if (str.startsWith("sendPonyData", Qt::CaseInsensitive))
    {
        QByteArray data(3,0xC8);
        data[0] = (quint8)(cmdPeer->pony.netviewId&0xFF);
        data[1] = (quint8)((cmdPeer->pony.netviewId>>8)&0xFF);
        data += cmdPeer->pony.ponyData;
        sendMessage(cmdPeer, MsgUserReliableOrdered18, data);
        return;
    }
    else if (str.startsWith("setStat", Qt::CaseInsensitive))
    {
        str = str.right(str.size()-8);
        QStringList args = str.split(' ');
        if (args.size() != 2)
        {
            logError(tr("Error : usage is setStat StatID StatValue"));
            return;
        }
        bool ok,ok2;
        quint8 statID = args[0].toInt(&ok);
        float statValue = args[1].toFloat(&ok2);
        if (!ok || !ok2)
        {
            logError(tr("Error : usage is setStat StatID StatValue"));
            return;
        }
        sendSetStatRPC(cmdPeer, statID, statValue);
    }
    else if (str.startsWith("setMaxStat", Qt::CaseInsensitive))
    {
        str = str.right(str.size()-11);
        QStringList args = str.split(' ');
        if (args.size() != 2)
        {
            logError(tr("Error : usage is setMaxStat StatID StatValue"));
            return;
        }
        bool ok,ok2;
        quint8 statID = args[0].toInt(&ok);
        float statValue = args[1].toFloat(&ok2);
        if (!ok || !ok2)
        {
            logError(tr("Error : usage is setMaxStat StatID StatValue"));
            return;
        }
        sendSetMaxStatRPC(cmdPeer, statID, statValue);
    }
    else if (str.startsWith("instantiate", Qt::CaseInsensitive))
    {
        if (str == "instantiate")
        {
            logMessage(tr("UDP: Instantiating"));
            sendNetviewInstantiate(cmdPeer);
            return;
        }

        QByteArray data(1,1);
        str = str.right(str.size()-12);
        QStringList args = str.split(' ');

        if (args.size() != 3 && args.size() != 6 && args.size() != 10)
        {
            logError(tr("Error : Instantiate takes 0,3,6 or 10 arguments").append(str));
            return;
        }
        // Au as au moins les 3 premiers de toute facon
        data += stringToData(args[0]);
        unsigned viewId, ownerId;
        bool ok1, ok2;
        viewId = args[1].toUInt(&ok1);
        ownerId = args[2].toUInt(&ok2);
        if (!ok1 || !ok2)
        {
            logError(tr("Error : instantiate key viewId ownerId x1 y1 z1 x2 y2 z2 w2"));
            return;
        }
        QByteArray params1(4,0);
        params1[0] = (quint8)(viewId&0xFF);
        params1[1] = (quint8)((viewId >> 8)&0xFF);
        params1[2] = (quint8)(ownerId&0xFF);
        params1[3] = (quint8)((ownerId >> 8)&0xFF);
        data += params1;
        float x1=0,y1=0,z1=0;
        float x2=0,y2=0,z2=0,w2=0;
        if (args.size() >= 6) // Si on a le vecteur position on l'ajoute
        {
            bool ok1, ok2, ok3;
            x1=args[3].toFloat(&ok1);
            y1=args[4].toFloat(&ok2);
            z1=args[5].toFloat(&ok3);

            if (!ok1 || !ok2 || !ok3)
            {
                logError(tr("Error : instantiate key viewId ownerId x1 y1 z1 x2 y2 z2 w2"));
                return;
            }
        }
        data+=floatToData(x1);
        data+=floatToData(y1);
        data+=floatToData(z1);

        if (args.size() == 10) // Si on a le quaternion rotation on l'ajoute
        {
            bool ok1, ok2, ok3, ok4;
            x2=args[6].toFloat(&ok1);
            y2=args[7].toFloat(&ok2);
            z2=args[8].toFloat(&ok3);
            w2=args[9].toFloat(&ok4);

            if (!ok1 || !ok2 || !ok3 || !ok4)
            {
                logError(tr("Error : instantiate key viewId ownerId x1 y1 z1 x2 y2 z2 w2"));
                return;
            }
        }
        data+=floatToData(x2);
        data+=floatToData(y2);
        data+=floatToData(z2);
        data+=floatToData(w2);

        logMessage(tr("UDP: Instantiating %1").arg(args[0]));
        sendMessage(cmdPeer,MsgUserReliableOrdered6,data);
    }
    else if (str.startsWith("beginDialog", Qt::CaseInsensitive))
    {
        QByteArray data(1,0);
        data[0] = 11; // Request number

        sendMessage(cmdPeer,MsgUserReliableOrdered4, data);
    }
    else if (str.startsWith("endDialog", Qt::CaseInsensitive))
    {
        QByteArray data(1,0);
        data[0] = 13; // Request number

        sendMessage(cmdPeer,MsgUserReliableOrdered4, data);
    }
    else if (str.startsWith("setDialogMsg", Qt::CaseInsensitive))
    {
        str = str.right(str.size()-13);
        QStringList args = str.split(" ", QString::SkipEmptyParts);
        if (args.size() != 2)
            logError(tr("setDialogMsg takes two args : dialog and npc name"));
        else
        {
            QByteArray data(1,0);
            data[0] = 0x11; // Request number
            data += stringToData(args[0]);
            data += stringToData(args[1]);
            data += (char)0; // emoticon
            data += (char)0; // emoticon

            sendMessage(cmdPeer,MsgUserReliableOrdered4, data);
        }
    }
    else if (str.startsWith("setDialogOptions", Qt::CaseInsensitive))
    {
        str = str.right(str.size()-17);
        QStringList args = str.split(" ", QString::SkipEmptyParts);
        sendDialogOptions(cmdPeer, args);
    }
    else if (str.startsWith("move", Qt::CaseInsensitive))
    {
        str = str.right(str.size()-5);
        QByteArray data(1,0);
        data[0] = 0xce; // Request number

        // Serialization : float x, float y, float z
        QStringList coords = str.split(' ');
        if (coords.size() != 3)
            return;

        sendMove(cmdPeer, coords[0].toFloat(), coords[1].toFloat(), coords[2].toFloat());
    }
    else if (str.startsWith("error", Qt::CaseInsensitive))
    {
        str = str.right(str.size()-6);
        QByteArray data(1,0);
        data[0] = 0x7f; // Request number

        data += stringToData(str);

        sendMessage(cmdPeer,MsgUserReliableOrdered4, data);
    }
    else if (str==("listQuests"))
    {
        for (const Quest& quest : cmdPeer->pony.quests)
        {
            logMessage(tr("Quest ")+QString().setNum(quest.id)+" ("+*(quest.name)
                           +") : "+QString().setNum(quest.state));
        }
    }
    else if (str==("listNpcs"))
    {
        for (const Pony* npc : Quest::npcs)
        {
            logMessage(tr("NPC ")+QString().setNum(npc->id)+"/"+QString().setNum(npc->netviewId)
                           +" "+npc->name);
        }
    }
    else if (str==("listMobs"))
    {
        for (const Mob* mob : Mob::mobs)
        {
            logMessage(tr("Mob ","As in a monster, a mob you can kill")+QString().setNum(mob->id)
                           +"/"+QString().setNum(mob->netviewId)
                           +" "+mob->modelName+" at "+QString().setNum(mob->pos.x)
                           +" "+QString().setNum(mob->pos.y)
                           +" "+QString().setNum(mob->pos.z));
        }
    }
    else if (str==("listInventory"))
    {
        for (const InventoryItem& item : cmdPeer->pony.inv)
        {
            logMessage(tr("Item ","As in something from the inventory")
                           +QString().setNum(item.id)+tr(" (pos ","Short for position")+QString().setNum(item.index)
                           +") : "+QString().setNum(item.amount));
        }
    }
    else if (str==("listWorn"))
    {
        for (const WearableItem& item : cmdPeer->pony.worn)
        {
            logMessage(tr("Item ","As in something from the inventory")
                           +QString().setNum(item.id)+tr(" : slot ","A slot in the inventory")
                           +QString().setNum(item.index));
        }
    }
}
