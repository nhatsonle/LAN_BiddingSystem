#include "auctionrepository.h"
#include <QDateTime>
#include <QDebug>

AuctionRepository::AuctionRepository(QObject *parent)
    : QObject(parent), m_nextBidId(1)
{
    initializeMockData();
}

AuctionRepository::~AuctionRepository()
{
    qDeleteAll(m_items);
    qDeleteAll(m_bids);
}

QList<AuctionItem*> AuctionRepository::getAllItems() const
{
    return m_items;
}

AuctionItem* AuctionRepository::getItemById(int id) const
{
    for (auto *item : m_items) {
        if (item->id() == id) {
            return item;
        }
    }
    return nullptr;
}

QList<Bid*> AuctionRepository::getBidsForItem(int itemId) const
{
    QList<Bid*> result;
    for (auto *bid : m_bids) {
        if (bid->itemId() == itemId) {
            result.append(bid);
        }
    }
    return result;
}

bool AuctionRepository::placeBid(int itemId, const QString &bidderName, double amount)
{
    AuctionItem *item = getItemById(itemId);
    if (!item) {
        return false;
    }

    if (amount <= item->currentBid()) {
        return false;
    }

    Bid *bid = new Bid(m_nextBidId++, itemId, bidderName, amount, QDateTime::currentDateTime(), this);
    m_bids.append(bid);

    item->setCurrentBid(amount);
    emit bidPlaced(bid);
    emit itemUpdated(item);

    return true;
}

void AuctionRepository::initializeMockData()
{
    QDateTime now = QDateTime::currentDateTime();

    m_items.append(new AuctionItem(1, "Vintage Watch", "A beautiful vintage watch from the 1950s", 150.0,
                                   now.addDays(3), "", this));
    m_items.append(new AuctionItem(2, "Antique Vase", "Chinese porcelain vase from Ming Dynasty", 500.0,
                                   now.addDays(5), "", this));
    m_items.append(new AuctionItem(3, "Rare Book Collection", "First edition books from 19th century", 300.0,
                                   now.addDays(2), "", this));
    m_items.append(new AuctionItem(4, "Classic Car", "1965 Mustang in excellent condition", 25000.0,
                                   now.addDays(7), "", this));
    m_items.append(new AuctionItem(5, "Art Painting", "Original oil painting by local artist", 800.0,
                                   now.addDays(4), "", this));

    m_bids.append(new Bid(m_nextBidId++, 1, "John Doe", 160.0, now.addSecs(-7200), this));      // 2 hours ago
    m_items[0]->setCurrentBid(160.0);

    m_bids.append(new Bid(m_nextBidId++, 2, "Jane Smith", 550.0, now.addSecs(-3600), this));    // 1 hour ago
    m_items[1]->setCurrentBid(550.0);

    m_bids.append(new Bid(m_nextBidId++, 3, "Bob Johnson", 320.0, now.addSecs(-1800), this));   // 30 minutes ago
    m_items[2]->setCurrentBid(320.0);
}

