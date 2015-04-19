#include "mob.h"
#include "mobzone.h"
#include "app.h"
#include "mobsStats.h"
#include "message.h"
#include "scene.h"

QList<Mob*> Mob::mobs;
QList<Mobzone*> Mob::mobzones;

Mob::Mob(Mobzone* zone)
{
    spawnZone = currentZone = zone;
    sceneName = zone->sceneName;

    id = SceneEntity::getNewId();
    netviewId = id;
    //netviewId = SceneEntity::getNewNetviewId();

    pos = getRandomPos(zone);

    rot = {0, (float)(rand()%4-2), 0, 1};

    health = 0; // We need to know the type to know the default healt
}

UVector Mob::getRandomPos(Mobzone* zone)
{
    UVector pos;

    // "Normalize" our coordinates for the random part
    UVector nstart = {std::min(zone->start.x, zone->end.x),
                        std::min(zone->start.y, zone->end.y),
                        std::min(zone->start.z, zone->end.z)};
    UVector nend = {std::max(zone->start.x, zone->end.x),
                      std::max(zone->start.y, zone->end.y),
                      std::max(zone->start.z, zone->end.z)};

    // Pick a random x/z point on the plane
    int xmod = floor(abs(nend.x-nstart.x));
    if (xmod == 0)      pos.x = nstart.x;
    else if (xmod <0)   pos.x = -(rand()%(xmod))+nstart.x;
    else                pos.x = (rand()%(xmod))+nstart.x;

    int zmod = floor(abs(nend.z-nstart.z));
    if (zmod == 0)      pos.z = nstart.z;
    else if (zmod < 0)  pos.z = -(rand()%(zmod))+nstart.z;
    else                pos.z = (rand()%(zmod))+nstart.z;

    // Compute the diagonal's properties
    float diagVectX = zone->end.x - zone->start.x;
    float diagVectZ = zone->end.z - zone->start.z;
    float diagLength = sqrt(diagVectX*diagVectX + diagVectZ*diagVectZ);

    // Coords of the point projected on the diagonal
    float projectedX = (diagVectX*diagVectX*(pos.x-zone->start.x) + diagVectX*diagVectZ*(pos.z-zone->start.z))
                        / (diagVectX*diagVectX + diagVectZ*diagVectZ);
    float projectedZ = (diagVectX*diagVectZ*(pos.x-zone->start.x) + diagVectZ*diagVectZ*(pos.z-zone->start.z))
                        / (diagVectX*diagVectX + diagVectZ*diagVectZ);
    float projectedLength = sqrt(projectedX*projectedX + projectedZ * projectedZ);

    // Deduce the height (y) of the point
    pos.y = zone->start.y + projectedLength/diagLength * (zone->end.y - zone->start.y);
    return pos;
}

void Mob::setType(QString ModelName)
{
    ModelName = ModelName.toLower();
    modelName = ModelName;
    if (modelName == "mobs/birch dryad")
        type = MobType::birch_dryad;
    else if (modelName == "mobs/bunny")
        type = MobType::bunny;
    else if (modelName == "mobs/cockatrice")
        type = MobType::cockatrice;
    else if (modelName == "mobs/dragon")
        type = MobType::dragon;
    else if (modelName == "mobs/hornet")
        type = MobType::hornet;
    else if (modelName == "mobs/husky diamond dog")
        type = MobType::husky_diamond_dog;
    else if (modelName == "mobs/lantern monster")
        type = MobType::lantern_monster;
    else if (modelName == "mobs/timberwolf")
        type = MobType::timberwolf;
    else
        throw QString(QObject::tr("Mob::setType(): Error, unknown type %1").arg(modelName));

    health = defaultMaxHealth[type];
}

void Mob::takeDamage(unsigned amount)
{
    if (health <= (float)amount/defaultDefense[type])
        kill();
    else
    {
        health -= (float)amount/defaultDefense[type];
        Scene* scene = findScene(sceneName);
        for (Player* player : scene->players)
        {
            sendSetStatRPC(player, netviewId, 1, health);
        }
    }
}

void Mob::kill()
{
    currentZone = spawnZone;
    health = 0;

    Scene* scene = findScene(sceneName);
    for (Player* player : scene->players)
    {
        sendSetStatRPC(player, netviewId, 1, 0);
        sendNetviewRemove(player, netviewId, NetviewRemoveReasonKill);
    }

    respawn();
}

void Mob::respawn()
{
    currentZone = spawnZone;
    pos = getRandomPos(spawnZone);
    rot = {0, (float)(rand()%4-2), 0, 1};

    health = defaultMaxHealth[type];

    Scene* scene = findScene(sceneName);
    for (Player* player : scene->players)
    {
        sendNetviewInstantiate(player, modelName, netviewId, id, pos, rot);
    }
}
