#include "quest.h"
#include "app.h"
#include "message.h"
#include "items.h"
#include "player.h"
#include <QFile>

QList<Pony*> Quest::npcs; // List of npcs from the npcs DB
QList<Quest> Quest::quests; // List of quests from the npcs DB

Quest::Quest(QString path, Player *Owner)
{
    QFile file(path);
    if (!file.open(QFile::ReadOnly))
    {
        app.logError(QObject::tr("Error reading quest DB"));
        app.stopGameServer();
        throw std::exception();
    }

    QList<QString> lines = QString(file.readAll().replace('\r',"")).split('\n');

    owner = Owner;
    commands = new QList<QList<QString> >;
    name = new QString();
    descr = new QString();
    npc = new Pony(nullptr); // A NPC doesn't have an owner player !
    npc->id = 0;
    npc->netviewId = 0;
    id = 0;
    state = 0;
    eip = 0;

    // Parse the metadata, add everything else as quest commands
    for (int i=0; i<lines.size(); i++)
    {
        QList<QString> line = lines[i].split(" ", QString::SkipEmptyParts);
        if (!line.size() || lines[i][0]=='#')
            continue;
        if (line[0] == "name")
            if (line.size()>=2)
                npc->name = lines[i].mid(line[0].size()+1);
            else throw QString(QObject::tr("Quest::Quest: Error reading name, quest %1").arg(path));
        else if (line[0] == "scene")
            if (line.size()>=2)
                npc->sceneName = lines[i].mid(line[0].size()+1).toLower();
            else throw QString(QObject::tr("Quest::Quest: Error reading scene, quest %1").arg(path));
        else if (line[0] == "ponyData")
        {
            if (line.size()==2)
                npc->ponyData = QByteArray::fromBase64(line[1].toLatin1());
            else throw QString(QObject::tr("Quest::Quest: Error reading ponyData, quest %1").arg(path));
        }
        else if (line[0] == "pos")
            if (line.size()==4)
            {
                bool ok1, ok2, ok3;
                npc->pos = UVector(line[1].toFloat(&ok1), line[2].toFloat(&ok2),
                                    line[3].toFloat(&ok3));
                if (!(ok1 && ok2 && ok3))
                    throw QString(QObject::tr("Quest::Quest: Error reading pos, quest %1").arg(path));
            }
            else throw QString(QObject::tr("Quest::Quest: Error reading pos, quest %1").arg(path));
        else if (line[0] == "rot")
            if (line.size()==5)
            {
                bool ok1, ok2, ok3, ok4;
                npc->rot = UQuaternion(line[1].toFloat(&ok1), line[2].toFloat(&ok2),
                                        line[3].toFloat(&ok3), line[4].toFloat(&ok4));
                if (!(ok1 && ok2 && ok3 && ok4))
                    throw QString(QObject::tr("Quest::Quest: Error reading rot, quest %1").arg(path));
            }
            else throw QString(QObject::tr("Quest::Quest: Error reading rot, quest %1").arg(path));
        else if (line[0] == "wear")
        {
            for (int i=1; i<line.size(); i++)
            {
                bool ok;
                int itemId = line[i].toInt(&ok);
                if (!ok)
                    throw QString(QObject::tr("Quest::Quest: Error reading wear, quest %1").arg(path));
                WearableItem item;
                item.id = itemId;
                item.index = wearablePositionsToSlot(wearablePositionsMap[itemId]);
                npc->worn << item;
            }
        }
        else if (line[0] == "shop")
        {
            for (int i=1; i<line.size(); i++)
            {
                bool ok;
                int itemId = line[i].toInt(&ok);
                if (!ok)
                    throw QString(QObject::tr("Quest::Quest: Error reading shop, quest %1").arg(path));
                InventoryItem item;
                item.id = itemId;
                item.index = i-1;
                item.amount = (quint32)-1;
                npc->inv << item;
            }
        }
        else if (line[0] == "questId")
            if (line.size()==2)
            {
                id = line[1].toInt();

                SceneEntity::lastIdMutex.lock();
                npc->id = 0;
                npc->netviewId = id;
                SceneEntity::lastIdMutex.unlock();
            }
            else throw QString(QObject::tr("Quest::Quest: Error reading questId, quest %1").arg(path));
        else if (line[0] == "questName")
            if (line.size()>=2)
                *name = lines[i].mid(line[0].size()+1);
            else throw QString(QObject::tr("Quest::Quest: Error reading questName, quest %1").arg(path));
        else if (line[0] == "questDescr")
            if (line.size()>=2)
                *descr = lines[i].mid(line[0].size()+1);
            else throw QString(QObject::tr("Quest::Quest: Error reading questDescr, quest %1").arg(path));
        else
            commands->append(line);
    }
}

