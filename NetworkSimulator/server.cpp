#include "Server.h"
#include "ServerClient.h"

#include <QFile>

#define PORT 64124
#define VERSION 2000

// геометрия GUI сервера
void Server::setgeometry()
{
    this->setFixedSize(QSize(700, 400));

    text = new QPlainTextEdit(this);
    text->setFont(QFont("Book Antiqua", 16));
    text->setReadOnly(true);
    text->setGeometry(0, 0, 600, 400);

    block = new QPushButton("block", this);
    block->setFont(QFont("Book Antiqua", 14));
    block->setGeometry(600, 0, 100, 150);

    resume = new QPushButton("resume", this);
    resume->setFont(QFont("Book Antiqua", 14));
    resume->setGeometry(600, 300, 100, 150);

    listWidget = new QListWidget(this);
    listWidget->setGeometry(600, 150, 100, 150);
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

    setgeometry();
    connect(block, SIGNAL(clicked()), this, SLOT(block_somebody()));
    connect(resume, SIGNAL(clicked()), this, SLOT(resume_listening()));

    // инициализация сервера
    server = new QTcpServer(this);
    if (!server->listen(QHostAddress("0.0.0.0"), port))
        this->say("ERROR: Can't listen!");
    else
        this->say("NetworkSimulation is working on port #" + QString::number(port));

    // все новые подключения обработаем в getConnection
    connect(server, SIGNAL(newConnection()), this, SLOT(getConnection()));
}
Server::~Server()
{
    foreach (ServerClient * client, clients)
        client->close();
}


void Server::getConnection()
{
    ServerClient * New = new ServerClient(server->nextPendingConnection(), this, ids++);

    clients[New->ID] = New;
    qApp->alert(dynamic_cast<QWidget *>(this));

    listWidget->addItem(New->id());
}
void Server::disconnected(ServerClient *client)
{
    QList<QListWidgetItem *> items = listWidget->findItems(client->id(), Qt::MatchExactly);
    if (items.size() > 0)
    {
        listWidget->removeItemWidget(items[0]);
        delete items[0];
    }

    say(client->id() + " disconnected...");

    clients.remove(client->ID);
}
void Server::block_somebody()
{
    foreach (QListWidgetItem * item, listWidget->selectedItems())
    {
        int ID = item->text().toInt();
        clients[ID]->disconnected();
    }
    server->pauseAccepting();
}
void Server::resume_listening()
{
    server->resumeAccepting();
}
