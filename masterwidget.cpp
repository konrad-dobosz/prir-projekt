#include "masterwidget.h"
#include "ui_masterwidget.h"
#include <QMessageBox>
#include <QDataStream>

/**
 * Konstruktor klasy MasterWidget - inicjalizuje interfejs użytkownika i konfiguruje serwer TCP.
 * Tworzy instancję serwera i łączy sygnał nowego połączenia z odpowiednią funkcją obsługi.
 * Ustawia domyślne wartości parametrów, takich jak zakres poszukiwania liczb pierwszych.
 */
MasterWidget::MasterWidget(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::MasterWidget),
    m_serverRunning(false),


    m_rangeStart(1),
    m_rangeEnd(1000000),
    m_sortAscending(true)
{
    ui->setupUi(this);

    // Inicjalizacja komponentów sieciowych
    m_server = new QTcpServer(this);
    connect(m_server, &QTcpServer::newConnection, this, &MasterWidget::handleNewConnection);
}

/**
 * Destruktor klasy MasterWidget - zwalnia zasoby i zamyka serwer.
 * Jeśli serwer jest uruchomiony, zatrzymuje go przed zwolnieniem zasobów.
 */
MasterWidget::~MasterWidget()
{
    // Zatrzymanie serwera i zamknięcie połączeń

    if (m_serverRunning) {
        on_stopServerButton_clicked();
    }

    delete ui;
}

/**
 * Obsługuje kliknięcie przycisku uruchamiającego serwer.
 * Uruchamia serwer TCP na określonym porcie i aktualizuje interfejs użytkownika.
 * W przypadku błędu wyświetla komunikat z informacją o problemie.
 */
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

/**
 * Obsługuje kliknięcie przycisku zatrzymującego serwer.
 * Zamyka wszystkie połączenia z klientami, zatrzymuje serwer TCP i aktualizuje interfejs użytkownika.
 * Czyści listy klientów i aktualizuje informacje o stanie serwera.
 */
void MasterWidget::on_stopServerButton_clicked()
{
    for (QTcpSocket *socket : m_clients) {
        socket->disconnectFromHost();
    }

    m_clients.clear();
    m_clientAddresses.clear();
    ui->clientsListWidget->clear();

    m_server->close();
    m_serverRunning = false;

    ui->startServerButton->setEnabled(true);
    ui->stopServerButton->setEnabled(false);
    ui->distributeButton->setEnabled(false);
    ui->portSpinBox->setEnabled(true);

    log("Server stopped");
    ui->statusLabel->setText("Server not running");
}

/**
 * Obsługuje kliknięcie przycisku dystrybucji zadań.
 * Pobiera zakres poszukiwania liczb pierwszych, dzieli go na części i przydziela każdemu podłączonemu klientowi.
 * Waliduje wprowadzone wartości zakresu i sprawdza dostępność klientów.
 * Dla każdego klienta oblicza indywidualny podzakres i wysyła zadanie przez TCP.
 */
void MasterWidget::on_distributeButton_clicked()
{


    if (m_clients.isEmpty()) {
        QMessageBox::warning(this, "Warning", "No connected slaves to distribute work");
        return;
    }

    bool ok;
    m_rangeStart = ui->rangeStartEdit->text().toULongLong(&ok);
    if (!ok) {
        QMessageBox::warning(this, "Warning", "Invalid range start value");
        return;
    }

    m_rangeEnd = ui->rangeEndEdit->text().toULongLong(&ok);
    if (!ok) {
        QMessageBox::warning(this, "Warning", "Invalid range end value");
        return;
    }

    if (m_rangeStart >= m_rangeEnd) {
        QMessageBox::warning(this, "Warning", "Range start must be less than range end");
        return;
    }


    // Obliczanie zakresu dla każdego slave'a
    quint64 totalRange = m_rangeEnd - m_rangeStart + 1;
    quint64 rangePerClient = totalRange / m_clients.size();

    log(QString("Distributing work range [%1-%2] to %3 slaves").arg(m_rangeStart).arg(m_rangeEnd).arg(m_clients.size()));

    // Czyszczenie listy znalezionych liczb pierwszych
    m_primes.clear();
    ui->primesListWidget->clear();

    for (int i = 0; i < m_clients.size(); i++) {
        QTcpSocket *client = m_clients[i];

        quint64 start = m_rangeStart + i * rangePerClient;
        quint64 end = (i == m_clients.size() - 1) ? m_rangeEnd : start + rangePerClient - 1;

        // Tworzenie pakietu danych
        QByteArray data;
        QDataStream stream(&data, QIODevice::WriteOnly);
        stream << quint8(1) << start << end; // 1 = kod operacji dla rozpoczęcia obliczeń

        // Wysyłanie zadania do slave'a
        client->write(data);

        log(QString("Sent range [%1-%2] to slave %3").arg(start).arg(end).arg(m_clientAddresses[client]));
    }
}

/**
 * Obsługuje nowe połączenie klienta z serwerem.
 * Pobiera kolejne oczekujące połączenie, łączy odpowiednie sygnały klienta z funkcjami obsługi,
 * dodaje klienta do listy i aktualizuje interfejs użytkownika.
 */
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

