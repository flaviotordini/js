#include "jsnamfactory.h"

class JSDiskCache : public QNetworkDiskCache {
public:
    JSDiskCache(QObject *parent) : QNetworkDiskCache(parent) {}
    void updateMetaData(const QNetworkCacheMetaData &meta) {
        qDebug() << "updateMetaData";
        QNetworkDiskCache::updateMetaData(fixMetadata(meta));
    }
    QIODevice *prepare(const QNetworkCacheMetaData &meta) {
        qDebug() << "prepare";
        return QNetworkDiskCache::prepare(fixMetadata(meta));
    }
    QNetworkCacheMetaData metaData(const QUrl &url) {
        qDebug() << "metaData" << url;
        // return QNetworkDiskCache::metaData(url);
        return fixMetadata(QNetworkDiskCache::metaData(url));
    }
    void insert(QIODevice *device) {
        qDebug() << "Caching";
        QNetworkDiskCache::insert(device);
    }

private:
    static QNetworkCacheMetaData fixMetadata(const QNetworkCacheMetaData &meta) {
        QNetworkCacheMetaData meta2 = meta;

        auto now = QDateTime::currentDateTimeUtc();
        if (meta2.expirationDate() < now) {
            meta2.setExpirationDate(now.addSecs(86400));
        }

        meta2.setSaveToDisk(true);

        // Remove caching headers
        static const QList<QByteArray> headersToRemove{"cache-control", "expires", "etag", "pragma",
                                                       "vary"};
        QNetworkCacheMetaData::RawHeaderList headers;
        for (const auto &h : meta2.rawHeaders()) {
            auto headerName = h.first.isLower() ? h.first : h.first.toLower();
            if (!headersToRemove.contains(headerName))
                headers << h;
            else
                qDebug() << "Removing" << h.first << h.second;
        }
        headers.append(QNetworkCacheMetaData::RawHeader{"cache-control", "max-age=86400"});
        meta2.setRawHeaders(headers);
        qDebug() << "Fixed headers" << meta2.rawHeaders();
        return meta2;
    }
};

JSNAM::JSNAM(QObject *parent, const JSNAMFactory &factory)
    : QNetworkAccessManager(parent), factory(factory) {
    auto cache = new JSDiskCache(this);
    cache->setCacheDirectory(QStandardPaths::writableLocation(QStandardPaths::CacheLocation) +
                             "/js");
    cache->setMaximumCacheSize(1024 * 1024 * 10);
    qDebug() << "Setting cache" << cache;
    setCache(cache);

    setRedirectPolicy(QNetworkRequest::NoLessSafeRedirectPolicy);
    setTransferTimeout(10000);
}

QNetworkReply *JSNAM::createRequest(QNetworkAccessManager::Operation op,
                                    const QNetworkRequest &request,
                                    QIODevice *outgoingData) {
    qDebug() << op << request.url();
    auto req2 = request;
    req2.setAttribute(QNetworkRequest::CacheLoadControlAttribute, QNetworkRequest::PreferCache);

    auto end = factory.getRequestHeaders().cend();
    for (auto i = factory.getRequestHeaders().cbegin(); i != end; ++i) {
        if (!req2.hasRawHeader(i.key()))
            req2.setRawHeader(i.key(), i.value());
        // else
        // qDebug() << "Not setting factory header" << i.key() << req2.rawHeader(i.key()) << "to
        // value" << i.value();
    }

    auto reply = QNetworkAccessManager::createRequest(op, req2, outgoingData);

#ifndef QT_NO_DEBUG_OUTPUT
    connect(reply, &QNetworkReply::finished, this, [reply] {
        bool fromCache = reply->attribute(QNetworkRequest::SourceIsFromCacheAttribute).toBool();
        qDebug() << (fromCache ? "HIT" : "MISS")
                 << reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toUInt()
                 << reply->url() << reply->rawHeaderPairs();
    });
    connect(reply, &QNetworkReply::redirectAllowed, this,
            [reply] { qDebug() << "redirectAllowed" << reply->url(); });
#endif

    return reply;
}

QNetworkAccessManager *JSNAMFactory::create(QObject *parent) {
    auto jsnam = new JSNAM(parent, *this);
    qDebug() << "Created" << jsnam;
    return jsnam;
}
