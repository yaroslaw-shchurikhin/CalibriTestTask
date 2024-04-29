#include "converter.hpp"

#include "../mainwindow.hpp"
#include "utils.hpp"

#include <immintrin.h>
#include <windows.h>
#include <atomic>

#include <QFile>
#include <QTemporaryFile>
#include <QCoreApplication>
#include <QMessageBox>
#include <QCheckBox>
#include <QPushButton>

namespace detail
{

Converter::Converter(uint64_t buf_size, uint64_t key, Converter::OperationType ot) :
    buf_size_(buf_size),
    key_(key),
    ot_(ot),
    stopTimer_(false)
{ }

static uint8_t s_collisionButtonIdx = 0;
static bool s_toAll = false;
void Converter::operator()(const QString& inputPath,
                           const QString& fileMask,
                           const QString& outputPath,
                           QString& collisionRule,
                           const bool needToDeleteInputFiles,
                           const bool isTimer,
                           const uint64_t timerSeconds) const
{
    for(;;)
    {
        // get list of files to convert
        QStringList files = detail::parseInputDirectory(inputPath,
                                                        QRegularExpression::fromWildcard(fileMask));
        bool oneFileRename = false,
             oneFileSkip = false;

        int fileCounter = 0;
        for(auto&& f : files)
        {
            ++fileCounter;
            QString ifilepath = inputPath + '/' + f;
            QString ofilepath = outputPath + '/' + f;

            // choose output file name
            QFile out_file(ofilepath);

            QString ofile = f;
            if(out_file.exists())
            {
                if (collisionRule == "Спросить")
                {
                    emit showMessageBoxOnCollision("Коллизия имени файла",
                                                   QString("Уже существует файл с именем \"%1\". Что делать?").arg(f),
                                                   {"Перезаписать", "Пропустить", "Добавить счётчик к имени файла"},
                                                   "Применить для всех");

                    QEventLoop loop;
                    QMetaObject::Connection connection = connect(this,
                                                                 &Converter::messageBoxOnCollisionAnswered,
                                                                 &loop,
                                                                 &QEventLoop::quit);
                    loop.exec();
                    disconnect(connection);

                    if (s_collisionButtonIdx == 0) // rewrite
                    {
                        if (s_toAll)
                        {
                            collisionRule = "Перезаписать";
                        }
                    }
                    else if (s_collisionButtonIdx == 1) // skip
                    {
                        if (s_toAll)
                        {
                            collisionRule = "Пропустить";
                        }
                        else
                        {
                            oneFileSkip = true;
                        }
                    }
                    else if (s_collisionButtonIdx == 2) // add counter
                    {
                        if (s_toAll)
                        {
                            collisionRule = "Счётчик к имени файла";
                        }
                        else
                        {
                            oneFileRename = true;
                        }
                    }
                }

                if (collisionRule == "Счётчик к имени файла" || oneFileRename)
                {
                    oneFileRename = false;
                    ofile = detail::generateOutputFileNameWithCounter(f,
                                                                      outputPath);
                }
                else if(collisionRule == "Пропустить" || oneFileSkip)
                {
                    oneFileSkip = false;
                    continue;
                }
            }

            out_file.close();
            ofilepath = outputPath + '/' + ofile;

            emit changeLabelFileCounter(QString("%1/%2 %3").arg(fileCounter)
                                            .arg(files.size())
                                            .arg(f));
            emit changeProgressBar(0);

            bool result = startConvertation(ifilepath, ofilepath, this->buf_size_ == 0 ? false : true);

            if(result && needToDeleteInputFiles && ifilepath != ofilepath)
            {
                if(!QFile::remove(ifilepath))
                {
                    // i don't think that i need to handle it
                }
            }
        }

        if(isTimer && !this->stopTimer_.load())
        {
            QThread::sleep(timerSeconds);
        }
        else if (!isTimer || this->stopTimer_.load())
        {
            break;
        }
    }
    this->stopTimer_.store(false);
    emit this->filesConverted();
}

void Converter::stopTimer()
{
    this->stopTimer_.store(true);
}

bool Converter::startConvertation(const QString& inputFileName, const QString& outputFileName, const bool isBufSizeSpecified) const
{
    QFile inputFile(inputFileName);

    if (!inputFile.open(QIODevice::ReadOnly | QIODevice::Unbuffered))
    {
        return false;
    }

    if(!isBufSizeSpecified)
    {
        this->buf_size_ = inputFile.size();
    }

    QFile outputFile(outputFileName);
    if(inputFileName != outputFileName && !outputFile.exists())
    {
        if (!outputFile.open(QIODevice::WriteOnly | QIODevice::Unbuffered))
        {
            return false;
        }
    }
    else if (inputFileName != outputFileName && outputFile.exists())
    {
        if (!outputFile.open(QIODevice::WriteOnly | QIODevice::Unbuffered | QIODevice::Truncate))
        {
             return false;
        }
    }
    else
    {
        if (!outputFile.open(QIODevice::ReadWrite | QIODevice::Unbuffered))
        {
            return false;
        }
        if (!outputFile.seek(0))
        {
            return false;
        }
    }

    bool result = this->modifyFile(inputFile, outputFile);

    emit this->changeProgressBar(100);
    return result;
}

bool Converter::modifyFile(QFile& inputFile, QFile& outputFile) const
{
    const uint64_t onePercentOfFile = inputFile.size() / 100 == 0 ? 1 : inputFile.size() / 100;
    uint64_t convertSincePreviousCheck = 0;

    uint64_t buf_size_local = inputFile.size() > this->buf_size_ ? this->buf_size_ : inputFile.size();
    buf_size_local = buf_size_local % 32 == 0 ? buf_size_local : buf_size_local + (32 - (buf_size_local % 32));

    std::vector<uint8_t> inputBuffer(buf_size_local);
    std::vector<uint8_t> outputBuffer(buf_size_local);

    uint8_t progressPercent = 0;
    for (uint64_t offset = 0; offset < inputFile.size(); offset += buf_size_local)
    {
        // Read input file into buffer
        qint64 bytesRead = inputFile.read(reinterpret_cast<char*>(inputBuffer.data()),
                                          buf_size_local);
        if (bytesRead == -1)
        {
            inputFile.close();
            outputFile.close();
            return false;
        }

        inputBuffer.resize(bytesRead);
        outputBuffer.resize(bytesRead);

// Start operation
#ifdef __AVX__
        // AVX-256
        __m256i key = _mm256_set1_epi64x(this->key_);
        for (qint64 i = 0; i < buf_size_local; i += 32, convertSincePreviousCheck += 32)
        {
            __m256i data = _mm256_loadu_si256((__m256i*)(inputBuffer.data() + i));

            __m256i result;
            switch (this->ot_)
            {
            case XOR:
                result = _mm256_xor_si256(data, key);
                break;
            case AND:
                result = _mm256_and_si256(data, key);
                break;
            case OR:
                result = _mm256_or_si256(data, key);
                break;
            }

            if (convertSincePreviousCheck >= onePercentOfFile)
            {
                convertSincePreviousCheck = 0;
                progressPercent += 1;
                emit this->changeProgressBar(progressPercent);
            }
            _mm256_storeu_si256((__m256i*)(outputBuffer.data() + i), result);
        }

#else
        // SSE-128
        __m128i key = _mm_set1_epi32(static_cast<int>(this->key_));
        for (qint64 i = 0; i < buf_size_local; i += 16, convertSincePreviousCheck += 16)
        {
            __m128i data = _mm_loadu_si128((__m128i*)(inputBuffer.data() + i));

            __m128i result;
            switch (this->ot_)
            {
            case XOR:
                result = _mm_xor_si128(data, key);
                break;
            case AND:
                result = _mm_and_si128(data, key);
                break;
            case OR:
                result = _mm_or_si128(data, key);
                break;
            }

            if (convertSincePreviousCheck >= onePercentOfFile)
            {
                convertSincePreviousCheck = 0;
                progressPercent += 1;
                emit this->changeProgressBar(progressPercent);
            }
            _mm_storeu_si128((__m128i*)(outputBuffer.data() + i), result);
        }
#endif
        outputFile.write(reinterpret_cast<const char*>(outputBuffer.data()), inputFile.size());
        outputFile.flush();
    }

    inputFile.close();
    outputFile.close();
    return true;
}

void Converter::onMessageBoxOnCollisionAnswer(int buttonIndex, bool checkBoxState)
{
    if (buttonIndex == 0) // rewrite
    {
        s_collisionButtonIdx = 0;
    }
    else if (buttonIndex == 1) // skip
    {
        s_collisionButtonIdx = 1;
    }
    else if (buttonIndex == 2) // add counter
    {
        s_collisionButtonIdx = 2;
    }

    if (checkBoxState)
    {
        s_toAll = true;
    }

    emit messageBoxOnCollisionAnswered();
}

void Converter::onStopTimerConverter()
{
    stopTimer_ = true;
}

} // namespace detail
