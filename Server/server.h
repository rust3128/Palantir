#ifndef SERVER_H
#define SERVER_H

#include <QObject>
#include <QHttpServer>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <optional>
#include "../config.h"

// Структура з параметрами підключення до бази клієнта
struct ClientDBParams {
    QString server;
    int port;
    QString database;
    QString username;
    QString password;
};

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
    QSqlDatabase clientDB; // підключення до БД клієнта
    std::optional<QString> connectToClientDatabase(const ClientDBParams &params);
    QJsonArray getDispensersInfo(QSqlDatabase &clientDB, int terminalId);
    QJsonObject getPumpsInfo(QSqlDatabase &clientDB, int terminalId);


    QHttpServerResponse handleStatus();                  // 🔹 Обробка `/status`
    QHttpServerResponse handleData();                    // 🔹 Обробка `/data`
    QHttpServerResponse handleDataById(int clientId);    // 🔹 Обробка `/data/<id>`
    QHttpServerResponse handleTerminalInfo(const QHttpServerRequest &request); ///terminal_info
    QHttpServerResponse handlePosInfo(const QHttpServerRequest &request);       //pos_info
    QHttpServerResponse handleReservoirsInfo(const QHttpServerRequest &request); //Tank info
    QHttpServerResponse handleAzsList(const QHttpServerRequest &request);       //AZS list
    QJsonArray getPosInfo(QSqlDatabase &clientDB, int terminalId);
    std::optional<ClientDBParams> getClientDBParams(int clientID);
    std::optional<QSqlDatabase> connectToClientDB(const ClientDBParams& params);
};

#endif // SERVER_H
