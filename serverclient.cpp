#include "ServerClient.h"

ServerClient::ServerClient(QTcpSocket *socket, Server *server, int ID) : QObject()
{
    this->socket = socket;
    this->server = server;
    this->ID = ID;

    connect(socket, SIGNAL(readyRead()), SLOT(read()));
    connect(socket, SIGNAL(disconnected()), SLOT(disconnected()));
}

void ServerClient::startPlaying(qint8 player_index, QList<ServerClient *> opponents, QList<qint32> random)
{
    this->state = PLAYING;
    this->opponents = opponents;
    this->opponents.removeAll(this);  // себя в списке оппонентов не храним
    this->game_index = player_index;

    // отправка сообщения
    QByteArray block;
    QDataStream out(&block, QIODevice::WriteOnly);
    out << (quint16)0 << START_GAME_MESSAGE << (qint32)ID << (qint8)game_index << random;
    out.device()->seek(0);
    out << (quint16)(block.size() - sizeof(quint16));
    socket->write(block);
}
void ServerClient::blocked()
{
    // мы заблокированы
    state = FINAL_STATE;

    // отправка сообщения
    QByteArray block;
    QDataStream out(&block, QIODevice::WriteOnly);
    out << (quint16)0 << BLOCKED;
    out.device()->seek(0);
    out << (quint16)(block.size() - sizeof(quint16));
    socket->write(block);

    // завершение общения
    socket->disconnectFromHost();
    server->ignore(ID);
    this->deleteLater();
}
void ServerClient::badVersion()
{
    // старая версия; прекращаем общение
    state = FINAL_STATE;
    IS_OLD_VER = true;

    // отправка сообщения
    QByteArray block;
    QDataStream out(&block, QIODevice::WriteOnly);
    out << (quint16)0 << BAD_VERSION;
    out.device()->seek(0);
    out << (quint16)(block.size() - sizeof(quint16));
    socket->write(block);

    // завершение общения
    server->say(id() + " has too old version, version = " + QString::number(version), true);
    socket->disconnectFromHost();
    server->ignore(ID);
    this->deleteLater();
}
void ServerClient::error_happened()
{
    // TODO
}

void ServerClient::disconnected()
{
    if (state == FINAL_STATE)
    {
        // самоудаляемся
        socket->deleteLater();
        deleteLater();
    }
    else
    {
        // сообщаем серверу, что мы отключились, не будучи в FINAL STATE
        server->disconnected(this);
    }
}
void ServerClient::reconnect(QTcpSocket *qts)
{
    // мы переподключились, и qts - новый сокет
    // отвязываем от старого сокета сигналы
    QObject::disconnect(socket, SIGNAL(readyRead()), this, SLOT(read()));
    QObject::disconnect(socket, SIGNAL(disconnected()), this, SLOT(disconnected()));
    socket->deleteLater();

    // привязываем к новому сокету сигналы
    socket = qts;
    connect(socket, SIGNAL(readyRead()), SLOT(read()));
    connect(socket, SIGNAL(disconnected()), SLOT(disconnected()));

    // отправляем по новому сокету все не дошедшие сообщения
    QMap<int, QByteArray> mes = messages;
    messages.clear();
    foreach (QByteArray m, mes)
    {
        sendMessage(m);
    }

    // посылаем сообщение о переподключении
    foreach (ServerClient * sc, opponents)
        sc->opponentReconnected(this);

    // возможно, мы также что-то не прочитали
    read();
}

