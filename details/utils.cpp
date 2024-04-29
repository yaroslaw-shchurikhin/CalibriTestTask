#include "utils.hpp"

#include <QRegularExpression>

namespace detail
{

QStringList parseInputDirectory(const QDir& dir, const QRegularExpression& regex)
{
    QStringList fileList = dir.entryList(QStringList() << "*", QDir::Files, QDir::Name);
    QStringList matchedFileList;
    for (const QString& fileName : fileList)
    {
        if (regex.match(fileName).hasMatch())
        {
            matchedFileList.append(fileName);
        }
    }

    qDebug() << "Matched files:";
    for (const QString& fileName : matchedFileList)
    {
        qDebug() << fileName;
    }

    return matchedFileList;
}

QString generateOutputFileNameWithCounter(const QString &inputFileName, const QString &inputDir)
{
    QString baseName = QFileInfo(inputFileName).baseName();
    QString extension = QFileInfo(inputFileName).suffix();
    QString numberStr = QFileInfo(inputFileName).completeSuffix();
    QString outputDir = inputDir;

    int number = 1;
    if (!numberStr.isEmpty())
    {
        numberStr.remove(0, 1); // delete '('
        numberStr.remove(numberStr.length() - 1, 1); // delete ')'
        number = numberStr.toInt() + 1;
    }

    QString outputFileName = baseName + QString("(%1).%2").arg(number).arg(extension);
    QFile outputFile(outputDir + "/" + outputFileName);

    while (outputFile.exists())
    {
        ++number;
        outputFileName = baseName + QString("(%1).%2").arg(number).arg(extension);
        outputFile.setFileName(outputDir + "/" + outputFileName);
    }

    return outputFileName;
}

} //namespace detail
