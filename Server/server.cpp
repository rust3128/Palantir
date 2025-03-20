
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
    httpServer.route("/pos_info", QHttpServerRequest::Method::Get,
                 [this](const QHttpServerRequest &request) {
                     return handlePosInfo(request);
                 });
    httpServer.route("/reservoirs_info", QHttpServerRequest::Method::Get,
                     [this](const QHttpServerRequest &request) {
                         return handleReservoirsInfo(request);
                     });
    httpServer.route("/azs_list", QHttpServerRequest::Method::Get,
                     [this](const QHttpServerRequest &request) {
                         return handleAzsList(request);
                     });


}

QHttpServerResponse Server::handleAzsList(const QHttpServerRequest &request) {
    QUrlQuery queryParams(request.query());  // ✅ Перейменовано для уникнення конфлікту
    qDebug() << "📥 Запит отримано: /azs_list";

    if (!queryParams.hasQueryItem("client_id")) {
        return QHttpServerResponse("application/json", R"({"error": "Missing client_id parameter"})");
    }

    int clientId = queryParams.queryItemValue("client_id").toInt();

    QSqlDatabase db = QSqlDatabase::database();  // Використовуємо основну базу
    if (!db.isOpen()) {
        qWarning() << "⚠️ Основна база не підключена!";
        return QHttpServerResponse("application/json", R"({"error": "Database is not connected"})");
    }

    QSqlQuery sqlQuery(db);  // ✅ Перейменовано для уникнення конфлікту
    QString sql = QString(R"(
        SELECT t.terminal_id, t.name
        FROM terminals t
        WHERE t.client_id = %1
        ORDER BY t.terminal_id;
    )").arg(clientId);

    if (!sqlQuery.exec(sql)) {
        qWarning() << "❌ Помилка SQL-запиту:" << sqlQuery.lastError().text();
        return QHttpServerResponse("application/json", R"({"error": "Database query failed"})");
    }

    QJsonArray azsArray;
    while (sqlQuery.next()) {
        QJsonObject azsObj;
        azsObj["terminal_id"] = sqlQuery.value("terminal_id").toInt();
        azsObj["name"] = sqlQuery.value("name").toString();
        azsArray.append(azsObj);
    }

    QJsonObject response;
    response["azs_list"] = azsArray;

    return QHttpServerResponse("application/json", QJsonDocument(response).toJson());
}



QHttpServerResponse Server::handleReservoirsInfo(const QHttpServerRequest &request) {
    QUrlQuery query(request.query());
    qDebug() << "📥 Отримано запит: /reservoirs_info";

    // Перевіряємо наявність параметрів
    if (!query.hasQueryItem("client_id") || !query.hasQueryItem("terminal_id")) {
        return QHttpServerResponse("application/json", R"({"error": "Missing parameters"})");
    }

    int clientId = query.queryItemValue("client_id").toInt();
    int terminalId = query.queryItemValue("terminal_id").toInt();

    // 🔹 Отримуємо параметри підключення до БД клієнта
    auto clientDbParams = getClientDBParams(clientId);
    if (!clientDbParams.has_value()) {
        qWarning() << "⚠️ Не вдалося отримати параметри БД клієнта!";
        return QHttpServerResponse("application/json", R"({"error": "Failed to get client DB parameters"})");
    }

    // 🔹 Підключаємося до бази даних клієнта
    if (!connectToClientDatabase(clientDbParams.value())) {
        qWarning() << "⚠️ Помилка підключення до БД клієнта!";
        return QHttpServerResponse("application/json", R"({"error": "Failed to connect to client database"})");
    }

    QString connectionName = QString("clientDB_%1").arg(clientDbParams->server);
    QSqlDatabase clientDB = QSqlDatabase::database(connectionName);

    // 🔹 Виконуємо SQL-запит
    QSqlQuery sqlQuery(clientDB);
    sqlQuery.prepare(R"(
        SELECT t.tank_id, t.fuel_id, f.shortname, f.name, t.maxvalue, t.minvalue,
               t.deadmax, t.deadmin, t.tubeamount
        FROM tanks t
        LEFT JOIN fuels f ON f.fuel_id = t.fuel_id
        WHERE t.terminal_id = :terminalId AND t.isactive = 'T'
        ORDER BY t.tank_id;
    )");
    sqlQuery.bindValue(":terminalId", terminalId);

    if (!sqlQuery.exec()) {
        qWarning() << "❌ Помилка виконання SQL-запиту:" << sqlQuery.lastError().text();
        return QHttpServerResponse("application/json", R"({"error": "Database query failed"})");
    }

    // 🔹 Формуємо JSON-відповідь
    QJsonArray reservoirsArray;
    while (sqlQuery.next()) {
        QJsonObject tankObj;
        tankObj["tank_id"] = sqlQuery.value("tank_id").toInt();
        tankObj["fuel_id"] = sqlQuery.value("fuel_id").toInt();
        tankObj["shortname"] = sqlQuery.value("shortname").toString();
        tankObj["name"] = sqlQuery.value("name").toString();
        tankObj["maxvalue"] = sqlQuery.value("maxvalue").toInt();
        tankObj["minvalue"] = sqlQuery.value("minvalue").toInt();
        tankObj["deadmax"] = sqlQuery.value("deadmax").toInt();
        tankObj["deadmin"] = sqlQuery.value("deadmin").toInt();
        tankObj["tubeamount"] = sqlQuery.value("tubeamount").toInt();
        reservoirsArray.append(tankObj);
    }

    // 🔹 Формуємо фінальну відповідь
    QJsonObject response;
    response["reservoirs_info"] = reservoirsArray;

    return QHttpServerResponse("application/json", QJsonDocument(response).toJson());
}


