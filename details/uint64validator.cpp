#include "uint64validator.hpp"

namespace detail
{

Uint64Validator::Uint64Validator(uint64_t bottom, uint64_t top, QObject *parent) :
    QValidator(parent), m_bottom(bottom), m_top(top)
{ }

QValidator::State Uint64Validator::validate(QString &input, int &pos) const
{
    bool ok;
    uint64_t value = input.toULongLong(&ok);

    if (ok && value >= m_bottom && value <= m_top)
    {
        return Acceptable;
    }
    else
    {
        return Invalid;
    }
}

} // namespace detail