/**
 * Obsługuje rozłączenie klienta.
 * Identyfikuje rozłączony socket, usuwa go z listy klientów i zwalnia zasoby.
 * Aktualizuje interfejs użytkownika, usuwając klienta z wyświetlanej listy.
 */
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

/**
 * Przetwarza wyniki otrzymane od klientów (slave'ów).
 * Odczytuje dane z połączenia TCP i interpretuje je zgodnie z protokołem:
 * - kod operacji 1: znaleziona liczba pierwsza - dodaje ją do listy i aktualizuje interfejs
 * - kod operacji 2: zakończenie obliczeń - rejestruje informację o zakończeniu pracy klienta
 */
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
            updatePrimeCount();

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

/**
 * Aktualizuje etykietę z liczbą znalezionych liczb pierwszych.
 * Wyświetla aktualną liczbę znalezionych liczb pierwszych na interfejsie.
 */
void MasterWidget::updatePrimeCount()
{
    ui->primeCountLabel->setText(QString("Found: %1").arg(m_primes.count()));
}

/**
 * Aktualizuje listę podłączonych klientów na interfejsie użytkownika.
 * Czyści obecną listę i wypełnia ją aktualnymi adresami klientów.
 */
void MasterWidget::updateClientList()
{
    ui->clientsListWidget->clear();
    for (const QString &client : m_clientAddresses.values()) {

        ui->clientsListWidget->addItem(client);

    }
}

/**
 * Aktualizuje pełną listę znalezionych liczb pierwszych na interfejsie użytkownika.
 * Czyści obecną listę i wypełnia ją wszystkimi znalezionymi liczbami pierwszymi.
 */
void MasterWidget::updatePrimesList()
{
    ui->primesListWidget->clear();

    for (const quint64 &prime : m_primes) {

        ui->primesListWidget->addItem(QString::number(prime));

    }
}

/**
 * Dodaje pojedynczą liczbę pierwszą do listy na interfejsie użytkownika.
 * @param prime Liczba pierwsza do dodania
 */
void MasterWidget::updatePrimesList(quint64 prime)
{
    ui->primesListWidget->addItem(QString::number(prime));
}

/**
 * Dodaje wiadomość do dziennika logów.
 * Dołącza znacznik czasu do wiadomości i wyświetla ją w polu tekstowym logów.
 * @param message Treść wiadomości do zalogowania
 */
void MasterWidget::log(const QString &message)
{

    ui->logTextEdit->append(QTime::currentTime().toString("[HH:mm:ss] ") + message);

}

/**
 * Obsługuje kliknięcie przycisku weryfikacji wyników.
 * Oblicza przybliżoną liczbę liczb pierwszych w zadanym zakresie na podstawie twierdzenia
 * o liczbach pierwszych, porównuje z faktycznie znalezioną liczbą i wyświetla różnicę.
 */
void MasterWidget::on_verifyButton_clicked()
{
    double approximation = primeCountApproximation(m_rangeEnd) - primeCountApproximation(m_rangeStart - 1);

    double difference = std::abs(m_primes.count() - approximation) / approximation * 100.0;

    QString message = QString("Znalezione liczby pierwsze: %1\n"
                              "Aproksymacja matematyczna: %2\n"
                              "Różnica: %3%")
                          .arg(m_primes.count())
                          .arg(approximation, 0, 'f', 2)
                          .arg(difference, 0, 'f', 2);

    QMessageBox::information(this, "Verification Results", message);

    log(QString("Verification: Found %1 primes, approximation: %2, difference: %3%")
            .arg(m_primes.count())
            .arg(approximation, 0, 'f', 2)
            .arg(difference, 0, 'f', 2));
}

/**
 * Oblicza matematyczną aproksymację liczby liczb pierwszych mniejszych lub równych x.
 * Wykorzystuje przybliżenie π(x) ≈ x/ln(x), które wynika z twierdzenia o liczbach pierwszych.
 * @param x Górna granica zakresu
 * @return Przybliżona liczba liczb pierwszych w zakresie [1,x]
 */
double MasterWidget::primeCountApproximation(quint64 x)
{

    if (x < 2) return 0;
    return x / std::log(x);
}

/**
 * Obsługuje kliknięcie przycisku sortowania liczb pierwszych.
 * Przełącza tryb sortowania (rosnąco/malejąco), sortuje listę i aktualizuje interfejs.
 */
void MasterWidget::on_sortButton_clicked()
{
    m_sortAscending = !m_sortAscending;
    sortPrimesList();

    if (m_sortAscending) {
        ui->sortButton->setText("Sort Descending");
        log("Sorted prime numbers in ascending order");
    } else {
        ui->sortButton->setText("Sort Ascending");
        log("Sorted prime numbers in descending order");
    }


}

/**
 * Sortuje listę znalezionych liczb pierwszych zgodnie z aktualnym trybem sortowania.
 * Wykorzystuje algorytm std::sort z odpowiednim komparatorem, a następnie aktualizuje
 * interfejs użytkownika, aby odzwierciedlić posortowaną listę.
 */
void MasterWidget::sortPrimesList()
{
    if (m_sortAscending) {
        std::sort(m_primes.begin(), m_primes.end());
    } else {
        std::sort(m_primes.begin(), m_primes.end(), std::greater<quint64>());
    }

    updatePrimesList();
}
