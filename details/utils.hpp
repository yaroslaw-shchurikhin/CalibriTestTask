#ifndef UTILS_HPP
#define UTILS_HPP

#include <QStringList>
#include <QRegularExpression>
#include <QDir>

namespace detail
{

QStringList parseInputDirectory(const QDir&, const QRegularExpression&);

QString generateOutputFileNameWithCounter(const QString&, const QString&);

} // namespace detail

#endif // UTILS_HPP
