#ifndef CRIPTPASS_H
#define CRIPTPASS_H

#include <QString>
#include <QByteArray>

class Config;  // forward declaration

class CriptPass {
public:
    CriptPass();

    QString encryptPassword(const QString& plainText);
    QString decryptPassword(const QString& encryptedText);

    QString cryptVNCPass(const QString& termID, const QString& pass);
    QString decryptVNCPass(const QString& pass);

private:
    QByteArray hashKey;
    QByteArray hashIV;
};

#endif // CRIPTPASS_H
