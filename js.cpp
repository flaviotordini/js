#include "js.h"

#include "jsnamfactory.h"

#include "cachedhttp.h"

JS &JS::instance() {
    static thread_local JS i;
    return i;
}

Http &JS::cachedHttp() {
    static Http *h = [] {
        CachedHttp *cachedHttp = new CachedHttp(Http::instance(), "js");
        cachedHttp->setMaxSeconds(3600 * 6);
        // Avoid expiring the cached js
        cachedHttp->setMaxSize(0);

        cachedHttp->getValidators().insert("application/javascript", [](const auto &reply) -> bool {
            return !reply.body().isEmpty();
        });

        return cachedHttp;
    }();
    return *h;
}

JS::JS(QObject *parent) : QObject(parent), engine(nullptr) {}

void JS::initialize(const QUrl &url) {
    this->url = url;
    initialize();
}

bool JS::checkError(const QJSValue &value) {
    if (value.isError()) {
        qWarning() << "Error" << value.toString();
        qDebug() << value.property("stack").toString().splitRef('\n');
        return true;
    }
    return false;
}

bool JS::isInitialized() {
    if (ready) return true;
    initialize();
    return false;
}

JSResult &JS::callFunction(JSResult *result, const QString &name, const QJSValueList &args) {
    if (!isInitialized()) {
        qDebug() << "Not initialized";
        QTimer::singleShot(1000, this,
                           [this, result, name, args] { callFunction(result, name, args); });
        return *result;
    }

    auto function = engine->evaluate(name);
    if (!function.isCallable()) {
        qWarning() << function.toString() << " is not callable";
        QTimer::singleShot(0, result, [result, function] { result->setError(function); });
        return *result;
    }

    auto args2 = args;
    args2.prepend(engine->newQObject(result));
    qDebug() << "Calling" << function.toString();
    auto v = function.call(args2);
    if (checkError(v)) QTimer::singleShot(0, result, [result, v] { result->setError(v); });

    return *result;
}

void JS::initialize() {
    if (url.isEmpty()) {
        qDebug() << "No js url set";
        return;
    }

    if (initializing) return;
    initializing = true;
    qDebug() << "Initializing";

    if (engine) engine->deleteLater();
    engine = new QQmlEngine(this);
    engine->setNetworkAccessManagerFactory(new JSNAMFactory);
    engine->globalObject().setProperty("global", engine->globalObject());

    QJSValue timer = engine->newQObject(new JSTimer(engine));
    engine->globalObject().setProperty("clearTimeout", timer.property("clearTimeout"));
    engine->globalObject().setProperty("setTimeout", timer.property("setTimeout"));

    connect(cachedHttp().get(url), &HttpReply::finished, this, [this](auto &reply) {
        if (!reply.isSuccessful()) {
            emit initFailed("Cannot load JS");
            qDebug() << "Cannot load JS";
            initializing = false;
            return;
        }
        auto value = engine->evaluate(reply.body());
        if (!checkError(value)) {
            qDebug() << "Initialized";
            ready = true;
            emit initialized();
        }
        initializing = false;
    });
}
