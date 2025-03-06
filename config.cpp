#include "config.h"
#include <QFile>
#include <QFileInfo>
#include <QTextStream>
#include <QDir>
#include <QDebug>
#include <iostream>
#include <QCoreApplication>

using namespace std;

// 🔹 Файл логування
static QFile logFile;
LogLevel Config::currentLogLevel = Debug;  // 🔹 За замовчуванням `Debug`
/**
 * @brief Конструктор класу Config
 * @param parent Батьківський QObject
 * @param manualConfig Якщо true – користувач вводить налаштування вручну
 */
Config::Config(QObject *parent, bool manualConfig) : QObject(parent) {
    QString configPath = QCoreApplication::applicationDirPath() + "/config/config.ini";

    if (!QFile::exists(configPath) || manualConfig) {
        if (manualConfig) {
            qWarning() << "Manual config settings...";
        } else {
            qWarning() << "Config settings file NOT FOUND!";
        }
        manualConfiguration(configPath);  // 🔹 Викликаємо ручне введення налаштувань
    }

    QFile file(configPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qCritical() << "Can't open `config.ini` for read!";
        return;
    }
    file.close();

    qDebug() << "Configuration from:" << QFileInfo(configPath).absoluteFilePath();
    settings = new QSettings(configPath, QSettings::IniFormat, this);


}

/**
 * @brief Створює стандартний `config.ini`, якщо його немає
 * @param configPath Шлях до файлу конфігурації
 */
void Config::createDefaultConfig(const QString &configPath) {
    QFile file(configPath);
    QDir configDir(QFileInfo(configPath).absolutePath());
    if (!configDir.exists()) {
        configDir.mkpath(".");
    }

    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qCritical() << "Can't create `config.ini`!";
        return;
    }

    QTextStream out(&file);
    out << "[Database]\n";
    out << "host=localhost\n";
    out << "port=3050\n";
    out << "database=D:/Develop/Database/GANDALF.GDB\n";
    out << "user=SYSDBA\n";
    out << "password=masterkey\n\n";

    out << "[Server]\n";
    out << "port=8181\n";
    out << "log_level=debug\n";

    file.close();
    qDebug() << "Default created`config.ini`";
}

/**
 * @brief Дозволяє користувачеві ввести параметри вручну та записує їх у `config.ini`
 * @param configPath Шлях до файлу конфігурації
 */
void Config::manualConfiguration(const QString &configPath) {
    QFile file(configPath);
    QDir configDir(QFileInfo(configPath).absolutePath());
    if (!configDir.exists()) {
        configDir.mkpath(".");
    }

    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qCritical() << "Failed to create `config.ini`!";
        return;
    }

    QTextStream out(&file);
    cout << "Enter configuration settings:\n";

    // 🔹 Database settings input
    cout << "Enter database host (default: localhost): ";
    string dbHost;
    getline(cin, dbHost);
    out << "[Database]\n";
    out << "host=" << (dbHost.empty() ? "localhost" : QString::fromStdString(dbHost)) << "\n";

    cout << "Enter database port (default: 3050): ";
    string dbPort;
    getline(cin, dbPort);
    out << "port=" << (dbPort.empty() ? "3050" : QString::fromStdString(dbPort)) << "\n";

    cout << "Enter database path (default: D:/Develop/Database/GANDALF.GDB): ";
    string dbName;
    getline(cin, dbName);
    out << "database=" << (dbName.empty() ? "D:/Develop/Database/GANDALF.GDB" : QString::fromStdString(dbName)) << "\n";

    cout << "Enter database username (default: SYSDBA): ";
    string dbUser;
    getline(cin, dbUser);
    out << "user=" << (dbUser.empty() ? "SYSDBA" : QString::fromStdString(dbUser)) << "\n";

    cout << "Enter database password (default: masterkey): ";
    string dbPassword;
    getline(cin, dbPassword);
    out << "password=" << encryptPassword(dbPassword.empty() ? "masterkey" : QString::fromStdString(dbPassword)) << "\n\n";

    // 🔹 Server settings input
    out << "[Server]\n";
    cout << "Enter server port (default: 8181): ";
    string serverPort;
    getline(cin, serverPort);
    out << "port=" << (serverPort.empty() ? "8181" : QString::fromStdString(serverPort)) << "\n";

    cout << "Enter log level (debug/info/warning/error) (default: debug): ";
    string logLevel;
    getline(cin, logLevel);
    out << "log_level=" << (logLevel.empty() ? "debug" : QString::fromStdString(logLevel)) << "\n";

    file.close();
    qDebug() << "config.ini` was created manually!";
}

