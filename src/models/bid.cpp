#include "bid.h"

Bid::Bid(QObject *parent)
    : QObject(parent), m_id(0), m_itemId(0), m_amount(0.0)
{
}

Bid::Bid(int id, int itemId, const QString &bidderName, double amount,
         const QDateTime &timestamp, QObject *parent)
    : QObject(parent)
    , m_id(id)
    , m_itemId(itemId)
    , m_bidderName(bidderName)
    , m_amount(amount)
    , m_timestamp(timestamp)
{
}

