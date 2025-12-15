#include "auctionitem.h"

AuctionItem::AuctionItem(QObject *parent)
    : QObject(parent), m_id(0), m_currentBid(0.0)
{
}

AuctionItem::AuctionItem(int id, const QString &title, const QString &description,
                         double startingBid, const QDateTime &endTime,
                         const QString &imageUrl, QObject *parent)
    : QObject(parent)
    , m_id(id)
    , m_title(title)
    , m_description(description)
    , m_currentBid(startingBid)
    , m_endTime(endTime)
    , m_imageUrl(imageUrl)
{
}

void AuctionItem::setCurrentBid(double bid)
{
    if (m_currentBid != bid) {
        m_currentBid = bid;
        emit currentBidChanged(bid);
    }
}

