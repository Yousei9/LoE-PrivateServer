#include "message.h"
#include "player.h"
#include "serialize.h"
#include "mob.h"
#include "mobsStats.h"
#include "animation.h"
#include "quest.h"
#include "log.h"
#include "scene.h"

#define DEBUG_LOG false

// File-global game-entering mutex (to prevent multiple instantiates)
static QMutex levelLoadMutex;

void sendPonies(Player* player)
{
    // The full request is like a normal sendPonies but with all the serialized ponies at the end
    QList<Pony> ponies = Player::loadPonies(player);
    quint32 poniesDataSize=0;
    for (int i=0;i<ponies.size();i++)
        poniesDataSize+=ponies[i].ponyData.size();

    QByteArray data(5,0);
    data[0] = 1; // Request number
    data[1] = (quint8)(ponies.size()&0xFF); // Number of ponies
    data[2] = (quint8)((ponies.size()>>8)&0xFF); // Number of ponies
    data[3] = (quint8)((ponies.size()>>16)&0xFF); // Number of ponies
    data[4] = (quint8)((ponies.size()>>24)&0xFF); // Number of ponies

    for (int i=0;i<ponies.size();i++)
        data += ponies[i].ponyData;

    logMessage(QObject::tr("UDP: Sending characters data to %1").arg(player->pony.netviewId));
    sendMessage(player, MsgUserReliableOrdered4, data);
}

void sendEntitiesList(Player *player)
{
    levelLoadMutex.lock(); // Protect player->inGame
    if (player->inGame == 0) // Not yet in game, send player's ponies list (Characters scene)
    {
        levelLoadMutex.unlock();
#if DEBUG_LOG
        app.logMessage("UDP: Sending ponies list");
#endif
        sendPonies(player);
        return;
    }
    else if (player->inGame > 1) // Not supposed to happen, let's do it anyway
    {
        //levelLoadMutex.unlock();
        logMessage(QObject::tr("UDP: Entities list already sent to %1, resending anyway").arg(player->pony.netviewId));
        //return;
    }
    else // Loading finished, sending entities list
    logMessage(QObject::tr("UDP: Sending entities list to %1").arg(player->pony.netviewId));
    Scene* scene = findScene(player->pony.sceneName); // Spawn all the players on the client
    for (int i=0; i<scene->players.size(); i++)
        sendNetviewInstantiate(&scene->players[i]->pony, player);

    // Send npcs
    for (int i=0; i<Quest::npcs.size(); i++)
        if (Quest::npcs[i]->sceneName.toLower() == player->pony.sceneName.toLower())
        {
#if DEBUG_LOG
            logMessage("UDP: Sending NPC "+Quest::npcs[i]->name);
#endif
            sendNetviewInstantiate(Quest::npcs[i], player);
        }

    // Send mobs
    for (int i=0; i<Mob::mobs.size(); i++)
        if (Mob::mobs[i]->sceneName.toLower() == player->pony.sceneName.toLower())
        {
#if DEBUG_LOG
            logMessage("UDP: Sending mob "+Mob::mobs[i]->modelName);
#endif
            sendNetviewInstantiate(player, Mob::mobs[i]);
        }

    player->inGame = 2;
    levelLoadMutex.unlock();

    // Send stats of the client's pony
    sendSetMaxStatRPC(player, 0, 100);
    sendSetStatRPC(player, 0, 100);
    sendSetMaxStatRPC(player, 1, 100);
    sendSetStatRPC(player, 1, 100);
}

