#ifndef CHARACTER_H
#define CHARACTER_H

#include <QObject>
#include <QString>
#include <QVector>
#include <QTimer>
#include <QMutex>
#include <QByteArray>
#include <QDateTime>
#include "dataType.h"
#include "quest.h"
#include "sceneEntity.h"
#include "statsComponent.h"

#define MAX_INVENTORY_SIZE 12
#define MAX_WORN_ITEMS 32
#define PONYDATA_SIZE 43 // Size of the PonyData minus the name

struct Pony : public SceneEntity, public StatsComponent
{
public:
    enum type
    {
        None,
        EarthPony,
        Unicorn,
        Pegasus,
        Moose
    };

public:
    Pony(Player* Owner);
    virtual ~Pony()=default;
    type getType();
    void saveQuests(QList<QString> ponyNames); ///< Saves the state of all the quests (NOT thread safe)
    void loadQuests(); ///< Loads the state of all the quests
    void saveInventory(QList<QString> ponyNames); ///< Saves the worn/possesed items and the money (NOT thread safe)
    bool loadInventory(); ///< Loads the worn/possesed items and the money. False on error.
    void addInventoryItem(quint32 id, quint32 qty); ///< Adds qty items with the given id to the inventory
    void removeInventoryItem(quint32 id, quint32 qty); ///< Removes qty of the item from the inventory
    bool hasInventoryItem(quint32 id, quint32 qty=1); ///< Whether of not there are qty of this item in inventory
    void unwearItemAt(quint8 index); ///< Unwear the item at the position index
    bool tryWearItem(quint8 invSlot); ///< Tries to wear this item. Must be in the inventory
    virtual void takeDamage(unsigned amount) override; ///< Remove health, update the client, may kill the pony
    virtual void kill() override; ///< Kills the player. He'll respawn
    virtual void respawn() override; ///< Respawn the player at the default spawn of his scene

public:
    QByteArray ponyData;
    QString name;
    QList<InventoryItem> inv; // Inventory
    QList<WearableItem> worn; // Worn items
    quint32 nBits; // Number of bits (money)
    QList<Quest> quests; // State of the player's quests
    quint32 lastQuest; // Last quest script the player ran
    quint32 wornSlots; // Flag of the used item-wearing slots (See enum WearablePositions)
    Player* owner;
    bool dead; // If true, the player is in the room, but not instantiated
    float maxHealth;
    float defense;
};

class Player : QObject
{
    Q_OBJECT
    Q_DISABLE_COPY(Player)

public:
    Player();
    ~Player();
    static void savePonies(Player* player,QList<Pony> ponies);
    static QList<Pony> loadPonies(Player *player);
    static bool savePlayers(QList<Player*>& playersData);
    static QList<Player*> loadPlayers();
    static Player* findPlayer(QList<Player*>& players, QString uname);
    static Player* findPlayer(QList<Player*>& players, QString uIP, quint16 uport);
    static Player* findPlayer(QList<Player*>& players, quint16 netviewId);
    static void removePlayer(QList<Player*>& players, QString uIP, quint16 uport);
    static void disconnectPlayerCleanup(Player* player);

public slots:
    void udpResendLast(); // If a reliable msg wasn't ACK'd yet, resend it now.
    void udpDelayedSend(); // Enqueue and send the content of the player's grouped message buffer

public:
    void reset(); // Reconstructs an empty Player
    void resetNetwork(); // Resets all the network-related members

public:
    QString IP;
    quint16 port;
    QString name;
    QString passhash;
    float lastPingTime;
    int lastPingNumber;
    bool connected;
    quint16 udpSequenceNumbers[33]; // Next seq number to use when sending a message
    quint16 udpRecvSequenceNumbers[33]; // Last seq number received
    QByteArray *receivedDatas;
    QVector<MessageHead> udpRecvMissing; // When a message is skipped, mark it as missing and wait for a retransmission
    QVector<QByteArray> udpSendReliableQueue; // Messages that we're sending and that aren't ACKd yet.
    QByteArray udpSendReliableGroupBuffer; // Groups the udp message in this buffer before sending them
    QTimer* udpSendReliableGroupTimer; // Delays the sending until we finished grouping the messages
    QTimer* udpSendReliableTimer; // If we didn't get an ACK before this timeouts, resend the last message
    QMutex udpSendReliableMutex; // Protects the buffer/queue/timers from concurrency hell
    Pony pony;
    QByteArray lastValidReceivedAnimation;
    quint8 inGame; // 0:Not in game, 1:Loading, 2:Instantiated & waiting savegame, 3:In game and loaded
    quint16 nReceivedDups; // Number of duplicate packets that we didn't miss and had to discard.
    QDateTime chatRollCooldownEnd; // When the cooldown for the roll chat command ends

public:
    static QList<Player*> tcpPlayers; // Used by the TCP login server
    static QList<Player*> udpPlayers; // Used by the UDP game server
};

#endif // CHARACTER_H
