#include "primerunnable.h"
#include <QMetaObject>
#include <QThread>

PrimeRunnable::PrimeRunnable(QObject* receiver, volatile bool *stopped, quint64 start, quint64 end)
    : m_receiver(receiver), m_stopped(stopped), m_start(start), m_end(end)
{
}

PrimeRunnable::~PrimeRunnable()
{
}

QList<quint64> PrimeRunnable::getPrimes() const
{
    return m_primes;
}

void PrimeRunnable::run()
{
    for (quint64 i = m_start; i <= m_end; i++) {
        if (*m_stopped) break;

        if (isPrime(i)) {
            m_primes.append(i);
            QMetaObject::invokeMethod(m_receiver, "primeFound",
                                      Qt::QueuedConnection,
                                      Q_ARG(quint64, i));
        }
    }

    QMetaObject::invokeMethod(m_receiver, "calculationFinished",
                              Qt::QueuedConnection,
                              Q_ARG(QList<quint64>, m_primes));
}

bool PrimeRunnable::isPrime(quint64 n)
{
    if (n <= 1) return false;
    if (n <= 3) return true;
    if (n % 2 == 0 || n % 3 == 0) return false;

    quint64 i = 5;
    while (i * i <= n) {
        if (*m_stopped) return false;

        if (n % i == 0 || n % (i + 2) == 0)
            return false;

        i += 6;

        if (i % 1000 == 0) {
            double progress = static_cast<double>(i * i) / n * 100.0;
            QMetaObject::invokeMethod(m_receiver, "updateProgress",
                                      Qt::QueuedConnection,
                                      Q_ARG(int, qMin(static_cast<int>(progress), 99)));
        }
    }

    return true;
}
