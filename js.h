#ifndef JS_H
#define JS_H

#include <QtQml>

#include "jsresult.h"

class JSTimer : public QTimer {
    Q_OBJECT

public:
    static auto &getTimers() {
        static QHash<QString, JSTimer *> timers;
        return timers;
    }
    // This should be static but cannot bind static functions to QJSEngine
    Q_INVOKABLE QJSValue clearTimeout(QJSValue id) {
        // qDebug() << id.toString();
        auto timer = getTimers().take(id.toString());
        if (timer) {
            timer->stop();
            timer->deleteLater();
        }
        return QJSValue();
    }
    // This should be static but cannot bind static functions to QJSEngine
    Q_INVOKABLE QJSValue setTimeout(QJSValue callback, QJSValue delayTime) {
        // qDebug() << callback.toString() << delayTime.toInt();
        auto timer = new JSTimer();
        timer->setInterval(delayTime.toInt());
        connect(timer, &JSTimer::timeout, this, [callback]() mutable {
            qDebug() << "Calling" << callback.toString();
            if (!callback.isCallable()) qDebug() << callback.toString() << "is not callable";
            auto value = callback.call();
            if (value.isError()) {
                qWarning() << "Error" << value.toString();
                qDebug() << value.property("stack").toString().splitRef('\n');
            }
        });
        timer->start();
        return timer->hashString();
    }

    Q_INVOKABLE JSTimer(QObject *parent = nullptr) : QTimer(parent) {
        setTimerType(Qt::CoarseTimer);
        setSingleShot(true);
        const auto hash = hashString();
        connect(this, &JSTimer::destroyed, this, [hash] { getTimers().remove(hash); });
        connect(this, &JSTimer::timeout, this, &QTimer::deleteLater);
        getTimers().insert(hash, this);
    }

    QString hashString() { return QString::number((std::uintptr_t)(this)); }

private:
};

class JS : public QObject {
    Q_OBJECT

public:
    static JS &instance();

    explicit JS(QObject *parent = nullptr);
    void initialize(const QUrl &url);
    bool checkError(const QJSValue &value);

    bool isInitialized();
    QQmlEngine &getEngine() { return *engine; }

    JSResult &callFunction(JSResult *result, const QString &name, const QJSValueList &args);

signals:
    void initialized();
    void initFailed(QString message);

private:
    void initialize();

    QQmlEngine *engine;
    bool initializing = false;
    bool ready = false;
    QUrl url;
};

#endif // JS_H