void sendPonySave(Player *player, QByteArray msg)
{
    if (player->inGame < 2) // Not supposed to happen, ignoring the request
    {
        logMessage(QObject::tr("UDP: Savegame requested too soon by %1").arg(player->pony.netviewId));
        return;
    }

    quint16 netviewId = (quint8)msg[6] + ((quint16)(quint8)msg[7]<<8);
    Player* refresh = Player::findPlayer(Player::udpPlayers, netviewId); // Find players

    // If we find a matching NPC, send him and exits
    Pony* npc = NULL;
    for (int i=0; i<Quest::npcs.size(); i++)
        if (Quest::npcs[i]->netviewId == netviewId)
            npc = Quest::npcs[i];
    if (npc != NULL)
    {
#if DEBUG_LOG
        logMessage("UDP: Sending ponyData and worn items for NPC "+npc->name);
#endif
        sendPonyData(npc, player);
        if (npc->inv.size()) // This NPC has a shop
        {
            sendAddViewAddShop(player, npc);
        }
        return;
    }

    // If we find a matching mob, send him and exits
    Mob* mob = nullptr;
    for (int i=0; i<Mob::mobs.size(); i++)
        if (Mob::mobs[i]->netviewId == netviewId)
            mob = Mob::mobs[i];
    if (mob != nullptr)
    {
        //app.logMessage("UDP: mob ponyData requested");
        // We should probably send the mob's stats here
        sendSetMaxStatRPC(player, mob->netviewId, 1, defaultMaxHealth[(unsigned)mob->type]);
        sendSetMaxStatRPC(player, mob->netviewId, 1, mob->health);
        return;
    }

    if (netviewId == player->pony.netviewId) // Current player
    {
        if (player->inGame == 3) // Hopefully that'll fix people stuck on the default cam without creating clones
        {
            logMessage(QObject::tr("UDP: Savegame already sent to %1, resending anyway")
                           .arg(player->pony.netviewId));
        }
        else
        {
#if DEBUG_LOG
            logMessage(QString("UDP: Sending pony save for/to ")+QString().setNum(netviewId));
#endif
        }

        // Set current/max stats
        sendSetMaxStatRPC(player, 0, 100);
        sendSetStatRPC(player, 0, 100);
        sendSetMaxStatRPC(player, 1, 100);
        sendSetStatRPC(player, 1, 100);

        sendPonyData(player);

        // Send inventory
        sendInventoryRPC(player, player->pony.inv, player->pony.worn, player->pony.nBits);

        // Send skills
        QList<QPair<quint32, quint32> > skills;
        skills << QPair<quint32, quint32>(10, 0); // Ground pound (all races)
        //skills << QPair<quint32, quint32>(17, 0); // Fire Breath ! (dragons)
        if (player->pony.getType() == Pony::EarthPony)
        {
            skills << QPair<quint32, quint32>(5, 0); // Seismic buck
            skills << QPair<quint32, quint32>(16, 0); // Rough Terrain
        }
        else if (player->pony.getType() == Pony::Pegasus)
        {
            skills << QPair<quint32, quint32>(11, 0); // Dual Cyclone
            skills << QPair<quint32, quint32>(14, 0); // Gale
        }
        else if (player->pony.getType() == Pony::Unicorn)
        {
            skills << QPair<quint32, quint32>(2, 0); // Teleport
            skills << QPair<quint32, quint32>(9, 0); // Rainbow Fields
            skills << QPair<quint32, quint32>(12, 0); // Heal
            skills << QPair<quint32, quint32>(15, 0); // Magical Arrow
        }
        //if (player->name == "mlkj")
        //    skills << QPair<quint32, quint32>(20, 0); // Admin Blast
        sendSkillsRPC(player, skills);

        // Set current/max stats again (that's what the official server does, not my idea !)
        sendSetMaxStatRPC(player, 0, 100);
        sendSetStatRPC(player, 0, 100);
        sendSetMaxStatRPC(player, 1, 100);
        sendSetStatRPC(player, 1, 100);

        refresh->inGame = 3;
    }
    else if (!refresh->IP.isEmpty())
    {
#if DEBUG_LOG
        app.logMessage(QString("UDP: Sending pony save for ")+QString().setNum(refresh->pony.netviewId)
                       +" to "+QString().setNum(player->pony.netviewId));
#endif

        //sendWornRPC(refresh, player, refresh->worn);

        sendSetStatRPC(refresh, player, 0, 100);
        sendSetMaxStatRPC(refresh, player, 0, 100);
        sendSetStatRPC(refresh, player, 1, 100);
        sendSetMaxStatRPC(refresh, player, 1, 100);

        sendPonyData(&refresh->pony, player);

        if (!refresh->lastValidReceivedAnimation.isEmpty())
            sendMessage(player, MsgUserReliableOrdered12, refresh->lastValidReceivedAnimation);
    }
    else
    {
        logError(QObject::tr("UDP: Error sending pony save : netviewId %1 not found").arg(netviewId));
    }
}

