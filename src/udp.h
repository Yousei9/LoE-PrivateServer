#ifndef UDP_H
#define UDP_H

class QUdpSocket;

void udpProcessPendingDatagrams();
void restartUdpServer();
void disconnectUdpPlayers();

extern QUdpSocket* udpSocket;

#endif // UDP_H
