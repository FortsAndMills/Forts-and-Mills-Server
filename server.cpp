#include "Server.h"
#include "ServerClient.h"

#include <QFile>

#define PORT 64124
#define VERSION 3100

// геометрия GUI сервера
void Server::setgeometry()
{
    this->setFixedSize(QSize(700, 400));

    text = new QPlainTextEdit(this);
    text->setFont(QFont("Book Antiqua", 16));
    text->setReadOnly(true);
    text->setGeometry(0, 0, 600, 400);

    playing = new QLabel(this);
    playing->setFont(QFont("Book Antiqua", 14));
    playing->setAlignment(Qt::AlignCenter);
    playing->setGeometry(600, 0, 100, 150);
    playingInc();

    played = new QLabel(this);
    played->setFont(QFont("Book Antiqua", 14));
    played->setAlignment(Qt::AlignCenter);
    played->setGeometry(600, 150, 100, 150);
    playedInc();

    block = new QPushButton("block", this);
    block->setCheckable(true);
    block->setFont(QFont("Book Antiqua", 14));
    block->setGeometry(600, 300, 100, 100);
}

Server::Server() : QMainWindow()
{
    // порт можно задать в текстовом файле
    int port = PORT;
    QFile * hostFile = new QFile("port.txt", this);
    if (hostFile->open(QIODevice::ReadOnly | QIODevice::Text))
    {
        QTextStream tstream(hostFile);
        tstream >> port;
        hostFile->close();
    }

    // инициализация GUI
    setgeometry();

    // также будем логгировать все события
    log_file = new QFile("server_log.txt");
    if (log_file->open(QIODevice::WriteOnly))
    {
        logger.setDevice(log_file);
        logger << "Logger created." << Qt::endl;
    }
    else
    {
        say("ERROR: can't create log file!");
        return;
    }

    // инициализация сервера
    server = new QTcpServer(this);
    if (!server->listen(QHostAddress("0.0.0.0"), port))
        this->say("ERROR: Can't listen!");
    else
        this->say("Good afternoon. FortsAndMills are working on port #" + QString::number(port));

    // все новые подключения обработаем в getConnection
    connect(server, SIGNAL(newConnection()), this, SLOT(getConnection()));
}
Server::~Server()
{
    foreach (ServerClient * client, clients)
        client->close();
    log_file->close();
}

