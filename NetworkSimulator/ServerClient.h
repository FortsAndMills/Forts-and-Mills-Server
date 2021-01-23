#ifndef SERVERCLIENT_H
#define SERVERCLIENT_H

#include "Server.h"

// я сам - сокет, подключённый к настоящему серверу
class ServerClient : public QTcpSocket
{
    Q_OBJECT

public:
    QTcpSocket * socket;  // а это сокет для общения с клиентом
    qint32 ID;
    QString id() { return QString::number(ID); }
private:
    Server * server;

public:
    explicit ServerClient(QTcpSocket * socket, Server * server, int ID);

public slots:
    void close()
    {
        socket->abort();
    }

private:  // нужно для чтения из сокетов
    qint16 BlockSize = 0;
    qint16 RealServerBlockSize = 0;

public slots:  // чтение от клиента / чтение от настоящего сервера
    void read();
    void disconnected();
    void read_from_real_server();
    void disconnected_from_real_server();
};

#endif // SERVERCLIENT_H
