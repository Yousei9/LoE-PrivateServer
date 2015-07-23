#include "app.h"
#include "build.h"
#include "skillparser.h"
#include "mobsParser.h"
#include "animationparser.h"
#include "animation.h"
#include "items.h"
#include "settings.h"
#include "sceneEntity.h"
#include "scene.h"
#include "quest.h"
#include "player.h"
#include "mob.h"
#include "sync.h"
#include "udp.h"
#include <QUdpSocket>
#include <QSettings>
#include <QDir>
#include <QProcess>

#if defined _WIN32 || defined WIN32
#include <windows.h>
#else
#include <sys/time.h>
#endif

using namespace Settings;

/// Starts up the application
void App::startup()
{
#ifdef USE_GUI
    ui->retranslateUi(this);
#endif

    logStatusMessage(QString("%1 v%2 (b%3)").arg(APP_NAME).arg(APP_VERSION).arg(BUILD));

    app.loginServerUp = false;
    app.gameServerUp = false;

#if defined USE_GUI && defined __APPLE__
    // this fixes the bundle directory in OSX so we can use the relative CONFIGFILEPATH and etc properly
    CFBundleRef mainBundle = CFBundleGetMainBundle();
    CFURLRef resourcesURL = CFBundleCopyBundleURL(mainBundle);
    char path[PATH_MAX];
    if (!CFURLGetFileSystemRepresentation(resourcesURL, TRUE, (UInt8 *)path, PATH_MAX))
    {
        // error!
    }
    CFRelease(resourcesURL);
    // the path we get is to the .app folder, so we go up after we chdir
    chdir(path);
    chdir("..");
#endif

    loadConfig();

    /// GUI setup and signal/slot connecting
#ifdef USE_GUI
    app.ui->loginPort->setText(QString::number(loginPort));
    app.ui->gamePort->setText(QString::number(gamePort));
    int connectedPlayers = Player::udpPlayers.length();
    app.ui->userCountLabel->setText(QString("%1 / %2").arg(connectedPlayers).arg(maxConnected));
    app.ui->builddateLabel->setText(tr("Built %1").arg(BUILDDATE));
    app.ui->versionLabel->setText(QString("v%1 <font size=14pt>(b%2)</font>").arg(APP_VERSION).arg(BUILD));

    connect(app.ui->sendButton, SIGNAL(clicked()), this, SLOT(sendCmdLine()));
    connect(app.ui->cmdLine, SIGNAL(returnPressed()), this, SLOT(sendCmdLine()));
#else
    connect(cin_notifier, SIGNAL(activated(int)), this, SLOT(sendCmdLine()));
#endif

    /// Timestamps
#if defined _WIN32 || defined WIN32
    startTimestamp = GetTickCount();
#elif __APPLE__
    timeval time;
    gettimeofday(&time, NULL);
    startTimestamp = (time.tv_sec * 1000) + (time.tv_usec / 1000);
#else
    struct timespec tp;
    clock_gettime(CLOCK_MONOTONIC, &tp);
    startTimestamp = tp.tv_sec*1000 + tp.tv_nsec/1000/1000;
#endif

    /// Starting servers, depends on above
    if (enableLoginServer)
    {
        startLoginServer();
    }

    if (enableGameServer)
    {
        startGameServer();
    }

    /// Example Stuff
    //logMessage("");
    //app.printBasicHelp();

    if (autostartClient)
    {
        startGameClient();
    }
}

/// Shuts down the application
void App::shutdown()
{
    // Servers
    app.stopLoginServer();
    app.stopGameServer();

    // Signals
#ifdef USE_GUI
    disconnect(app.ui->sendButton, SIGNAL(clicked()), this, SLOT(sendCmdLine()));
    disconnect(app.ui->cmdLine, SIGNAL(returnPressed()), this, SLOT(sendCmdLine()));
#else
    disconnect(cin_notifier, SIGNAL(activated(int)), this, SLOT(sendCmdLine()));
#endif
    disconnect(udpSocket);
    disconnect(tcpServer, SIGNAL(newConnection()), this, SLOT(tcpConnectClient()));
    disconnect(pingTimer, SIGNAL(timeout()), this, SLOT(checkPingTimeouts()));
    disconnect(this);

    // Shutdown
    QAPP_TYPE::exit();
}

