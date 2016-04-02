#include <QFile>
#include <QDir>
#include <QLocale>
#include "player.h"
#include "log.h"
#include "serialize.h"
#include "quest.h"
#include "items.h"
#include <QXmlStreamWriter>
#include <QtXml/qdom.h>

#define DEBUG_LOG false

bool Player::savePlayers(QList<Player*>& playersData)
{
    QFile playersFile(QDir::currentPath()+"/data/players/players.xml");

    if (!playersFile.open(QIODevice::ReadWrite | QIODevice::Truncate))
    {
        logError(QObject::tr("Error writing %1").arg(playersFile.fileName()));
        return false;
    }

    QXmlStreamWriter xmlWriter(&playersFile);
    xmlWriter.setAutoFormatting(true);
    xmlWriter.writeStartDocument();
    xmlWriter.writeStartElement("root");

    for (int i=0; i<playersData.size(); i++)
    {
        xmlWriter.writeStartElement("player");
            xmlWriter.writeTextElement("name", playersData[i]->name);
            xmlWriter.writeTextElement("passhash", playersData[i]->passhash);
            xmlWriter.writeTextElement("accesslevel", QString::number(playersData[i]->accessLvl));
            xmlWriter.writeTextElement("lastOnline", playersData[i]->lastOnline.toString("yyyy-MM-dd HH:mm:ss"));
        xmlWriter.writeEndElement();
    }

    xmlWriter.writeEndElement();
    xmlWriter.writeEndDocument();

    if (xmlWriter.hasError())
    {
        logError(QObject::tr("XML Writer Error writing %1").arg(playersFile.fileName()));
        return false;
    }

    playersFile.close();
    return true;
}

QList<Player*> Player::loadPlayers()
{
    QList<Player*> players;
    QFile playersFile(QDir::currentPath()+"/data/players/players.xml");
    QDomDocument doc;

    if (!playersFile.exists())
    {
        logMessage(tr("Players database not found, creating it"));
        QDir playersDir("data/players");
        if(!playersDir.exists())
            playersDir.mkpath(".");

        if (!playersFile.open(QIODevice::ReadWrite | QIODevice::Truncate))
        {
            logError(QObject::tr("Error creating %1").arg(playersFile.fileName()));
            return players;
        }

        QXmlStreamWriter xmlWriter(&playersFile);
        xmlWriter.setAutoFormatting(true);
        xmlWriter.writeStartDocument();
        xmlWriter.writeStartElement("root");
        xmlWriter.writeEndElement();
        xmlWriter.writeEndDocument();

        playersFile.close();

        if (xmlWriter.hasError())
        {
            logError(QObject::tr("XML Writer Error writing %1").arg(playersFile.fileName()));
            return players;
        }
    }

    if(!playersFile.open(QIODevice::ReadOnly) || !doc.setContent(&playersFile)) // parse xml file
    {
        logError(QObject::tr("Error opening %1").arg(playersFile.fileName()));
        return players;
    }

    QDomNodeList DomPlayers = doc.elementsByTagName("player");
    for (int i = 0; i < DomPlayers.size(); i++)
    {
        QDomNode n = DomPlayers.item(i);
        Player* newPlayer = new Player;

        QDomElement DomName = n.firstChildElement("name");
        QDomElement DomPassHash = n.firstChildElement("passhash");
        QDomElement DomAccessLvl = n.firstChildElement("accesslevel");
        QDomElement DomLastOnline = n.firstChildElement("lastOnline");

        if (DomName.isNull() || DomPassHash.isNull() || DomAccessLvl.isNull() || DomLastOnline.isNull() )
            continue;

        newPlayer->name = DomName.text();
        newPlayer->passhash = DomPassHash.text();
        newPlayer->accessLvl = DomAccessLvl.text().toInt();

        QString sLastOn = DomLastOnline.text();
        newPlayer->lastOnline = QLocale("en_US").toDateTime(sLastOn.simplified(), "yyyy-M-d H:m:s");

        players << newPlayer;
    }

    playersFile.close();
    logMessage(tr("Got %1 players in database").arg(players.size()));
    return players;
}

