#include "jsnamfactory.h"

JSDiskCache::JSDiskCache(QObject *parent) : QNetworkDiskCache(parent) {}

void JSDiskCache::updateMetaData(const QNetworkCacheMetaData &meta) {
    auto meta2 = fixMetadata(meta);
    QNetworkDiskCache::updateMetaData(meta2);
}

QIODevice *JSDiskCache::prepare(const QNetworkCacheMetaData &meta) {
    auto meta2 = fixMetadata(meta);
    return QNetworkDiskCache::prepare(meta2);
}

QNetworkCacheMetaData JSDiskCache::fixMetadata(const QNetworkCacheMetaData &meta) {
    QNetworkCacheMetaData meta2 = meta;

    auto now = QDateTime::currentDateTimeUtc();
    if (meta2.expirationDate() < now) {
        meta2.setExpirationDate(now.addSecs(3600));
    }

    // Remove caching headers
    auto headers = meta2.rawHeaders();
    for (auto i = headers.begin(); i != headers.end(); ++i) {
        // qDebug() << i->first << i->second;
        static const QList<QByteArray> headersToRemove{"cache-control", "expires", "pragma",
                                                       "Cache-Control", "Expires", "Pragma"};
        if (headersToRemove.contains(i->first)) {
            qDebug() << "Removing" << i->first << i->second;
            headers.erase(i);
        }
    }
    meta2.setRawHeaders(headers);

    return meta2;
}

JSNAM::JSNAM(QObject *parent, const JSNAMFactory &factory)
    : QNetworkAccessManager(parent), factory(factory) {
    auto cache = new JSDiskCache(this);
    cache->setCacheDirectory(QStandardPaths::writableLocation(QStandardPaths::CacheLocation) +
                             "/js");
    cache->setMaximumCacheSize(1024 * 1024 * 10);
    setCache(cache);
    setRedirectPolicy(QNetworkRequest::NoLessSafeRedirectPolicy);
#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
    setTransferTimeout(10000);
#endif
}

QNetworkReply *JSNAM::createRequest(QNetworkAccessManager::Operation op,
                                    const QNetworkRequest &request,
                                    QIODevice *outgoingData) {
    auto req2 = request;
    req2.setAttribute(QNetworkRequest::CacheLoadControlAttribute, QNetworkRequest::PreferCache);

    auto end = factory.getRequestHeaders().cend();
    for (auto i = factory.getRequestHeaders().cbegin(); i != end; ++i) {
        if (!req2.hasRawHeader(i.key()))
            req2.setRawHeader(i.key(), i.value());
        else
            qDebug() << "Request for" << req2.url() << "already contains header" << i.key()
                     << req2.rawHeader(i.key());
    }

#ifndef QT_NO_DEBUG_OUTPUT
    qDebug() << req2.url();
    // for (const auto &h : req2.rawHeaderList())
    //     qDebug() << h << req2.rawHeader(h);
#endif

    auto reply = QNetworkAccessManager::createRequest(op, req2, outgoingData);

#ifndef QT_NO_DEBUG_OUTPUT
    connect(reply, &QNetworkReply::finished, this, [reply] {
        qDebug() << "finished"
                 << reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toUInt()
                 << reply->url() << reply->rawHeaderPairs();
    });
    connect(reply, &QNetworkReply::redirectAllowed, this,
            [reply] { qDebug() << "redirectAllowed" << reply->url(); });
#endif

    return reply;
}

QNetworkAccessManager *JSNAMFactory::create(QObject *parent) {
    qDebug() << "Creating NAM";
    return new JSNAM(parent, *this);
}
