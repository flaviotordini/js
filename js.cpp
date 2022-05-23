#include "js.h"

#include "cachedhttp.h"
#include "jsvm.h"

namespace {
Http &cachedHttp() {
    static Http *h = [] {
        CachedHttp *cachedHttp = new CachedHttp(Http::instance(), "js");
        cachedHttp->setMaxSeconds(3600);
        // Avoid expiring the cached js
        cachedHttp->setMaxSize(0);

        cachedHttp->getValidators().insert("application/javascript", [](const auto &reply) -> bool {
            return !reply.body().isEmpty();
        });

        return cachedHttp;
    }();
    return *h;
}
} // namespace

JS &JS::instance() {
    static thread_local JS i;
    return i;
}

JS::JS(QObject *parent) : QObject(parent), engine(nullptr) {}

void JS::initialize(const QUrl &url) {
    this->url = url;
    initialize();
}

bool JS::checkError(const QJSValue &value) {
    if (value.isError()) {
        qWarning() << "Error" << value.toString();
        qDebug() << value.property("stack").toString().split('\n');
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

void JS::resetNAM() {
    class MyCookieJar : public QNetworkCookieJar {
        bool insertCookie(const QNetworkCookie &cookie) {
            if (cookie.name().contains("CONSENT")) {
                qDebug() << "Fixing CONSENT cookie" << cookie;
                auto cookie2 = cookie;
                cookie2.setValue(cookie.value().replace("PENDING", "YES"));
                return QNetworkCookieJar::insertCookie(cookie2);
            }
            return QNetworkCookieJar::insertCookie(cookie);
        }
    };

    auto nam = getEngine().networkAccessManager();
    nam->clearAccessCache();
    nam->setCookieJar(new MyCookieJar());
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
    engine->setNetworkAccessManagerFactory(&namFactory);
    engine->globalObject().setProperty("global", engine->globalObject());

    QJSValue timer = engine->newQObject(new JSTimer(engine));
    engine->globalObject().setProperty("setTimeoutQt", timer.property("setTimeout"));
    QJSValue setTimeoutWrapperFunction =
            engine->evaluate("function setTimeout(cb, delay) {"
                             "const args = Array.prototype.slice.call(arguments, 2);"
                             "return setTimeoutQt(cb, delay, args);"
                             "}");
    checkError(setTimeoutWrapperFunction);
    engine->globalObject().setProperty("clearTimeout", timer.property("clearTimeout"));

    QJSValue vm = engine->newQObject(new JSVM(engine));
    engine->globalObject().setProperty("runInContextQt", vm.property("runInContext"));

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
            resetNAM();
            ready = true;
            emit initialized();
        }
        initializing = false;
    });
}