/// Read config file and load values in
void App::loadConfig()
{
    logStatusMessage(tr("Reading config file ..."));
    QSettings config(CONFIGFILEPATH, QSettings::IniFormat);

    loginPort = config.value("loginPort", DEFAULT_LOGIN_PORT).toInt();
    gamePort = config.value("gamePort", DEFAULT_GAME_PORT).toInt();
    maxConnected = config.value("maxConnected", DEFAULT_MAX_CONNECTED).toInt();
    maxRegistered = config.value("maxRegistered", DEFAULT_MAX_REGISTERED).toInt();
    pingTimeout = config.value("pingTimeout", DEFAULT_PING_TIMEOUT).toInt();
    pingCheckInterval = config.value("pingCheckInterval", DEFAULT_PING_CHECK).toInt();
    logInfos = config.value("logInfosMessages", DEFAULT_LOG_INFOSMESSAGES).toBool();
    saltPassword = config.value("saltPassword", DEFAULT_SALT_PASSWORD).toString();
    enableSessKeyValidation = config.value("enableSessKeyValidation", DEFAULT_SESSKEY_VALIDATION).toBool();
    enableLoginServer = config.value("enableLoginServer", DEFAULT_ENABLE_LOGIN_SERVER).toBool();
    enableGameServer = config.value("enableGameServer", DEFAULT_ENABLE_GAME_SERVER).toBool();
    enableMultiplayer = config.value("enableMultiplayer", DEFAULT_ENABLE_MULTIPLAYER).toBool();
    syncInterval = config.value("syncInterval", DEFAULT_SYNC_INTERVAL).toInt();
    remoteLoginIP = config.value("remoteLoginIP", DEFAULT_REMOTE_LOGIN_IP).toString();
    remoteLoginPort = config.value("remoteLoginPort", DEFAULT_REMOTE_LOGIN_PORT).toInt();
    remoteLoginTimeout = config.value("remoteLoginTimeout", DEFAULT_REMOTE_LOGIN_TIMEOUT).toInt();
    useRemoteLogin = config.value("useRemoteLogin", DEFAULT_USE_REMOTE_LOGIN).toBool();
    enableGetlog = config.value("enableGetlog", DEFAULT_ENABLE_GETLOG).toBool();
    enablePVP = config.value("enablePVP", DEFAULT_ENABLE_PVP).toBool();
    autostartClient = config.value("autostartClient",DEFAULT_AUTOSTART_CLIENT).toBool();

#ifdef USE_GUI
    app.ui->loginPortConfig->setValue(loginPort);
    app.ui->gamePortConfig->setValue(gamePort);
    app.ui->maxConnectedPlayersConfig->setValue(maxConnected);
    app.ui->maxRegisteredPlayersConfig->setValue(maxRegistered);
    app.ui->pingTimeoutConfig->setValue(pingTimeout);
    app.ui->pingCheckConfig->setValue(pingCheckInterval);
    app.ui->logInfosMessagesConfig->setChecked(logInfos);
    app.ui->saltPasswordConfig->setText(saltPassword);
    app.ui->sessKeyValidationConfig->setChecked(enableSessKeyValidation);
    app.ui->enableLoginServerConfig->setChecked(enableLoginServer);
    app.ui->enableGameServerConfig->setChecked(enableGameServer);
    app.ui->multiplayerConfig->setChecked(enableMultiplayer);
    app.ui->syncIntervalConfig->setValue(syncInterval);
    app.ui->getlogConfig->setCheckable(enableGetlog);
    app.ui->pvpConfig->setChecked(enablePVP);
    app.ui->enableClientAutostart->setChecked(autostartClient);
#endif
}