void Player::savePonies(Player *player, QList<Pony> ponies)
{
    logMessage(tr("UDP: Saving ponies for %1 (%2)").arg(player->pony.netviewId).arg(player->name));

    QDir playerDir(QDir::currentPath()+"/data/players/"+player->name.toLatin1());
    if (!playerDir.exists())
        playerDir.mkpath(".");

    QFile xmlfile(playerDir.path()+"/"+player->name.toLatin1()+".xml");

    // save in xml format
    if (!xmlfile.open(QIODevice::ReadWrite | QIODevice::Truncate))
    {
        logError(QObject::tr("Error saving ponies for %1 (%2)").arg(player->name).arg(player->pony.netviewId));
        return;
    }

    QXmlStreamWriter xmlWriter(&xmlfile);
    xmlWriter.setAutoFormatting(true);
    xmlWriter.writeStartDocument();
    xmlWriter.writeStartElement("root");

    for (int i=0; i<ponies.size(); i++)
    {
        xmlWriter.writeStartElement("pony");
            // Write pony
            xmlWriter.writeTextElement("name", ponies[i].name);
            xmlWriter.writeTextElement("ponydata", ponies[i].ponyData.toBase64());
//            if (ponies[i].accessLvl == 0)
//                ponies[i].accessLvl = 1;
//            xmlWriter.writeTextElement("accesslevel", QString::number(ponies[i].accessLvl));
            xmlWriter.writeStartElement("pos");
                xmlWriter.writeAttribute("x", QString::number(ponies[i].pos.x));
                xmlWriter.writeAttribute("y", QString::number(ponies[i].pos.y));
                xmlWriter.writeAttribute("z", QString::number(ponies[i].pos.z));
            xmlWriter.writeEndElement();
            xmlWriter.writeStartElement("rot");
                xmlWriter.writeAttribute("x", QString::number(ponies[i].rot.x));
                xmlWriter.writeAttribute("y", QString::number(ponies[i].rot.y));
                xmlWriter.writeAttribute("z", QString::number(ponies[i].rot.z));
                xmlWriter.writeAttribute("w", "1");
            xmlWriter.writeEndElement();
            xmlWriter.writeTextElement("scene", ponies[i].sceneName.toLower());

            // write inventory
            xmlWriter.writeTextElement("bits", QString::number(ponies[i].nBits));
            xmlWriter.writeStartElement("inventory");
            for (const InventoryItem& item : ponies[i].inv)
            {
                xmlWriter.writeStartElement("item");
                    xmlWriter.writeAttribute("slot", QString::number(item.index));
                    xmlWriter.writeAttribute("id", QString::number(item.id));
                    xmlWriter.writeAttribute("amount", QString::number(item.amount));
                xmlWriter.writeEndElement();
            }
            xmlWriter.writeEndElement();

            // write equipped items
            xmlWriter.writeStartElement("equipped");
            for (const WearableItem& item : ponies[i].worn)
            {
                xmlWriter.writeStartElement("item");
                    xmlWriter.writeAttribute("slot", QString::number(item.index));
                    xmlWriter.writeAttribute("id", QString::number(item.id));
                xmlWriter.writeEndElement();
            }
            xmlWriter.writeEndElement();

            // write Quests
            xmlWriter.writeStartElement("quests");
            for (int j=0; j<ponies[i].quests.size(); j++)
            {
                if (ponies[i].quests[j].state != 0) //write only quests with states
                {
                    xmlWriter.writeStartElement("quest");
                    xmlWriter.writeAttribute("id", QString::number(ponies[i].quests[j].id));
                    xmlWriter.writeAttribute("state", QString::number(ponies[i].quests[j].state));
                    xmlWriter.writeEndElement();
                }
            }
            xmlWriter.writeEndElement();

        xmlWriter.writeEndElement();
    }

    xmlWriter.writeEndElement();
    xmlWriter.writeEndDocument();

    if (xmlWriter.hasError())
    {
        logError("XMLWriter Error");
    }

    xmlfile.close();

}

