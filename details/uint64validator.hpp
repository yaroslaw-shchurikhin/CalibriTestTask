#ifndef UINT64VALIDATOR_H
#define UINT64VALIDATOR_H

#include <QValidator>

namespace detail
{

class Uint64Validator : public QValidator
{
    Q_OBJECT

public:
    explicit Uint64Validator(uint64_t bottom, uint64_t top, QObject *parent = nullptr);

    State validate(QString &input, int &pos) const override;

private:
    uint64_t m_bottom;
    uint64_t m_top;
};

} // namespace detail

#endif // UINT64VALIDATOR_H
