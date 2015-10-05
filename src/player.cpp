#include "app.h"
#include "player.h"
#include "utils.h"
#include "message.h"
#include "sendMessage.h"
#include "items.h"
#include "packetloss.h"
#include "scene.h"
#include "log.h"
#include "udp.h"
#include "items.h"
#include <QUdpSocket>
#ifdef USE_GUI
#include <QSettings>
#include "settings.h"
#endif

#define DEBUG_LOG false

QList<Player*> Player::tcpPlayers; // Used by the TCP login server
QList<Player*> Player::udpPlayers; // Used by the UDP game server

SceneEntity::SceneEntity()
{
    modelName = QString();
    id = 0;
    netviewId = 0;
    pos=UVector(0,0,0);
    rot=UQuaternion(0,0,0,0);
    sceneName = QString();
}

Pony::Pony(Player *Owner)
    : SceneEntity(), StatsComponent(), owner(Owner), dead{false},
      maxHealth{100}, defense{2.0}
{
    modelName = "PlayerBase";
    name = "";
    wornSlots = 0;
    health = maxHealth;
}

Pony::type Pony::getType()
{
    // Variable UInt32
    unsigned char num3;
    int num = 0;
    int num2 = 0;
    int i=0;
    do
    {
        num3 = ponyData[i]; i++;
        num |= (num3 & 0x7f) << num2;
        num2 += 7;
    } while ((num3 & 0x80) != 0);
    unsigned off = (uint) num  + i;
    return (type)(quint8)ponyData[off];
}

Player::Player()
    : pony{Pony(this)}
{
    connected=false;
    inGame=0;
    accessLvl=0;
    nReceivedDups=0;
    lastPingNumber=0;
    lastPingTime=timestampNow();
    port=0;
    IP=QString();
    receivedDatas = new QByteArray();
    for (int i=0;i<33;i++)
        udpSequenceNumbers[i]=0;
    for (int i=0;i<33;i++)
        udpRecvSequenceNumbers[i]=0;
    udpRecvMissing.clear();

    // Prepare timers
    chatRollCooldownEnd = QDateTime::currentDateTime();
    udpSendReliableTimer =  new QTimer;
    udpSendReliableTimer->setInterval(UDP_RESEND_TIMEOUT);
    udpSendReliableTimer->setSingleShot(true);
    connect(udpSendReliableTimer, SIGNAL(timeout()), this, SLOT(udpResendLast()));
    udpSendReliableGroupTimer =  new QTimer;
    udpSendReliableGroupTimer->setInterval(UDP_GROUPING_TIMEOUT);
    udpSendReliableGroupTimer->setSingleShot(true);
    connect(udpSendReliableGroupTimer, SIGNAL(timeout()), this, SLOT(udpDelayedSend()));
}

Player::~Player()
{
    disconnect(udpSendReliableGroupTimer);
    disconnect(udpSendReliableTimer);
    delete udpSendReliableGroupTimer;
    delete udpSendReliableTimer;
    delete receivedDatas;
}

void Player::reset()
{
    name.clear();
    connected=false;
    inGame=0;
    nReceivedDups=0;
    lastPingNumber=0;
    lastPingTime=timestampNow();
    port=0;
    IP.clear();
    receivedDatas->clear();
    lastValidReceivedAnimation.clear();
    pony = Pony(this);
    for (int i=0;i<33;i++)
        udpSequenceNumbers[i]=0;
    for (int i=0;i<33;i++)
        udpRecvSequenceNumbers[i]=0;
    udpRecvMissing.clear();
}

void Player::resetNetwork()
{
    connected=false;
    nReceivedDups=0;
    lastPingNumber=0;
    lastPingTime=timestampNow();
    port=0;
    IP.clear();
    receivedDatas->clear();
    for (int i=0;i<33;i++)
        udpSequenceNumbers[i]=0;
    for (int i=0;i<33;i++)
        udpRecvSequenceNumbers[i]=0;
    udpRecvMissing.clear();
}

