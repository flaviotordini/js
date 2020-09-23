#ifndef YTJSNAMFACTORY_H
#define YTJSNAMFACTORY_H

#include <QtQml>

class JSDiskCache : public QNetworkDiskCache {
public:
    JSDiskCache(QObject *parent);
    void updateMetaData(const QNetworkCacheMetaData &meta);
    QIODevice *prepare(const QNetworkCacheMetaData &meta);

private:
    QNetworkCacheMetaData fixMetadata(const QNetworkCacheMetaData &meta);
};

class JSNAM : public QNetworkAccessManager {
    Q_OBJECT

public:
    JSNAM(QObject *parent);

protected:
    QNetworkReply *
    createRequest(Operation op, const QNetworkRequest &request, QIODevice *outgoingData);
};

class JSNAMFactory : public QQmlNetworkAccessManagerFactory {
public:
    QNetworkAccessManager *create(QObject *parent);
};

#endif // YTJSNAMFACTORY_H
