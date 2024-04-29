#ifndef CONVERTER_HPP
#define CONVERTER_HPP

#include <cstdint>

#include <QString>
#include <QFile>

namespace detail
{

class Converter : public QObject
{
    Q_OBJECT
public:
    enum OperationType { XOR, AND, OR };

    Converter(uint64_t, uint64_t, OperationType);

    void operator()( const QString& ,
                    const QString& ,
                    const QString& ,
                    QString& ,
                    const bool,
                    const bool,
                    const uint64_t) const;

    void stopTimer();

private:
    // mutable cause if buf_size_ = 0 -> buf_size_ is unspecified ->
    // -> buf_suze_ = inputFile.size()
    // but it's not obvious why operator() change buf_size
    mutable uint64_t buf_size_;
    uint64_t key_;
    OperationType ot_;

    // mutable cause after stop we need to return stopTimer_ to false
    mutable std::atomic<bool> stopTimer_;

    bool startConvertation(const QString&, const QString&, const bool) const;
    bool modifyFile(QFile&, QFile&) const;

signals:
    void filesConverted() const;
    void changeProgressBar(uint64_t) const;
    void changeLabelFileCounter(QString) const;

    void showMessageBoxOnCollision(const QString& title, const QString& text, const QStringList& buttons, const QString& checkBoxText) const;
    void messageBoxOnCollisionAnswered() const;

public slots:
    void onMessageBoxOnCollisionAnswer(int buttonIndex, bool checkBoxState);
    void onStopTimerConverter();
};

} // namespace detail

#endif // CONVERTER_HPP
