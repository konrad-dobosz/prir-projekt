#include "slavewidget.h"
#include "ui_slavewidget.h"
#include "primerunnable.h"
#include <QDataStream>

/**
 * Konstruktor klasy SlaveWidget - inicjalizuje interfejs użytkownika i konfiguruje klienta TCP.
 * Tworzy instancję gniazda, łączy odpowiednie sygnały z funkcjami obsługi i inicjalizuje pulę wątków.
 * Ustawia liczbę wątków roboczych na podstawie liczby dostępnych rdzeni procesora.
 */
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

/**
 * Destruktor klasy SlaveWidget - zwalnia zasoby i zamyka połączenie.
 * Ustawia flagę zatrzymania obliczeń, a jeśli istnieje aktywne połączenie z serwerem, rozłącza je.
 */
SlaveWidget::~SlaveWidget()
{
    m_stopped = true;
    if (m_socket->state() == QAbstractSocket::ConnectedState) {
        m_socket->disconnectFromHost();
    }

    delete ui;
}

/**
 * Obsługuje kliknięcie przycisku łączenia z serwerem master.
 * Pobiera adres i port z interfejsu użytkownika, a następnie nawiązuje połączenie TCP.
 */
void SlaveWidget::on_connectButton_clicked()
{
    QString address = ui->serverAddressEdit->text();
    int port = ui->portSpinBox->value();

    log(QString("Connecting to master at %1:%2").arg(address).arg(port));

    m_socket->connectToHost(address, port);
}

/**
 * Obsługuje kliknięcie przycisku rozłączenia z serwerem master.
 * Ustawia flagę zatrzymania obliczeń i zamyka połączenie TCP.
 */
void SlaveWidget::on_disconnectButton_clicked()
{
    m_stopped = true;
    m_socket->disconnectFromHost();
}

/**
 * Obsługuje zdarzenie nawiązania połączenia z serwerem master.
 * Aktualizuje interfejs użytkownika, blokując elementy, które nie powinny być modyfikowane
 * podczas aktywnego połączenia.
 */
void SlaveWidget::handleConnected()
{
    ui->connectButton->setEnabled(false);
    ui->disconnectButton->setEnabled(true);
    ui->serverAddressEdit->setEnabled(false);
    ui->portSpinBox->setEnabled(false);

    log("Connected to master");
    ui->statusLabel->setText("Connected to master");
}

/**
 * Obsługuje zdarzenie rozłączenia połączenia z serwerem master.
 * Aktualizuje interfejs użytkownika, odblokowując elementy związane z konfiguracją połączenia,
 * zatrzymuje trwające obliczenia i resetuje pasek postępu.
 */
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

/**
 * Obsługuje błędy połączenia sieciowego.
 * Zapisuje informację o błędzie w dzienniku i wyświetla komunikat ostrzegawczy dla użytkownika.
 * @param error Kod błędu gniazda
 */
void SlaveWidget::handleError(QAbstractSocket::SocketError error)
{

    log(QString("Socket error: %1").arg(m_socket->errorString()));

    QMessageBox::warning(this, "Connection Error", m_socket->errorString());
}

/**
 * Przetwarza dane otrzymane od serwera master.
 * Interpretuje dane zgodnie z protokołem:
 * - kod operacji 1: zlecenie obliczeń - uruchamia poszukiwanie liczb pierwszych w określonym zakresie
 * - kod operacji 2: zatrzymanie obliczeń - ustawia flagę zatrzymania dla trwających obliczeń
 */
void SlaveWidget::handleData()
{


    QDataStream stream(m_socket);

    while (m_socket->bytesAvailable() >= sizeof(quint8)) {
        quint8 opCode;
        stream >> opCode;

        if (opCode == 1) {
            if (m_socket->bytesAvailable() < sizeof(quint64) * 2)
            return;


            quint64 start, end;
            stream >> start >> end;

            log(QString("Received calculation task: range [%1-%2]").arg(start).arg(end));
            startCalculation(start, end);

        } else if (opCode == 2) {
            m_stopped = true;
            log("Calculation stopped by master");
        }
    }
}

