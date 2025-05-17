#ifndef PRIMERUNNABLE_H
#define PRIMERUNNABLE_H

#include <QRunnable>
#include <QObject>
#include <QList>

class PrimeRunnable : public QRunnable
{
public:
    PrimeRunnable(QObject* receiver, volatile bool *stopped, quint64 start, quint64 end);
    ~PrimeRunnable();

    QList<quint64> getPrimes() const;

protected:
    void run() override;

private:
    bool isPrime(quint64 n);

    QObject* m_receiver;
    volatile bool *m_stopped;
    quint64 m_start;
    quint64 m_end;
    QList<quint64> m_primes;
};

#endif // PRIMERUNNABLE_H
