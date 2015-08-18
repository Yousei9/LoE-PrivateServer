#include "app.h"
#include "message.h"
#include "utils.h"
#include "player.h"
#include "settings.h"
#include <QCryptographicHash>

#define DEBUG_LOG false

using namespace Settings;

void App::tcpConnectClient()
{
#if DEBUG_LOG
    logMessage(tr("TCP: New client connected"));
#endif

    QTcpSocket *newClient = tcpServer->nextPendingConnection();
    tcpClientsList << QPair<QTcpSocket*,QByteArray*>(newClient,new QByteArray());

    connect(newClient, SIGNAL(readyRead()), this, SLOT(tcpProcessPendingDatagrams()));
    connect(newClient, SIGNAL(disconnected()), this, SLOT(tcpDisconnectClient()));
}

void App::tcpDisconnectClient()
{
    // Find who's disconnecting, if we can't, just give up
    QTcpSocket *socket = qobject_cast<QTcpSocket *>(sender());
    if (socket == 0)
        return;
#if DEBUG_LOG
    logMessage(tr("TCP: Client disconnected"));
#endif
    disconnect(socket);

    for (int i=0; i<tcpClientsList.size(); i++)
    {
        if (tcpClientsList[i].first == socket)
        {
            delete tcpClientsList[i].second;
            tcpClientsList.removeAt(i);
            break;
        }
    }

    socket->deleteLater();
}