Player* Player::findPlayer(QList<Player*>& players, QString uname)
{
    for (int i=0; i<players.size(); i++)
    {
        if (players[i]->name.toLower() == uname.toLower())
            return players[i];
    }

    Player* emptyPlayer = new Player();
    return emptyPlayer;
}

Player* Player::findPlayer(QList<Player*>& players, QString uIP, quint16 uport)
{
    for (int i=0; i<players.size(); i++)
    {
        if (players[i]->IP == uIP && players[i]->port == uport)
            return players[i];
    }

    Player* emptyPlayer = new Player();
    return emptyPlayer;
}

Player* Player::findPlayer(QList<Player*>& players, quint16 netviewId)
{
    for (int i=0; i<players.size(); i++)
    {
        if (players[i]->pony.netviewId == netviewId)
            return players[i];
    }

    Player* emptyPlayer = new Player();
    return emptyPlayer;
}

void Player::removePlayer(QList<Player*>& players, QString uIP, quint16 uport)
{
    for (int i=0; i<players.size(); i++)
    {
        if (players[i]->IP == uIP && players[i]->port == uport)
            players.removeAt(i);
    }
#ifdef USE_GUI
    int connectedPlayers = Player::udpPlayers.length();
    app.ui->userCountLabel->setText(QString("%1 / %2").arg(connectedPlayers).arg(Settings::maxConnected));
#endif
}

void Player::updatePlayer(QList<Player*>& players, Player* player)
{
    for (int i=0; i<players.length(); i++)
    {
        if (players[i]->name.toLower() == player->name.toLower())
        {
            // sync only relevant attributes
            players[i]->lastOnline = player->lastOnline;
            players[i]->accessLvl = player->accessLvl;
        }
    }
}

void Player::disconnectPlayerCleanup(Player* player)
{
    static QMutex playerCleanupMutex;

    // Save the pony
    QList<Pony> ponies = loadPonies(player);
    for (int i=0; i<ponies.size(); i++)
    {
        if (ponies[i].ponyData == player->pony.ponyData)
            ponies[i] = player->pony;
    }
    savePonies(player, ponies);

    QString uIP = player->IP;
    quint16 uPort = player->port;

    Scene* scene = findScene(player->pony.sceneName);
    if (scene->name.isEmpty())
        logMessage(tr("UDP: Can't find scene for player cleanup"));

    //app.logMessage("playerCleanup locking");
    playerCleanupMutex.lock();
    removePlayer(scene->players, uIP, uPort);
    for (int i=0; i<scene->players.size(); i++)
        sendNetviewRemove(scene->players[i], player->pony.netviewId);
    player->udpDelayedSend(); // We're about to remove the player, we can't delay the send
    player->udpSendReliableTimer->stop();
    player->udpSendReliableGroupTimer->stop();
    removePlayer(Player::udpPlayers, uIP, uPort);
    delete player;
    //app.logMessage("playerCleanup unlocking");
    playerCleanupMutex.unlock();
}

