#include "itemdetailview.h"
#include <QDateTime>

ItemDetailView::ItemDetailView(QWidget *parent)
    : QWidget(parent)
    , m_currentItem(nullptr)
{
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(10, 10, 10, 10);
    layout->setSpacing(10);

    m_titleLabel = new QLabel("Select an item to view details", this);
    QFont titleFont = m_titleLabel->font();
    titleFont.setPointSize(16);
    titleFont.setBold(true);
    m_titleLabel->setFont(titleFont);
    m_titleLabel->setWordWrap(true);
    layout->addWidget(m_titleLabel);

    m_bidLabel = new QLabel("", this);
    QFont bidFont = m_bidLabel->font();
    bidFont.setPointSize(14);
    m_bidLabel->setFont(bidFont);
    layout->addWidget(m_bidLabel);

    m_timeLabel = new QLabel("", this);
    layout->addWidget(m_timeLabel);

    QLabel *descLabel = new QLabel("Description:", this);
    QFont descLabelFont = descLabel->font();
    descLabelFont.setBold(true);
    descLabel->setFont(descLabelFont);
    layout->addWidget(descLabel);

    m_descriptionEdit = new QTextEdit(this);
    m_descriptionEdit->setReadOnly(true);
    m_descriptionEdit->setMaximumHeight(150);
    layout->addWidget(m_descriptionEdit);

    layout->addStretch();
}

void ItemDetailView::setItem(AuctionItem *item)
{
    m_currentItem = item;
    if (!item) {
        clear();
        return;
    }

    m_titleLabel->setText(item->title());
    m_bidLabel->setText(QString("Current Bid: $%1").arg(item->currentBid(), 0, 'f', 2));
    m_descriptionEdit->setPlainText(item->description());

    qint64 secondsUntilEnd = QDateTime::currentDateTime().secsTo(item->endTime());
    QString timeText;
    if (secondsUntilEnd > 0) {
        int days = secondsUntilEnd / 86400;
        int hours = (secondsUntilEnd % 86400) / 3600;
        int minutes = (secondsUntilEnd % 3600) / 60;
        if (days > 0) {
            timeText = QString("Time Remaining: %1 days, %2 hours").arg(days).arg(hours);
        } else if (hours > 0) {
            timeText = QString("Time Remaining: %1 hours, %2 minutes").arg(hours).arg(minutes);
        } else {
            timeText = QString("Time Remaining: %1 minutes").arg(minutes);
        }
    } else {
        timeText = "Auction Ended";
    }
    m_timeLabel->setText(timeText);
}

void ItemDetailView::clear()
{
    m_currentItem = nullptr;
    m_titleLabel->setText("Select an item to view details");
    m_bidLabel->setText("");
    m_timeLabel->setText("");
    m_descriptionEdit->setPlainText("");
}

