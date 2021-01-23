#ifndef EVENTWINDOW_H
#define EVENTWINDOW_H

#include <QMap>
#include <QMainWindow>
#include <QPlainTextEdit>
#include <QLabel>
#include <QPushButton>
#include <QTcpServer>
#include <QTime>
#include <QTcpSocket>
#include <QObject>
#include <QApplication>
#include <QTimer>

class ServerClient;
class Server : public QMainWindow
{
    Q_OBJECT

    QTcpServer * server;

    // GUI и счётчики сыгранных / текущих игр.
    QPlainTextEdit * text;
    int now = -1;
    QLabel * playing;
    int done = -1;
    QLabel * played;
    QPushButton * block;

    // лог-файл
    QFile * log_file;
    QTextStream logger;

private:
    void playingInc()
    {
        ++now;
        playing->setText("NOW\nPLAYING:\n" + QString::number(now));
    }
    void playingDec()
    {
        --now;
        playing->setText("NOW\nPLAYING:\n" + QString::number(now));
    }
    void playedInc()
    {
        ++done;
        played->setText("PLAYED:\n" + QString::number(done));
    }

public:
    // добавление текста в лог и в GUI
    void say(QString NewText, bool sub = false)
    {
        if (sub)
            NewText = "    " + NewText;
        else
            NewText = QDate::currentDate().toString() + " " + QTime::currentTime().toString() + ")\n    " + NewText + "\n";

        text->appendPlainText(NewText);
        logger << NewText << Qt::endl;
    }

    explicit Server();
    ~Server();

    void setgeometry();

    // функции, которые будут вызывать сами клиенты
    bool isVersionGood(ServerClient * client);
    void wantToListenNews(ServerClient * client);
    void createGame(ServerClient *client, QList <qint32> rules);
    void join(ServerClient * client, qint32 game_id);
    void leaveGame(ServerClient * client, qint32 i);
    void removeGame(qint32 game_id);
    void finishesGame(ServerClient *client);
    void ignore(qint32 ID);
    void reconnected(ServerClient * client, qint32 ID);
    void leave(ServerClient * client);

private:
    QMap <qint32, ServerClient *> clients;
    int ids = 0;

    // "комнаты" в главной менюшке
    int game_ids = 0;
    struct Game
    {
        QList <qint32> rules;
        QList <int> player_ids;

        int players_needed() { return rules[0]; }
        Game(QList <qint32> _rules) : rules(_rules) {}
        Game() {}
    };
    QMap <qint32, Game > games;

    // серверные события: новое подключение и отключение.
public slots:
    void getConnection();
    void disconnected(ServerClient * client);
};

#endif // EVENTWINDOW_H



