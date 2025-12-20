#ifndef CREATEROOMDIALOG_H
#define CREATEROOMDIALOG_H

#include <QDialog>

namespace Ui {
class CreateRoomDialog;
}

class CreateRoomDialog : public QDialog
{
    Q_OBJECT

public:
    explicit CreateRoomDialog(QWidget *parent = nullptr);
    ~CreateRoomDialog();

    QString productName() const;
    QString productStartingPrice() const;
    QString productBuyNowPrice() const;

private:
    Ui::CreateRoomDialog *ui;
};

#endif // CREATEROOMDIALOG_H