void App::tcpProcessPendingDatagrams()
{
    // Find who's sending
    QTcpSocket *socket = qobject_cast<QTcpSocket *>(sender());
    if (socket == nullptr)
        return;

    QByteArray* recvBuffer=nullptr;
    for (auto pair : tcpClientsList)
    {
        if (pair.first == socket)
        {
            recvBuffer = pair.second;
            break;
        }
    }
    if (recvBuffer == nullptr)
    {
        logError(tr("TCP: Error fetching the socket's associated recv buffer"));
        return;
    }

    unsigned nTries = 0;

    // Acquire data
    while(socket->state()==QAbstractSocket::ConnectedState && nTries<3) // Exit if disconnected, too much retries, malformed HTTP request, or after all requests are processed
    {
        recvBuffer->append(socket->readAll());
        nTries++;

        if (!recvBuffer->size())
        {
#if DEBUG_LOG
            logMessage(tr("TCP: Nothing to read"));
#endif
            continue;
        }

        if (!recvBuffer->startsWith("POST") && !recvBuffer->startsWith("GET")) // Not HTTP, clear the buffer
        {
#if DEBUG_LOG
            logMessage(tr("TCP: Received non-HTTP request : ")+*recvBuffer->toHex());
#endif
            recvBuffer->clear();
            socket->close();
            return;
        }
        else if (recvBuffer->contains("Content-Length:")) // POST or GET request, wait for Content-Length header
        {
            QByteArray contentLength = *recvBuffer;
            contentLength = contentLength.right(contentLength.size() - contentLength.indexOf("Content-Length:") - 15);
            QList<QByteArray> lengthList = contentLength.trimmed().split('\n');
            if (lengthList.size()>1) // We want a number on this line and a next line to be sure we've got the full number
            {
                bool isNumeric;
                int length = lengthList[0].trimmed().toInt(&isNumeric);
                if (!isNumeric) // We've got something but it's not a number
                {
                    logError(tr("TCP: Error: Content-Length must be a (decimal) number !"));
                    recvBuffer->clear();
                    socket->close();
                    return;
                }

                // Detect and send data files if we need to
                QByteArray data = *recvBuffer;
#if DEBUG_LOG
                logMessage(tr("TCP: Got content-length request:")+data);
#endif
                // Get the payload only (remove headers)
                data = removeHTTPHeader(data, "POST ");
                data = removeHTTPHeader(data, "GET ");
                data = removeHTTPHeader(data, "User-Agent:");
                data = removeHTTPHeader(data, "Host:");
                data = removeHTTPHeader(data, "host:");
                data = removeHTTPHeader(data, "Accept:");
                data = removeHTTPHeader(data, "Content-Length:");
                data = removeHTTPHeader(data, "Content-Type:");
                data = removeHTTPHeader(data, "Server:");
                data = removeHTTPHeader(data, "Date:");
                data = removeHTTPHeader(data, "Transfert-Encoding:");
                data = removeHTTPHeader(data, "Connection:");
                data = removeHTTPHeader(data, "Vary:");
                data = removeHTTPHeader(data, "X-Powered-By:");
                data = removeHTTPHeader(data, "accept-encoding:");
                data = removeHTTPHeader(data, "if-modified-since:");

                if (data.size() >= length) // Wait until we have all the data, then process it all
                {
                    data.truncate(length);
                    tcpProcessData(data, socket);
                    *recvBuffer = recvBuffer->right(recvBuffer->size() - recvBuffer->indexOf(data) - data.size());
                    if (recvBuffer->isEmpty())
                        return;
                    nTries=0;
                }
            }
        }
        else if (recvBuffer->contains("\r\n\r\n")) // POST or GET request, without a Content-Length header
        {
            QByteArray data = *recvBuffer;
            data = data.left(data.indexOf("\r\n\r\n")+4);
            int dataSize = data.size();
#if DEBUG_LOG
            logMessage(tr("Got non-content length request:")+data);
#endif

            int i1=0;
            do
            {
                i1 = data.indexOf("GET");
                if (i1 != -1)
                {
                    int i2 = data.indexOf("HTTP")-1;
                    QString path = data.mid(i1 + 4, i2-i1-4);
                    if (path == "/log") // GET /log
                    {
                        data = removeHTTPHeader(data, "POST ");
                        data = removeHTTPHeader(data, "GET ");
                        data = removeHTTPHeader(data, "if-modified-since:");
                        data = removeHTTPHeader(data, "accept-encoding:");
                        data = removeHTTPHeader(data, "host:");
                        if (!enableGetlog)
                            continue;
                        QFile head(QString(NETDATAPATH)+"/dataTextHeader.bin");
                        head.open(QIODevice::ReadOnly);
                        if (!head.isOpen())
                        {
                            logError(tr("Can't open header : ","The header is a file")+head.errorString());
                            continue;
                        }
#ifdef USE_GUI
                        QByteArray logData = ui->log->toPlainText().toLatin1();
#else
                        QByteArray logData = ""; //TODO: Fix this later
#endif
                        socket->write(head.readAll());
                        socket->write(QString("Content-Length: "+QString().setNum(logData.size())+"\r\n\r\n").toLocal8Bit());
                        socket->write(logData);
                        head.close();
                        logMessage(tr("Sent log to %1").arg(socket->peerAddress().toString()));
                        continue;
                    }
                    // Other GETs (not getlog)
                    data = removeHTTPHeader(data, "POST ");
                    data = removeHTTPHeader(data, "GET ");
#if DEBUG_LOG
                    logMessage(tr("TCP: Replying to HTTP GET %1").arg(path));
#endif
                    QFile head(QString(NETDATAPATH)+"/dataHeader.bin");
                    QFile res("data/"+path);
                    head.open(QIODevice::ReadOnly);
                    if (!head.isOpen())
                    {
                        logError(tr("TCP: Can't open header : ","The header is a file")+head.errorString());
                        continue;
                    }
                    res.open(QIODevice::ReadOnly);
                    if (!res.isOpen())
                    {
                        logError(tr("TCP: File not found %1").arg(path));
                        head.close();
                        // If we 404, we send a 304 Not Modified, and the client will use it's local version
                        // The game clients chokes if we send anything but a 200 or 304 back
                        QFile head404(QString(NETDATAPATH)+"/notmodified.bin");
                        head404.open(QIODevice::ReadOnly);
                        if (!head404.isOpen())
                        {
                            logError(tr("TCP: Can't open 304 Not Modified header : ","The header is a file")
                                       +head404.errorString());
                            continue;
                        }
                        socket->write(head404.readAll());
                        head404.close();
                        continue;
                    }
                    socket->write(head.readAll());
                    socket->write(QString("Content-Length: "+QString().setNum(res.size())+"\r\n\r\n").toLocal8Bit());
                    socket->write(res.readAll());
                    head.close();
                    res.close();
#if DEBUG_LOG
                    logMessage(tr("TCP: Sent %1 bytes").arg(res.size()+head.size()));
#endif
                }
            } while (i1 != -1);

            *recvBuffer = recvBuffer->mid(dataSize);
        }
    }
}

