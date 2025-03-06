#include <QCoreApplication>
#include <QDebug>
#include "config.h"
#include "Server/server.h"

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

    // 🔹 Ініціалізуємо логування у файл
    Config::initLogging(config.getLogLevelEnum());  // 🔹 Викликаємо ініціалізацію логування

    // 🔹 Створюємо та запускаємо сервер
    Server server(&config);
    server.start();

    return a.exec();
}
