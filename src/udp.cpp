#include "message.h"
#include "serialize.h"
#include "utils.h"
#include "player.h"
#include "settings.h"
#include "udp.h"
#include "log.h"
#include "app.h"
#include <QUdpSocket>
#include <QCryptographicHash>

using namespace Settings;

QUdpSocket* udpSocket;

void restartUdpServer()
{
    logStatusMessage(QObject::tr("Restarting UDP server ..."));
    udpSocket->close();
    if (!udpSocket->bind(gamePort, QUdpSocket::ReuseAddressHint|QUdpSocket::ShareAddress))
    {
        logStatusMessage(QObject::tr("UDP: Unable to start server on port %1").arg(gamePort));
        app.stopGameServer();
        return;
    }
}

void udpProcessPendingDatagrams()
{
    while (udpSocket->hasPendingDatagrams())
    {
        QHostAddress rAddr;
        quint16 rPort;
        QByteArray datagram;
        qint64 dataRead = 0;
        int datagramSize = udpSocket->pendingDatagramSize();
        datagram.resize(datagramSize);

        while(dataRead < datagram.size())
        {
            qint64 readNow = udpSocket->readDatagram(datagram.data()+dataRead, datagramSize, &rAddr, &rPort);
            if(readNow != -1)
            {
                dataRead += readNow; // Remember the total number of bytes read, so we know when to stop
                if (datagramSize > (datagram.size() - dataRead)) // Make sure we don't overflow
                    datagramSize = (datagram.size() - dataRead);
            }
            else
            {
                logError(QObject::tr("Socket error : %1").arg(udpSocket->errorString()));
                return;
            }
        }

        // Add player on connection
        if ((unsigned char)datagram[0]==MsgConnect && (unsigned char)datagram[1]==0
                && (unsigned char)datagram[2]==0 && datagram.size()>=22)
        {
            QString name = dataToString(datagram.right(datagram.size()-22));
            int nameFullSize = getVUint32Size(datagram.right(datagram.size()-22))+name.size();
            QString sesskey = dataToString(datagram.right(datagram.size()-22-nameFullSize));

            bool is_sesskey_valid = true;
            if (enableSessKeyValidation) {
                is_sesskey_valid = QCryptographicHash::hash(QString(sesskey.right(40) + saltPassword).toLatin1(),
                                                            QCryptographicHash::Md5).toHex() == sesskey.left(32);
            }

            if (is_sesskey_valid)
            {
                // Create new player if needed, else just update player
                Player* newPlayer = Player::findPlayer(Player::udpPlayers, rAddr.toString(),rPort);
                if (newPlayer->IP != rAddr.toString()) // IP:Port not found in player list
                {
                    newPlayer->resetNetwork();
                    newPlayer->name = name;
                    newPlayer->IP = rAddr.toString();
                    newPlayer->port = rPort;

                    // Check if we have too many players connected
                    int n=0;
                    for (int i=0;i<Player::udpPlayers.size();i++)
                        if (Player::udpPlayers[i]->connected)
                            n++;
                    if (n>=maxConnected)
                    {
                        sendMessage(newPlayer, MsgDisconnect, "Error : Too many players connected. Try again later.");
                    }
                    else
                        // If not add the player
                        Player::udpPlayers << newPlayer;
#ifdef USE_GUI
                        int connectedPlayers = Player::udpPlayers.length();
                        app.ui->userCountLabel->setText(QString("%1 / %2").arg(connectedPlayers).arg(maxConnected));
#endif
                }
                else  // IP:Port found in player list
                {
                    if (newPlayer->connected)
                    {
                        sendMessage(newPlayer, MsgDisconnect, "Error : Player already connected.");
                        return;
                    }

                    // Check if we have too many players connected
                    int n=0;
                    for (int i=0;i<Player::udpPlayers.size();i++)
                        if (Player::udpPlayers[i]->connected)
                            n++;
                    if (n>=maxConnected)
                    {
                        sendMessage(newPlayer, MsgDisconnect, "Error : Too many players connected. Try again later.");
                    }

                    newPlayer->resetNetwork();
                    newPlayer->name = name;
                    newPlayer->IP = rAddr.toString();
                    newPlayer->port = rPort;
                }
            }
            else
            {
                logError(QObject::tr("UDP: %1:%2 Sesskey rejected","The sesskey is a cryptographic hash, short for session key").arg(rAddr.toString()).arg(rPort));
                Player* newPlayer = new Player;
                newPlayer->IP = rAddr.toString();
                newPlayer->port = rPort;
                sendMessage(newPlayer, MsgDisconnect, "Error: Wrong server password. This server is protected with a salt password.");
                return;
            }
        }

        Player* player = Player::findPlayer(Player::udpPlayers, rAddr.toString(), rPort);
        if (player->IP == rAddr.toString() && player->port == rPort) // Process data
        {
            player->receivedDatas->append(datagram);
            receiveMessage(player);
        }
        else // You need to connect with TCP first
        {
            logError(QObject::tr("UDP: Request from unknown peer %1:%2 rejected").arg(rAddr.toString()).arg(rPort));
            QString data("You're not connected, please login first. (aka the server has no idea who the hell you are)");
            QByteArray msg(6,0);
            msg[0] = MsgDisconnect;
            msg[3] = (quint8)(((data.size()+1)*8)&0xFF);
            msg[4] = (quint8)((((data.size()+1)*8)>>8)&0xFF);
            msg[5] = (quint8)data.size();
            msg += data;
            udpSocket->writeDatagram(msg,rAddr,rPort);
        }
    }
}

void disconnectUdpPlayers()
{
    //logMessage(tr("UDP: Disconnecting all players"));
    while (Player::udpPlayers.size())
    {
        Player* player = Player::udpPlayers[0];
        sendMessage(player, MsgDisconnect, "Server closed by the admin");

        // Save the pony
        QList<Pony> ponies = Player::loadPonies(player);
        for (int i=0; i<ponies.size(); i++)
        {
            if (ponies[i].ponyData == player->pony.ponyData)
                ponies[i] = player->pony;
        }
        Player::savePonies(player, ponies);
        //player->pony.saveQuests(ponyNames);
        //player->pony.saveInventory(ponyNames);

        // Free
        delete player;
        Player::udpPlayers.removeFirst();
    }
}
