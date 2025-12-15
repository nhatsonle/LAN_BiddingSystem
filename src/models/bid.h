#ifndef BID_H
#define BID_H

#include <QString>
#include <QDateTime>
#include <QObject>

class Bid : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int id READ id CONSTANT)
    Q_PROPERTY(int itemId READ itemId CONSTANT)
    Q_PROPERTY(QString bidderName READ bidderName CONSTANT)
    Q_PROPERTY(double amount READ amount CONSTANT)
    Q_PROPERTY(QDateTime timestamp READ timestamp CONSTANT)

public:
    explicit Bid(QObject *parent = nullptr);
    Bid(int id, int itemId, const QString &bidderName, double amount,
        const QDateTime &timestamp, QObject *parent = nullptr);

    int id() const { return m_id; }
    int itemId() const { return m_itemId; }
    QString bidderName() const { return m_bidderName; }
    double amount() const { return m_amount; }
    QDateTime timestamp() const { return m_timestamp; }

private:
    int m_id;
    int m_itemId;
    QString m_bidderName;
    double m_amount;
    QDateTime m_timestamp;
};

#endif // BID_H

