#include "mainwindow.h"
#include "createroomdialog.h"
#include "ui_mainwindow.h"
#include <QMessageBox>
#include <QTimer>
#include <QDebug>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    // Khởi tạo Socket
    m_socket = new QTcpSocket(this);
    connect(m_socket, &QTcpSocket::connected, this, &MainWindow::onConnected);
    connect(m_socket, &QTcpSocket::readyRead, this, &MainWindow::onReadyRead);

    // Mặc định hiển thị trang Login (Index 0)
    ui->stackedWidget->setCurrentIndex(0);
}

MainWindow::~MainWindow()
{
    delete ui;
}

// --- XỬ LÝ NÚT BẤM ---

void MainWindow::on_btnBid_clicked()
{
    QString amountStr = ui->txtBidAmount->text();
    bool ok;
    int amount = amountStr.toInt(&ok);

    if (!ok) return;

    // Gửi lệnh BID lên Server
    // Cú pháp: BID|<room_id>|<price>
    QString cmd = QString("BID|%1|%2\n").arg(m_currentRoomId).arg(amount);
    m_socket->write(cmd.toUtf8());

    ui->txtBidAmount->clear();
}

void MainWindow::on_btnLogin_clicked()
{
    // 1. Kết nối tới Server (nếu chưa)
    if (m_socket->state() != QAbstractSocket::ConnectedState) {
        m_socket->connectToHost("127.0.0.1", 8080); // Hardcode IP cho nhanh, hoặc lấy từ ô nhập
        if (!m_socket->waitForConnected(3000)) {
            QMessageBox::critical(this, "Lỗi", "Không thể kết nối Server!");
            return;
        }
    }

    // 2. Gửi lệnh Login
    QString user = ui->txtUser->text();
    QString pass = ui->txtPass->text();

    if (user.isEmpty()) return;

    m_username = user; // Lưu lại
    QString command = "LOGIN|" + user + "|" + pass + "\n";
    m_socket->write(command.toUtf8());
}

void MainWindow::on_btnRefresh_clicked()
{
    // Gửi lệnh lấy danh sách phòng
    m_socket->write("LIST_ROOMS\n");
}

void MainWindow::on_btnLeave_clicked()
{
    // Quay lại sảnh
    ui->stackedWidget->setCurrentIndex(1);
    // TODO: Gửi lệnh LEAVE_ROOM lên server (cần implement sau)
}

void MainWindow::on_btnCreateRoom_clicked()
{
    CreateRoomDialog dlg(this);

    if (dlg.exec() == QDialog::Accepted) {

        QString productName = dlg.productName();
        QString productStartingPrice = dlg.productStartingPrice();

        // Validate tối thiểu
        if (productName.isEmpty()) {
            QMessageBox::warning(this, "Lỗi", "Tên phòng không được trống");
            return;
        }

        // Build message theo protocol
        QString message = QString("CREATE_ROOM|%1|%2\n")
                              .arg(productName)
                              .arg(productStartingPrice);


        // Gửi qua socket
        m_socket->write(message.toUtf8());
    }
}

void MainWindow::on_btnJoin_clicked(){
    // Lấy item đang được chọn
    QListWidgetItem *current = ui->listRooms->currentItem();
    if (!current) {
        QMessageBox::warning(this, "Chú ý", "Vui lòng chọn một phòng để tham gia!");
        return;
    }

    // Lấy ID từ dữ liệu ẩn
    int roomId = current->data(Qt::UserRole).toInt();

    // Gửi lệnh JOIN
    m_socket->write(QString("JOIN_ROOM|%1\n").arg(roomId).toUtf8());
}




// --- XỬ LÝ MẠNG (CORE) ---

void MainWindow::onConnected()
{
    // Có thể hiện thông báo nhỏ ở thanh status bar
    statusBar()->showMessage("Đã kết nối Server");
}

