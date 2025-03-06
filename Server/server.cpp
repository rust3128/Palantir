
#include "server.h"
#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QSqlError>

/**
 * @brief Конструктор класу Server
 * @param config Вказівник на об'єкт конфігурації
 * @param parent Батьківський QObject
 */
Server::Server(Config *config, QObject *parent) : QObject(parent), config(config) {
    port = config->getServerPort();
    if (!connectToDatabase()) {
        qCritical() << "❌ Failed to connect to database!";
    }
}

/**
 * @brief Підключається до бази Firebird
 * @return true, якщо підключення успішне, інакше false
 */
bool Server::connectToDatabase() {
    db = QSqlDatabase::addDatabase("QIBASE");
    db.setHostName(config->getDatabaseHost());
    db.setPort(config->getDatabasePort());
    db.setDatabaseName(config->getDatabaseName());
    db.setUserName(config->getDatabaseUser());
    db.setPassword(config->getDatabasePassword());

    if (!db.open()) {
        qCritical() << "❌ Database connection failed:" << db.lastError().text();
        return false;
    }

    qInfo() << "✅ Connected to database" << config->getDatabaseName();
    return true;
}


/**
 * @brief Запускає сервер на вказаному порту
 */
void Server::start() {
    setupRoutes();  // 🔹 Додаємо маршрути перед запуском сервера
    QString serverAddress = QString("http://localhost:%1").arg(port);
    if (!httpServer.listen(QHostAddress::Any, port)) {
        qCritical() << "Failed to start server " << serverAddress;
        return;
    }

    qInfo() << "Server started on" << serverAddress;
}

/**
 * @brief Налаштовує маршрути для обробки HTTP-запитів
 */
void Server::setupRoutes() {
    httpServer.route("/status", [this]() { return handleStatus(); });
    qDebug() << "🔹 Route `/status` added.";

    httpServer.route("/data", [this]() { return handleData(); });
    qDebug() << "🔹 Route `/data` added.";

    httpServer.route("/data/<arg>", [this](const QString &clientId) {
        return handleDataById(clientId.toInt());
    });
    qDebug() << "Route `/data/<id>` added.";
}

/**
 * @brief Обробляє запит `/status`, повертає JSON
 * @return JSON-відповідь { "status": "ok" }
 */
QByteArray Server::handleStatus() {
    QJsonObject response;
    response["status"] = "ok";
    return QJsonDocument(response).toJson();
}


QByteArray Server::handleData() {
    QSqlQuery query(db);
    if (!query.exec("SELECT client_id, client_name FROM clients_list")) {
        qCritical() << "❌ Database query failed:" << query.lastError().text();
        return R"({"error": "Database query failed"})";
    }

    QJsonArray results;
    while (query.next()) {
        QJsonObject obj;
        obj["id"] = query.value(0).toInt();
        obj["name"] = query.value(1).toString();
        results.append(obj);
    }

    QJsonObject response;
    response["data"] = results;
    return QJsonDocument(response).toJson();
}

QByteArray Server::handleDataById(int clientId) {
    QSqlQuery query(db);
    query.prepare("SELECT client_id, client_name FROM clients_list WHERE client_id = :id");
    query.bindValue(":id", clientId);

    if (!query.exec()) {
        qCritical() << "Database query failed:" << query.lastError().text();
        return R"({"error": "Database query failed"})";
    }

    if (!query.next()) {
        return R"({"error": "Client not found"})";
    }

    QJsonObject obj;
    obj["id"] = query.value(0).toInt();
    obj["name"] = query.value(1).toString();

    return QJsonDocument(obj).toJson();
}