void ServerClient::read()
{
    if (socket == NULL)
        return;

    // читаем размер сообщения
    QDataStream in(socket);
    if (BlockSize == 0)
    {
        if (socket->bytesAvailable() < (int)sizeof(quint16))
                return;
        in >> BlockSize;

        if (BlockSize == 0)
            server->say("NET ERROR: BlockSize is 0! My id: " + this->id());

        // так проверяем о том, что-то пошло не так
        if (in.status() != QDataStream::Ok)
        {
            server->say("ERROR while reading message size. My id: " + this->id());
            error_happened();
            return;
        }
     }

    // сообщение дошло не целиком
    if (socket->bytesAvailable() < BlockSize)
        return;

    // размер самого сообщения помимо id сообщения
    qint64 message_size = BlockSize - sizeof(quint8);
    BlockSize = 0;

    // читаем id сообщения
    quint8 MessageId;
    in >> MessageId;

    // обрабатываем
    ProcessMessage(in, MessageId, message_size);

    // возможно, уже пришло следующее
    read();
}
void ServerClient::ProcessMessage(QDataStream &in, qint8 mes, qint64 message_size)
{
    if (IS_OLD_VER)
        return;

    switch (mes)
    {
    case WANT_TO_LISTEN_NEWS:
    {
        in >> version;
        if (in.status() != QDataStream::Ok)
        {
            server->say("ERROR: WANT_TO_LISTEN_NEWS message came wrong! My id: " + this->id());
            error_happened();
            return;
        }

        // проверка актуальности версии
        if (server->isVersionGood(this))
        {
            if (state == PLAYING)
                server->finishesGame(this);
            state = MENU_STATE;  // мы точно вернулись в главное меню
            opponents.clear();

            server->wantToListenNews(this);
        }
        else
        {
            badVersion();
        }
        return;
    }
    case JOIN_GAME:
    {
        qint32 index;
        in >> index;
        if (in.status() != QDataStream::Ok)
        {
            server->say("ERROR: JOIN_GAME message came wrong! My id: " + this->id());
            error_happened();
            return;
        }

        server->join(this, index);
        return;
    }
    case LEAVE_GAME:
    {
        qint32 index;
        in >> index;

        if (in.status() != QDataStream::Ok)
        {
            server->say("ERROR: LEAVE_GAME message came wrong! My id: " + this->id());
            error_happened();
            return;
        }

        server->leaveGame(this, index);
        return;
    }
    case CREATE_GAME:
    {
        QList <qint32> rules;
        in >> rules;

        if (in.status() != QDataStream::Ok)
        {
            server->say("ERROR: CREATE_GAME message came wrong! My id: " + this->id());
            error_happened();
            return;
        }

        server->createGame(this, rules);
        return;
    }
    case RESEND_THIS_TO_OPPONENT:
    {
        // читаем id сообщения
        qint16 mes_key;
        in >> mes_key;

        // посылаем подтверждение о получении
        this->sendRecievedMessage(mes_key);

        // читаем остальное сообщение
        message_size -= sizeof(qint16);
        QByteArray message = in.device()->read(message_size);

        if (in.status() != QDataStream::Ok)
        {
            server->say("ERROR: RESEND_THIS_TO_OPPONENT message came wrong! My id: " + this->id());
            error_happened();
            return;
        }

        // пересылаем игровое сообщение всем оппонентам
        foreach (ServerClient * opponent, opponents)
            opponent->sendMessage(message);
        return;
    }
    case MESSAGE_RECEIVED:
    {
        qint16 mes_key;
        in >> mes_key;

        if (in.status() != QDataStream::Ok)
        {
            server->say("ERROR: MESSAGE_RECEIVED message came wrong! My id: " + this->id());
            error_happened();
            return;
        }

        messages.remove(mes_key);
        return;
    }
    case I_RECONNECTED:
    {
        qint32 ID;
        in >> ID;        

        if (in.status() != QDataStream::Ok)
        {
            server->say("ERROR: I RECONNECTED message came wrong! My id: " + this->id());
            error_happened();
            return;
        }

        server->reconnected(this, ID);
        return;
    }
    case I_LEAVE:
    {
        server->leave(this);
        state = FINAL_STATE;
        opponents.clear();
        return;
    }
    case CONNECTION_CHECK:
    {
        return;
    }
    default:
        server->say("ERROR: unknown message from " + id() + ", code " + QString::number(mes));
    }
}