void MainWindow::onReadyRead()
{
    while (m_socket->canReadLine()) {
        QString line = QString::fromUtf8(m_socket->readLine()).trimmed();
        qDebug() << "Server msg:" << line; // <-- Nhìn vào đây xem có thấy NEW_BID không?
        // --- PHÂN TÍCH GÓI TIN TỪ SERVER ---

        // 1. Phản hồi Đăng nhập
        if (line.startsWith("OK|LOGIN_SUCCESS")) {
            // Chuyển sang trang Lobby (Index 1)
            ui->stackedWidget->setCurrentIndex(1);

            // Tự động lấy danh sách phòng luôn
            on_btnRefresh_clicked();
        }
        else if (line.startsWith("ERR|WRONG_PASS")) {
            QMessageBox::warning(this, "Lỗi", "Sai mật khẩu!");
        }

        // 2. Phản hồi Danh sách phòng
        // Server gửi: OK|LIST|1:Laptop:1500;2:Phone:500;
        else if (line.startsWith("OK|LIST")) {
            ui->listRooms->clear();

            QString data = line.section('|', 2, 2); // Lấy phần sau OK|LIST|
            QStringList rooms = data.split(';', Qt::SkipEmptyParts);

            for (const QString &r : rooms) {
                QStringList parts = r.split(':'); // 1:Laptop:1500
                if (parts.size() >= 3) {
                    QString display = "Phòng " + parts[0] + ": " + parts[1] + " - Giá: " + parts[2];
                    QListWidgetItem *item = new QListWidgetItem(display);
                    // LƯU ID VÀO DỮ LIỆU ẨN CỦA ITEM
                    item->setData(Qt::UserRole, parts[0].toInt());

                    ui->listRooms->addItem(item);
                }
            }
        }
        else if(line.startsWith("OK|ROOM_CREATED")){
            //Server replies: OK|ROOM_CREATED|5 (5 is the new room's ID)
            QMessageBox::information(this, "Thành công", "Đã tạo phòng đấu giá mới");
            // Tự động refresh lại list để thấy phòng mình vừa tạo
            on_btnRefresh_clicked();
        }
        // 1. XỬ LÝ VÀO PHÒNG THÀNH CÔNG
        // Server: OK|JOINED|<id>|<name>|<price>
        else if (line.startsWith("OK|JOINED")) {
            QStringList parts = line.split('|');
            if (parts.size() >= 5) {
                m_currentRoomId = parts[2].toInt(); // Lưu ID phòng đang ở
                QString name = parts[3];
                QString price = parts[4];

                // Cập nhật UI trang Room
                ui->lblRoomName->setText("Đang đấu giá: " + name);
                ui->lblCurrentPrice->setText(price);
                ui->txtRoomLog->clear();
                ui->txtRoomLog->append("--- Bắt đầu phiên đấu giá ---");

                // Chuyển trang
                ui->stackedWidget->setCurrentIndex(2); // Page Room
            }
        }

        // 2. XỬ LÝ KHI CÓ NGƯỜI RA GIÁ (BROADCAST)
        // Server: NEW_BID|<price>|<user_socket>
        else if (line.startsWith("NEW_BID")) {
            // In ra chuỗi thô để xem có ký tự lạ không
            qDebug() << "1. Raw string received:" << line;

            QStringList parts = line.split('|');
            qDebug() << "2. Split size:" << parts.size(); // Phải >= 3 mới đúng

            if (parts.size() >= 3) {
                QString newPrice = parts[1];
                QString userSocket = parts[2];

                qDebug() << "3. New Price extracted:" << newPrice;

                // --- CẬP NHẬT GIAO DIỆN ---

                // Cố tình update cứng để test xem Label có chết không
                // Nếu dòng này chạy mà giao diện không đổi -> Label bị lỗi kết nối UI
                ui->lblCurrentPrice->setText(newPrice);

                ui->txtRoomLog->append("User " + userSocket + " trả giá: " + newPrice);

                qDebug() << "4. UI Updated successfully!";
            } else {
                qDebug() << "ERROR: Split size wrong! Check protocol separator.";
            }
        }
        // 1. XỬ LÝ ĐỒNG HỒ ĐẾM NGƯỢC
        // Server: TIME_UPDATE|299
        else if (line.startsWith("TIME_UPDATE")) {
            int seconds = line.section('|', 1, 1).toInt();

            // Hiển thị lên LCD Number hoặc Label
            ui->lcdTimer->display(seconds);

            // Đổi màu nếu sắp hết giờ (< 30s)
            if (seconds < 30) {
                ui->lcdTimer->setStyleSheet("color: red;");
            } else {
                ui->lcdTimer->setStyleSheet("color: black;"); // Mặc định
            }
        }

        // 2. XỬ LÝ KẾT THÚC PHIÊN (SOLD)
        // Server: SOLD|<price>|<winner_socket>
        else if (line.startsWith("SOLD")) {
            QString price = line.section('|', 1, 1);
            QString winner = line.section('|', 2, 2);

            ui->lcdTimer->display(0);
            ui->txtRoomLog->append("--- KẾT THÚC ---");
            ui->txtRoomLog->append("Sản phẩm đã thuộc về User " + winner + " với giá " + price);

            QMessageBox::information(this, "Kết thúc", "Sản phẩm đã được bán!");

            // Disable các nút để không bấm được nữa
            ui->btnBid->setEnabled(false);
        }

        // 3. XỬ LÝ KHÔNG AI MUA (CLOSED)
        else if (line.startsWith("CLOSED")) {
            ui->lcdTimer->display(0);
            ui->txtRoomLog->append("--- KẾT THÚC ---");
            ui->txtRoomLog->append("Không có ai đấu giá. Phiên bị hủy.");
            ui->btnBid->setEnabled(false);
        }

}
}


