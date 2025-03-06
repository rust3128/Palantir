#ifndef SERVER_H
#define SERVER_H

#include <QObject>
#include <QHttpServer>
#include <QSqlDatabase>
#include <QSqlQuery>
#include "../config.h"

class Server : public QObject {
    Q_OBJECT
public:
    explicit Server(Config *config, QObject *parent = nullptr);
    void start();  // 🔹 Запуск сервера

private:
    QHttpServer httpServer;
    int port;
    Config *config;  // 🔹 Зберігаємо конфігурацію
    QSqlDatabase db;  // 🔹 Підключення до бази даних

    bool connectToDatabase();  // 🔹 Метод для підключення до бази
    void setupRoutes();  // 🔹 Налаштування всіх маршрутів
    QByteArray handleStatus();                  // 🔹 Обробка `/status`
    QByteArray handleData();                    // 🔹 Обробка `/data`
    QByteArray handleDataById(int clientId);    // 🔹 Обробка `/data/<id>`
};

#endif // SERVER_H
