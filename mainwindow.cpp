#include "mainwindow.hpp"
#include "./ui_mainwindow.h"

#include "details/converter.hpp"
#include "details/uint64validator.hpp"

#include <QFileDialog>
#include <QMessageBox>
#include <QThread>
#include <QTimer>
#include <QRegularExpressionValidator>

#include <limits>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , thread_ (new QThread)
    , eventLoop_(new QEventLoop(this))
{
    ui->setupUi(this);
    ui->lineEditBufSize->setVisible(false);
    ui->comboBoxBufSizeType->setVisible(false);
    ui->progressBarConvertation->setVisible(false);
    ui->labelFileCounter->setVisible(false);
    ui->pushButtonStopTimer->setVisible(false);
    ui->labelInfoAboutStop->setVisible(false);

    ui->labelInfoAboutStop->setWordWrap(true);
    ui->labelInfoAboutStop->setFixedHeight(50);

    ui->labelFileCounter->setWordWrap(true);
    ui->labelFileCounter->setFixedHeight(50);

    detail::Uint64Validator* validator =
        new detail::Uint64Validator(0, std::numeric_limits<uint64_t>::max(), this);
    ui->lineEditNumber->setValidator(validator);

    detail::Uint64Validator* validator2 =
        new detail::Uint64Validator(1, std::numeric_limits<uint64_t>::max(), this);
    ui->lineEditBufSize->setValidator(validator2);
}

MainWindow::~MainWindow()
{
    delete ui;
}

QProgressBar* MainWindow::getProgressBarConvertation() const
{
    return ui->progressBarConvertation;
}

void MainWindow::on_pushButtonChooseInputDirectory_clicked()
{
    QString directory = QFileDialog::getExistingDirectory(this, tr("Select Directory"),
                                                          "",
                                                          QFileDialog::ShowDirsOnly
                                                              | QFileDialog::DontResolveSymlinks);
        qDebug() << "Selected input directory:" << directory;
        ui->lineEditInputPath->setText(directory);
}

void MainWindow::on_pushButtonChooseOutputDirectory_clicked()
{
    QString directory = QFileDialog::getExistingDirectory(this,
                                                          tr("Select Directory"),
                                                          "",
                                                          QFileDialog::ShowDirsOnly
                                                              | QFileDialog::DontResolveSymlinks);
    qDebug() << "Selected output directory:" << directory;
    ui->lineEditOutputPath->setText(directory);
}

