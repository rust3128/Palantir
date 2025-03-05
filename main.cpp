#include <QCoreApplication>
#include <QDebug>
#include "config.h"

int main(int argc, char *argv[]) {
    QCoreApplication a(argc, argv);

    // 🔹 Перевіряємо, чи передано аргумент `--config`
    bool manualConfig = false;
    for (int i = 1; i < argc; ++i) {
        QString arg = argv[i];
        if (arg == "--config") {
            manualConfig = true;
            qDebug() << "🔧 Manual configuration mode enabled!";
        }
    }

    // 🔹 Створюємо об'єкт конфігурації
    Config config(nullptr, manualConfig);

    // 🔹 Виводимо параметри для перевірки
    qDebug() << "Database settings:";
    qDebug() << "Host:" << config.getDatabaseHost();
    qDebug() << "Port:" << config.getDatabasePort();
    qDebug() << "Database:" << config.getDatabaseName();
    qDebug() << "User:" << config.getDatabaseUser();
    qDebug() << "Password:" << config.getDatabasePassword();

    qDebug() << "Server settings:";
    qDebug() << "Server Port:" << config.getServerPort();
    qDebug() << "Log Level:" << config.getLogLevel();

    return a.exec();
}