void App::tcpProcessData(QByteArray data, QTcpSocket* socket)
{
    QByteArray* recvBuffer=nullptr;
    for (auto pair : tcpClientsList)
    {
        if (pair.first == socket)
        {
            recvBuffer = pair.second;
            break;
        }
    }
    if (recvBuffer == nullptr)
    {
        logError(tr("TCP: Error fetching the socket's associated recv buffer"));
        return;
    }

#if DEBUG_LOG
    logMessage("tcpProcessData received : "+data);
#endif

    // Login request (forwarded)
    if (useRemoteLogin && recvBuffer->contains("commfunction=login&") && recvBuffer->contains("&version="))
    {
        logError(tr("TCP: Remote login not implemented yet."));
        // We need to add the client with his IP/port/passhash to tcpPlayers if he isn't already there
        Player newPlayer;
        newPlayer.IP = socket->peerAddress().toIPv4Address();
        newPlayer.port = socket->peerPort();
        QString passhash = QString(*recvBuffer);
        passhash = passhash.mid(passhash.indexOf("passhash=")+9);
        passhash.truncate(passhash.indexOf('&'));
        newPlayer.passhash = passhash;
        logMessage(tr("IP:","IP address")+newPlayer.IP
                   +tr(", passhash:","A cryptographic hash of a password")+newPlayer.passhash);

        // Then connect to the remote and forward the client's requests
        if (!remoteLoginSock.isOpen())
        {
            remoteLoginSock.connectToHost(remoteLoginIP, remoteLoginPort);
            remoteLoginSock.waitForConnected(remoteLoginTimeout);
            if (!remoteLoginSock.isOpen())
            {
                logError(tr("TCP: Can't connect to remote login server : timed out"));
                return;
            }
        }
        // We just blindly send everything that we're going to remove from recvBuffer at the end of tcpProcessData
        QByteArray toSend = *recvBuffer;
        toSend.left(toSend.indexOf(data) + data.size());
        remoteLoginSock.write(toSend);
    }
    else if (useRemoteLogin && recvBuffer->contains("Server:")) // Login reply (forwarded)
    {
        logError(tr("TCP: Remote login not implemented yet."));
        // First we need to find a player matching the received passhash in tcpPlayers
        // Use the player's IP/port to find a matching socket in tcpClientsList
        // The login headers are all the same, so we can just use loginHeader.bin and send back data
    }
    else if (recvBuffer->contains("commfunction=login&") && recvBuffer->contains("&version=")) // Login request
    {
        QString postData = QString(*recvBuffer);
        *recvBuffer = recvBuffer->right(postData.size()-postData.indexOf("version=")-8-4); // 4 : size of version number (ie:version=1344)
        QFile file(QString(NETDATAPATH)+"/loginHeader.bin");
        QFile fileServersList(SERVERSLISTFILEPATH);
        QFile fileBadPassword(QString(NETDATAPATH)+"/loginWrongPassword.bin");
        QFile fileMaxRegistration(QString(NETDATAPATH)+"/loginMaxRegistered.bin");
        if (!file.open(QIODevice::ReadOnly) || !fileBadPassword.open(QIODevice::ReadOnly)
        || !fileMaxRegistration.open(QIODevice::ReadOnly) || !fileServersList.open(QIODevice::ReadOnly))
        {
            logError(tr("TCP: Error reading data files"));
            app.stopGameServer();
        }
        else
        {
            bool ok=true;
            postData = postData.right(postData.size()-postData.indexOf("username=")-9);
            QString username = postData;
            username.truncate(postData.indexOf('&'));
            postData = postData.right(postData.size()-postData.indexOf("passhash=")-9);
            QString passhash = postData;
            passhash.truncate(postData.indexOf('&'));

            //client version check
            QString clientVersion = postData.mid(postData.indexOf("version=")+8);
            if (clientVersion != "20140416")
            {
                logError(tr("TCP: Login rejected from %1 (%2), unsupported client version %3").arg(socket->peerAddress().toString()).arg(username).arg(clientVersion));
                socket->write(fileBadPassword.readAll());
                socket->close();
                return;
            }
            logMessage(tr("TCP: Login request received from %1 (%2)").arg(socket->peerAddress().toString()).arg(username));

            // check if playername contains illegal characters
            // regex checked with https://regex101.com/r/vS7vZ3/11
            QRegularExpression re("^(?!\\s)([\\w\\s\\-]+)+$");
            QRegularExpressionMatch match = re.match(username);
            if (!match.hasMatch() || username.toLower() == "players")
            {
                logMessage(tr("TCP: Login failed, invalid username"));
                socket->write(fileBadPassword.readAll());
                socket->close();
                return;
            }

            // Add player to the players list
            Player* player = Player::findPlayer(Player::tcpPlayers, username);
            if (player->name.toLower() != username.toLower()) // Not found, create a new player
            {
                // Check max registered number
                if (Player::tcpPlayers.size() >= maxRegistered)
                {
                    logError(tr("TCP: Registration failed, too many players registered"));
                    socket->write(fileMaxRegistration.readAll());
                    ok = false;
                }
                else
                {
                    logMessage(tr("TCP: Creating user %1 in database").arg(username));
                    Player* newPlayer = new Player;
                    newPlayer->name = username;
                    newPlayer->passhash = passhash;
                    newPlayer->IP = socket->peerAddress().toString();
                    newPlayer->connected = false; // The connection checks are done by the game servers

                    Player::tcpPlayers << newPlayer;
                    if (!Player::savePlayers(Player::tcpPlayers))
                        ok = false;
                }
            }
            else // Found, compare passhashes, check if already connected
            {
                if (player->passhash != passhash) // Bad password
                {
                    logMessage(tr("TCP: Login failed, wrong password"));
                    socket->write(fileBadPassword.readAll());
                    socket->close();
                    ok=false;
                }
                else // Good password
                {
                    player->IP = socket->peerAddress().toString();
                    player->lastPingTime = timestampNow();
                    player->connected = true;
                }
            }

            if (ok) // Send servers list
            {
                // HTTP reply template
                QByteArray data1 = QByteArray::fromHex("0D0A61757468726573706F6E73653A0A747275650A");
                QByteArray data2 = QByteArray::fromHex("0A310A");
                if(player->accessLvl == 2)
                    data2 = QByteArray::fromHex("0A320A");
                if(player->accessLvl == 3)
                    data2 = QByteArray::fromHex("0A330A");
                if(player->accessLvl == 4)
                    data2 = QByteArray::fromHex("0A340A");
                QByteArray data3 = QByteArray::fromHex("0D0A300D0A0D0A");

                QByteArray customData = file.readAll();
                QByteArray sesskey = QCryptographicHash::hash(QString(passhash + saltPassword).toLatin1(), QCryptographicHash::Md5).toHex();
#if DEBUG_LOG
                logMessage("TCP: Hash of '"+QString(passhash + saltPassword).toLatin1()+"' is '"+sesskey+"'");
                logMessage("TCP: Sesskey is '"+sesskey+"', passhash is '"+passhash+"', salt is '"+saltPassword+"'");
#endif
                sesskey += passhash;
                QByteArray serversList;
                QByteArray line;
                do {
                    line = fileServersList.readLine().trimmed();
                    serversList+=line;
                    serversList+=0x0A;
                } while (!line.isEmpty());
                serversList = serversList.trimmed();
                int dataSize = data1.size() + sesskey.size() + data2.size() + serversList.size() - 2;
                QString dataSizeStr = QString().setNum(dataSize, 16);

                customData += dataSizeStr;
                customData += data1;
                customData += sesskey;
                customData += data2;
                customData += serversList;
                customData += data3;

                logMessage(tr("TCP: Login successful, sending servers list"));
                socket->write(customData);
                socket->close();
            }
        }
    }
    else if (data.contains("commfunction=removesession"))
    {
#if DEBUG_LOG
        logMessage(tr("TCP: Session closed by client"));
#endif
    }
    else // Unknown request, erase tcp buffer
    {
        // Display data
        logError(tr("TCP: Unknown request received : "));
        logMessage(QString(data.data()));
    }
}
