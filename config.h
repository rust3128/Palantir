#ifndef CONFIG_H
#define CONFIG_H

#include <QObject>
#include <QSettings>


enum LogLevel {
    Debug,
    Info,
    Warning,
    Error
};

/**
 * @brief Клас для роботи з файлом конфігурації `config.ini`
 */
class Config : public QObject {
    Q_OBJECT
public:
    explicit Config(QObject *parent = nullptr, bool manualConfig = false);

    // Методи для отримання параметрів
    QString getDatabaseHost() const;
    int getDatabasePort() const;
    QString getDatabaseName() const;
    QString getDatabaseUser() const;
    QString getDatabasePassword() const;

    int getServerPort() const;
    QString getLogLevel() const;
    LogLevel getLogLevelEnum() const;  // 🔹 Додаємо метод для переведення `log_level` у enum
    static void initLogging(LogLevel logLevel);  // 🔹 Оновлюємо `initLogging()`, щоб підтримувати `log_level`
private:
    QSettings *settings;  // Об'єкт для роботи з `config.ini`
    void createDefaultConfig(const QString &configPath);  // Метод створення `config.ini`, якщо його немає
    void manualConfiguration(const QString &configPath);  // 🔹 Додаємо ручне введення налаштувань

    // 🔹 Методи для шифрування та дешифрування
    static QString encryptPassword(const QString &password);
    static QString decryptPassword(const QString &encoded);

    static void messageHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg);  // 🔹 Додаємо статичну функцію
    static LogLevel currentLogLevel;  // 🔹 Поточний рівень логування
};

#endif // CONFIG_H