QString Quest::concatAfter(QList<QString> list, int id) const
{
    QString result;
    if (list.size() <= id)
        return result;
    result = list[id];
    for (int i=id+1; i<list.size(); i++)
        result += " " + list[i];
    return result;
}

int Quest::findLabel(QString label)
{
    for (int i=0; i<commands->size(); i++)
        if ((*commands)[i].size()==2 && (*commands)[i][0] == "label" && (*commands)[i][1].trimmed()==label.trimmed())
            return i;
    return -1;
}

void Quest::logError(QString message)
{
    app.logError(QObject::tr("Error running quest script %1, eip=%2 : %3").arg(id).arg(eip).arg(message));
}

void Quest::setOwner(Player* Owner)
{
    owner = Owner;
}

void Quest::runScript()
{
    runScript(0);
}

void Quest::runScript(int Eip)
{
    for (eip=Eip; eip<commands->size();)
        if (doCommand(eip))
            eip++;
        else
            break;
}

bool Quest::doCommand(int commandEip)
{
    if (!owner)
    {
        app.logMessage(QObject::tr("Quest::doCommand called with no owner"));
        return false;
    }

    //app.logMessage("Executing command "+QString().setNum(eip));

    QStringList command = (*commands)[commandEip];

    if (command[0] == "label")
        return true;
    else if (command[0] == "goto")
    {
        if (command.size() != 2)
        {
            logError(QObject::tr("goto takes exactly one argument"));
            return false;
        }
        int newEip = findLabel(command[1]);
        if (newEip == -1)
        {
            logError(QObject::tr("label not found"));
            return false;
        }
        eip = newEip;
    }
    else if (command[0] == "end")
    {
        sendEndDialog(owner);
        return false;
    }
    else if (command[0] == "say")
    {
        QString msg, npcName;
        QList<QString> answers;
        quint16 iconId=0;
        bool hasIconId=false;
        if (command.size() >= 2)
        {
            msg = concatAfter(command, 1);
            npcName = npc->name;
        }
        else
        {
            logError(QObject::tr("say takes 2 arguments"));
            return false;
        }

        // Parse answers, icon, and name
        for (int i=commandEip+1; i<commands->size();i++)
        {
            if ((*commands)[i][0] == "answer")
                answers << concatAfter((*commands)[i], 2);
            else if ((*commands)[i][0] == "sayName")
                npcName = concatAfter((*commands)[i], 1);
            else if ((*commands)[i][0] == "sayIcon")
            {
                bool ok;
                int id = (*commands)[i][1].toInt(&ok);
                if (!ok || id < 0)
                {
                    logError(QObject::tr("invalid icon id"));
                    return false;
                }
                hasIconId = true;
                iconId = id;
            }
            else
                break;
        }

        // Replace special variables
        msg.replace("$PLAYERNAME", owner->pony.name);
        for (QString& s : answers)
            s.replace("$PLAYERNAME", owner->pony.name);

        // Send
        sendBeginDialog(owner);
        if (hasIconId) // Use the 2D sprites
        {
            sendDialogMessage(owner, msg, npcName, iconId);
            sendDialogMessage(owner, msg, npcName, iconId);
        }
        else // Use the 3D view
        {
            sendDialogMessage(owner, msg, npcName, npc->netviewId, iconId);
            sendDialogMessage(owner, msg, npcName, npc->netviewId, iconId);
        }
        sendDialogOptions(owner, answers);
        return false; // We stop the script until we get a reply
    }
    else if (command[0] == "answer") // can only be ran after a say, sayName, sayIcon, or another answer
    {
        logError(QObject::tr("trying to run answer command by itself"));
        return false;
    }
    else if (command[0] == "sayName") // can only be ran after a say, sayIcon, answer, or another sayName
    {
        logError(QObject::tr("trying to run sayName command by itself"));
        return false;
    }
    else if (command[0] == "sayIcon") // Answer can only be ran after a say, sayName, answer, or another sayIcon
    {
        logError(QObject::tr("trying to run sayIcon command by itself"));
        return false;
    }
    else if (command[0] == "give")
    {
        if (command.size() != 3)
        {
            logError(QObject::tr("give takes 2 arguments"));
            return false;
        }
        bool ok1,ok2;
        int itemId = command[1].toInt(&ok1);
        int qty = command[2].toInt(&ok2);
        if (!ok1 || !ok2 || itemId<0)
        {
            logError(QObject::tr("invalid arguments for command give"));
            return false;
        }
        if (qty > 0)
            owner->pony.addInventoryItem(itemId, qty);
        else
            owner->pony.removeInventoryItem(itemId, -qty);
        //sendInventoryRPC(owner, owner->pony.inv, owner->pony.worn, owner->pony.nBits);
    }
    else if (command[0] == "giveBits")
    {
        if (command.size() != 2)
        {
            logError(QObject::tr("giveBits takes 1 argument"));
            return false;
        }
        bool ok1;
        int qty = command[1].toInt(&ok1);
        if (!ok1)
        {
            logError(QObject::tr("invalid argument for command giveBits"));
            return false;
        }
        if (qty<0 && (quint32)-qty > owner->pony.nBits)
            owner->pony.nBits = 0;
        else
            owner->pony.nBits += qty;
        sendSetBitsRPC(owner);
    }
    else if (command[0] == "setQuestState")
    {
        if (command.size() == 2)
        {
            bool ok1;
            int newState = command[1].toInt(&ok1);
            if (!ok1 || newState<0)
            {
                logError(QObject::tr("invalid argument for command setQuestState"));
                return false;
            }
            this->state = newState;
        }
        else if (command.size() == 3)
        {
            bool ok1,ok2;
            int newState = command[1].toInt(&ok1);
            int id = command[2].toInt(&ok2);
            if (!ok1 || !ok2 || newState<0 || id<0)
            {
                logError(QObject::tr("invalid arguments for command setQuestState"));
                return false;
            }
            for (Quest& quest : owner->pony.quests)
                if (quest.id == id)
                    quest.state = newState;
        }
        else
        {
            logError(QObject::tr("setQuestState takes 1 or 2 arguments"));
            return false;
        }
    }
    else if (command[0] == "hasItem")
    {
        int itemId, qty=1, yesEip, noEip;
        if (command.size() == 4)
        {
            bool ok1;
            itemId = command[1].toInt(&ok1);
            if (!ok1 || itemId<0)
            {
                logError(QObject::tr("invalid arguments for command hasItem"));
                return false;
            }
            yesEip = findLabel(command[2]);
            noEip = findLabel(command[3]);
        }
        else if (command.size() == 5)
        {
            bool ok1,ok2;
            itemId = command[1].toInt(&ok1);
            qty = command[2].toInt(&ok2);
            if (!ok1 || !ok2 || qty<=0 || itemId<0)
            {
                logError(QObject::tr("invalid arguments for command hasItem"));
                return false;
            }
            yesEip = findLabel(command[3]);
            noEip = findLabel(command[4]);
        }
        else
        {
            logError(QObject::tr("hasItem takes 3 or 4 arguments"));
            return false;
        }
        if (yesEip == -1)
        {
            logError(QObject::tr("label for a verified condition not found"));
            return false;
        }
        else if (noEip == -1)
        {
            logError(QObject::tr("label for an unverified condition not found"));
            return false;
        }
        if (owner->pony.hasInventoryItem(itemId, qty))
            eip=yesEip;
        else
            eip=noEip;
        return true;
    }
    else if (command[0] == "hasBits")
    {
        if (command.size() != 4)
        {
            logError(QObject::tr("hasBits takes 3 arguments"));
            return false;
        }
        int qty, yesEip, noEip;
        bool ok1;
        qty = command[1].toInt(&ok1);
        if (!ok1 || qty<=0)
        {
            logError(QObject::tr("invalid arguments for command hasBits"));
            return false;
        }
        yesEip = findLabel(command[2]);
        noEip = findLabel(command[3]);
        if (yesEip == -1)
        {
            logError(QObject::tr("label for a verified condition not found"));
            return false;
        }
        else if (noEip == -1)
        {
            logError(QObject::tr("label for an unverified condition not found"));
            return false;
        }
        eip = owner->pony.nBits >= (quint32)qty ? yesEip : noEip;
        return true;
    }
    else if (command[0] == "gotoIfState")
    {
        if (command.size() == 3)
        {
            int uState, destEip;
            bool ok1;
            uState = command[1].toInt(&ok1);
            destEip = findLabel(command[2]);
            if (!ok1 || uState<0)
            {
                logError(QObject::tr("invalid arguments for command gotoIfState"));
                return false;
            }
            else if (destEip<0)
            {
                logError(QObject::tr("can't find dest label for command gotoIfState"));
                return false;
            }
            if (this->state == uState)
                eip = destEip;
            return true;
        }
        else if (command.size() == 4)
        {
            int uState, questId, destEip;
            bool ok1,ok2;
            uState = command[1].toInt(&ok1);
            questId = command[2].toInt(&ok2);
            destEip = findLabel(command[3]);
            if (!ok1 || !ok2 || questId<0 || uState<0)
            {
                logError(QObject::tr("invalid arguments for command gotoIfState"));
                return false;
            }
            else if (destEip<0)
            {
                logError(QObject::tr("can't find dest label for command gotoIfState"));
                return false;
            }
            for (const Quest& quest : owner->pony.quests)
            {
                if (quest.id == questId)
                {
                    if (quest.state == uState)
                        eip = destEip;
                    return true;
                }
            }
            logError(QObject::tr("invalid quest id for command gotoIfState"));
            return false;
        }
        else
        {
            logError(QObject::tr("gotoIfState takes 2 or 3 arguments"));
            return false;
        }
    }
    else if (command[0] == "gotoAfterState")
    {
        if (command.size() == 3)
        {
            int uState, destEip;
            bool ok1;
            uState = command[1].toInt(&ok1);
            destEip = findLabel(command[2]);
            if (!ok1 || uState<0)
            {
                logError(QObject::tr("invalid arguments for command gotoAfterState"));
                return false;
            }
            else if (destEip<0)
            {
                logError(QObject::tr("can't find dest label for command gotoAfterState"));
                return false;
            }
            if (this->state >= uState)
                eip = destEip;
            return true;
        }
        else if (command.size() == 4)
        {
            int uState, questId, destEip;
            bool ok1,ok2;
            uState = command[1].toInt(&ok1);
            questId = command[2].toInt(&ok2);
            destEip = findLabel(command[3]);
            if (!ok1 || !ok2 || questId<0 || uState<0)
            {
                logError(QObject::tr("invalid arguments for command gotoAfterState"));
                return false;
            }
            else if (destEip<0)
            {
                logError(QObject::tr("can't find dest label for command gotoAfterState"));
                return false;
            }
            for (const Quest& quest : owner->pony.quests)
            {
                if (quest.id == questId)
                {
                    if (quest.state >= uState)
                        eip = destEip;
                    return true;
                }
            }
            logError(QObject::tr("invalid quest id for command gotoAfterState"));
            return false;
        }
        else
        {
            logError(QObject::tr("gotoAfterState takes 2 or 3 arguments"));
            return false;
        }
    }
    else
    {
        logError(QObject::tr("unknown command : %1").arg(command[0]));
        return false;
    }
    return true;
}

void Quest::processAnswer(int answer)
{
    int curAnswer = 0;
    for (int i=eip+1; i<commands->size();i++)
    {
        if ((*commands)[i][0] == "answer")
        {
            if (curAnswer == answer)
            {
                int newEip = findLabel((*commands)[i][1]);
                if (newEip == -1)
                {
                    logError(QObject::tr("label not found for answer %1").arg(answer));
                    sendEndDialog(owner);
                    return;
                }
                eip = newEip;
                sendEndDialog(owner);
                runScript(eip);
                return;
            }
            else
                curAnswer++;
        }
        else if ((*commands)[i][0] == "sayName")
            continue;
        else if ((*commands)[i][0] == "sayIcon")
            continue;
        else
        {
            logError(QObject::tr("answer %1 not found").arg(answer));
            sendEndDialog(owner);
            return;
        }
    }
}

void Quest::processAnswer()
{
    for (int i=eip+1; i<commands->size();i++)
    {
        if ((*commands)[i][0] == "answer")
            continue;
        else if ((*commands)[i][0] == "sayName")
            continue;
        else if ((*commands)[i][0] == "sayIcon")
            continue;
        else
        {
            eip = i;
            sendEndDialog(owner);
            runScript(eip);
            return;
        }
    }
}
