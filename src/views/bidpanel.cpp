#include "bidpanel.h"
#include <QMessageBox>
#include <QDateTime>

BidPanel::BidPanel(QWidget *parent)
    : QWidget(parent)
    , m_currentItem(nullptr)
{
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(10, 10, 10, 10);
    layout->setSpacing(10);

    m_titleLabel = new QLabel("Place a Bid", this);
    QFont titleFont = m_titleLabel->font();
    titleFont.setPointSize(14);
    titleFont.setBold(true);
    m_titleLabel->setFont(titleFont);
    layout->addWidget(m_titleLabel);

    m_currentBidLabel = new QLabel("", this);
    layout->addWidget(m_currentBidLabel);

    QLabel *bidderLabel = new QLabel("Your Name:", this);
    layout->addWidget(bidderLabel);

    m_bidderNameEdit = new QLineEdit(this);
    m_bidderNameEdit->setPlaceholderText("Enter your name");
    layout->addWidget(m_bidderNameEdit);

    QLabel *amountLabel = new QLabel("Bid Amount:", this);
    layout->addWidget(amountLabel);

    m_bidAmountSpinBox = new QDoubleSpinBox(this);
    m_bidAmountSpinBox->setMinimum(0.01);
    m_bidAmountSpinBox->setMaximum(999999.99);
    m_bidAmountSpinBox->setDecimals(2);
    m_bidAmountSpinBox->setPrefix("$");
    layout->addWidget(m_bidAmountSpinBox);

    m_placeBidButton = new QPushButton("Place Bid", this);
    m_placeBidButton->setEnabled(false);
    connect(m_placeBidButton, &QPushButton::clicked, this, &BidPanel::onPlaceBidClicked);
    layout->addWidget(m_placeBidButton);

    layout->addStretch();
}

void BidPanel::setItem(AuctionItem *item)
{
    if (m_currentItem) {
        disconnect(m_currentItem, &AuctionItem::currentBidChanged, this, &BidPanel::onItemBidChanged);
    }

    m_currentItem = item;
    if (!item) {
        clear();
        return;
    }

    connect(item, &AuctionItem::currentBidChanged, this, &BidPanel::onItemBidChanged);
    updateMinimumBid();
    m_placeBidButton->setEnabled(true);
}

void BidPanel::clear()
{
    if (m_currentItem) {
        disconnect(m_currentItem, &AuctionItem::currentBidChanged, this, &BidPanel::onItemBidChanged);
    }
    m_currentItem = nullptr;
    m_titleLabel->setText("Place a Bid");
    m_currentBidLabel->setText("");
    m_bidderNameEdit->clear();
    m_bidAmountSpinBox->setValue(0.01);
    m_placeBidButton->setEnabled(false);
}

void BidPanel::onPlaceBidClicked()
{
    if (!m_currentItem) {
        return;
    }

    QString bidderName = m_bidderNameEdit->text().trimmed();
    if (bidderName.isEmpty()) {
        QMessageBox::warning(this, "Invalid Input", "Please enter your name.");
        return;
    }

    double bidAmount = m_bidAmountSpinBox->value();
    if (bidAmount <= m_currentItem->currentBid()) {
        QMessageBox::warning(this, "Invalid Bid", 
                           QString("Your bid must be higher than the current bid of $%1")
                           .arg(m_currentItem->currentBid(), 0, 'f', 2));
        return;
    }

    qint64 secondsUntilEnd = QDateTime::currentDateTime().secsTo(m_currentItem->endTime());
    if (secondsUntilEnd <= 0) {
        QMessageBox::warning(this, "Auction Ended", "This auction has already ended.");
        return;
    }

    emit bidPlaced(m_currentItem->id(), bidderName, bidAmount);
}

void BidPanel::onItemBidChanged(double newBid)
{
    Q_UNUSED(newBid)
    updateMinimumBid();
}

void BidPanel::updateMinimumBid()
{
    if (!m_currentItem) {
        return;
    }

    double minBid = m_currentItem->currentBid() + 0.01;
    m_bidAmountSpinBox->setMinimum(minBid);
    m_bidAmountSpinBox->setValue(minBid);
    m_currentBidLabel->setText(QString("Current Bid: $%1\nMinimum Bid: $%2")
                              .arg(m_currentItem->currentBid(), 0, 'f', 2)
                              .arg(minBid, 0, 'f', 2));
}

