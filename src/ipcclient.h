#pragma once

#include <QObject>
#include <QLocalSocket>
#include <QSocketNotifier>
#include <QJsonDocument>

class IPCClient : public QObject
{
    Q_OBJECT

public:
    explicit IPCClient(QObject *parent = nullptr);
    ~IPCClient();

    bool connect(QString *errorOut = nullptr);
    bool isConnected() const;
    bool sendRequest(const QJsonObject &request, QString *errorOut = nullptr);

signals:
    void connected();
    void disconnected();
    void errorOccurred(const QString &error);
    void eventReceived(const QJsonObject &event);

private slots:
    void onReadyRead();
    void onSocketError();

private:
    QLocalSocket *m_eventSocket = nullptr;
    QLocalSocket *m_requestSocket = nullptr;
    QByteArray m_readBuffer;
    QString m_socketPath;
};
