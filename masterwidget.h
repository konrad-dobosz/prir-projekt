#ifndef MASTERWIDGET_H
#define MASTERWIDGET_H

#include <QWidget>
#include <QTcpServer>
#include <QTcpSocket>
#include <QList>
#include <QMap>
#include <QTime>

namespace Ui {
class MasterWidget;
}

class MasterWidget : public QWidget
{
    Q_OBJECT

public:
    explicit MasterWidget(QWidget *parent = nullptr);
    ~MasterWidget();

private slots:
    void on_startServerButton_clicked();
    void on_stopServerButton_clicked();
    void on_distributeButton_clicked();

    void handleNewConnection();
    void handleClientDisconnected();
    void processResults();

private:
    Ui::MasterWidget *ui;

    // Network components
    QTcpServer *m_server;
    QList<QTcpSocket*> m_clients;
    QMap<QTcpSocket*, QString> m_clientAddresses;

    // Data
    QList<quint64> m_primes;
    bool m_serverRunning;

    void updateClientList();
    void updatePrimesList(quint64 prime);
    void log(const QString &message);
};

#endif // MASTERWIDGET_H
