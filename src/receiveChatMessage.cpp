#include "receiveChatMessage.h"
#include "player.h"
#include "serialize.h"
#include "message.h"
#include "app.h"
#include "log.h"
#include "scene.h"

void receiveChatMessage(QByteArray msg, Player* player)
{
    QString author = player->pony.name;
    quint8 accessLevel = player->accessLvl;
    quint8 accessServer = 0;
    quint8 channel = (quint8)msg[6];
    int msgIndex = 7;
    quint8 msgLength = (quint8)msg[msgIndex];
    QStringList messages;

    // grab all messages
    while(msgLength > 0)
    {
        messages << dataToString(msg.mid(msgIndex, msgLength+1));
        msgIndex = msgIndex+msgLength+1;
        msgLength = (quint8)msg[msgIndex];
        //logMessage(QObject::tr("msgIndex:%1 msgLength:%2 msg.Size:%3").arg(msgIndex).arg(msgLength).arg(msg.size()));
    }

//    for (int i=0;i < messages.size();i++)
//    {
//        logMessage(QObject::tr("message[%1/%2]: %3").arg(i).arg(messages.size()).arg(messages[i]));
//    }

    if (messages[0].startsWith("/stuck") || messages[0].startsWith("unstuck me")) // "/stuck" is sent as "unstuck me" from client
    {
        sendLoadSceneRPC(player, player->pony.sceneName);
        return;
    }

    if (messages[0].startsWith(":commands", Qt::CaseInsensitive) || messages[0].startsWith(":help", Qt::CaseInsensitive))  // "/help" is handled by the client
    {
        sendChatMessage(player, QString("<span color=\"yellow\">List of additional Commands:</span><br />")
                        +QString("<em>:players</em><br /><span color=\"yellow\">Lists all players on the server</span><br />")
                        +QString("<em>:w player message</em><br /><span color=\"yellow\">Sends a private message to a player</span><br />")
                        +QString("<em>:me action</em><br /><span color=\"yellow\">States your current action</span><br />")
                        +QString("<em>:tp location</em><br /><span color=\"yellow\">Teleports your pony to the specified region</span><br />")
                        +QString("<em>:roll</em><br /><span color=\"yellow\">Rolls a random number between 00 and 99</span>"), "[Server]", channel, accessServer);
        return;
    }

    // list all players in all instances
    if (messages[0].startsWith(":players", Qt::CaseInsensitive))
    {
        QString namesmsg2;
        int playersInGame = 0;

        for (int i=0; i<Player::udpPlayers.size(); i++)
            if (Player::udpPlayers[i]->inGame>=1)
            {
                playersInGame++;
                namesmsg2 += "<br />#b" + Player::udpPlayers[i]->pony.name;
                if (player->accessLvl >= 3)
                    namesmsg2 += " (" + Player::udpPlayers[i]->name + ")";
                namesmsg2 += "#b<br /><span color=\"yellow\"> - in "
                            + Player::udpPlayers[i]->pony.sceneName + "</span>";
            }

        QString namesmsg = QString("<span color=\"yellow\">%1 Players currently in game:</span>").arg(playersInGame);
        namesmsg += namesmsg2;

        sendChatMessage(player, namesmsg, "[Server]", channel, accessServer);
        return;
    }

    // allow players to teleport to any scene
    if (messages[0].startsWith(":tp", Qt::CaseInsensitive))
    {
        if (messages[0].count(" ") < 1)
        {
          QString msgtosend = ":tp<br /><span color=\"yellow\">Usage:</span><br /><em>:tp location</em><br /><span color=\"yellow\">Available locations:</span><em>";

            for (int i=0; i<Scene::scenes.size(); i++)
                msgtosend += "<br />" + Scene::scenes[i].name;

            sendChatMessage(player, msgtosend + "</em>", "[Server]", channel, accessServer);
        }
        else
        {
            QString scene = messages[0].remove(0, 4);

            if (sendLoadSceneRPC(player, scene))
            {
                sendChatMessage(player, QObject::tr("<span color=\"yellow\">teleporting to %1</span>").arg(scene), "[Server]", channel, accessServer); //show our command to us
            }
            else
            {
                sendChatMessage(player, QObject::tr("<span color=\"yellow\">teleporting to %1 failed</span>").arg(scene), "[Server]", channel, accessServer); //show our command to us
            }
        }
        return;
    }

    // advanced whisper :w
    if (messages[0].startsWith(":w", Qt::CaseInsensitive))
    {
        if(messages[0].count(" ") < 2)
        {
            sendChatMessage(player, ":w<br /><span color=\"yellow\">Usage:</span><br /><em>:w player message</em><br /><span color=\"yellow\">Player names are case-insensitive, ignore spaces and you do not need to type out their full name.</span>", "[Server]", channel, accessServer);
            return;
        }

        QString recipient = "";
        QString recipientsFound = "";

        if (messages[0].count("\"") >= 2) // search for exact match
        {
            recipient = messages[0].section("\"", 1, 1);
            QString message = messages[0].section("\"",2);

            if (message.isEmpty() || recipient.length() < 3) return;

            for (int i=0; i<Player::udpPlayers.size(); i++)
            {
                if (Player::udpPlayers[i]->inGame>=1 &&
                    Player::udpPlayers[i]->pony.name == recipient )
                {
                    // until we can make whisper chat get update notice send it to all tabs
                    sendChatBroadcast(player, "#c19B1FE" + message, "to " + Player::udpPlayers[i]->pony.name, accessLevel); // show in own messages
                    sendChatBroadcast(Player::udpPlayers[i], "#c19B1FE" + message, "from " + author, accessLevel); // show in recipients messages
                    recipientsFound += Player::udpPlayers[i]->pony.name +",";
                }
            }
        }
        else //use best match
        {
            recipient = messages[0].section(" ", 1, 1).toLower();
            QString message = messages[0].section(" ",2);

            if (message.isEmpty() || recipient.length() < 3) return;

            for (int i=0; i<Player::udpPlayers.size(); i++)
            {
                if (Player::udpPlayers[i]->inGame>=1 &&
                    Player::udpPlayers[i]->pony.name.toLower().remove(" ").startsWith(recipient))
                {
                    // until we can make whisper chat get update notice send it to all tabs
                    sendChatBroadcast(player, "#c19B1FE" + message, "to " + Player::udpPlayers[i]->pony.name, accessLevel); // show in own messages
                    sendChatBroadcast(Player::udpPlayers[i], "#c19B1FE" + message, "from" + author, accessLevel); // show in recipients messages
                    recipientsFound += Player::udpPlayers[i]->pony.name +",";
                }
            }
        }

        if (recipientsFound.count(",") < 1)
            sendChatBroadcast(player, QString("<span color=\"yellow\">player %1 not found</span>").arg(recipient), "[Server]", accessServer);

        return;
    }

    // normal whisper /w
    if (channel == ChatWhisper && messages.size() == 2)
    {
        if (messages[0].length() < 3 || messages[1].length() < 3)
            return;

        QString recipientsFound = "";
        for (int i=0; i<Player::udpPlayers.size(); i++)
        {
            if (Player::udpPlayers[i]->inGame>=1 &&
                Player::udpPlayers[i]->pony.name.toLower().remove(" ").startsWith(messages[0].toLower()))
            {
                // until we can make whisper chat get update notice send it to all tabs
                sendChatBroadcast(player, "#c19B1FE" + messages[1], "#to " + Player::udpPlayers[i]->pony.name, accessLevel); // show in own messages
                sendChatBroadcast(Player::udpPlayers[i], "#c19B1FE" + messages[1], "#from " + author, accessLevel); // show in recipients messages
                recipientsFound += Player::udpPlayers[i]->pony.name +",";
            }
        }

        if (recipientsFound.count(",") < 1)
            sendChatBroadcast(player, QString("<span color=\"yellow\">player %1 not found</span>").arg(messages[0]), "[Server]", accessServer);

        return;
    }

    // broadcast chat emote in current channel
    if (messages[0].startsWith(":me", Qt::CaseInsensitive))
    {
        if (messages[0].count(" ") < 1)
                sendChatMessage(player, ":me<br /><span color=\"yellow\">Usage:</span><br /><em>:me action</em>", "[Server]", channel, accessServer);
        else
        {
            messages[0].remove(0, 3);
            messages[0] = "<em>#b* " + author + "#b" + messages[0] + " *</em>";
            sendChannelMessage(player->pony.sceneName, channel, messages[0], "", 0);
        }
        return;
    }

    // roll a dice
    if (messages[0].startsWith(":roll", Qt::CaseInsensitive))
    {
        int rollnum = -1;
        QString rollstr;

        if (player->chatRollCooldownEnd < QDateTime::currentDateTime())
        {
           rollnum = qrand() % 100;
           rollstr.sprintf("<span color=\"yellow\">#b%s#b rolls %02d</span>", author.toLocal8Bit().data(), rollnum);
           player->chatRollCooldownEnd = QDateTime::currentDateTime().addSecs(10);
           sendChannelMessage(player->pony.sceneName, channel, rollstr, "[Server]", accessServer);
        }
        return;
    }

    // Broadcast the message in current channel
    if (messages[0].length() > 0)
    {
        if (messages[0].startsWith(">>") && accessLevel >= 3) // temp gm broadcast
        {
            messages[0].remove(0, 2);
            //messages[0] = "#b<span color=\"red\">" + messages[0] + "</span>";
            for (int i=0; i<Player::udpPlayers.size(); i++)
            {
                if (Player::udpPlayers[i]->inGame>=1)
                {
                    sendChatBroadcast(Player::udpPlayers[i], messages[0], author, accessLevel);
                }
            }
        }
        else
            sendChannelMessage(player->pony.sceneName, channel, messages[0], author, accessLevel);
    }
}