void MainWindow::on_pushButtonStart_clicked()
{
    ui->pushButtonStart->setEnabled(false);

    detail::Converter::OperationType ot = detail::Converter::OperationType::XOR;
    if(ui->comboBoxOperation->currentText() == "AND")
    {
        ot = detail::Converter::OperationType::AND;
    }
    else if(ui->comboBoxOperation->currentText() == "OR")
    {
        ot = detail::Converter::OperationType::OR;
    }

    // convert all files
    uint64_t buf_size = 0;
    if(ui->comboBoxBufSize->currentText() == "Указать вручную")
    {
        buf_size = ui->lineEditBufSize->text().toULongLong();
        uint64_t multiplier = 0;

        if (ui->comboBoxBufSizeType->currentText() == "Kb")
        {
            multiplier = 1024;
        }
        else if (ui->comboBoxBufSizeType->currentText() == "Mb")
        {
            multiplier = 1048576;
        }
        else if (ui->comboBoxBufSizeType->currentText() == "Gb")
        {
            multiplier = 1073741824;
        }

        if (multiplier > 0)
        {
            uint64_t max_value = std::numeric_limits<uint64_t>::max();
            if (buf_size > max_value / multiplier)
            {
                this->showMessageBoxWithError("Ошибка переполнения при попытке рассчитать размер буфера.");
                return;
            }
            else
            {
                buf_size *= multiplier;
            }
        }
    }

    try
    {
        this->converter_ = new detail::Converter(buf_size,
                                    ui->lineEditNumber->text().toULongLong(),
                                    ot);
        this->converter_->moveToThread(thread_);

        ui->labelFileCounter->setVisible(true);
        ui->progressBarConvertation->setValue(0);
        ui->progressBarConvertation->setVisible(true);

        // set converter parameters

        // for non const in converter()
        QString comboBoxCollisionTextCopy = ui->comboBoxCollision->currentText();

        QTime time = ui->timeEditInputPathCheck->time();
        if(ui->checkBoxTimer->isChecked())
        {
            ui->pushButtonStopTimer->setVisible(true);
            ui->labelInfoAboutStop->setVisible(true);
        }

        QString inputPath = ui->lineEditInputPath->text();
        QString fileMask = ui->lineEditFileMask->text();
        QString outputPath = ui->lineEditOutputPath->text();

        if (!QDir(inputPath).exists())
        {
            this->showMessageBoxWithError("Входная директория не существует.");
            return;
        }

        if (!QDir(outputPath).exists())
        {
            this->showMessageBoxWithError("Выходная директория не существует.");
            return;
        }

        QRegularExpression wildcardRegex("^[*?\\[].*|.*[*?\\]]$");
        if (!wildcardRegex.match(fileMask).hasMatch())
        {
            this->showMessageBoxWithError("Маска файла должна содержать вайлдкард (* или ?).");
            return;
        }

        // Если все проверки пройдены, вы можете продолжить работу с директориями и маской файла


        connect(thread_, &QThread::started, &*this->converter_, [&]()
            {
            (*this->converter_)(ui->lineEditInputPath->text(),
                      ui->lineEditFileMask->text(),
                      ui->lineEditOutputPath->text(),
                      comboBoxCollisionTextCopy,
                      ui->checkBoxDeleteInputFiles->isChecked(),
                      ui->checkBoxTimer->isChecked(),
                      time.minute() * 60 + time.second());
            }
        );
        connect(&*converter_, &detail::Converter::filesConverted, this, &MainWindow::onFilesConverted);
        connect(&*converter_, &detail::Converter::changeProgressBar, this, &MainWindow::onChangeProgressBar);
        connect(&*converter_, &detail::Converter::changeLabelFileCounter, this, &MainWindow::onChangeLabelFileCounter);
        connect(&*converter_, &detail::Converter::showMessageBoxOnCollision, this, &MainWindow::onShowMessageBoxOnCollision);
        connect(this, &MainWindow::messageBoxOnCollisionAnswer, &*converter_, &detail::Converter::onMessageBoxOnCollisionAnswer);


        thread_->start();
        eventLoop_->exec();

        delete converter_;
        converter_ = nullptr;
    }
    catch(...)
    {
        // in order to avoid memory leaks i add try-catch
        delete converter_;
        converter_ = nullptr;
    }

    ui->pushButtonStopTimer->setVisible(false);
    ui->pushButtonStart->setEnabled(true);
}

void MainWindow::on_comboBoxBufSize_currentTextChanged(const QString &arg1)
{
    if(ui->comboBoxBufSize->currentText() == "Указать вручную")
    {
        ui->lineEditBufSize->setVisible(true);
        ui->comboBoxBufSizeType->setVisible(true);
    }
    else
    {
        ui->lineEditBufSize->setVisible(false);
        ui->comboBoxBufSizeType->setVisible(false);
    }
}

void MainWindow::onFilesConverted()
{
    thread_->quit();
    thread_->wait();
    eventLoop_->quit();
}

void MainWindow::onChangeProgressBar(uint64_t value)
{
    ui->progressBarConvertation->setValue(value);
}

void MainWindow::onChangeLabelFileCounter(QString str)
{
    ui->labelFileCounter->setText(str);
}

void MainWindow::onShowMessageBoxOnCollision(const QString& title,
                                             const QString& text,
                                             const QStringList& buttons,
                                             const QString& checkBoxText)
{
    QMessageBox msgBox(QMessageBox::Question, title, text, QMessageBox::NoButton);
    msgBox.addButton(QString("Перезаписать"), QMessageBox::ActionRole);
    QPushButton* skipButton = msgBox.addButton(QString("Пропустить"), QMessageBox::RejectRole);
    QPushButton* addCounterButton = msgBox.addButton(QString("Добавить счётчик к имени файла"), QMessageBox::ActionRole);
    msgBox.setCheckBox(new QCheckBox(checkBoxText));

    msgBox.exec();

    int buttonIndex = 0;
    if (msgBox.clickedButton() == skipButton )
    {
        buttonIndex = 1;
    }
    else if (msgBox.clickedButton() == addCounterButton)
    {
        buttonIndex = 2;
    }

    bool checkBoxState = msgBox.checkBox()->isChecked();
    emit messageBoxOnCollisionAnswer(buttonIndex, checkBoxState);
}

void MainWindow::on_pushButtonStopTimer_clicked()
{
    converter_->stopTimer();
    ui->pushButtonStopTimer->setVisible(false);
    ui->labelInfoAboutStop->setVisible(false);
}

void MainWindow::showMessageBoxWithError(const QString& msg)
{
    QMessageBox::critical(this, "Ошибка!", msg);
    ui->pushButtonStart->setEnabled(true);
}