void sendNetviewInstantiate(Player *player, QString key, quint16 NetviewId, quint16 ViewId, UVector pos, UQuaternion rot)
{
    QByteArray data(1,1);
    data += stringToData(key);
    QByteArray data2(4,0);
    data2[0]=(quint8)(NetviewId&0xFF);
    data2[1]=(quint8)((NetviewId>>8)&0xFF);
    data2[2]=(quint8)(ViewId&0xFF);
    data2[3]=(quint8)((ViewId>>8)&0xFF);
    data += data2;
    data += vectorToData(pos);
    data += quaternionToData(rot);
    sendMessage(player, MsgUserReliableOrdered6, data);
}

void sendNetviewInstantiate(Player* player, Mob* mob)
{
    sendNetviewInstantiate(player, mob->modelName, mob->netviewId, mob->id, mob->pos, mob->rot);
}

void sendNetviewInstantiate(Player *player)
{
#if DEBUG_LOG
    app.logMessage("UDP: Send instantiate for/to "+QString().setNum(player->pony.netviewId));
#endif
    QByteArray data(1,1);
    data += stringToData("PlayerBase");
    QByteArray data2(4,0);
    data2[0]=(quint8)(player->pony.netviewId&0xFF);
    data2[1]=(quint8)((player->pony.netviewId>>8)&0xFF);
    data2[2]=(quint8)(player->pony.id&0xFF);
    data2[3]=(quint8)((player->pony.id>>8)&0xFF);
    data += data2;
    data += vectorToData(player->pony.pos);
    data += quaternionToData(player->pony.rot);
    sendMessage(player, MsgUserReliableOrdered6, data);

    logMessage(QObject::tr("Instantiate at %1 %2 %3").arg(player->pony.pos.x)
                    .arg(player->pony.pos.y).arg(player->pony.pos.z));
}

void sendNetviewInstantiate(Pony *src, Player *dst)
{
#if DEBUG_LOG
    app.logMessage("UDP: Send instantiate for "+QString().setNum(src->netviewId)
                   +" to "+QString().setNum(dst->pony.netviewId));
#endif
    QByteArray data(1,1);
    data += stringToData("PlayerBase");
    QByteArray data2(4,0);
    data2[0]=(quint8)(src->netviewId&0xFF);
    data2[1]=(quint8)((src->netviewId>>8)&0xFF);
    data2[2]=(quint8)(src->id&0xFF);
    data2[3]=(quint8)((src->id>>8)&0xFF);
    data += data2;
    data += vectorToData(src->pos);
    data += quaternionToData(src->rot);
    sendMessage(dst, MsgUserReliableOrdered6, data);

   //app.logMessage(QString("Instantiate at ")+QString().setNum(rSrc.pony.pos.x)+" "
   //                +QString().setNum(rSrc.pony.pos.y)+" "
   //                +QString().setNum(rSrc.pony.pos.z));
}

void sendNetviewRemove(Player *player, quint16 netviewId)
{
    logMessage(QObject::tr("UDP: Removing netview %1 to %2").arg(netviewId).arg(player->pony.netviewId));

    QByteArray data(3,2);
    data[1] = (quint8)(netviewId&0xFF);
    data[2] = (quint8)((netviewId>>8)&0xFF);
    sendMessage(player, MsgUserReliableOrdered6, data);
}

void sendNetviewRemove(Player* player, quint16 netviewId, quint8 reasonCode)
{
    //app.logMessage(QObject::tr("UDP: Removing netview %1 to %2, reason code %3").arg(netviewId)
    //               .arg(player->pony.netviewId).arg(reasonCode));

    QByteArray data(4,2);
    data[1] = (quint8)(netviewId&0xFF);
    data[2] = (quint8)((netviewId>>8)&0xFF);
    data[3] = reasonCode;
    sendMessage(player, MsgUserReliableOrdered6, data);
}

void sendSetStatRPC(Player* player, quint16 netviewId, quint8 statId, float value)
{
    QByteArray data(4,50);
    data[0] = (quint8)(netviewId&0xFF);
    data[1] = (quint8)((netviewId>>8)&0xFF);
    data[3] = statId;
    data += floatToData(value);
    sendMessage(player, MsgUserReliableOrdered18, data);
}

void sendSetMaxStatRPC(Player* player, quint16 netviewId, quint8 statId, float value)
{
    QByteArray data(4,51);
    data[0] = (quint8)(netviewId&0xFF);
    data[1] = (quint8)((netviewId>>8)&0xFF);
    data[3] = statId;
    data += floatToData(value);
    sendMessage(player, MsgUserReliableOrdered18, data);
}