/**
 * @brief Обробляє запит `/terminal_info`, виконує SQL-запит для отримання інформації про термінал
 * @param request HTTP-запит з параметрами `client_id` та `terminal_id`
 * @return JSON-відповідь з інформацією про термінал або повідомленням про помилку
 */
QHttpServerResponse Server::handleTerminalInfo(const QHttpServerRequest &request) {
    QUrlQuery query(request.query());
    qDebug() << "📥 Запит отримано: /terminal_info";

    if (!query.hasQueryItem("client_id") || !query.hasQueryItem("terminal_id")) {
        return QHttpServerResponse("application/json", R"({"error": "Missing parameters"})");
    }

    int clientId = query.queryItemValue("client_id").toInt();
    int terminalId = query.queryItemValue("terminal_id").toInt();

    // 🔹 Спочатку перевіряємо, чи є термінал у головній базі Palantir
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
        qWarning() << "⚠️ Помилка запиту до основної БД:" << sqlQuery.lastError().text();
        return QHttpServerResponse("application/json", R"({"error": "Database query failed"})");
    }

    // 🔹 Якщо термінал не знайдено в базі — повертаємо помилку
    if (!sqlQuery.next()) {
        qWarning() << "❌ Термінал не знайдено! client_id =" << clientId << ", terminal_id =" << terminalId;
        return QHttpServerResponse("application/json", R"({"error": "Terminal not found"})");
    }

    // 🔹 Формуємо базову відповідь із даними про АЗС
    QJsonObject response;
    response["client_name"] = sqlQuery.value("client_name").toString();
    response["terminal_id"] = sqlQuery.value("terminal_id").toInt();
    response["adress"] = sqlQuery.value("adress").toString();
    response["phone"] = sqlQuery.value("phone").toString();

    // 🔹 Отримуємо параметри підключення до БД клієнта
    auto clientDbParams = getClientDBParams(clientId);
    if (!clientDbParams.has_value()) {
        qWarning() << "⚠️ Не вдалося отримати параметри БД клієнта!";
        return QHttpServerResponse("application/json", R"({"error": "Failed to get client DB parameters"})");
    }

    // 🔹 Підключаємося до бази даних клієнта
    if (!connectToClientDatabase(clientDbParams.value())) {
        qWarning() << "⚠️ Помилка підключення до БД клієнта!";
        return QHttpServerResponse("application/json", R"({"error": "Failed to connect to client database"})");
    }

    QString connectionName = QString("clientDB_%1").arg(clientDbParams->server);
    QSqlDatabase clientDB = QSqlDatabase::database(connectionName);

    // 🔹 Отримуємо ТРК та пістолети
    QJsonArray dispensersInfo = getDispensersInfo(clientDB, terminalId);
    QJsonObject pumpsGroupedByDispenser = getPumpsInfo(clientDB, terminalId);

    // 🔹 Додаємо `pumps_info` у відповідні `dispenser_id`
    QJsonArray updatedDispensersInfo;
    for (const QJsonValue &dispenserVal : dispensersInfo) {
        QJsonObject dispenserObj = dispenserVal.toObject();
        int dispenserId = dispenserObj["dispenser_id"].toInt();

        if (pumpsGroupedByDispenser.contains(QString::number(dispenserId))) {
            dispenserObj["pumps_info"] = pumpsGroupedByDispenser[QString::number(dispenserId)];
        }

        updatedDispensersInfo.append(dispenserObj);
    }

    // 🔹 Додаємо дані про підключення до БД клієнта
    response["client_db_connection"] = "OK";
    response["dispensers_info"] = updatedDispensersInfo;

    return QHttpServerResponse("application/json", QJsonDocument(response).toJson());
}



