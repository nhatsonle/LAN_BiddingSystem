#include "mainwindow.h"
#include "createroomdialog.h"
#include "ui_mainwindow.h"
#include <QMessageBox>

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
                // r có dạng: "1:Laptop:1500"
                QStringList parts = r.split(':');
                if (parts.size() >= 3) {
                    QString display = "Phòng " + parts[0] + " - " + parts[1] + " (Giá: " + parts[2] + ")";
                    ui->listRooms->addItem(display);
                }
            }
        }
        else if(line.startsWith("OK|ROOM_CREATED")){
            //Server replies: OK|ROOM_CREATED|5 (5 is the new room's ID)
            QMessageBox::information(this, "Thành công", "Đã tạo phòng đấu giá mới");
            // Tự động refresh lại list để thấy phòng mình vừa tạo
            on_btnRefresh_clicked();
        }



        // 3. Phản hồi Vào phòng (Join)
        // (Sẽ làm ở bước tiếp theo)
    }
}