bool Server::isVersionGood(ServerClient *client)
{
    return client->version >= VERSION;
}
void Server::wantToListenNews(ServerClient *client)
{
    // клиент просит список всех текущих игр
    // (вышел в главное меню или только-только подключился)
    QList<qint32> ids;
    QList < QList<qint32> > rules;
    QList <qint8> players;

    for (QMap<qint32, Game>::iterator it = games.begin(); it != games.end(); ++it)
    {
        ids.push_back(it.key());
        rules.push_back(it->rules);
        players.push_back(it->player_ids.size());
    }

    client->sendNews(ids, rules, players);
}
void Server::createGame(ServerClient * author, QList<qint32> rules)
{
    // создана новая комната
    games[game_ids++] = Game(rules);

    // в логе можно посмотреть настройки созданной комнаты
    say("Game #" + QString::number(game_ids - 1) + " created: " +
                QString::number(rules[0]) + " pl, " +
                QString::number(rules[1]) + " timer type, " +
                QString::number(rules[2]) + "x" + QString::number(rules[3]) +
                ", code " + QString::number(rules[4]) + "/" + QString::number(rules[5]) + ", " +
                (rules[6] == 179 ? "CDT" : QString::number(rules[6]) + " dt"));

    // отправка сообщения о новой комнате всем, кто в меню
    foreach (ServerClient * client, clients)
        if (client->state == ServerClient::MENU_STATE)
            client->sendNewGameCreated(game_ids - 1, rules, client == author);
}
void Server::join(ServerClient *client, qint32 game_id)
{
    // клиент присоединился к игре
    say(client->id() + " wants to play game #" + QString::number(game_id));
    games[game_id].player_ids.push_back(client->ID);

    // сообщаем всем, кто в меню
    foreach (ServerClient * client, clients)
        if (client->state == ServerClient::MENU_STATE)
            client->sendJoinMessage(game_id);

    // накопилось достаточное число игроков
    if (games[game_id].player_ids.size() == games[game_id].players_needed())
    {
        // создаём рандом сид
        // он должен быть одинаковый у всех игроков
        // (используется для создания карты)
        QList <qint32> random;
        for (int i = 0; i < 1000; ++i)  // TODO константа
            random << rand();

        // создаём список клиентов-игроков
        QList <ServerClient *> opponents;
        for (int i = 0; i < games[game_id].player_ids.size(); ++i)
        {
            opponents.push_back(clients[games[game_id].player_ids[i]]);
        }

        // всем отправляем сообщение о старте игры
        for (int i = 0; i < opponents.size(); ++i)
        {
            opponents[i]->startPlaying(i, opponents, random);
        }

        // логгинг
        QString phr = opponents[0]->id();
        for (int i = 1; i < opponents.size(); ++i)
            phr += " and " + opponents[i]->id();
        say(phr + " started game!", true);

        playingInc();

        // удаляем игру из списка комнат
        removeGame(game_id);
    }
}
void Server::leaveGame(ServerClient *client, qint32 i)
{
    // клиент покинул комнату
    say(client->id() + " leaves playing game #" + QString::number(i));
    games[i].player_ids.removeAll(client->ID);

    // сообщаем всем, кто в меню
    foreach (ServerClient * client, clients)
        if (client->state == ServerClient::MENU_STATE)
            client->sendUnjoinMessage(i);

    // если нету игроков, удаляем комнату
    if (games[i].player_ids.size() == 0)
    {
        removeGame(i);
    }
}
void Server::removeGame(qint32 game_id)
{
    // сообщаем всем об удалении комнаты
    foreach (ServerClient * client, clients)
        if (client->state == ServerClient::MENU_STATE)
            client->sendRemoveGameMessage(game_id);

    // elfkztv
    say("game #" + QString::number(game_id) + " is removed.", true);
    games.remove(game_id);
}
void Server::finishesGame(ServerClient *client)
{
    // клиент закончил игру (вышел в главное меню или закрыл приложение)
    // ему больше не нужно присылать игровые сообщения об этой партии
    // удаляем его из списка оппонентов всех его оппонентов)))
    foreach (ServerClient * sc, client->opponents)
        sc->opponents.removeAll(client);

    // игра заканчивается, если вышел последний игрок
    // у которого оппонентов-то уже нет
    if (client->opponents.size() == 0)
    {
        say(client->id() + "'s game ends here");
        playingDec();
        playedInc();
    }
}
void Server::ignore(qint32 ID)
{
    // с этим клиентом не общаемся
    clients.remove(ID);
}
void Server::reconnected(ServerClient *client, qint32 ID)
{
    // клиент на самом деле переподключившийся клиент с данным ID
    say(client->id() + " is reconnected " + QString::number(ID), true);
    if (clients.contains(ID))
    {
        // делаем замену сокета у клиента с индексом ID
        clients[ID]->reconnect(client->socket);
        clients.remove(client->ID);

        client->socket = NULL;  // не позволяем в деструкторе убить сокет
        client->deleteLater();
    }
    else
    {
        say("...what happened?", true);
    }
}
void Server::leave(ServerClient *client)
{
    // клиент закрыл приложение
    say(client->id() + " leaves!");

    // если клиент был в игре, надо убрать его из оппонентов
    if (client->state == ServerClient::PLAYING)
        finishesGame(client);

    // если клиент был в главном меню, надо убрать его из всех комнат
    if (client->state == ServerClient::MENU_STATE)
    {
        for (QMap<qint32, Game>::iterator it = games.begin(); it != games.end(); ++it)
        {
            if (it->player_ids.contains(client->ID))
            {
                leaveGame(client, it.key());
                break;
            }
        }
    }

    // пока
    clients.remove(client->ID);
}

void Server::getConnection()
{
    ServerClient * New = new ServerClient(server->nextPendingConnection(), this, ids++);

    // если на сервере нажать кнопку "блокировать",
    // будет послано уведомление об обновлении сервера
    if (block->isChecked())
    {
        say("New connection blocked");
        New->blocked();
    }
    else
    {
        say("New connection: " + New->id());

        clients[New->ID] = New;
    }
    qApp->alert(dynamic_cast<QWidget *>(this));
}
void Server::disconnected(ServerClient *client)
{
    // сообщаем его оппонентам, что товарищ вырубился
    foreach (ServerClient * sc, client->opponents)
        sc->opponentDisconnected(client);

    say(client->id() + " disconnected...");

    // из комнат его точно выкидываем
    // некоторые комнаты могут пропасть, поэтому создаёт копию мапы.
    QMap<qint32, Game> current_games = games;
    for (QMap<qint32, Game>::iterator it = current_games.begin(); it != current_games.end(); ++it)
        if (it->player_ids.contains(client->ID))
            leaveGame(client, it.key());
}