/**
 * @brief Виконує SQL-запит для отримання інформації про каси.
 * @param clientDB Посилання на базу даних клієнта
 * @param terminalId ID терміналу
 * @return JSON-масив з інформацією про каси
 */
QHttpServerResponse Server::handlePosInfo(const QHttpServerRequest &request) {
    QUrlQuery query(request.query());  // Змінна для параметрів запиту
    qDebug() << "📥 Запит отримано: /pos_info";

    if (!query.hasQueryItem("client_id") || !query.hasQueryItem("terminal_id")) {
        return QHttpServerResponse("application/json", R"({"error": "Missing parameters"})");
    }

    int clientId = query.queryItemValue("client_id").toInt();
    int terminalId = query.queryItemValue("terminal_id").toInt();

    // 🔹 Отримуємо параметри підключення до БД клієнта
    auto clientDbParams = getClientDBParams(clientId);
    if (!clientDbParams.has_value()) {
        qWarning() << "⚠️ Не вдалося отримати параметри БД клієнта!";
        return QHttpServerResponse("application/json", R"({"error": "Failed to get client DB parameters"})");
    }

    // 🔹 Підключаємося до бази даних клієнта
    if (!connectToClientDatabase(clientDbParams.value())) {
        qWarning() << "⚠️ Помилка підключення до БД клієнта!";
        return QHttpServerResponse("application/json", R"({"error": "Failed to connect to client database"})");
    }

    QString connectionName = QString("clientDB_%1").arg(clientDbParams->server);
    QSqlDatabase clientDB = QSqlDatabase::database(connectionName);

    // 🔹 Виконуємо SQL-запит
    QSqlQuery sqlQuery(clientDB);  // ✅ Оновлена назва змінної
    QString sql = QString(R"(
        WITH RankedVersions AS (
            SELECT
                a.terminal_id,
                a.pos_id,
                a.pos_version,
                a.db_version,
                a.posterm_version,
                ROW_NUMBER() OVER (PARTITION BY a.terminal_id, a.pos_id ORDER BY a.build_date DESC) AS rn
            FROM APP_VERSION a
        )
        SELECT
            z.pos_id,
            z.factorynumber,
            z.regnumber,
            NULLIF(v.pos_version, '') AS pos_version,
            NULLIF(v.db_version, '') AS db_version,
            NULLIF(v.posterm_version, '') AS posterm_version
        FROM ZNUMBERS z
        LEFT JOIN RankedVersions v
            ON z.terminal_id = v.terminal_id
            AND z.pos_id = v.pos_id
            AND v.rn = 1
        WHERE z.terminal_id = %1
          AND z.shift_id = (
              SELECT MAX(shift_id)
              FROM SHIFTS
              WHERE terminal_id = %1
                AND isclose = 'T'
          )
        ORDER BY z.pos_id;
    )").arg(terminalId);

    if (!sqlQuery.exec(sql)) {
        qWarning() << "❌ Помилка виконання SQL-запиту:" << sqlQuery.lastError().text();
        return QHttpServerResponse("application/json", R"({"error": "Database query failed"})");
    }

    // 🔹 Формуємо JSON-відповідь
    QJsonArray posInfoArray;
    while (sqlQuery.next()) {
        QJsonObject posInfo;
        posInfo["pos_id"] = sqlQuery.value("pos_id").toInt();
        posInfo["factorynumber"] = sqlQuery.value("factorynumber").toString();
        posInfo["regnumber"] = sqlQuery.value("regnumber").toString();

        if (!sqlQuery.value("pos_version").isNull()) {
            posInfo["pos_version"] = sqlQuery.value("pos_version").toString();
        }
        if (!sqlQuery.value("db_version").isNull()) {
            posInfo["db_version"] = sqlQuery.value("db_version").toString();
        }
        if (!sqlQuery.value("posterm_version").isNull()) {
            posInfo["posterm_version"] = sqlQuery.value("posterm_version").toString();
        }

        posInfoArray.append(posInfo);
    }

    // 🔹 Формуємо фінальну відповідь
    QJsonObject response;
    response["pos_info"] = posInfoArray;

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
    if (!query.exec("SELECT client_id, client_name FROM clients_list WHERE isactive=1")) {
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

/**
 * @brief Підключається до бази даних клієнта з переданими параметрами
 * @param params Параметри підключення до БД клієнта
 * @return true, якщо підключення успішне, інакше false
 */
std::optional<QString> Server::connectToClientDatabase(const ClientDBParams &params) {
    QString connectionName = QString("clientDB_%1").arg(params.server);

    if (QSqlDatabase::contains(connectionName)) {
        QSqlDatabase existingDb = QSqlDatabase::database(connectionName);
        if (existingDb.isOpen()) {
            qDebug() << "🔸 Використовуємо вже відкрите підключення:" << connectionName;
            return connectionName;
        }
    }

    QSqlDatabase clientDB = QSqlDatabase::addDatabase("QIBASE", connectionName);
    clientDB.setHostName(params.server);
    clientDB.setPort(params.port);
    clientDB.setDatabaseName(params.database);
    clientDB.setUserName(params.username);
    clientDB.setPassword(params.password);

    if (!clientDB.open()) {
        qCritical() << "❌ Помилка підключення до бази клієнта:" << clientDB.lastError().text();
        return std::nullopt;
    }

    qInfo() << "✅ Успішне підключення до бази клієнта:" << connectionName;
    return connectionName;
}



QJsonArray Server::getDispensersInfo(QSqlDatabase &clientDB, int terminalId) {
    QJsonArray dispensers;

    // ?? Перевіряємо, що БД відкрита
    if (!clientDB.isOpen()) {
        qCritical() << "? База даних клієнта не відкрита!";
        return dispensers;
    }

    qDebug() << "?? Поточна база даних:" << clientDB.connectionName() << clientDB.databaseName();

    QSqlQuery query(clientDB);
    query.prepare(R"(
        SELECT d.dispenser_id, p.name, d.channelport, d.channelspeed, d.netaddress
        FROM dispensers d
        LEFT JOIN protocols p ON p.protocol_id = d.protocol_id
        WHERE d.terminal_id = :terminalId AND d.isactive = 'T'
          AND p.postype_id = (SELECT s.postype_id FROM POSS s WHERE s.terminal_id = :terminalId AND s.pos_id = 1)
    )");

    query.bindValue(":terminalId", terminalId);

    if (!query.exec()) {
        qWarning() << "?? Помилка запиту інформації по ТРК:" << query.lastError().text();
        qWarning() << "?? SQL-запит:" << query.lastQuery();
        return dispensers;
    }

    while (query.next()) {
        QJsonObject disp;
        disp["dispenser_id"] = query.value("dispenser_id").toInt();
        disp["protocol"] = query.value("name").toString();
        disp["port"] = query.value("channelport").toInt();
        disp["speed"] = query.value("channelspeed").toInt();
        disp["address"] = query.value("netaddress").toInt();
        dispensers.append(disp);
    }

    return dispensers;
}

QJsonObject Server::getPumpsInfo(QSqlDatabase &clientDB, int terminalId) {
    QJsonObject pumpsGroupedByDispenser;

    QString queryStr = QString(R"(
        SELECT t.dispenser_id, t.trk_id AS pump_id, t.tank_id, f.shortname
        FROM trks t
        LEFT JOIN tanks s ON s.tank_id = t.tank_id
        LEFT JOIN fuels f ON f.fuel_id = s.fuel_id
        WHERE t.terminal_id = %1
          AND s.terminal_id = %1
          AND t.isactive = 'T'
        ORDER BY t.dispenser_id, t.trk_id
    )").arg(terminalId);

    QSqlQuery query(clientDB);
    if (!query.exec(queryStr)) {
        qWarning() << "❌ Помилка запиту інформації по пістолетам:" << query.lastError().text();
        qWarning() << "❌ SQL-запит:" << queryStr;
        return pumpsGroupedByDispenser;
    }

    while (query.next()) {
        int dispenserId = query.value("dispenser_id").toInt();
        QJsonObject pump;
        pump["pump_id"] = query.value("pump_id").toInt();
        pump["tank_id"] = query.value("tank_id").toInt();
        pump["fuel_shortname"] = query.value("shortname").toString();

        // Додаємо пістолет до відповідного ТРК
        QJsonArray pumpsArray = pumpsGroupedByDispenser[QString::number(dispenserId)].toArray();
        pumpsArray.append(pump);
        pumpsGroupedByDispenser[QString::number(dispenserId)] = pumpsArray;
    }

    qDebug() << "✅ Отримано інформацію про пістолети, кількість записів:" << pumpsGroupedByDispenser.size();
    return pumpsGroupedByDispenser;
}

