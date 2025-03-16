#include <QCoreApplication>
#include <QDebug>
#include "config.h"
#include "Server/server.h"


int main(int argc, char *argv[]) {
    QCoreApplication a(argc, argv);

    qInfo() << "✅ Palantir запущено! Перевіряємо параметри запуску...";

    Config config;
    Config::initLogging(config.getLogLevelEnum());

    qInfo() << "✅ Запуск у звичайному режимі...";
    Server server(&config);
    server.start();

    return a.exec(); // 🔹 Головний цикл обробки подій Qt
}
