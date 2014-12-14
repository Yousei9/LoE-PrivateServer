#include "message.h"
#include "player.h"
#include "sync.h"
#include "utils.h"
#include "serialize.h"
#include "receiveAck.h"
#include "receiveChatMessage.h"
#include "mob.h"
#include "packetloss.h"
#include "skill.h"
#include "scene.h"
#include "sceneEntity.h"
#include "log.h"
#include "settings.h"

#define DEBUG_LOG false

void receiveMessage(Player* player)
{
    QByteArray msg = *(player->receivedDatas);
    int msgSize=5 + (((unsigned char)msg[3]) + (((unsigned char)msg[4]) << 8))/8;

#if UDP_SIMULATE_PACKETLOSS
    if (qrand() % 100 <= UDP_RECV_PERCENT_DROPPED)
    {
        //app.logMessage("UDP: Received packet dropped !");
        *(player->receivedDatas) = player->receivedDatas->mid(msgSize);
        if (player->receivedDatas->size())
            receiveMessage(player);
        return; // When we're done with the recursion, we still need to skip this message.
    }
    else
    {
        //app.logMessage("UDP: Received packet got through !");
    }
#endif

    // Check the sequence (seq) of the received messag
    if ((unsigned char)msg[0] >= MsgUserReliableOrdered1 && (unsigned char)msg[0] <= MsgUserReliableOrdered32)
    {
        quint16 seq = (quint8)msg[1] + ((quint8)msg[2]<<8);
        quint8 channel = ((unsigned char)msg[0])-MsgUserReliableOrdered1;
        if (seq <= player->udpRecvSequenceNumbers[channel] && player->udpRecvSequenceNumbers[channel]!=0)
        {
            // If this is a missing packet, accept it
            MessageHead missingMsg;
            missingMsg.channel = channel;
            missingMsg.seq = seq;
            if (player->udpRecvMissing.contains(missingMsg))
            {
                logMessage(QObject::tr("UDP: Processing retransmission (-%1) from %2")
                               .arg(player->udpRecvSequenceNumbers[channel]-seq).arg(player->pony.netviewId));
                for (int i=0; i<player->udpRecvMissing.size(); i++)
                    if (player->udpRecvMissing[i] == missingMsg)
                        player->udpRecvMissing.remove(i);
            }
            else
            {
                // We already processed this packet, we should discard it
#if DEBUG_LOG
                app.logMessage("UDP: Discarding double message (-"+QString().setNum(player->udpRecvSequenceNumbers[channel]-seq)
                               +") from "+QString().setNum(player->pony.netviewId));
                app.logMessage("UDP: Message was : "+QString(player->receivedDatas->left(msgSize).toHex().data()));
#endif
                player->nReceivedDups++;
                if (player->nReceivedDups >= 100) // Kick the player if he's infinite-looping on us
                {
                    logMessage(QObject::tr("UDP: Kicking %1 : Too many packet dups").arg(player->pony.netviewId));
                    sendMessage(player,MsgDisconnect, "You were kicked for lagging the server, sorry. You can login again.");
                    Player::disconnectPlayerCleanup(player); // Save game and remove the player
                    return;
                }
                *(player->receivedDatas) = player->receivedDatas->mid(msgSize);

                // Ack if needed, so that the client knows to move on already.
                if ((unsigned char)msg[0] >= MsgUserReliableOrdered1 && (unsigned char)msg[0] <= MsgUserReliableOrdered32) // UserReliableOrdered
                {
#if DEBUG_LOG
                    app.logMessage("UDP: ACKing discarded message");
#endif
                    QByteArray data(3,0);
                    data[0] = (quint8)(msg[0]); // ack type
                    data[1] = (quint8)(((quint8)msg[1])/2); // seq
                    data[2] = (quint8)(((quint8)msg[2])/2); // seq
                    sendMessage(player, MsgAcknowledge, data);
                }
                if (player->receivedDatas->size())
                    receiveMessage(player);
                return; // When we're done with the recursion, we still need to skip this message.
            }
        }
        else if (seq > player->udpRecvSequenceNumbers[channel]+2) // If a message was skipped, keep going.
        {
            logMessage(QObject::tr("UDP: Unordered message (+%1) received from %2")
                           .arg(seq-player->udpRecvSequenceNumbers[channel]).arg(player->pony.netviewId));
            player->udpRecvSequenceNumbers[channel] = seq;

            // Mark the packets we skipped as missing
            for (int i=player->udpRecvSequenceNumbers[channel]+2; i<seq; i+=2)
            {
                MessageHead missing;
                missing.channel = channel;
                missing.seq = i;
                player->udpRecvMissing.append(missing);
            }
        }
        else
        {
            if (player->nReceivedDups > 0) // If he stopped sending dups, forgive him slowly.
                player->nReceivedDups--;
            //app.logMessage("UDP: Received message (="+QString().setNum(seq)
            //               +") from "+QString().setNum(player->pony.netviewId));
            player->udpRecvSequenceNumbers[channel] = seq;
        }
    }

    // Process the received message
    if ((unsigned char)msg[0] == MsgPing) // Ping
    {
        //app.logMessage("UDP: Ping received from "+player->IP+":"+QString().setNum(player->port)
        //        +" ("+QString().setNum((timestampNow() - player->lastPingTime))+"s)");
        player->lastPingNumber = (quint8)msg[5];
        player->lastPingTime = timestampNow();
        sendMessage(player,MsgPong);
    }
    else if ((unsigned char)msg[0] == MsgPong) // Pong
    {
#if DEBUG_LOG
        //app.logMessage("UDP: Pong received");
#endif
    }
    else if ((unsigned char)msg[0] == MsgConnect) // Connect SYN
    {
        msg.resize(18); // Supprime le message LocalHail et le Timestamp
        msg = msg.right(13); // Supprime le Header
#if DEBUG_LOG
        app.logMessage(QString("UDP: Connecting ..."));
#endif

        for (int i=0; i<32; i++) // Reset sequence counters
            player->udpSequenceNumbers[i]=0;

        if (!player->connected)
            sendMessage(player, MsgConnectResponse, msg);
    }
    else if ((unsigned char)msg[0] == MsgConnectionEstablished) // Connect ACK
    {
        if (player->connected)
            logMessage(QObject::tr("UDP: Received duplicate connect ACK"));
        else
        {
            logMessage(QObject::tr("UDP: Connected to client"));
            player->connected=true;
            for (int i=0; i<32; i++) // Reset sequence counters
                player->udpSequenceNumbers[i]=0;
            onConnectAckReceived(player); // Clean the reliable message queue from SYN|ACKs

            // Start game
    #if DEBUG_LOG
            logMessage(QString("UDP: Starting game"));
    #endif
            // Set player id
            SceneEntity::lastIdMutex.lock();
            player->pony.id = SceneEntity::getNewId();
            player->pony.netviewId = SceneEntity::getNewNetviewId();
            SceneEntity::lastIdMutex.unlock();
            logMessage(QObject::tr("UDP: Set id request : %1/%2").arg(player->pony.id).arg(player->pony.netviewId));
            QByteArray id(3,0); // Set player Id request
            id[0]=4;
            id[1]=(quint8)(player->pony.id&0xFF);
            id[2]=(quint8)((player->pony.id>>8)&0xFF);
            sendMessage(player,MsgUserReliableOrdered6,id);

            // Load characters screen request
            QByteArray data(1,5);
            data += stringToData("characters");
            sendMessage(player,MsgUserReliableOrdered6,data);
        }
    }
    else if ((unsigned char)msg[0] == MsgAcknowledge) // Acknowledge
    {
        onAckReceived(msg, player);
    }
    else if ((unsigned char)msg[0] == MsgDisconnect) // Disconnect
    {
        logMessage(QObject::tr("UDP: Client disconnected"));
        Player::disconnectPlayerCleanup(player); // Save game and remove the player
        return; // We can't use Player& player anymore, it refers to free'd memory.
    }
    else if ((unsigned char)msg[0] >= MsgUserReliableOrdered1 && (unsigned char)msg[0] <= MsgUserReliableOrdered32) // UserReliableOrdered
    {
        //app.logMessage("UDP: Data received (hex) : \n"+player->receivedDatas->toHex().constData());

        QByteArray data(3,0);
        data[0] = (quint8)msg[0]; // ack type
        data[1] = (quint8)(((quint8)msg[1])/2); // seq
        data[2] = (quint8)(((quint8)msg[2])/2); // seq
        sendMessage(player, MsgAcknowledge, data);

        if ((unsigned char)msg[0]==MsgUserReliableOrdered6 && (unsigned char)msg[3]==8 && (unsigned char)msg[4]==0 && (unsigned char)msg[5]==6 ) // Prefab (player/mobs) list instantiate request
        {
            sendEntitiesList(player); // Called when the level was loaded on the client side
        }
        else if ((unsigned char)msg[0]==MsgUserReliableOrdered6 && (unsigned char)msg[3]==0x18 && (unsigned char)msg[4]==0 && (unsigned char)msg[5]==8 ) // Player game info (inv/ponyData/...) request
        {
            sendPonySave(player, msg); // Called when instantiate finished on the client side
        }
        else if ((unsigned char)msg[0]==MsgUserReliableOrdered4 && (unsigned char)msg[5]==0xf) // Chat
        {
            receiveChatMessage(msg, player);
        }
        else if ((quint8)msg[0]==MsgUserReliableOrdered4 && (quint8)msg[5]==0x1 && player->inGame!=0) // Edit ponies request error (happens if you click play twice quicly, for example)
        {
            logMessage(QObject::tr("UDP: Rejecting game start request from %1 : player already in game")
                           .arg(player->pony.netviewId));
            // Fix the buggy state we're now in
            // Reload to hide the "saving ponies" message box
            QByteArray data(1,5);
            data += stringToData(player->pony.sceneName);
            sendMessage(player,MsgUserReliableOrdered6,data);
            // Try to cancel the loading callbacks with inGame=1
            player->inGame = 1;
        }
        else if ((quint8)msg[0]==MsgUserReliableOrdered4 && (quint8)msg[5]==0x1 && player->inGame==0) // Edit ponies request
        {
            QList<Pony> ponies = Player::loadPonies(player);
            QByteArray ponyData = msg.right(msg.size()-10);
            Pony pony{player};

            // Fix invalid names
            QString ponyName = dataToString(ponyData);
            if (ponyName.count(' ') > 1)
            {
                QStringList words = ponyName.split(' ');
                ponyName = words[0] + ' ' + words[1];
            }

            if ((unsigned char)msg[6]==0xff && (unsigned char)msg[7]==0xff && (unsigned char)msg[8]==0xff && (unsigned char)msg[9]==0xff)
            {
                // Create the new pony for this player
                pony.ponyData = ponyData;
                pony.name = dataToString(ponyData);
                if (pony.getType() == Pony::Unicorn)
                    pony.sceneName = "canterlot";
                else if (pony.getType() == Pony::Pegasus)
                    pony.sceneName = "cloudsdale";
                else
                    pony.sceneName = "ponyville";
                pony.pos = findVortex(pony.sceneName, 0).destPos;
                ponies += pony;
            }
            else
            {
                quint32 id = (quint8)msg[6] +((quint8)msg[7]<<8) + ((quint8)msg[8]<<16) + ((quint8)msg[9]<<24);
                if (ponies.size()<0 || (quint32)ponies.size() <= id)
                {
                    logMessage(QObject::tr("UDP: Received invalid id in 'edit ponies' request. Disconnecting user."));
                    sendMessage(player,MsgDisconnect, "You were kicked for sending invalid data.");
                    Player::disconnectPlayerCleanup(player); // Save game and remove the player
                    return; // It's ok, since we just disconnected the player
                }
                ponies[id].ponyData = ponyData;
                pony = ponies[id];
            }
            pony.id = player->pony.id;
            pony.netviewId = player->pony.netviewId;
            player->pony = pony;

            Player::savePonies(player, ponies);

            player->pony.loadQuests();
            if (!player->pony.loadInventory()) // Create a default inventory if we can't find one saved
            {
                player->pony.nBits = 15;
                player->pony.saveInventory();
            }

            sendLoadSceneRPC(player, player->pony.sceneName, player->pony.pos);
            // Send instantiate to the players of the new scene
            // removed according to bug report 73: Duplicate server sided ponies instantiated on scene load
//            Scene* scene = findScene(player->pony.sceneName);
//            for (int i=0; i<scene->players.size(); i++)
//                if (scene->players[i]->pony.netviewId!=player->pony.netviewId && scene->players[i]->inGame>=2)
//                    sendNetviewInstantiate(&player->pony, scene->players[i]);

            //Send the 46s init messages
            //app.logMessage(QString("UDP: Sending the 46 init messages"));
            sendMessage(player,MsgUserReliableOrdered4,QByteArray::fromHex("141500000000")); // Sends a 46, init friends
            sendMessage(player,MsgUserReliableOrdered4,QByteArray::fromHex("0e00000000")); // Sends a 46, init journal
        }
        else if ((unsigned char)msg[0]==MsgUserReliableOrdered20 && (unsigned char)msg[3]==0x18 && (unsigned char)msg[4]==0) // Vortex messages
        {
            if (player->inGame>=2)
            {
                quint8 id = (quint8)msg[5];
                Vortex vortex = findVortex(player->pony.sceneName, id);
                if (vortex.destName.isEmpty())
                    logMessage(QObject::tr("Can't find vortex %1 on map %2").arg(id).arg(player->pony.sceneName));
                else
                    sendLoadSceneRPC(player, vortex.destName, vortex.destPos);
            }
        }
        else if ((unsigned char)msg[0]==MsgUserReliableOrdered4 && (unsigned char)msg[5]==0x2) // Delete pony request
        {
            logMessage(QObject::tr("UDP: Deleting a character"));
            QList<Pony> ponies = Player::loadPonies(player);
            quint32 id = (quint8)msg[6] +((quint8)msg[7]<<8) + ((quint8)msg[8]<<16) + ((quint8)msg[9]<<24);
            ponies.removeAt(id);

            Player::savePonies(player,ponies);
            sendPonies(player);
        }
        else if ((unsigned char)msg[0]==MsgUserReliableOrdered12 && (unsigned char)msg[7]==0xCA) // Animation
        {
            //app.logMessage("UDP: Broadcasting animation");
            // Send to everyone
            Scene* scene = findScene(player->pony.sceneName);
            if (scene->name.isEmpty())
                logMessage(QObject::tr("UDP: Can't find the scene for animation message, aborting"));
            else
            {
                if (player->lastValidReceivedAnimation.isEmpty() ||
                    (quint8)player->lastValidReceivedAnimation[3] != (quint8)0x01 || (quint8)msg[5 + 3] == 0x00)
                {
                    // Don't accept invalid animation (0x01 Flying 0x00 Landing)
                    // XXX The game lets players send nonsense (dancing while sitting down), those should be filtered
                    player->lastValidReceivedAnimation = msg.mid(5, msgSize - 5);
                    for (int i=0; i<scene->players.size(); i++)
                    {
                        if (scene->players[i] == player)
                            continue; // Don't send the animation to ourselves, it'll be played regardless
                        else if (scene->players[i]->inGame>=2)
                            sendMessage(scene->players[i], MsgUserReliableOrdered12, player->lastValidReceivedAnimation); // Broadcast
                    }
                }
            }
        }
        else if ((unsigned char)msg[0]==MsgUserReliableOrdered11 && (unsigned char)msg[7]==0x3D) // Skill
        {
#if DEBUG_LOG
            app.logMessage("UDP: Broadcasting skill "+QString().setNum(dataToUint32(msg.mid(8))));
#endif

            bool skillOk=true;
            QByteArray reply;
            uint32_t skillId = dataToUint32(msg.mid(8));
            if (skillId == 2) // Teleport is a special case
            {
                if (msgSize == 28)
                {
                    reply += msg.mid(5, 7); // Netview, RPC, skill IDs
                    reply += msg.mid(16, 4*3); // Pos X, Y, Z (floats)
                    reply += uint32ToData(0); // Skill upgrade (0)
                    reply += floatToData(timestampNow());
                }
                else if (msgSize == 18)
                {
                    // Targeted teleport. First try to find the target in the udp players
                    quint16 targetNetId = dataToUint16(msg.mid(16));
                    Player* target = Player::findPlayer(Player::udpPlayers, targetNetId);
                    Pony* targetPony = nullptr;
                    if (target->pony.netviewId == targetNetId && target->connected)
                        targetPony = &target->pony;
                    else
                    {
                        // The target isn't a player. Check if it's a NPC
                        for (Pony* npc : Quest::npcs)
                            if (npc->netviewId == targetNetId)
                                targetPony = npc;
                    }

                    if (targetPony != nullptr)
                    {
                        reply += msg.mid(5, 7);
                        reply += floatToData(targetPony->pos.x);
                        reply += floatToData(targetPony->pos.y);
                        reply += floatToData(targetPony->pos.z);
                        reply += uint32ToData(0); // Skill upgrade (0)
                        reply += floatToData(timestampNow());
                    }
                    else
                        logMessage(QObject::tr("UDP: Teleport target not found"));
                }
            }
            else // Apply skill
            {
                reply =  msg.mid(5, msgSize - 5);

                if (msgSize == 18)
                {
                    // Targeted skill. First try to find the target in the mobs
                    quint16 targetNetId = dataToUint16(msg.mid(16));
                    for (Mob* mob : Mob::mobs)
                    {
                        if (mob->netviewId == targetNetId)
                        {
                            skillOk = Skill::applySkill(skillId, *mob, SkillTarget::Enemy);
                            break;
                        }
                    }

                    Player* target = Player::findPlayer(Player::udpPlayers, targetNetId);
                    if (target->pony.netviewId == player->pony.netviewId)
                        skillOk = Skill::applySkill(skillId, target->pony, SkillTarget::Self);
                    else if (Settings::enablePVP) // During PVP, all friendly ponies are now ennemies !
                        skillOk = Skill::applySkill(skillId, target->pony, SkillTarget::Enemy);
                    else
                        skillOk = Skill::applySkill(skillId, target->pony, SkillTarget::Friendly);
                }

                // Apply animation
                if (skillOk)
                {
                    Skill& skill = Skill::skills[skillId];
                    SkillUpgrade& upgrade(skill.upgrades[0]);
                    sendAnimation(player, upgrade.casterAnimation);
                }
            }

            // Send to everyone
            Scene* scene = findScene(player->pony.sceneName);
            if (scene->name.isEmpty())
                logMessage(QObject::tr("UDP: Can't find the scene for skill message, aborting"));
            else
            {
                for (int i=0; i<scene->players.size(); i++)
                    if (scene->players[i]->inGame>=2)
                        sendMessage(scene->players[i], MsgUserReliableOrdered11,reply); // Broadcast
            }
        }
        else if ((unsigned char)msg[0]==MsgUserReliableOrdered11 && (unsigned char)msg[7]==0x08) // Wear request
        {
            quint8 index = msg[9];
            Scene* scene = findScene(player->pony.sceneName);
            if (scene->name.isEmpty())
                logMessage(QObject::tr("UDP: Can't find the scene for wear message, aborting"));
            else
            {
                if (player->pony.tryWearItem(index))
                {
                    for (int i=0; i<scene->players.size(); i++)
                        if (scene->players[i]->inGame>=2
                                && scene->players[i]->pony.netviewId != player->pony.netviewId)
                            sendWornRPC(&player->pony, scene->players[i], player->pony.worn);
                }
                else
                    logError(QObject::tr("Error trying to wear item"));
            }
        }
        else if ((unsigned char)msg[0]==MsgUserReliableOrdered11 && (unsigned char)msg[7]==0x16) // BeginShop request
        {
            // BeginShop doesn't specify wich shop you want to buy from
            // We'll assume that there's never more than one shop per scene.
            uint16_t netviewId = dataToUint16(msg.mid(5));
            Pony* targetNpc = nullptr;
            for (Pony* npc : Quest::npcs)
            {
                if (npc->netviewId == netviewId && npc->inv.size()) // Has a shop
                {
                    targetNpc = npc;
                    break;
                }
            }
            if (targetNpc)
                sendBeginShop(player, targetNpc);
            else
                logMessage(QObject::tr("UDP: Can't find a shop on scene %1 for BeginShop")
                               .arg(player->pony.sceneName));
        }
        else if ((unsigned char)msg[0]==MsgUserReliableOrdered11 && (unsigned char)msg[7]==0x17) // EndShop request
        {
            sendEndShop(player);
        }
        else if ((unsigned char)msg[0]==MsgUserReliableOrdered11 && (unsigned char)msg[7]==0x0A) // BuyItem request
        {
            /// TODO: At the moment we don't actually pay for items, since there are no monsters.
            uint16_t itemId = dataToUint32(msg.mid(8));
            uint16_t amount = dataToUint32(msg.mid(12));

            player->pony.addInventoryItem(itemId, amount);
            sendSetBitsRPC(player);
        }
        else if ((unsigned char)msg[0]==MsgUserReliableOrdered11 && (unsigned char)msg[7]==0x0B) // SellItem request
        {
            /// TODO: At the moment we don't actually pay for items, since there are no monsters.
            uint16_t itemId = dataToUint32(msg.mid(8));
            uint16_t amount = dataToUint32(msg.mid(12));

            player->pony.removeInventoryItem(itemId, amount);
            sendSetBitsRPC(player);
        }
        else if ((unsigned char)msg[0]==MsgUserReliableOrdered11 && (unsigned char)msg[7]==0x04) // Get worn items request
        {
            quint16 targetId = ((quint16)(quint8)msg[5]) + (((quint16)(quint8)msg[6])<<8);
            Player* target = Player::findPlayer(Player::udpPlayers, targetId);
            if (target->pony.netviewId == targetId && target->connected)
                sendWornRPC(&target->pony, player, target->pony.worn);
            else
            {
                Pony* targetNpc = nullptr;
                for (Pony* npc : Quest::npcs)
                {
                    if (npc->netviewId == targetId)
                    {
                        targetNpc = npc;
                        break;
                    }
                }
                if (targetNpc)
                    sendWornRPC(targetNpc, player, targetNpc->worn);
                else
                    logMessage(QObject::tr("UDP: Can't find netviewId %1 to send worn items")
                                   .arg(targetId));
            }
        }
        else if ((unsigned char)msg[0]==MsgUserReliableOrdered11 && (unsigned char)msg[7]==0x09) // Unwear item request
        {
            quint16 targetId = ((quint16)(quint8)msg[5]) + (((quint16)(quint8)msg[6])<<8);
            Player* target = Player::findPlayer(Player::udpPlayers, targetId);
            if (target->pony.netviewId == targetId)
                target->pony.unwearItemAt(dataToUint8(msg.mid(8)));
            else
                logMessage(QObject::tr("UDP: Can't find netviewId %1 to unwear item")
                               .arg(targetId));
        }
        else if ((unsigned char)msg[0]==MsgUserReliableOrdered11 && (unsigned char)msg[7]==0x31) // Run script (NPC) request
        {
            quint16 targetId = ((quint16)(quint8)msg[5]) + (((quint16)(quint8)msg[6])<<8);
            //app.logMessage("UDP: Quest "+QString().setNum(targetId)+" requested");
            for (int i=0; i<player->pony.quests.size(); i++)
                if (player->pony.quests[i].id == targetId)
                {
                    player->pony.lastQuest = i;
                    player->pony.quests[i].runScript();
                    break;
                }
        }
        else if ((unsigned char)msg[0]==MsgUserReliableOrdered4 && (unsigned char)msg[5]==0xB) // Continue dialog
        {
            //app.logMessage("UDP: Resuming script for quest "+QString().setNum(player->pony.lastQuest));
            player->pony.quests[player->pony.lastQuest].processAnswer();
        }
        else if ((unsigned char)msg[0]==MsgUserReliableOrdered4 && (unsigned char)msg[5]==0xC) // Continue dialog (with answer)
        {
            quint32 answer = ((quint32)(quint8)msg[6])
                            + (((quint32)(quint8)msg[7])<<8)
                            + (((quint32)(quint8)msg[8])<<16)
                            + (((quint32)(quint8)msg[9])<<24);
            //app.logMessage("UDP: Resuming script with answer "+QString().setNum(answer)
            //               +" for quest "+QString().setNum(player->pony.lastQuest));
            player->pony.quests[player->pony.lastQuest].processAnswer(answer);
        }
        else
        {
            // Display data
            quint32 unknownMsgSize =  (((quint16)(quint8)msg[3]) +(((quint16)(quint8)msg[4])<<8)) / 8;
            logMessage(QObject::tr("UDP: Unknown message received : %1")
                           .arg(player->receivedDatas->left(unknownMsgSize+5).toHex().data()));
            *player->receivedDatas = player->receivedDatas->mid(unknownMsgSize+5);
            msgSize=0;
        }
    }
    else if ((unsigned char)msg[0]==MsgUserUnreliable) // Sync (position) update
    {
        if (dataToUint16(msg.mid(5)) == player->pony.netviewId)
            Sync::receiveSync(player, msg);
    }
    else
    {
        // Display data
        logMessage(QObject::tr("Unknown data received (UDP) (hex) : "));
        logMessage(QString(player->receivedDatas->toHex().data()));
        quint32 unknownMsgSize = (((quint16)(quint8)msg[3]) +(((quint16)(quint8)msg[4])<<8)) / 8;
        *player->receivedDatas = player->receivedDatas->mid(unknownMsgSize+5);
        msgSize=0;
    }

    *player->receivedDatas = player->receivedDatas->mid(msgSize);
    if (player->receivedDatas->size())
        receiveMessage(player);
}