void sendSetStatRPC(Player *player, quint8 statId, float value)
{
    sendSetStatRPC(player, player->pony.netviewId, statId, value);
}

void sendSetMaxStatRPC(Player* player, quint8 statId, float value)
{
    sendSetMaxStatRPC(player, player->pony.netviewId, statId, value);
}

void sendSetStatRPC(Player* affected, Player* dest, quint8 statId, float value)
{
    QByteArray data(4,50);
    data[0] = (quint8)(affected->pony.netviewId&0xFF);
    data[1] = (quint8)((affected->pony.netviewId>>8)&0xFF);
    data[3] = statId;
    data += floatToData(value);
    sendMessage(dest, MsgUserReliableOrdered18, data);
}

void sendSetMaxStatRPC(Player* affected, Player* dest, quint8 statId, float value)
{
    QByteArray data(4,51);
    data[0] = (quint8)(affected->pony.netviewId&0xFF);
    data[1] = (quint8)((affected->pony.netviewId>>8)&0xFF);
    data[3] = statId;
    data += floatToData(value);
    sendMessage(dest, MsgUserReliableOrdered18, data);
}

void sendWornRPC(Player* player)
{
    sendWornRPC(player, player->pony.worn);
}

void sendWornRPC(Player *player, QList<WearableItem> &worn)
{
    QByteArray data(3, 4);
    data[0] = (quint8)(player->pony.netviewId&0xFF);
    data[1] = (quint8)((player->pony.netviewId>>8)&0xFF);
    data += 32; // Max Worn Items
    data += (uint8_t)worn.size();
    for (int i=0;i<worn.size();i++)
    {
        data += (quint8)((worn[i].index-1)&0xFF);
        data += (quint8)(worn[i].id&0xFF);
        data += (quint8)((worn[i].id>>8)&0xFF);
        data += (quint8)((worn[i].id>>16)&0xFF);
        data += (quint8)((worn[i].id>>24)&0xFF);
    }
    sendMessage(player, MsgUserReliableOrdered18, data);
}

void sendWornRPC(Pony *wearing, Player *dest, QList<WearableItem> &worn)
{
    QByteArray data(3, 4);
    data[0] = (quint8)(wearing->netviewId&0xFF);
    data[1] = (quint8)((wearing->netviewId>>8)&0xFF);
    data += 32; // Max Worn Items
    data += worn.size();
    for (int i=0;i<worn.size();i++)
    {
        data += (quint8)((worn[i].index-1)&0xFF);
        data += (quint8)(worn[i].id&0xFF);
        data += (quint8)((worn[i].id>>8)&0xFF);
        data += (quint8)((worn[i].id>>16)&0xFF);
        data += (quint8)((worn[i].id>>24)&0xFF);
    }
    sendMessage(dest, MsgUserReliableOrdered18, data);
}

void sendInventoryRPC(Player* player)
{
    sendInventoryRPC(player, player->pony.inv, player->pony.worn, player->pony.nBits);
}

void sendInventoryRPC(Player *player, QList<InventoryItem>& inv, QList<WearableItem>& worn, quint32 nBits)
{
    QByteArray data(5, 5);
    data[0] = (quint8)(player->pony.netviewId & 0xFF);
    data[1] = (quint8)((player->pony.netviewId>>8) & 0xFF);
    data[3] = MAX_INVENTORY_SIZE; // Max Inventory Size
    data[4] = (quint8)inv.size();
    for (int i=0;i<inv.size();i++)
    {
        data += (quint8)inv[i].index;
        data += (quint8)(inv[i].id & 0xFF);
        data += (quint8)((inv[i].id>>8) & 0xFF);
        data += (quint8)((inv[i].id>>16) & 0xFF);
        data += (quint8)((inv[i].id>>24) & 0xFF);
        data += (quint8)(inv[i].amount & 0xFF);
        data += (quint8)((inv[i].amount>>8) & 0xFF);
        data += (quint8)((inv[i].amount>>16) & 0xFF);
        data += (quint8)((inv[i].amount>>24) & 0xFF);
    }
    data += MAX_WORN_ITEMS; // Max Worn Items
    data += worn.size();
    for (int i=0;i<worn.size();i++)
    {
        data += (quint8)worn[i].index-1;
        data += (quint8)(worn[i].id & 0xFF);
        data += (quint8)((worn[i].id>>8) & 0xFF);
        data += (quint8)((worn[i].id>>16) & 0xFF);
        data += (quint8)((worn[i].id>>24) & 0xFF);
    }
    data += (quint8)(nBits & 0xFF);
    data += (quint8)((nBits>>8) & 0xFF);
    data += (quint8)((nBits>>16) & 0xFF);
    data += (quint8)((nBits>>24) & 0xFF);
    sendMessage(player, MsgUserReliableOrdered18, data);
}