QList<Pony> Player::loadPonies(Player* player)
{
    QList<Pony> ponies;
    QDomDocument doc;

    QDir playerDir(QDir::currentPath()+"/data/players/"+player->name.toLatin1());
    if (!playerDir.exists())
        playerDir.mkpath(".");

    QFile xmlfile(playerDir.path()+"/"+player->name.toLatin1()+".xml");

    if(!xmlfile.open(QIODevice::ReadOnly) || !doc.setContent(&xmlfile)) // parse xml file
    {
        logError(QObject::tr("Error reading ponies for %1").arg(player->name));
        return ponies;
    }

    QDomNodeList DomPonies = doc.elementsByTagName("pony");
    for (int i = 0; i < DomPonies.size(); i++)
    {
        QDomNode n = DomPonies.item(i);
        // read pony
        QDomElement DomName = n.firstChildElement("name");
        QDomElement DomPonyData = n.firstChildElement("ponydata");
        QDomElement DomScene = n.firstChildElement("scene");
        QDomElement DomPos = n.firstChildElement("pos");
        QDomElement DomRot = n.firstChildElement("rot");

        if (DomName.isNull() || DomPonyData.isNull() || DomScene.isNull() || DomPos.isNull() )
            continue;

        Pony pony{player};
        pony.ponyData = QByteArray::fromBase64(DomPonyData.text().toUtf8());
        pony.name = dataToString(pony.ponyData);
        pony.sceneName = DomScene.text();

        pony.pos.x = DomPos.attribute("x").toFloat();
        pony.pos.y = DomPos.attribute("y").toFloat();
        pony.pos.z = DomPos.attribute("z").toFloat();

        pony.rot.x = DomRot.attribute("x","0").toFloat();
        pony.rot.y = DomRot.attribute("y","0").toFloat();
        pony.rot.z = DomRot.attribute("z","0").toFloat();
        pony.rot.w = DomRot.attribute("w","0").toFloat();

        //logMessage(QObject::tr("found pony %1, access lvl: %2").arg(pony.name).arg(pony.accessLvl));

        // read inventory
        QDomElement DomBits = n.firstChildElement("bits");
        if (DomBits.isNull() || DomBits.text().isNull() || DomBits.text().isEmpty())
            pony.nBits = 0;
        else
            pony.nBits = DomBits.text().toInt();

        QDomElement DomInventory = n.firstChildElement("inventory");
        QDomNodeList DomInventoryItems = DomInventory.elementsByTagName("item");
        for (int j = 0; j < DomInventoryItems.size(); j++)
        {
             if(DomInventoryItems.item(j).isElement())
             {
                 QDomElement DomItem = DomInventoryItems.item(j).toElement();
                 InventoryItem item;
                 item.index = DomItem.attribute("slot").toInt();
                 item.id = DomItem.attribute("id").toInt();
                 item.amount = DomItem.attribute("amount").toInt();

                 pony.inv.append(item);
             }
        }

//            logMessage(pony.name + "'s inventory from xml:");
//            for (const InventoryItem& item : pony.inv)
//            {
//                logMessage(QObject::tr("slot: %1, item: %2, amount: %3").arg(item.index).arg(item.id).arg(item.amount));
//            }

        QDomElement DomWorn = n.firstChildElement("equipped");
        QDomNodeList DomWornItems = DomWorn.elementsByTagName("item");
        for (int j = 0; j < DomWornItems.size(); j++)
        {
            if(DomWornItems.item(j).isElement())
            {
                QDomElement DomItem = DomWornItems.item(j).toElement();
                WearableItem item;
                item.index = DomItem.attribute("slot").toInt();
                item.id = DomItem.attribute("id").toInt();

                pony.worn.append(item);
                pony.wornSlots |= wearablePositionsMap[item.id];
            }
        }

        // make sure the player has all quests (otherwise NPC refuse to talk)
        for (int j=0; j<Quest::quests.size(); j++)
        {
            Quest quest = Quest::quests[j];
            quest.setOwner(player);
            pony.quests << quest;
        }

        // read quests
         QDomElement DomQuests = n.firstChildElement("quests");
         QDomNodeList DomQuestList = DomQuests.elementsByTagName("quest");
         for (int j = 0; j < DomQuestList.size(); j++)
         {
             if(DomQuestList.item(j).isElement())
             {
                 QDomElement DomQuest = DomQuestList.item(j).toElement();
                 for (int k=0; k<pony.quests.size(); k++)
                 {
                     if (pony.quests[k].id == DomQuest.attribute("id").toInt())
                     {
                         pony.quests[k].state = DomQuest.attribute("state").toInt();
                         //logMessage(QObject::tr("found %1 Quest ID %2 State: %3").arg(pony.name).arg(pony.quests[k].id).arg(pony.quests[k].state));
                         break;
                     }
                 }
             }
         }

        ponies << pony;
    }

    xmlfile.close();
    return ponies;
} // end load Ponies

void Player::removePonies(Player* player)
{
    QString playerName = player->name.toLatin1();
    playerName.remove(".");
    playerName.remove("\\");
    playerName.remove("/");
    QDir ponyPath(QDir::currentPath()+"/data/players/"+ playerName);
    bool result = false;
    result = ponyPath.removeRecursively();
    if ( result == false )
        logError(tr("Error Removing Path %1").arg(ponyPath.absolutePath()));
}
