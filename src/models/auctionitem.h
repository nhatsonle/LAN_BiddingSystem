#ifndef AUCTIONITEM_H
#define AUCTIONITEM_H

#include <QString>
#include <QDateTime>
#include <QObject>

class AuctionItem : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int id READ id CONSTANT)
    Q_PROPERTY(QString title READ title CONSTANT)
    Q_PROPERTY(QString description READ description CONSTANT)
    Q_PROPERTY(double currentBid READ currentBid NOTIFY currentBidChanged)
    Q_PROPERTY(QDateTime endTime READ endTime CONSTANT)
    Q_PROPERTY(QString imageUrl READ imageUrl CONSTANT)

public:
    explicit AuctionItem(QObject *parent = nullptr);
    AuctionItem(int id, const QString &title, const QString &description,
                double startingBid, const QDateTime &endTime,
                const QString &imageUrl = "", QObject *parent = nullptr);

    int id() const { return m_id; }
    QString title() const { return m_title; }
    QString description() const { return m_description; }
    double currentBid() const { return m_currentBid; }
    QDateTime endTime() const { return m_endTime; }
    QString imageUrl() const { return m_imageUrl; }

    void setCurrentBid(double bid);

signals:
    void currentBidChanged(double newBid);

private:
    int m_id;
    QString m_title;
    QString m_description;
    double m_currentBid;
    QDateTime m_endTime;
    QString m_imageUrl;
};

#endif // AUCTIONITEM_H