/**
 * @brief Використовує XOR + Base64 для шифрування пароля
 * @param password Пароль у відкритому вигляді
 * @return Зашифрований пароль у Base64
 */
QString Config::encryptPassword(const QString &password) {
    QByteArray key = "PalantirSecureKey";  // 🔹 Ключ XOR (має бути завжди однаковий)
    QByteArray data = password.toUtf8();

    for (int i = 0; i < data.size(); ++i) {
        data[i] = data[i] ^ key[i % key.size()];  // 🔹 XOR з ключем
    }

    return data.toBase64();  // 🔹 Кодуємо у Base64
}

/**
 * @brief Використовує XOR + Base64 для дешифрування пароля
 * @param encoded Зашифрований пароль у Base64
 * @return Розшифрований пароль
 */
QString Config::decryptPassword(const QString &encoded) {
    QByteArray key = "PalantirSecureKey";
    QByteArray data = QByteArray::fromBase64(encoded.toUtf8());

    for (int i = 0; i < data.size(); ++i) {
        data[i] = data[i] ^ key[i % key.size()];
    }

    return QString::fromUtf8(data);
}

// 🔹 Методи для отримання параметрів конфігурації

QString Config::getDatabaseHost() const {
    return settings->value("Database/host", "localhost").toString();
}

int Config::getDatabasePort() const {
    return settings->value("Database/port", 3050).toInt();
}

QString Config::getDatabaseName() const {
    return settings->value("Database/database", "").toString();
}

QString Config::getDatabaseUser() const {
    return settings->value("Database/user", "SYSDBA").toString();
}

/**
 * @brief Отримує пароль бази даних (розшифровує його)
 * @return Розшифрований пароль
 */
QString Config::getDatabasePassword() const {
    QString encoded = settings->value("Database/password", "").toString();
    return encoded.isEmpty() ? "" : decryptPassword(encoded);
}

int Config::getServerPort() const {
    return settings->value("Server/port", 8181).toInt();
}

QString Config::getLogLevel() const {
    return settings->value("Server/log_level", "debug").toString();
}

LogLevel Config::getLogLevelEnum() const
{
    QString level = getLogLevel().toLower();
    if (level == "info") return Info;
    if (level == "warning") return Warning;
    if (level == "error") return Error;
    return Debug;
}

/**
 * @brief Ініціалізує логування у файл
 */
void Config::initLogging(LogLevel logLevel) {
    QString logDirPath = QCoreApplication::applicationDirPath() + "/logs";
    QDir logDir(logDirPath);
    if (!logDir.exists()) {
        logDir.mkpath(".");
    }

    QString logFilePath = logDirPath + "/palantir.log";
    logFile.setFileName(logFilePath);
    if (!logFile.open(QIODevice::Append | QIODevice::Text)) {
        qCritical() << "Failed to open log file for writing!";
        return;
    }

    // 🔹 Зберігаємо поточний рівень логування
    currentLogLevel = logLevel;

    // 🔹 Встановлюємо глобальний обробник повідомлень
    qInstallMessageHandler(messageHandler);

    qDebug() << "Logging initialized. Log level:" << logLevel;
}

void Config::messageHandler(QtMsgType type, const QMessageLogContext &, const QString &msg) {
    QString logEntry = QString("[%1] %2\n")
    .arg(QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss"))
        .arg(msg);

    // 🔹 Фільтруємо логи за рівнем `log_level`
    bool shouldLog = false;
    switch (type) {
    case QtDebugMsg:
        shouldLog = (currentLogLevel <= Debug);
        break;
    case QtInfoMsg:
        shouldLog = (currentLogLevel <= Info);
        break;
    case QtWarningMsg:
        shouldLog = (currentLogLevel <= Warning);
        break;
    case QtCriticalMsg:
    case QtFatalMsg:
        shouldLog = (currentLogLevel <= Error);
        break;
    }

    if (shouldLog) {
        QTextStream logStream(&logFile);
        logStream << logEntry;
        logStream.flush();
        std::cout << logEntry.toStdString();
    }
}

