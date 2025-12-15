#ifndef AUCTIONREPOSITORY_H
#define AUCTIONREPOSITORY_H

#include "auctionitem.h"
#include "bid.h"
#include <QObject>
#include <QList>

/**
 * @brief Repository interface for auction data access.
 *
 * This mock implementation uses in-memory data. To integrate a real API,
 * provide another implementation with the same interface and swap the instance
 * in MainWindow.
 */
class AuctionRepository : public QObject
{
    Q_OBJECT

public:
    explicit AuctionRepository(QObject *parent = nullptr);
    ~AuctionRepository();

    QList<AuctionItem*> getAllItems() const;
    AuctionItem* getItemById(int id) const;
    QList<Bid*> getBidsForItem(int itemId) const;

    bool placeBid(int itemId, const QString &bidderName, double amount);

signals:
    void itemUpdated(AuctionItem *item);
    void bidPlaced(Bid *bid);

private:
    void initializeMockData();

    QList<AuctionItem*> m_items;
    QList<Bid*> m_bids;
    int m_nextBidId;
};

#endif // AUCTIONREPOSITORY_H

