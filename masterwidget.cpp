#include "masterwidget.h"
#include "ui_masterwidget.h"
#include <QMessageBox>
#include <QDataStream>

MasterWidget::MasterWidget(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::MasterWidget),
    m_serverRunning(false)
{
    ui->setupUi(this);

    // Inicjalizacja komponentów sieciowych
    m_server = new QTcpServer(this);
    connect(m_server, &QTcpServer::newConnection, this, &MasterWidget::handleNewConnection);
}

MasterWidget::~MasterWidget()
{
    // Zatrzymanie serwera i zamknięcie połączeń
    if (m_serverRunning) {
        on_stopServerButton_clicked();
    }

    delete ui;
}

void MasterWidget::on_startServerButton_clicked()
{
    int port = ui->portSpinBox->value();

    if (!m_server->listen(QHostAddress::Any, port)) {
        QMessageBox::critical(this, "Error", "Could not start server: " + m_server->errorString());
        return;
    }

    m_serverRunning = true;
    ui->startServerButton->setEnabled(false);
    ui->stopServerButton->setEnabled(true);
    ui->distributeButton->setEnabled(true);
    ui->portSpinBox->setEnabled(false);

    log(QString("Server started on port %1").arg(port));
    ui->statusLabel->setText(QString("Server running on port %1").arg(port));
}

void MasterWidget::on_stopServerButton_clicked()
{
    // Zamknięcie wszystkich połączeń klienckich
    for (QTcpSocket *socket : m_clients) {
        socket->disconnectFromHost();
    }

    m_clients.clear();
    m_clientAddresses.clear();
    ui->clientsListWidget->clear();

    // Zatrzymanie serwera
    m_server->close();
    m_serverRunning = false;

    // Aktualizacja UI
    ui->startServerButton->setEnabled(true);
    ui->stopServerButton->setEnabled(false);
    ui->distributeButton->setEnabled(false);
    ui->portSpinBox->setEnabled(true);

    log("Server stopped");
    ui->statusLabel->setText("Server not running");
}

void MasterWidget::on_distributeButton_clicked()
{
    if (m_clients.isEmpty()) {
        QMessageBox::warning(this, "Warning", "No connected slaves to distribute work");
        return;
    }

    bool ok;
    quint64 rangeStart = ui->rangeStartEdit->text().toULongLong(&ok);
    if (!ok) {
        QMessageBox::warning(this, "Warning", "Invalid range start value");
        return;
    }

    quint64 rangeEnd = ui->rangeEndEdit->text().toULongLong(&ok);
    if (!ok) {
        QMessageBox::warning(this, "Warning", "Invalid range end value");
        return;
    }

    if (rangeStart >= rangeEnd) {
        QMessageBox::warning(this, "Warning", "Range start must be less than range end");
        return;
    }

    // Obliczanie zakresu dla każdego slave'a
    quint64 totalRange = rangeEnd - rangeStart + 1;
    quint64 rangePerClient = totalRange / m_clients.size();

    log(QString("Distributing work range [%1-%2] to %3 slaves")
            .arg(rangeStart).arg(rangeEnd).arg(m_clients.size()));

    // Czyszczenie listy znalezionych liczb pierwszych
    m_primes.clear();
    ui->primesListWidget->clear();

    for (int i = 0; i < m_clients.size(); i++) {
        QTcpSocket *client = m_clients[i];

        quint64 start = rangeStart + i * rangePerClient;
        quint64 end = (i == m_clients.size() - 1) ? rangeEnd : start + rangePerClient - 1;

        // Tworzenie pakietu danych
        QByteArray data;
        QDataStream stream(&data, QIODevice::WriteOnly);
        stream << quint8(1) << start << end; // 1 = kod operacji dla rozpoczęcia obliczeń

        // Wysyłanie zadania do slave'a
        client->write(data);

        log(QString("Sent range [%1-%2] to slave %3")
                .arg(start).arg(end).arg(m_clientAddresses[client]));
    }
}

void MasterWidget::handleNewConnection()
{
    QTcpSocket *clientSocket = m_server->nextPendingConnection();

    connect(clientSocket, &QTcpSocket::readyRead, this, &MasterWidget::processResults);
    connect(clientSocket, &QTcpSocket::disconnected, this, &MasterWidget::handleClientDisconnected);

    QString clientAddress = clientSocket->peerAddress().toString() + ":" +
                            QString::number(clientSocket->peerPort());

    m_clients.append(clientSocket);
    m_clientAddresses[clientSocket] = clientAddress;

    updateClientList();
    log(QString("New client connected: %1").arg(clientAddress));
}

void MasterWidget::handleClientDisconnected()
{
    QTcpSocket *clientSocket = qobject_cast<QTcpSocket*>(sender());
    if (!clientSocket) return;

    QString clientAddress = m_clientAddresses[clientSocket];
    log(QString("Client disconnected: %1").arg(clientAddress));

    m_clients.removeOne(clientSocket);
    m_clientAddresses.remove(clientSocket);
    clientSocket->deleteLater();

    updateClientList();
}

void MasterWidget::processResults()
{
    QTcpSocket *clientSocket = qobject_cast<QTcpSocket*>(sender());
    if (!clientSocket) return;

    QDataStream stream(clientSocket);

    while (clientSocket->bytesAvailable() >= sizeof(quint8)) {
        quint8 opCode;
        stream >> opCode;

        if (opCode == 1) { // Znaleziona liczba pierwsza
            if (clientSocket->bytesAvailable() < sizeof(quint64))
                return;

            quint64 prime;
            stream >> prime;

            m_primes.append(prime);
            updatePrimesList(prime);

        } else if (opCode == 2) { // Zakończenie obliczeń
            if (clientSocket->bytesAvailable() < sizeof(quint32))
                return;

            quint32 count;
            stream >> count;

            log(QString("Slave %1 finished calculation, found %2 primes")
                    .arg(m_clientAddresses[clientSocket]).arg(count));
        }
    }
}

void MasterWidget::updateClientList()
{
    ui->clientsListWidget->clear();
    for (const QString &client : m_clientAddresses.values()) {
        ui->clientsListWidget->addItem(client);
    }
}

void MasterWidget::updatePrimesList(quint64 prime)
{
    ui->primesListWidget->addItem(QString::number(prime));
}

void MasterWidget::log(const QString &message)
{
    ui->logTextEdit->append(QTime::currentTime().toString("[HH:mm:ss] ") + message);
}