void sendSetBitsRPC(Player* player)
{
    QByteArray data(3, 0x10);
    data[0] = (quint8)(player->pony.netviewId & 0xFF);
    data[1] = (quint8)((player->pony.netviewId>>8) & 0xFF);
    data += uint32ToData(player->pony.nBits);
    sendMessage(player, MsgUserReliableOrdered18, data);
}

void sendSkillsRPC(Player* player, QList<QPair<quint32, quint32> > &skills)
{
    QByteArray data(8, 0xC3);
    data[0] = (quint8)(player->pony.netviewId&0xFF);
    data[1] = (quint8)((player->pony.netviewId>>8)&0xFF);
    data[3] = 0x00; // Use dictionnary flag
    data[4] = (quint8)(skills.size()&0xFF);
    data[5] = (quint8)((skills.size()>>8)&0xFF);
    data[6] = (quint8)((skills.size()>>16)&0xFF);
    data[7] = (quint8)((skills.size()>>24)&0xFF);
    for (int i=0;i<skills.size();i++)
    {
        data += (quint8)(skills[i].first&0xFF);
        data += (quint8)((skills[i].first>>8)&0xFF);
        data += (quint8)((skills[i].first>>16)&0xFF);
        data += (quint8)((skills[i].first>>24)&0xFF);
        data += (quint8)(skills[i].second&0xFF);
        data += (quint8)((skills[i].second>>8)&0xFF);
        data += (quint8)((skills[i].second>>16)&0xFF);
        data += (quint8)((skills[i].second>>24)&0xFF);
    }
    sendMessage(player, MsgUserReliableOrdered18, data);
}

void sendPonyData(Player *player)
{
    // Sends the ponyData
    //app.logMessage(QString("UDP: Sending the ponyData for/to "+QString().setNum(player->pony.netviewId)));
    QByteArray data(3,0xC8);
    data[0] = (quint8)(player->pony.netviewId&0xFF);
    data[1] = (quint8)((player->pony.netviewId>>8)&0xFF);
    data += player->pony.ponyData;
    sendMessage(player, MsgUserReliableOrdered18, data);
}

void sendPonyData(Pony *src, Player *dst)
{
    // Sends the ponyData
    //app.logMessage(QString("UDP: Sending the ponyData for "+QString().setNum(src->pony.netviewId)
    //                       +" to "+QString().setNum(dst->pony.netviewId)));
    QByteArray data(3,0xC8);
    data[0] = (quint8)(src->netviewId&0xFF);
    data[1] = (quint8)((src->netviewId>>8)&0xFF);
    data += src->ponyData;
    sendMessage(dst, MsgUserReliableOrdered18, data);
}

void sendLoadSceneRPC(Player* player, QString sceneName) // Loads a scene and send to the default spawn
{
    logMessage(QObject::tr("UDP: Loading scene \"%1\" on %2").arg(sceneName).arg(player->pony.netviewId));
    Vortex vortex = findVortex(sceneName, 0);
    if (vortex.destName.isEmpty())
    {
        logMessage(QObject::tr("UDP: Scene not in vortex DB. Aborting scene load."));
        return;
    }

    Scene* scene = findScene(sceneName);
    Scene* oldScene = findScene(player->pony.sceneName);
    if (scene->name.isEmpty() || oldScene->name.isEmpty())
    {
        logMessage(QObject::tr("UDP: Can't find the scene, aborting"));
        return;
    }

    // Update scene players
    //app.logMessage("sendLoadSceneRPC: locking");
    levelLoadMutex.lock();
    player->inGame=1;
    player->pony.pos = vortex.destPos;
    player->pony.sceneName = sceneName.toLower();
    player->lastValidReceivedAnimation.clear(); // Changing scenes resets animations
    Player::removePlayer(oldScene->players, player->IP, player->port);
    // Send remove RPC to the other players of the old scene
    for (int i=0; i<oldScene->players.size(); i++)
        sendNetviewRemove(oldScene->players[i], player->pony.netviewId);
    // Send instantiate to the players of the new scene
    for (int i=0; i<scene->players.size(); i++)
        if (scene->players[i]->inGame>=2)
            sendNetviewInstantiate(&player->pony, scene->players[i]);
    scene->players << player;

    QByteArray data(1,5);
    data += stringToData(sceneName.toLower());
    sendMessage(player,MsgUserReliableOrdered6,data); // Sends a 48
    //app.logMessage("sendLoadSceneRPC: unlocking");
    levelLoadMutex.unlock();
}