/**
 * Rozpoczyna obliczenia poszukiwania liczb pierwszych w określonym zakresie.
 * Dzieli otrzymany zakres na części i przydziela je do równoległego przetwarzania
 * w puli wątków. Dla każdego zakresu tworzy i uruchamia zadanie PrimeRunnable.
 * @param start Początek zakresu liczbowego
 * @param end Koniec zakresu liczbowego
 */
void SlaveWidget::startCalculation(quint64 start, quint64 end)
{

    m_stopped = false;


    m_primes.clear();
    ui->progressBar->setValue(0);


    log(QString("Starting calculation with %1 threads").arg(m_threadPool->maxThreadCount()));

    int threadCount = m_threadPool->maxThreadCount();
    quint64 rangeSize = end - start + 1;
    quint64 rangePerThread = rangeSize / threadCount;

    for (int i = 0; i < threadCount; i++) {

        quint64 threadStart = start + i * rangePerThread;

        quint64 threadEnd = (i == threadCount - 1) ? end : threadStart + rangePerThread - 1;

        log(QString("Thread %1: range [%2-%3]").arg(i).arg(threadStart).arg(threadEnd));

        PrimeRunnable *task = new PrimeRunnable(this, &m_stopped, threadStart, threadEnd);
        task->setAutoDelete(true);
        m_threadPool->start(task);
    }
}

/**
 * Aktualizuje pasek postępu obliczeń na interfejsie użytkownika.
 * Wywoływana przez zadania PrimeRunnable, aby informować o postępie poszukiwania.
 * @param percent Wartość procentowa postępu (0-100)
 */
void SlaveWidget::updateProgress(int percent)
{

    ui->progressBar->setValue(percent);
}

/**
 * Obsługuje znalezienie nowej liczby pierwszej przez wątek obliczeniowy.
 * Dodaje liczbę do lokalnej listy i wysyła ją do serwera master.
 * Co 100 znalezionych liczb pierwszych, aktualizuje dziennik zdarzeń.
 * @param prime Znaleziona liczba pierwsza
 */
void SlaveWidget::primeFound(quint64 prime)
{
    m_primes.append(prime);

    QByteArray data;
    QDataStream stream(&data, QIODevice::WriteOnly);
    stream << quint8(1) << prime; // 1 = kod operacji dla znalezionej liczby pierwszej

    m_socket->write(data);

    if (m_primes.size() % 100 == 0) {
        log(QString("Found %1 prime numbers so far").arg(m_primes.size()));
    }
}

/**
 * Obsługuje zakończenie obliczeń przez wszystkie wątki.
 * Aktualizuje dziennik zdarzeń, ustawia pasek postępu na 100% i wysyła
 * informację do serwera master o zakończeniu obliczeń wraz z liczbą znalezionych liczb pierwszych.
 * @param primes Lista znalezionych liczb pierwszych
 */
void SlaveWidget::calculationFinished(const QList<quint64> &primes)
{

    log(QString("Calculation finished. Found %1 prime numbers").arg(primes.size()));
    ui->progressBar->setValue(100);

    QByteArray data;

    QDataStream stream(&data, QIODevice::WriteOnly);
    stream << quint8(2) << quint32(primes.size()); // 2 = kod operacji dla zakończenia obliczeń

    m_socket->write(data);
}

/**
 * Dodaje wiadomość do dziennika logów.
 * Dołącza znacznik czasu do wiadomości i wyświetla ją w polu tekstowym logów.
 * @param message Treść wiadomości do zalogowania
 */
void SlaveWidget::log(const QString &message)
{

    ui->logTextEdit->append(QTime::currentTime().toString("[HH:mm:ss] ") + message);

}