#ifdef USE_GUI
/// Read config options from GUI and load values in
void App::loadConfigFromGui()
{
    loginPort = app.ui->loginPortConfig->value();
    gamePort = app.ui->gamePortConfig->value();
    maxConnected = app.ui->maxConnectedPlayersConfig->value();
    maxRegistered = app.ui->maxRegisteredPlayersConfig->value();
    pingTimeout = app.ui->pingTimeoutConfig->value();
    pingCheckInterval = app.ui->pingCheckConfig->value();
    logInfos = app.ui->logInfosMessagesConfig->isChecked();
    saltPassword = app.ui->saltPasswordConfig->text();
    enableSessKeyValidation = app.ui->sessKeyValidationConfig->isChecked();
    enableLoginServer = app.ui->enableLoginServerConfig->isChecked();
    enableGameServer = app.ui->enableGameServerConfig->isChecked();
    enableMultiplayer = app.ui->multiplayerConfig->isChecked();
    syncInterval = app.ui->syncIntervalConfig->value();
//    remoteLoginIP = ;
//    remoteLoginPort = ;
//    remoteLoginTimeout = ;
//    useRemoteLogin = ;
    enableGetlog = app.ui->getlogConfig->isChecked();
    enablePVP = app.ui->pvpConfig->isChecked();
    autostartClient = app.ui->enableClientAutostart->isChecked();
}

/// Reset GUI config options to defaults
void App::resetGuiConfigToDefault()
{
    app.ui->loginPortConfig->setValue(DEFAULT_LOGIN_PORT);
    app.ui->gamePortConfig->setValue(DEFAULT_GAME_PORT);
    app.ui->maxConnectedPlayersConfig->setValue(DEFAULT_MAX_CONNECTED);
    app.ui->maxRegisteredPlayersConfig->setValue(DEFAULT_MAX_REGISTERED);
    app.ui->pingTimeoutConfig->setValue(DEFAULT_PING_TIMEOUT);
    app.ui->pingCheckConfig->setValue(DEFAULT_PING_CHECK);
    app.ui->logInfosMessagesConfig->setChecked(DEFAULT_LOG_INFOSMESSAGES);
    app.ui->saltPasswordConfig->setText(DEFAULT_SALT_PASSWORD);
    app.ui->sessKeyValidationConfig->setChecked(DEFAULT_SESSKEY_VALIDATION);
    app.ui->enableLoginServerConfig->setChecked(DEFAULT_ENABLE_LOGIN_SERVER);
    app.ui->enableGameServerConfig->setChecked(DEFAULT_ENABLE_GAME_SERVER);
    app.ui->multiplayerConfig->setChecked(DEFAULT_ENABLE_MULTIPLAYER);
    app.ui->syncIntervalConfig->setValue(DEFAULT_SYNC_INTERVAL);
    app.ui->getlogConfig->setCheckable(DEFAULT_ENABLE_GETLOG);
    app.ui->pvpConfig->setChecked(DEFAULT_ENABLE_PVP);
    app.ui->enableClientAutostart->setChecked(DEFAULT_AUTOSTART_CLIENT);
}

#endif

/// Saves values to config file
void App::saveConfig()
{
    QSettings config(CONFIGFILEPATH, QSettings::IniFormat);

    config.setValue("loginPort", loginPort);
    config.setValue("gamePort", gamePort);
    config.setValue("maxConnected", maxConnected);
    config.setValue("maxRegistered", maxRegistered);
    config.setValue("pingTimeout", pingTimeout);
    config.setValue("pingCheckInterval", pingCheckInterval);
    config.setValue("logInfos", logInfos);
    config.setValue("saltPassword", saltPassword);
    config.setValue("enableSessKeyValidation", enableSessKeyValidation);
    config.setValue("enableLoginServer", enableLoginServer);
    config.setValue("enableGameServer", enableGameServer);
    config.setValue("enableMultiplayer", enableMultiplayer);
    config.setValue("syncInterval", syncInterval);
    config.setValue("remoteLoginIP", remoteLoginIP);
    config.setValue("remoteLoginPort", remoteLoginPort);
    config.setValue("remoteLoginTimeout", remoteLoginTimeout);
    config.setValue("useRemoteLogin", useRemoteLogin);
    config.setValue("enableGetlog", enableGetlog);
    config.setValue("enablePVP", enablePVP);
    config.setValue("autostartClient", autostartClient);

    logStatusMessage(tr("Saved config file ..."));
}