void sendLoadSceneRPC(Player* player, QString sceneName, UVector pos) // Loads a scene and send to the given pos
{
    logMessage(QString(QString("UDP: Loading scene \"%1\" to %2 at %3 %4 %5")
                           .arg(sceneName).arg(player->pony.netviewId)
                           .arg(pos.x).arg(pos.y).arg(pos.z)));

    Scene* scene = findScene(sceneName);
    Scene* oldScene = findScene(player->pony.sceneName);
    if (scene->name.isEmpty() || oldScene->name.isEmpty())
    {
        logMessage(QObject::tr("UDP: Can't find the scene, aborting"));
        return;
    }

    // Update scene players
    //app.logMessage("sendLoadSceneRPC pos: locking");
    levelLoadMutex.lock();
    player->inGame=1;
    player->pony.pos = pos;
    player->pony.sceneName = sceneName.toLower();
    player->lastValidReceivedAnimation.clear(); // Changing scenes resets animations
    Player::removePlayer(oldScene->players, player->IP, player->port);
    // Send remove RPC to the other players of the old scene
    for (int i=0; i<oldScene->players.size(); i++)
        sendNetviewRemove(oldScene->players[i], player->pony.netviewId);
    // Send instantiate to the players of the new scene
    for (int i=0; i<scene->players.size(); i++)
        if (scene->players[i]->inGame>=2)
            sendNetviewInstantiate(&player->pony, scene->players[i]);
    scene->players << player;

    QByteArray data(1,5);
    data += stringToData(sceneName.toLower());
    sendMessage(player,MsgUserReliableOrdered6,data); // Sends a 48
    //app.logMessage("sendLoadSceneRPC pos: unlocking");
    levelLoadMutex.unlock();
}

void sendChatMessage(Player* player, QString message, QString author, quint8 chatType)
{
    QByteArray idAndAccess(5,0);
    idAndAccess[0] = (quint8)(player->pony.netviewId&0xFF);
    idAndAccess[1] = (quint8)((player->pony.netviewId << 8)&0xFF);
    idAndAccess[2] = (quint8)((player->pony.id&0xFF));
    idAndAccess[3] = (quint8)((player->pony.id << 8)&0xFF);
    idAndAccess[4] = 0x0; // Access level
    QByteArray data(2,0);
    data[0] = 0xf; // RPC ID
    data[1] = chatType;
    data += stringToData(author);
    data += stringToData(message);
    data += idAndAccess;

    sendMessage(player,MsgUserReliableOrdered4,data); // Sends a 46
}

void sendMove(Player* player, float x, float y, float z)
{
    QByteArray data(1,0);
    data[0] = 0xce; // Request number
    data += floatToData(x);
    data += floatToData(y);
    data += floatToData(z);
    logMessage(QObject::tr(("UDP: Moving character")));
    sendMessage(player,MsgUserReliableOrdered4, data);
}

void sendBeginDialog(Player* player)
{
    QByteArray data(1,0);
    data[0] = 11; // Request number
    sendMessage(player,MsgUserReliableOrdered4, data);
}

void sendDialogMessage(Player* player, QString& message, QString NPCName, quint16 iconId)
{
    QByteArray data(1,0);
    data[0] = 0x11; // Request number
    data += stringToData(message);
    data += stringToData(NPCName);
    data += (quint8)(iconId&0xFF);
    data += (quint8)((iconId>>8)&0xFF);

    sendMessage(player,MsgUserReliableOrdered4, data);
}