void Player::udpResendLast()
{
    //app.logMessage("udpResendLast locking");
    if (!udpSendReliableMutex.tryLock())
    {
        logMessage(tr("udpResendLast failed to lock."));
        return; // Avoid deadlock if sendMessage just locked but didn't have the time to stop the timers
    }
    //udpSendReliableMutex.lock();
    QByteArray msg = udpSendReliableQueue.first();
    if (msg.isEmpty())
    {
        logMessage(tr("udpResendLast: Empty message"));
        udpSendReliableTimer->start();
        //app.logMessage("udpResendLast unlocking");
        udpSendReliableMutex.unlock();
        return;
    }

#if DEBUG_LOG
    app.logMessage("Resending message : "+QString(msg.toHex().data()));
#endif

    // Simulate packet loss if enabled (DEBUG ONLY!)
#if UDP_SIMULATE_PACKETLOSS
    if (qrand() % 100 <= UDP_SEND_PERCENT_DROPPED)
    {
        if (UDP_LOG_PACKETLOSS)
            app.logMessage("UDP: ResendLast packet dropped !");
        udpSendReliableTimer->start();
        //app.logMessage("udpResendLast unlocking");
        udpSendReliableMutex.unlock();
        return;
    }
    else if (UDP_LOG_PACKETLOSS)
        app.logMessage("UDP: ResendLast packet got throught");
#endif

    if (udpSocket->writeDatagram(msg,QHostAddress(IP),port) != msg.size())
    {
        logError(tr("UDP: Error sending last message"));
        restartUdpServer();
    }

    udpSendReliableTimer->start();

    //app.logMessage("udpResendLast unlocking");
    udpSendReliableMutex.unlock();
}

void Player::udpDelayedSend()
{
    //app.logMessage("udpDelayedSend locking");
    if (!udpSendReliableMutex.tryLock())
    {
#if DEBUG_LOG
        app.logMessage("UDP: udpDelayedSend failed to lock.");
#endif
        if (!udpSendReliableTimer->isActive())
            udpSendReliableTimer->start();
        return; // Avoid deadlock if sendMessage just locked but didn't have the time to stop the timers
    }
    //udpSendReliableMutex.lock();
#if DEBUG_LOG
    app.logMessage("UDP: Sending delayed grouped message : "+QString(udpSendReliableGroupBuffer.toHex()));
#endif

    // Move the grouped message to the reliable queue
    udpSendReliableQueue.append(udpSendReliableGroupBuffer);

    // If this is the only message queued, send it now
    // If it isn't, we need to wait until the previous one was ACK'd
    if (udpSendReliableQueue.size() >= 1)
    {
        // Simulate packet loss if enabled (DEBUG ONLY!)
#if UDP_SIMULATE_PACKETLOSS
        if (qrand() % 100 <= UDP_SEND_PERCENT_DROPPED)
        {
            if (UDP_LOG_PACKETLOSS)
                app.logMessage("UDP: Delayed send packet dropped !");
            udpSendReliableGroupBuffer.clear();
            if (!udpSendReliableTimer->isActive())
                udpSendReliableTimer->start();
            //app.logMessage("udpDelayedSend unlocking");
            udpSendReliableMutex.unlock();
            return;
        }
        else if (UDP_LOG_PACKETLOSS)
            logMessage("UDP: Delayed send packet got throught");
#endif

        if (udpSocket->writeDatagram(udpSendReliableGroupBuffer,QHostAddress(IP),port) != udpSendReliableGroupBuffer.size())
        {
            logError(tr("UDP: Error sending last message"));
            restartUdpServer();
        }
    }

    udpSendReliableGroupBuffer.clear();

    if (!udpSendReliableTimer->isActive())
        udpSendReliableTimer->start();

    //app.logMessage("udpDelayedSend unlocking");
    udpSendReliableMutex.unlock();
}

void Pony::addInventoryItem(quint32 id, quint32 qty)
{
    bool found=false;
    int firstFreeIndex=0;
    while (!found && firstFreeIndex < MAX_INVENTORY_SIZE)
    {
        found=true;
        for (InventoryItem& item : inv)
        {
            if (item.id == id)
            {
                item.amount += qty;
                sendAddItemRPC(owner, item);
                return;
            }
            else if (item.index == firstFreeIndex)
            {
                found=false;
                firstFreeIndex++;
                break;
            }
        }
    }
    if (!found)
        return;
    InventoryItem newItem(firstFreeIndex, id, qty);
    inv << newItem;
    sendAddItemRPC(owner, newItem);
}

