#ifndef SLAVEWIDGET_H
#define SLAVEWIDGET_H

#include <QWidget>
#include <QTcpSocket>
#include <QThreadPool>
#include <QTime>
#include <QMessageBox>

namespace Ui {
class SlaveWidget;
}

class SlaveWidget : public QWidget
{
    Q_OBJECT

public:
    explicit SlaveWidget(QWidget *parent = nullptr);
    ~SlaveWidget();

private slots:
    void on_connectButton_clicked();
    void on_disconnectButton_clicked();

    void handleData();
    void handleError(QAbstractSocket::SocketError error);
    void handleConnected();
    void handleDisconnected();

    // Sloty dla obliczeń
    void updateProgress(int percent);
    void primeFound(quint64 prime);
    void calculationFinished(const QList<quint64> &primes);

private:
    Ui::SlaveWidget *ui;

    // Network components
    QTcpSocket *m_socket;

    // Calculation components
    QThreadPool *m_threadPool;
    QList<quint64> m_primes;
    volatile bool m_stopped;

    void startCalculation(quint64 start, quint64 end);
    void log(const QString &message);
};

#endif // SLAVEWIDGET_H
