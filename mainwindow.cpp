#include "mainwindow.h"
#include "ui_mainwindow.h"

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    // Domyślnie pokazujemy tryb master
    on_actionMaster_triggered();
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::on_actionMaster_triggered()
{
    ui->stackedWidget->setCurrentWidget(ui->masterWidget);
    setWindowTitle("Prime Calculator - Master Mode");
    ui->statusbar->showMessage("Master mode active");
}

void MainWindow::on_actionSlave_triggered()
{
    ui->stackedWidget->setCurrentWidget(ui->slaveWidget);
    setWindowTitle("Prime Calculator - Slave Mode");
    ui->statusbar->showMessage("Slave mode active");
}
