#ifndef BIDPANEL_H
#define BIDPANEL_H

#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QDoubleSpinBox>
#include <QPushButton>
#include <QLineEdit>
#include "../models/auctionitem.h"

class BidPanel : public QWidget
{
    Q_OBJECT

public:
    explicit BidPanel(QWidget *parent = nullptr);
    void setItem(AuctionItem *item);
    void clear();

signals:
    void bidPlaced(int itemId, const QString &bidderName, double amount);

private slots:
    void onPlaceBidClicked();
    void onItemBidChanged(double newBid);

private:
    void updateMinimumBid();

    AuctionItem *m_currentItem;
    QLabel *m_titleLabel;
    QLabel *m_currentBidLabel;
    QLineEdit *m_bidderNameEdit;
    QDoubleSpinBox *m_bidAmountSpinBox;
    QPushButton *m_placeBidButton;
};

#endif // BIDPANEL_H

