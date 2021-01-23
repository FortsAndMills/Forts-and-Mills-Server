#ifndef EVENTWINDOW_H
#define EVENTWINDOW_H

#include <QMap>
#include <QMainWindow>
#include <QPlainTextEdit>
#include <QLabel>
#include <QPushButton>
#include <QListWidget>
#include <QTcpServer>
#include <QTime>
#include <QTcpSocket>
#include <QObject>
#include <QApplication>
#include <QTimer>


// Перехватчик общения между сервером и клиентом
// для тестирования вырубания интернета / повреждения пакетов
class ServerClient;
class Server : public QMainWindow
{
    Q_OBJECT

    QTcpServer * server;

    // GUI
    QPlainTextEdit * text;
    QListWidget *listWidget;  // можно выбрать клиента
    QPushButton * block;      // вырубить выбранного клиента
                              // и отключить возможность заново подключаться
    QPushButton * resume;     // возобновить возможность подключения

public:
    // написать что-то в текстовое поле
    void say(QString NewText, bool sub = false)
    {
        if (sub)
            NewText = "    " + NewText;
        else
            NewText = QDate::currentDate().toString() + " " + QTime::currentTime().toString() + ")\n    " + NewText + "\n";

        text->appendPlainText(NewText);
    }
    explicit Server();
    ~Server();

    void setgeometry();

private:
    QMap <qint32, ServerClient *> clients;
    int ids = 0;

public slots:
    void getConnection();
    void disconnected(ServerClient * client);
    void block_somebody();
    void resume_listening();
};

#endif // EVENTWINDOW_H