void sendDialogMessage(Player* player, QString& message, QString NPCName, quint16 npc3DNetviewId, quint16 iconId)
{
    QByteArray data(1,0);
    data[0] = 0x11; // Request number
    data += stringToData(message);
    data += stringToData(NPCName);
    data += uint16ToData(npc3DNetviewId);
    data += (quint8)(iconId&0xFF);
    data += (quint8)((iconId>>8)&0xFF);

    sendMessage(player,MsgUserReliableOrdered4, data);
}

void sendDialogOptions(Player* player, QList<QString>& answers)
{
    QByteArray data(5,0);
    data[0] = 0xC; // Request number
    data[1] = (quint8)(answers.size()&0xFF);
    data[2] = (quint8)((answers.size()>>8)&0xFF);
    data[3] = (quint8)((answers.size()>>16)&0xFF);
    data[4] = (quint8)((answers.size()>>24)&0xFF);
    for (int i=0; i<answers.size(); i++)
        data += stringToData(answers[i]);

    sendMessage(player,MsgUserReliableOrdered4, data);
}

void sendEndDialog(Player* player)
{
    QByteArray data(1,0);
    data[0] = 13; // Request number
    sendMessage(player,MsgUserReliableOrdered4, data);
}

void sendWearItemRPC(Player* player, const WearableItem& item)
{
    QByteArray data;
    data += uint16ToData(player->pony.netviewId);
    data += 0x8;
    data += uint32ToData(item.id);
    data += uint8ToData(item.index);
    sendMessage(player, MsgUserReliableOrdered18, data);
}

void sendWearItemRPC(Pony* wearing, Player* dest, const WearableItem& item)
{
    QByteArray data;
    data += uint16ToData(wearing->netviewId);
    data += 0x8;
    data += uint32ToData(item.id);
    data += uint8ToData(item.index);
    sendMessage(dest, MsgUserReliableOrdered18, data);
}

void sendUnwearItemRPC(Player* player, uint8_t slot)
{
    QByteArray data;
    data += uint16ToData(player->pony.netviewId);
    data += 0x9;
    data += uint8ToData(slot);
    sendMessage(player, MsgUserReliableOrdered18, data);
}

void sendUnwearItemRPC(Pony* wearing, Player* dest, uint8_t slot)
{
    QByteArray data;
    data += uint16ToData(wearing->netviewId);
    data += 0x9;
    data += uint8ToData(slot);
    sendMessage(dest, MsgUserReliableOrdered18, data);
}

void sendAddItemRPC(Player* player, const InventoryItem& item)
{
    QByteArray data;
    data += uint16ToData(player->pony.netviewId);
    data += 0x6;
    data += uint32ToData(item.id);
    data += uint32ToData(item.amount);
    data += uint32ToData(item.index);
    sendMessage(player, MsgUserReliableOrdered18, data);
}

void sendDeleteItemRPC(Player* player, uint8_t index, uint32_t qty)
{
    QByteArray data;
    data += uint16ToData(player->pony.netviewId);
    data += 0x7;
    data += uint8ToData(index);
    data += uint32ToData(qty);
    sendMessage(player, MsgUserReliableOrdered18, data);
}

void sendAddViewAddShop(Player* player, Pony* npcShop)
{
    QByteArray data;
    data += 0x0A; // AddView RPC
    data += uint16ToData(npcShop->netviewId);
    data += uint16ToData(npcShop->netviewId);
    //data += uint16ToData(player->pony.netviewId);
    data += stringToData("AddShop");

    sendMessage(player, MsgUserReliableOrdered6, data);
}

void sendBeginShop(Player* player, Pony* npcShop)
{
    QByteArray data;
    data += uint16ToData(npcShop->netviewId);
    data += 0x16; // BeginShop

    data += uint32ToData(npcShop->inv.size());

    for (const InventoryItem& item : npcShop->inv)
    {
        data += uint32ToData(item.id);
        data += uint32ToData(item.amount);
    }
    data += stringToData("Wearable Items");

    sendMessage(player, MsgUserReliableOrdered18, data);
}

void sendEndShop(Player* player)
{
    QByteArray data;
    data += uint16ToData(player->pony.netviewId);
    data += 0x17; // EndShop

    sendMessage(player, MsgUserReliableOrdered18, data);
}

void sendAnimation(Player* player, const Animation* animation)
{
    QByteArray data;
    data += uint16ToData(player->pony.netviewId);
    data += 0xCA; // Animation
    data += uint32ToData(animation->id);

    sendMessage(player, MsgUserReliableOrdered12, data);
}
