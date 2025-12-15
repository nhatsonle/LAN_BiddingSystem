#include "itemlistview.h"
#include <QItemSelectionModel>
#include <QSortFilterProxyModel>
#include <QHeaderView>
#include <QStyledItemDelegate>
#include <QPainter>
#include <QDateTime>

class ItemDelegate : public QStyledItemDelegate
{
public:
    void paint(QPainter *painter, const QStyleOptionViewItem &option,
               const QModelIndex &index) const override
    {
        QStyledItemDelegate::paint(painter, option, index);

        QString title = index.data(ItemListModel::TitleRole).toString();
        double currentBid = index.data(ItemListModel::CurrentBidRole).toDouble();
        QDateTime endTime = index.data(ItemListModel::EndTimeRole).toDateTime();

        QRect rect = option.rect;
        painter->save();

        QFont titleFont = painter->font();
        titleFont.setBold(true);
        titleFont.setPointSize(titleFont.pointSize() + 1);
        painter->setFont(titleFont);
        painter->setPen(QColor("#212121"));
        painter->drawText(rect.adjusted(10, 5, -10, -30), Qt::AlignLeft | Qt::AlignTop, title);

        QFont normalFont = painter->font();
        normalFont.setBold(false);
        painter->setFont(normalFont);

        QString bidText = QString("Current Bid: $%1").arg(currentBid, 0, 'f', 2);
        painter->drawText(rect.adjusted(10, 25, -10, -10), Qt::AlignLeft | Qt::AlignTop, bidText);

        qint64 secondsUntilEnd = QDateTime::currentDateTime().secsTo(endTime);
        QString timeText;
        if (secondsUntilEnd > 0) {
            int days = secondsUntilEnd / 86400;
            int hours = (secondsUntilEnd % 86400) / 3600;
            if (days > 0) {
                timeText = QString("%1d %2h remaining").arg(days).arg(hours);
            } else {
                timeText = QString("%1h remaining").arg(hours);
            }
        } else {
            timeText = "Ended";
        }
        painter->drawText(rect.adjusted(10, 45, -10, 5), Qt::AlignLeft | Qt::AlignTop, timeText);

        painter->restore();
    }

    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override
    {
        Q_UNUSED(option)
        Q_UNUSED(index)
        return QSize(200, 80);
    }
};

ItemListView::ItemListView(QWidget *parent)
    : QWidget(parent)
    , m_model(nullptr)
    , m_filteredModel(nullptr)
{
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(5, 5, 5, 5);
    layout->setSpacing(5);

    m_titleLabel = new QLabel("Available Items", this);
    QFont titleFont = m_titleLabel->font();
    titleFont.setPointSize(14);
    titleFont.setBold(true);
    m_titleLabel->setFont(titleFont);
    layout->addWidget(m_titleLabel);

    m_searchEdit = new QLineEdit(this);
    m_searchEdit->setPlaceholderText("Search items...");
    connect(m_searchEdit, &QLineEdit::textChanged, this, &ItemListView::onSearchTextChanged);
    layout->addWidget(m_searchEdit);

    m_listView = new QListView(this);
    m_listView->setItemDelegate(new ItemDelegate());
    m_listView->setSelectionMode(QAbstractItemView::SingleSelection);
    connect(m_listView, &QListView::doubleClicked, this, &ItemListView::onDoubleClicked);
    connect(m_listView->selectionModel(), &QItemSelectionModel::selectionChanged,
            this, &ItemListView::onSelectionChanged);
    layout->addWidget(m_listView);
}

void ItemListView::setModel(ItemListModel *model)
{
    m_model = model;

    if (m_filteredModel) {
        delete m_filteredModel;
    }
    m_filteredModel = new QSortFilterProxyModel(this);
    m_filteredModel->setSourceModel(model);
    m_filteredModel->setFilterRole(ItemListModel::TitleRole);
    m_filteredModel->setFilterCaseSensitivity(Qt::CaseInsensitive);

    m_listView->setModel(m_filteredModel);
}

void ItemListView::onSelectionChanged(const QItemSelection &selected, const QItemSelection &deselected)
{
    Q_UNUSED(deselected)
    if (!selected.indexes().isEmpty()) {
        QModelIndex proxyIndex = selected.indexes().first();
        QModelIndex sourceIndex = m_filteredModel->mapToSource(proxyIndex);
        AuctionItem *item = m_model->getItem(sourceIndex.row());
        if (item) {
            emit itemSelected(item);
        }
    }
}

void ItemListView::onDoubleClicked(const QModelIndex &index)
{
    QModelIndex sourceIndex = m_filteredModel->mapToSource(index);
    AuctionItem *item = m_model->getItem(sourceIndex.row());
    if (item) {
        emit itemDoubleClicked(item);
    }
}

void ItemListView::onSearchTextChanged(const QString &text)
{
    if (m_filteredModel) {
        m_filteredModel->setFilterFixedString(text);
    }
}

