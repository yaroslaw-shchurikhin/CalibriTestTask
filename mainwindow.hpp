#ifndef MAINWINDOW_HPP
#define MAINWINDOW_HPP

#include <QMainWindow>
#include <QProgressBar>
#include <QThread>
#include <QEventLoop>

#include "details/converter.hpp"

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

    // to change progressBar from converter
    QProgressBar* getProgressBarConvertation() const;

signals:
    void messageBoxOnCollisionAnswer(int buttonIndex, bool checkBoxState);
    void stopTimerConverter();

private slots:
    void on_pushButtonChooseOutputDirectory_clicked();

    void on_pushButtonChooseInputDirectory_clicked();

    void on_pushButtonStart_clicked();

    void on_comboBoxBufSize_currentTextChanged(const QString &arg1);

    void onFilesConverted();

    void onChangeProgressBar(uint64_t);

    void onChangeLabelFileCounter(QString);

    void onShowMessageBoxOnCollision(const QString& title,
                                     const QString& text,
                                     const QStringList& buttons,
                                     const QString& checkBoxText);

    void on_pushButtonStopTimer_clicked();

private:
    Ui::MainWindow *ui;

    QThread* thread_;
    QEventLoop* eventLoop_;

    detail::Converter* converter_;

    void showMessageBoxWithError(const QString&);
};
#endif // MAINWINDOW_HPP
