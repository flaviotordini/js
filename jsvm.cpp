#include "jsvm.h"

JSVM::JSVM(QQmlEngine *parent) : QObject{parent}, parentEngine(parent) {}

QJSValue JSVM::runInContext(QString code, QJSValue props) {
    auto engine = QQmlEngine(this);

    auto objectKeysFunction = parentEngine->evaluate("Object.keys");
    auto keys = objectKeysFunction.call({props});
    const int keyLength = keys.property("length").toInt();
    for (int i = 0; i < keyLength; ++i) {
        auto key = keys.property(i).toString();
        auto value = props.property(key).toString();
        qDebug() << "Setting property" << key << value;
        engine.globalObject().setProperty(key, value);
    }

    auto res = engine.evaluate(code).toString();
    return parentEngine->toScriptValue(res);
}