void App::startLoginServer()
{
    if (app.loginServerUp)
    {
        logMessage(tr("Login server is already up, shutdown login server before attempting to start"));
        return;
    }

    /// Player DB
    //logStatusMessage(tr("Loading players database ..."));
    Player::tcpPlayers = Player::loadPlayers();

    /// TCP Server
    logStatusMessage(tr("Starting TCP login server on port %1...").arg(loginPort));
    if (!tcpServer->listen(QHostAddress::Any,loginPort))
    {
        logStatusError(tr("TCP: Unable to start server on port %1 : %2").arg(loginPort).arg(tcpServer->errorString()));
        app.stopLoginServer();
        return;
    }

    // If we use a remote login server, try to open a connection preventively.
    if (useRemoteLogin)
        remoteLoginSock.connectToHost(remoteLoginIP, remoteLoginPort);

    connect(tcpServer, SIGNAL(newConnection()), this, SLOT(tcpConnectClient()));

    app.loginServerUp = true;
#ifdef USE_GUI
    app.ui->loginStatus->setText("<font color=\"#339933\">ONLINE</font>");
    app.ui->toggleLoginServerButton->setText(tr("Stop Login Server"));
#endif
}

void App::stopLoginServer()
{
    stopLoginServer(true);
}

void App::stopLoginServer(bool log)
{
    if (log)
        logStatusMessage(tr("Stopping Login Server"));

    app.loginServerUp = false;

    tcpServer->close();

#ifdef USE_GUI
    app.ui->loginStatus->setText("<font color=\"#993333\">OFFLINE</font>");
    app.ui->toggleLoginServerButton->setText(tr("Start Login Server"));
#endif
}