void Pony::removeInventoryItem(quint32 id, quint32 qty)
{
    for (int i=0; i<inv.size(); i++)
    {
        if (inv[i].id == id)
        {
            sendDeleteItemRPC(owner, inv[i].index, qty);
            if (qty < inv[i].amount)
            {
                inv[i].amount -= qty;
                return;
            }
            else if (qty == inv[i].amount)
            {
                inv.removeAt(i);
                return;
            }
            else
            {
                qty -= inv[i].amount;
                inv.removeAt(i);
            }
        }
    }
}

bool Pony::hasInventoryItem(quint32 id, quint32 qty)
{
    for (const InventoryItem& item : inv)
    {
        if (item.id == id)
        {
            if (item.amount >= qty)
                return true;
            else
                qty -= item.amount;
        }
    }
    return qty==0;
}

void Pony::unwearItemAt(quint8 index)
{
    bool found=false;
    for (int i=0; i<worn.size();i++)
    {
        if (worn[i].index == index+1)
        {
            found=true;
            int itemSlots = wearablePositionsMap[worn[i].id];
            wornSlots = (wornSlots | itemSlots) ^ itemSlots;
            addInventoryItem(worn[i].id, 1);
            worn.removeAt(i);
            break;
        }
    }
    if (!found)
    {
        logMessage(QObject::tr("Couldn't unwear item, index %1 not found").arg(index));
        return;
    }
    sendUnwearItemRPC(owner, index);

    Scene* scene = findScene(sceneName);
    if (scene->name.isEmpty())
        logMessage(QObject::tr("UDP: Can't find the scene for unwearItem RPC, aborting"));
    else
    {
        for (Player* dest : scene->players)
            if (dest->pony.netviewId != netviewId)
                sendUnwearItemRPC(this, dest, index);
    }
}

bool Pony::tryWearItem(quint8 invSlot)
{
    //app.logMessage("Invslot is "+QString().setNum(invSlot));
    uint32_t id = -1;
    uint32_t itemSlots;
    for (int i=0; i<inv.size(); i++)
    {
        if (inv[i].index == invSlot)
        {
            id = inv[i].id;

            itemSlots = wearablePositionsMap[id];
            if (wornSlots & itemSlots)
            {
                //logMessage(QObject::tr("Can't wear item : slots occupied"));
                return false;
            }

            sendDeleteItemRPC(owner,inv[i].index,1);
            if (inv[i].amount>1)
                inv[i].amount--;
            else
                inv.removeAt(i);
            break;
        }
    }
    if (id == (uint32_t)-1)
    {
        logError(QObject::tr("Index %1 not found").arg(id));
        return false;
    }
    wornSlots |= itemSlots;

    WearableItem item;
    item.id = id;
    item.index = wearablePositionsToSlot(itemSlots);
    //app.logMessage("Wearing at slot "+QString().setNum(item.index));
    worn << item;
    sendWearItemRPC(owner, item);

    Scene* scene = findScene(sceneName);
    if (scene->name.isEmpty())
        logError(QObject::tr("UDP: Can't find the scene for wearItem RPC, aborting"));
    else
    {
        for (Player* dest : scene->players)
            if (dest->pony.netviewId != netviewId)
                sendWearItemRPC(this, dest, item);
    }

    return true;
}

void Pony::takeDamage(unsigned amount)
{
    if (health <= (float)amount/defense)
        kill();
    else
    {
        health -= (float)amount/defense;
        Scene* scene = findScene(sceneName);
        for (Player* player : scene->players)
        {
            sendSetStatRPC(player, netviewId, 1, health);
        }
    }
}

void Pony::kill()
{
    health = 0;
    dead = true;

    Scene* scene = findScene(sceneName);
    for (Player* player : scene->players)
    {
        if (player->pony.netviewId != netviewId)
            sendNetviewRemove(player, netviewId, NetviewRemoveReasonKill);
    }

    respawn();
}

void Pony::respawn()
{
    health = maxHealth;

    sendLoadSceneRPC(owner, sceneName);
    dead = false;
}
