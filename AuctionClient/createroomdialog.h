#ifndef CREATEROOMDIALOG_H
#define CREATEROOMDIALOG_H

#include <QDialog>
#include <vector>
#include <QString>

// Định nghĩa struct ProductInfo ngay tại đây để Class bên dưới hiểu được
struct ProductInfo {
    QString name;
    int startPrice;
    int buyNowPrice;
    int duration;
};

namespace Ui {
class CreateRoomDialog;
}

class CreateRoomDialog : public QDialog
{
    Q_OBJECT

public:
    // --- DÒNG QUAN TRỌNG: Khai báo Constructor ---
    explicit CreateRoomDialog(QWidget *parent = nullptr);
    ~CreateRoomDialog();

    QString getRoomName() const;
    QString getStartTimeString() const;
    std::vector<ProductInfo> getProductList() const;

private slots:
    void on_btnAddProduct_clicked();
    void onVerify();
    void on_btnEdit_clicked();       // Slot cho nút Sửa
    void on_btnDelete_clicked();     // Slot cho nút Xóa

private:
    Ui::CreateRoomDialog *ui;
    std::vector<ProductInfo> m_products; // Biến lưu danh sách sản phẩm
    int m_editingRow; //Lưu chỉ số dòng đang sửa (-1 nếu đang thêm mới)

    void resetInputForm(); // Hàm tiện ích để xóa trắng form
};

#endif // CREATEROOMDIALOG_H
