
#include "server.h"
#include "criptpass.h"
#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QSqlError>
#include <QByteArray>




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

    httpServer.route("/clients", [this]() -> QHttpServerResponse { return handleData(); });
    qDebug() << "?? Route `/clients` added.";

    httpServer.route("/clients/<arg>", [this](const QString &clientId) {
        return handleDataById(clientId.toInt());
    });
    qDebug() << "Route `/data/<id>` added.";

    httpServer.route("/terminal_info", [this](const QHttpServerRequest &request) {
        return handleTerminalInfo(request);
    });
    qDebug() << "✅ Route `/terminal_info` added.";

}
/**
 * @brief Обробляє запит `/terminal_info`, виконує SQL-запит для отримання інформації про термінал
 * @param request HTTP-запит з параметрами `client_id` та `terminal_id`
 * @return JSON-відповідь з інформацією про термінал або повідомленням про помилку
 */
QHttpServerResponse Server::handleTerminalInfo(const QHttpServerRequest &request) {
    QUrlQuery query(request.query());
    qDebug() << "🔹 Запит отримано: /terminal_info";

    if (!query.hasQueryItem("client_id") || !query.hasQueryItem("terminal_id")) {
        return QHttpServerResponse("application/json", R"({"error": "Missing parameters"})");
    }

    int clientId = query.queryItemValue("client_id").toInt();
    int terminalId = query.queryItemValue("terminal_id").toInt();

    // ?? Отримуємо параметри підключення до БД клієнта
    auto clientDbParams = getClientDBParams(clientId);

    if (!clientDbParams.has_value()) {
        qWarning() << "? Не вдалося отримати параметри БД клієнта!";
        return QHttpServerResponse("application/json", R"({"error": "Failed to get client DB parameters"})");
    }

    qDebug() << "? Параметри отримано:" << clientDbParams->server << clientDbParams->database;


    QSqlQuery sqlQuery(db);
    sqlQuery.prepare(R"(
        SELECT c.client_name, t.terminal_id, t.adress, t.phone
        FROM terminals t
        LEFT JOIN clients_list c ON c.client_id = t.client_id
        WHERE t.client_id = :client_id AND t.terminal_id = :terminal_id
    )");

    sqlQuery.bindValue(":client_id", clientId);
    sqlQuery.bindValue(":terminal_id", terminalId);

    if (!sqlQuery.exec()) {
        qWarning() << "❌ SQL Error:" << sqlQuery.lastError().text();
        return QHttpServerResponse("application/json", R"({"error": "Database query failed"})");
    }

    if (!sqlQuery.next()) {
        return QHttpServerResponse("application/json", R"({"error": "Terminal not found"})");
    }



    QJsonObject response;
    response["client_name"] = sqlQuery.value("client_name").toString();
    response["terminal_id"] = sqlQuery.value("terminal_id").toInt();
    response["adress"] = sqlQuery.value("adress").toString();
    response["phone"] = sqlQuery.value("phone").toString();

    return QHttpServerResponse("application/json", QJsonDocument(response).toJson());
}



/**
 * @brief Обробляє запит `/status`, повертає JSON
 * @return JSON-відповідь { "status": "ok" }
 */
QHttpServerResponse Server::handleStatus() {
    qInfo() << "✅ Отримано запит на /status";
    QJsonObject response;
    response["status"] = "ok";
    QByteArray jsonData = QJsonDocument(response).toJson(QJsonDocument::Compact);
    qInfo() << "✅ Відправляємо JSON-відповідь" << response;
    QHttpServerResponse httpResponse("application/json; charset=utf-8", jsonData);
    return httpResponse;
}

/**
 * @brief Обробляє запит `/clients`, повертає JSON
 * @return JSON-відповідь { "id": "Clent Name" }
 */
QHttpServerResponse Server::handleData() {
    QSqlQuery query(db);
    if (!query.exec("SELECT client_id, client_name FROM clients_list")) {
        qCritical() << "? Database query failed:" << query.lastError().text();
        return QHttpServerResponse("application/json", QByteArray(R"({"error": "Database query failed"})"));
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

    QByteArray jsonData = QJsonDocument(response).toJson(QJsonDocument::Compact);

    // Створюємо коректну HTTP-відповідь із заголовком UTF-8
    QHttpServerResponse httpResponse("application/json; charset=utf-8", jsonData);

    return httpResponse;
}


    /**
 * @brief Обробляє запит `/clients/<arg>`, <arg> - код клієнта повертає JSON
 * @return JSON-відповідь { "id": "Clent Name" }
 */

QHttpServerResponse Server::handleDataById(int clientId) {
    QSqlQuery query(db);
    query.prepare("SELECT client_id, client_name FROM clients_list WHERE client_id = :id");
    query.bindValue(":id", clientId);

    if (!query.exec()) {
        qCritical() << "Database query failed:" << query.lastError().text();
        return QHttpServerResponse("application/json", QByteArray(R"({"error": "Database query failed"})"));
    }

    if (!query.next()) {
        return QHttpServerResponse("application/json", QByteArray(R"({"error": "Client not found"})"));
    }

    QJsonObject response;

//    QJsonObject obj;
    response["id"] = query.value(0).toInt();
    response["name"] = query.value(1).toString();

    QByteArray jsonData = QJsonDocument(response).toJson(QJsonDocument::Compact);

    QHttpServerResponse httpResponse("application/json; charset=utf-8", jsonData);
    return httpResponse;
}


/**
 * @brief Отримує параметри підключення до бази даних клієнта
 * @param clientID ID клієнта
 * @return std::optional<ClientDBParams> - Параметри підключення або порожній об'єкт, якщо не вдалося отримати дані
 */
std::optional<ClientDBParams> Server::getClientDBParams(int clientID) {
    QSqlQuery query;
    query.prepare("SELECT client_db_server, client_db_port, client_db_file, "
                  "client_db_user, client_db_pass FROM clients_settings WHERE client_id = :clientID");
    query.bindValue(":clientID", clientID);

    if (!query.exec()) {
        qCritical() << "? Помилка виконання SQL-запиту:" << query.lastError().text();
        return std::nullopt;
    }

    if (!query.next()) {
        qWarning() << "?? Немає даних для client_id =" << clientID;
        return std::nullopt;
    }

    ClientDBParams params;
    params.server = query.value(0).toString();
    params.port = query.value(1).toInt();
    params.database = query.value(2).toString();
    params.username = query.value(3).toString();
    // ?? Дешифруємо пароль перед збереженням
    CriptPass criptPass;
    QString encryptedPass = query.value(4).toString();
    params.password = criptPass.decryptPassword(encryptedPass);

    qInfo() << "? Отримані параметри підключення для client_id =" << clientID
            << "\n  Сервер:" << params.server
            << "\n  Порт:" << params.port
            << "\n  Файл БД:" << params.database
            << "\n  Користувач:" << params.username
            << "\n  Пароль:" << params.password;

    return params;
}

