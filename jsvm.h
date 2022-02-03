#ifndef JSVM_H
#define JSVM_H

#include <QtCore>
#include <QtQml>

class JSVM : public QObject {
    Q_OBJECT
public:
    explicit JSVM(QQmlEngine *parent);

    Q_INVOKABLE QJSValue runInContext(QString code, QJSValue props);

private:
    QQmlEngine *parentEngine;
};

#endif // JSVM_H
