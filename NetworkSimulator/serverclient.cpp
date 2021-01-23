#include "ServerClient.h"

#define REAL_SERVER_PORT 21137

ServerClient::ServerClient(QTcpSocket *socket, Server *server, int ID) : QTcpSocket(0)
{
    this->socket = socket;
    this->server = server;
    this->ID = ID;

    // общение с клиентом
    connect(socket, SIGNAL(readyRead()), SLOT(read()));
    connect(socket, SIGNAL(disconnected()), SLOT(disconnected()));

    // подключаемся к настоящему серверу
    this->connectToHost("127.0.0.1", REAL_SERVER_PORT);
    if (socket->waitForConnected(-1))
        server->say(this->id() + " connected to real server, he-he");
    connect(this, SIGNAL(readyRead()), SLOT(read_from_real_server()));
    connect(this, SIGNAL(disconnected()), SLOT(disconnected_from_real_server()));
}

// чтение от клиента
void ServerClient::read()
{
    if (socket == NULL)
        return;

    QDataStream in(socket);
    if (BlockSize == 0)
    {
        int available = socket->bytesAvailable();
        if (available < (int)sizeof(quint16))
                return;
        in >> BlockSize;

        if (BlockSize == 0)
            server->say("NET ERROR: BlockSize is 0!");
     }

    int available = socket->bytesAvailable();
    if (available < BlockSize)
        return;

    QByteArray message = in.device()->read(BlockSize);
    BlockSize = 0;

    // копируем сообщение в настоящий сервер
    QByteArray block;
    QDataStream out(&block, QIODevice::WriteOnly);
    out << (quint16)(message.size());
    block.push_back(message);
    this->write(block);

    QMap<int, QString> mes_descr;
    mes_descr[1] = "I want to listen news!";
    mes_descr[2] = "I join game!";
    mes_descr[3] = "I leave game!";
    mes_descr[4] = "I create game!";
    mes_descr[5] = "Resend this to opponent!";
    mes_descr[6] = "I reconnected!";
    mes_descr[7] = "Message with opponent's plan was received!";
    mes_descr[8] = "I leave";
    mes_descr[9] = "[Connection check]";

    if (int(message[0]) != 9)
        server->say(this->id() + " sends message: " + mes_descr[message[0]]);

    read();
}
void ServerClient::disconnected()  // клиент отрубился
{
    server->disconnected(this);
    this->abort();
    this->socket->abort();
    this->socket->deleteLater();
    this->deleteLater();
}

// чтение от настоящего сервера
void ServerClient::read_from_real_server()
{
    QDataStream in(this);
    if (RealServerBlockSize == 0)
    {
        if (this->bytesAvailable() < (int)sizeof(quint16))
                return;
        in >> RealServerBlockSize;

        if (RealServerBlockSize == 0)
            server->say("NET ERROR: RealServerBlockSize is 0!\n");
     }

    if (this->bytesAvailable() < RealServerBlockSize)
        return;

    QByteArray message = in.device()->read(RealServerBlockSize);
    RealServerBlockSize = 0;

    // пересылаем копию сообщения клиенту
    QByteArray block;
    QDataStream out(&block, QIODevice::WriteOnly);
    out << (quint16)(message.size());
    block.push_back(message);
    socket->write(block);

    QMap<int, QString> mes_descr;
    mes_descr[1] = "Blocked!";
    mes_descr[2] = "Bad version!";
    mes_descr[3] = "News (sends list of games)!";
    mes_descr[4] = "Game joined!";
    mes_descr[5] = "Game left!";
    mes_descr[6] = "Remove game!";
    mes_descr[7] = "Game created!";
    mes_descr[8] = "Start playing game!";
    mes_descr[9] = "Message for game (opponent's turn)";
    mes_descr[10] = "Opponent disconnected!";
    mes_descr[11] = "Opponent reconnected!";
    mes_descr[12] = "Resend message received.";

    server->say(this->id() + " gets server message: " + mes_descr[message[0]]);

    read_from_real_server();
}
void ServerClient::disconnected_from_real_server()
{

}
