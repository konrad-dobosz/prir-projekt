#include "slavewidget.h"
#include "ui_slavewidget.h"
#include "primerunnable.h"
#include <QDataStream>

SlaveWidget::SlaveWidget(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::SlaveWidget),
    m_stopped(false)
{
    ui->setupUi(this);

    // Inicjalizacja komponentów sieciowych
    m_socket = new QTcpSocket(this);

    connect(m_socket, &QTcpSocket::readyRead, this, &SlaveWidget::handleData);
    connect(m_socket, &QTcpSocket::connected, this, &SlaveWidget::handleConnected);
    connect(m_socket, &QTcpSocket::disconnected, this, &SlaveWidget::handleDisconnected);

// Użyj starej składni dla sygnału error, który został zmieniony w Qt 5.15
#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
    connect(m_socket, &QAbstractSocket::errorOccurred, this, &SlaveWidget::handleError);
#else
    connect(m_socket, static_cast<void(QAbstractSocket::*)(QAbstractSocket::SocketError)>(&QAbstractSocket::error),
            this, &SlaveWidget::handleError);
#endif

    // Inicjalizacja puli wątków
    m_threadPool = new QThreadPool(this);
    m_threadPool->setMaxThreadCount(QThread::idealThreadCount());

    log(QString("Slave initialized with %1 worker threads").arg(QThread::idealThreadCount()));
}

SlaveWidget::~SlaveWidget()
{
    // Zatrzymanie obliczeń i zamknięcie połączenia
    m_stopped = true;
    if (m_socket->state() == QAbstractSocket::ConnectedState) {
        m_socket->disconnectFromHost();
    }

    delete ui;
}

void SlaveWidget::on_connectButton_clicked()
{
    QString address = ui->serverAddressEdit->text();
    int port = ui->portSpinBox->value();

    log(QString("Connecting to master at %1:%2").arg(address).arg(port));

    m_socket->connectToHost(address, port);
}

void SlaveWidget::on_disconnectButton_clicked()
{
    m_stopped = true;
    m_socket->disconnectFromHost();
}

void SlaveWidget::handleConnected()
{
    ui->connectButton->setEnabled(false);
    ui->disconnectButton->setEnabled(true);
    ui->serverAddressEdit->setEnabled(false);
    ui->portSpinBox->setEnabled(false);

    log("Connected to master");
    ui->statusLabel->setText("Connected to master");
}

void SlaveWidget::handleDisconnected()
{
    ui->connectButton->setEnabled(true);
    ui->disconnectButton->setEnabled(false);
    ui->serverAddressEdit->setEnabled(true);
    ui->portSpinBox->setEnabled(true);

    m_stopped = true;

    log("Disconnected from master");
    ui->statusLabel->setText("Not connected");
    ui->progressBar->setValue(0);
}

void SlaveWidget::handleError(QAbstractSocket::SocketError error)
{
    log(QString("Socket error: %1").arg(m_socket->errorString()));
    QMessageBox::warning(this, "Connection Error", m_socket->errorString());
}

void SlaveWidget::handleData()
{
    QDataStream stream(m_socket);

    while (m_socket->bytesAvailable() >= sizeof(quint8)) {
        quint8 opCode;
        stream >> opCode;

        if (opCode == 1) { // Rozpoczęcie obliczeń
            if (m_socket->bytesAvailable() < sizeof(quint64) * 2)
                return;

            quint64 start, end;
            stream >> start >> end;

            log(QString("Received calculation task: range [%1-%2]").arg(start).arg(end));
            startCalculation(start, end);

        } else if (opCode == 2) { // Przerwanie obliczeń
            m_stopped = true;
            log("Calculation stopped by master");
        }
    }
}

void SlaveWidget::startCalculation(quint64 start, quint64 end)
{
    m_stopped = false;
    m_primes.clear();
    ui->progressBar->setValue(0);

    log(QString("Starting calculation with %1 threads").arg(m_threadPool->maxThreadCount()));

    // Określenie liczby wątków na podstawie dostępnych rdzeni
    int threadCount = m_threadPool->maxThreadCount();
    quint64 rangeSize = end - start + 1;
    quint64 rangePerThread = rangeSize / threadCount;

    // Tworzenie i uruchamianie zadań dla każdego wątku
    for (int i = 0; i < threadCount; i++) {
        quint64 threadStart = start + i * rangePerThread;
        quint64 threadEnd = (i == threadCount - 1) ? end : threadStart + rangePerThread - 1;

        log(QString("Thread %1: range [%2-%3]").arg(i).arg(threadStart).arg(threadEnd));

        PrimeRunnable *task = new PrimeRunnable(this, &m_stopped, threadStart, threadEnd);
        task->setAutoDelete(true);
        m_threadPool->start(task);
    }
}

void SlaveWidget::updateProgress(int percent)
{
    ui->progressBar->setValue(percent);
}

void SlaveWidget::primeFound(quint64 prime)
{
    m_primes.append(prime);

    // Wysyłanie znalezionej liczby pierwszej do mastera
    QByteArray data;
    QDataStream stream(&data, QIODevice::WriteOnly);
    stream << quint8(1) << prime; // 1 = kod operacji dla znalezionej liczby pierwszej

    m_socket->write(data);

    // Aktualizacja logu co 100 liczb pierwszych
    if (m_primes.size() % 100 == 0) {
        log(QString("Found %1 prime numbers so far").arg(m_primes.size()));
    }
}

void SlaveWidget::calculationFinished(const QList<quint64> &primes)
{
    log(QString("Calculation finished. Found %1 prime numbers").arg(primes.size()));
    ui->progressBar->setValue(100);

    // Wysyłanie informacji o zakończeniu obliczeń do mastera
    QByteArray data;
    QDataStream stream(&data, QIODevice::WriteOnly);
    stream << quint8(2) << quint32(primes.size()); // 2 = kod operacji dla zakończenia obliczeń

    m_socket->write(data);
}

void SlaveWidget::log(const QString &message)
{
    ui->logTextEdit->append(QTime::currentTime().toString("[HH:mm:ss] ") + message);
}
