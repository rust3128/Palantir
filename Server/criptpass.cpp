#include "criptpass.h"
#include "qaesencryption.h"
#include <QCryptographicHash>

CriptPass::CriptPass() {
    const QString key = "SapForever";  // Винеси в конфігурацію
    const QString iv = "Poltava1970Rust";

    hashKey = QCryptographicHash::hash(key.toUtf8(), QCryptographicHash::Sha256);
    hashIV = QCryptographicHash::hash(iv.toUtf8(), QCryptographicHash::Md5);
}

QString CriptPass::encryptPassword(const QString &plainText) {
    QAESEncryption encryption(QAESEncryption::AES_256, QAESEncryption::CBC);
    QByteArray encoded = encryption.encode(plainText.toUtf8(), hashKey, hashIV);
    return QString(encoded.toBase64());
}

QString CriptPass::decryptPassword(const QString &encryptedBase64) {
    QAESEncryption encryption(QAESEncryption::AES_256, QAESEncryption::CBC);
    QByteArray decoded = encryption.decode(QByteArray::fromBase64(encryptedBase64.toUtf8()), hashKey, hashIV);
    return QString(encryption.removePadding(decoded));
}

QString CriptPass::cryptVNCPass(const QString &termID, const QString &pass) {
    QString term = termID.rightJustified(5, '0');
    QString combined = term.mid(3) + pass + term.left(2);
    return encryptPassword(combined);
}

QString CriptPass::decryptVNCPass(const QString &pass) {
    QString decrypted = decryptPassword(pass);
    return decrypted.mid(3, decrypted.length() - 5);
}