void App::startGameServer()
{
    if (app.gameServerUp)
    {
        logMessage(tr("Game server is already up, shutdown game server before attempting to start"));
        return;
    }

    SceneEntity::lastNetviewId=0;
    SceneEntity::lastId=0;
    for (int i=0; i < 65536; i++) {
        SceneEntity::usedids[i] = false;
    }

    /// Init
    tcpClientsList.clear();

    /// Read vortex DB
    bool corrupted=false;
    QDir vortexDir("data/vortex/");
    QStringList files = vortexDir.entryList(QDir::Files);
    int nVortex=0;
    for (int i=0; i<files.size(); i++) // For each vortex file
    {
        if (!ReadVortxXml(vortexDir.absolutePath() + "/" + files[i]))
            corrupted = true;
    }

    if (corrupted)
    {
        app.stopGameServer();
        return;
    }

    for (int i=0; i<Scene::scenes.size(); i++)
    {
        nVortex += Scene::scenes[i].vortexes.size();
    }
    logMessage(tr("Loaded %1 vortexes in %2 scenes").arg(nVortex).arg(Scene::scenes.size()));

    /// Read/parse Items.xml
    QFile itemsFile("data/data/Items.xml");
    if (itemsFile.open(QIODevice::ReadOnly))
    {
        QByteArray data = itemsFile.readAll();
        parseItemsXml(data);
        app.logMessage(tr("Loaded %1 items").arg(wearablePositionsMap.size()));
    }
    else
    {
        app.logError(tr("Couldn't open Items.xml"));
        app.stopGameServer();
        return;
    }

    /// Read NPC/Quests DB
    try
    {
        unsigned nQuests = 0;
        QDir npcsDir("data/npcs/");
        QStringList files = npcsDir.entryList(QDir::Files);
        for (int i=0; i<files.size(); i++, nQuests++) // For each NPC file
        {
            try
            {
                Quest quest("data/npcs/"+files[i], NULL);
                for (const Quest& q : Quest::quests)
                    if (q.id == quest.id)
                        logError(tr("Error, two quests are using the same id (%1) !").arg(quest.id));
                Quest::quests << quest;
                Quest::npcs << quest.npc;
            }
            catch (QString& error)
            {
                app.logError(error);
                app.stopGameServer();
                throw error;
            }
        }
        logMessage(tr("Loaded %1 quests/npcs").arg(nQuests));
    }
    catch (QString& e)
    {
        logMessage(tr("Error loading NPCs/Quests"));
        return;
    }

    /// Read/parse mob zones
    try
    {
        QDir mobsDir(MOBSPATH);
        QStringList files = mobsDir.entryList(QDir::Files);
        for (int i=0; i<files.size(); i++) // For each mobzone file
        {
            QFile file(MOBSPATH+files[i]);
            if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
            {
                logStatusError(tr("Error reading mob zones"));
                return;
            }
            QByteArray data = file.readAll();
            file.close();
            try {
                parseMobzoneData(data); // Will fill our Mobzone and Mobs list
            }
            catch (QString& error)
            {
                logError(error);
                stopGameServer();
                throw error;
            }
        }
        logMessage(tr("Loaded %1 mobs in %2 zones").arg(Mob::mobs.size()).arg(Mob::mobzones.size()));
    }
    catch (...)
    {
        logMessage(tr("Error loading mob zones"));
        return;
    }

    // Parse animations
    try
    {
        AnimationParser(GAMEDATAPATH+QString("Animations.json"));
        logMessage(tr("Loaded %1 animations").arg(Animation::animations.size()));
    }
    catch (const QString& e)
    {
        logError(tr("Error parsing animations: ")+e);
        app.stopGameServer();
    }
    catch (const char* e)
    {
        logError(tr("Error parsing animations: ")+e);
        app.stopGameServer();
    }

    // Parse skills
    try
    {
        SkillParser(GAMEDATAPATH+QString("Skills.json"));
        logMessage(tr("Loaded %1 skills").arg(Skill::skills.size()));
    }
    catch (const QString& e)
    {
        logError(tr("Error parsing skills: ")+e);
        app.stopGameServer();
    }
    catch (const char* e)
    {
        logError(tr("Error parsing skills: ")+e);
        app.stopGameServer();
    }

    // UDP server
    logStatusMessage(tr("Starting UDP game server on port %1...").arg(gamePort));
    if (!udpSocket->bind(gamePort, QUdpSocket::ReuseAddressHint|QUdpSocket::ShareAddress))
    {
        logStatusError(tr("UDP: Unable to start server on port %1").arg(gamePort));
        app.stopGameServer();
        return;
    }

    // Start ping timeout timer
    pingTimer->start(pingCheckInterval);

    if (enableMultiplayer)
        sync->startSync(syncInterval);

    // Signals
    connect(udpSocket, &QUdpSocket::readyRead, &::udpProcessPendingDatagrams);
    connect(pingTimer, SIGNAL(timeout()), this, SLOT(checkPingTimeouts()));

    app.gameServerUp = true;

#ifdef USE_GUI
    app.ui->toggleGameServerButton->setText(tr("Stop Game Server"));
    app.ui->gameStatus->setText("<font color=\"#339933\">ONLINE</font>");
#endif
}

void App::stopGameServer()
{
    stopGameServer(true);
}

void App::stopGameServer(bool log)
{
    if (log)
        logStatusMessage(tr("Stopping Game Server"));

    //logMessage(tr("UDP: Disconnecting all players"));
    disconnectUdpPlayers();

    sync->stopSync();
    pingTimer->stop();

    for (int i=0;i<tcpClientsList.size();i++)
        tcpClientsList[i].first->close();

    udpSocket->close();

    Quest::quests.clear();
    Quest::npcs.clear();

    app.gameServerUp = false;

#ifdef USE_GUI
    app.ui->toggleGameServerButton->setText(tr("Start Game Server"));
    app.ui->gameStatus->setText("<font color=\"#993333\">OFFLINE</font>");
#endif
}

/// start a new instance of the game client
void App::startGameClient()
{
#if defined WIN32 || defined _WIN32
    // get working directory
    QString clientPath = QCoreApplication::applicationDirPath();
    clientPath.append("/LoE.exe");

    // check if client application exists
    QFile clientApp(clientPath);
    if (clientApp.exists()){
        // start client in seperate process
        if (QProcess::startDetached(clientPath, QStringList()))
        {
            logMessage("Game started.");
        }
        else
        {
            logError("Couldn't start game.");
        }
    }
    else
    {
        logError(tr("Game executable '%1' not found.").arg(clientPath));
    }
#else
    //TODO: start client in non win enviroment
    logError("Game start not implemented on this platform.");
#endif
}
