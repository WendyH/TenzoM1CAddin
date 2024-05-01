#include <iostream>
#include <string>
#include <locale>
#include <codecvt>
#include <random>
#include "TenzoM.h"

#if defined(_WIN64) || defined(__WIN32__) || defined(_WIN32) || defined(WIN32) || defined(__WINDOWS__) || defined(__TOS_WIN__) || defined(__CYGWIN__)
    #define TM_WINDOWS 
#elif defined(unix) || defined(__unix) || defined(__unix__)
    #define TM_LINUX
#endif

using namespace std;

#define MAX_RECV_BYTES 1024*4

/// <summary>
/// Открыть COM порт для общения с устройствами тензотермическими датчиками
/// </summary>
/// <param name="comName">Имя COM-порта в виде "COM1" для Windows или "tty1" для Linux</param>
/// <param name="boud">Скорость обмена (бод)</param>
/// <param name="deviceAddress">Адрес устройсва, как он прописан в самих весах</param>
/// <returns>Возвращает true, если порт успешно открыт</returns>
bool TenzoM::OpenPort(string comName, long boud, int deviceAddress)
{
    Adr = deviceAddress;

    if (Emulate) return true;

    com.SetBaudRate(boud);
    com.SetPortName(comName);

    bool success = (com.Open() == 0);

    if (success && (Protocol == eProtocol643))
    {
        // Активация терминала. Посылаем 01 с адресом устройсва как строка из 4 цифр.
        // Работать с весами можно через 20 мс.
        vector<char> message = { '\x01', '\x30', '\x30', '\x30', '\x31', '\x00'};
        snprintf(&message.at(1), 5, "%04d", (int)Adr);
        success = com.Write(&message.at(0), 5);
        if (success)
        {
            success = (com.ReadChar(success) == '\xFF');
            com.Delay(20);
        }
    }

    if (!success) CheckLastError();

    return success;
}

/// <summary>
/// Закрывает COM-порт и все дескрипторы.
/// Завершаем работу с весами по данному Com-порту.
/// </summary>
void TenzoM::ClosePort()
{
    try
    {
        com.Close();
    }
    catch (exception e)
    {

    }
}

/// <summary>
/// Определяет, подключены ли мы к устройству через COM-порт
/// </summary>
/// <returns>Возвращает true, если к устройству подключены</returns>
bool TenzoM::PortOpened()
{
    return com.IsOpened();
}

/// <summary>
/// Послать команду весам без дополнительных передаваемых данных.
/// </summary>
/// <param name="command">Байт-команда</param>
/// <returns>Возвращает TRUE - если передача успешно завершилась</returns>
bool TenzoM::SendCommand(char command)
{
    bool success = false;
    vector<char> message;

    switch (Protocol)
    {
    case TenzoM::eProtocolTenzoM:
        message.push_back('\xFF');    // Начало кадра
        message.push_back((char)Adr); // Байт адреса устройства
        message.push_back(command);
        message.push_back('\x00');    // Байт для контрольной суммы
        message.push_back('\xFF');    // \ 
        message.push_back('\xFF');    // / Конец кадра
        // "Подписываем" - устанавливаем трейтий байт с конца (перед FF FF) как CRC сообщения
        SetCrcOfMessage(message, message.size());

        success = com.Write(&message.at(0), message.size());

        break;
    case TenzoM::eProtocol643:
        success = com.WriteChar(command);
        break;
    case TenzoM::eProtocolNet:
        break;
    case TenzoM::eProtocolWeb:
        break;
    default:
        break;
    }

    if (!success) CheckLastError();

    return success;
}

/// <summary>
/// Получить ответ от весов в указанный буфер.
/// </summary>
/// <returns>Возвращает количество считанных байт в буфер</returns>
long TenzoM::Receive(vector<char>& readBuffer)
{
    long readBytes    = 0;
    bool successRead  = false;
    char currentChar  = 0;
    char previousChar = 0;

    readBuffer.clear();

    if (Protocol == eProtocolTenzoM)
    {
        bool packetBegan = false;
        // Иногда в ответ идут всякие разные символы.
        // Ждем начало пакета 0xFF, но чтобы следующий символ небыл равен 0xFE.
        do
        {
            currentChar = com.ReadChar(successRead);
            
            if (successRead)
            {
                readBytes++;
                if (readBytes > MAX_RECV_BYTES)
                    break;
            }
            
            if (!packetBegan && (previousChar == '\xFF') && (currentChar != '\xFF') && (currentChar != '\xFE'))
            {
                // Это начало пакета. Начинаем записывать в буффер.
                packetBegan = true; // Включаем признак, что пакет начался.
            }
            if (packetBegan && (previousChar == '\xFF') && (currentChar == '\xFF'))
            {
                // Это конец пакета.
                break;
            }
            if (packetBegan)
            {
                readBuffer.push_back(currentChar);
            }

            previousChar = currentChar;
        } while (successRead);
    }
    else
    {
        do
        {
            currentChar = com.ReadChar(successRead);
            if (successRead)
            {
                readBytes++;
                if (readBytes > MAX_RECV_BYTES)
                    break;
                readBuffer.push_back(currentChar);
            }
        } while (successRead);
    }

    return readBuffer.size();
}

/// <summary>
/// Устанавливает байт CRC для передаваемого сообщения
/// </summary>
/// <param name="buffer">Адрес сообщения</param>
/// <param name="bufSize">Длина сообщения</param>
void TenzoM::SetCrcOfMessage(vector<char>& buffer, long bufSize)
{
    if (bufSize < 6) return; // minimum message lenght
    buffer.at(bufSize - 3) = 0; // set crc to 0
    char byte, crc = 0; constexpr char polinom = '\x69';
    for (int i = 1; i < bufSize - 2; i++)
    {
        byte = buffer.at(i);
        if ((byte == '\xFF') || (byte == '\xFE')) continue;
        for (int j = 0; j < 8; j++)
        {
            if (crc & (1 << 7))
            {
                crc *= 2;
                if (byte & (1 << 7)) crc++;
                crc ^= polinom;
            }
            else
            {
                crc *= 2;
                if (byte & (1 << 7)) crc++;
            }
            byte *= 2;
        }
        byte = 0;
    }
    buffer.at(bufSize - 3) = crc;
}

/// <summary>
/// Извлечение веса из полученных данных
/// </summary>
/// <returns>Возвращает полученный вес в граммах.
/// Также устанавливает свойства стабильности веса и перегруза.</returns>
int TenzoM::ExtractWeight(vector<char>& readBuffer)
{
    int       weight     = 0; // Вычисленный вес
    const int offset     = 2; // Смещение от начала буфера, где идут байты веса
    const int bytes      = 3; // Количество байт для считывания упакованного в BCD веса (у TenzoM - 3)
    int       multiplier = 1; // Множитель (вычисляется для разных разрядов)

    // Распаковка цифр веса из BCD формата (младшие байты первые)
    for (int i = 0; i < bytes; i++)
    {
        auto byte = readBuffer.at(offset + i);
        weight += ((byte >> 4) * 10 + (byte & '\x0F')) * multiplier;
        multiplier *= 100;
    }

    // За весом в полученном буфере идёт байт CON с дополнительными фалагами
    const char con = readBuffer.at(offset + bytes);

    // Т.к. возврращаем в граммах, то считаем, сколько добавить нулей к результату
    int addNulls = 3 - (con & '\x07'); // Первые три бита - позиция запятой
    multiplier = 1; while (addNulls--) multiplier *= 10;

    if (con & '\x80') weight *= -1; // Определяем, стоит ли флаг минус

    Calm = (con & '\x10'); // Флаг успокоения веса (вес стабилен)
    Overload = (con & '\x08'); // Флаг перегруза

    return weight * multiplier;
}

/// <summary>
/// Получить состояние весоизмерительной системы
/// </summary>
/// <returns>Струткура, содержащая фалги состояния весов</returns>
TenzoMSTATUS TenzoM::GetStatus()
{
    TenzoMSTATUS status = { 0 };

    if (Protocol == eProtocolTenzoM)
    {
        if (SendCommand('\xBF'))
        {
            vector<char> readBuffer;
            if (Receive(readBuffer))
            {
                const char statusByte = readBuffer.at(2);
                status.Reset          = (statusByte & '\x80');
                status.Error          = (statusByte & '\x40');
                status.Netto          = (statusByte & '\x20');
                status.KeyPressed     = (statusByte & '\x10');
                status.EndDosing      = (statusByte & '\x08');
                status.WeightFixed    = (statusByte & '\x04');
                status.ADCCalibration = (statusByte & '\x02');
                status.Dosing         = (statusByte & '\x01');
            }

        }

    }

    return status;
}

/// <summary>
/// Обнулить показания веса
/// </summary>
bool TenzoM::SetZero()
{
    bool success = false;

    if (Protocol == eProtocol643)
    {
        success = com.WriteChar('\x0D');
        if (success)
        {
            com.Delay(20);
            success = (com.ReadChar(success) == '\xFF');
        }
    }
    else if (Protocol == eProtocolTenzoM)
    {
        if (SendCommand('\xBF'))
        {
            vector<char> readBuffer;
            success = Receive(readBuffer) > 0;
        }
    }

    return success;
}

/// <summary>
/// Получить вес
/// </summary>
/// <returns>Возвращает вес брутто</returns>
int TenzoM::GetWeight()
{
    if (Emulate) return RandomWeight();

    if (Protocol == eProtocol643)
    {
        return GetWeight643();
    }

    int brutto = 0;

    if (Protocol == eProtocolTenzoM)
    {
        if (SendCommand('\xC3'))
        {
            vector<char> readBuffer;
            if (Receive(readBuffer))
            {
                brutto = ExtractWeight(readBuffer);
            }

        }
    }

    return brutto;
}


/// <summary>
/// Переключить в режим взвешивания
/// </summary>
void TenzoM::SwitchToWeighing()
{
    bool success;

    switch (Protocol)
    {
    case TenzoM::eProtocolTenzoM:
    {
        if (SendCommand('\xCD'))
        {
            vector<char> readBuffer;
            Receive(readBuffer);
        }
        break;
    }
    case TenzoM::eProtocol643:
    {
        if (com.WriteChar('\x18'))
        {
            com.ReadChar(success);
        }
        break;
    }
    case TenzoM::eProtocolNet:
        break;
    case TenzoM::eProtocolWeb:
        break;
    default:
        break;
    }
}

/// <summary>
/// Получение списка доступных COM-портов
/// </summary>
/// <returns>Возвращается строка с именами доступных COM-портов, разделёнными точкой с запятой.</returns>
string TenzoM::GetFreeComPorts()
{
    string ports = { 0 };
    ceSerial  _com;
    #ifdef CE_WINDOWS
        for (int i = 1; i < 255; i++)
        {
            string name = "COM" + i;
            _com.SetPortName(name);
            _com.Open();
            if (_com.Open() == 0)
            {
                if (ports.size() > 0)
                    ports += ";";
                _com.Close();
                ports += name;
            }
        }
    #endif
    #ifdef TM_LINUX
    #endif
    
    return string(ports);
}

/// <summary>
/// Получение рандомного веса
/// </summary>
/// <returns>Случайный показатель веса</returns>
int TenzoM::RandomWeight()
{
    if (!emulTargetWeight)
    {
        random_device dev;
        mt19937 rng(dev());
        uniform_int_distribution<mt19937::result_type> dist48_145(48000, 145000);

        emulTargetWeight = dist48_145(rng);

        emulMaxOffset = (emulTargetWeight - emulCurrentWeight) / emulTotalSteps;
        if ((emulMaxOffset > -2) && (emulMaxOffset < 2))
        {
            if (emulMaxOffset < 0)
                emulMaxOffset = -2;
            else
                emulMaxOffset = 2;
        }

    }

    if (emulCurrentWeight != emulTargetWeight)
    {
        Calm = false;
        const int offset = (emulTargetWeight - emulCurrentWeight) / 2;
        emulCurrentWeight += (offset / 2);
        if (offset < emulMaxOffset) {
            emulCurrentWeight = emulTargetWeight;
            emulTargetWeight = 0;
            Calm = true;
        }
    }
    else
    {
        emulTargetWeight = 0;
        Calm = true;
    }

    return emulCurrentWeight;
}

/// <summary>
/// Получить вес по протокоглу 6.43
/// </summary>
/// <returns>Возвращает вес на индикаторе</returns>
int TenzoM::GetWeight643()
{
    int brutto = 0;
    vector<char> readBuffer;

    bool success = com.WriteChar('\x10');

    if (success)
    {
        const auto bytesRead = Receive(readBuffer);

        if (bytesRead > 8)
        {
            //=  2859. 
            //=   286.$
            string text(&readBuffer.at(1), 6);
            brutto = stoi(text, 0, 10) * 1000;

            const char lastByte = readBuffer.at(8);
            if (!(lastByte & '\x04'))
            {
                brutto /= 10;
            }
            Calm = (lastByte & '\x20');
            success = true;
        }
    }
    
    if (!success) CheckLastError();

    return brutto;
}

void TenzoM::CheckLastError()
{
    LastError = 0;
    Error.clear();
    #ifdef CE_WINDOWS
        LastError = GetLastError();
        if (LastError != 0)
        {
            LPSTR messageBuffer = nullptr;
            const size_t size = FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                NULL, LastError, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&messageBuffer, 0, NULL);

            string errorText(messageBuffer);
            Error.assign(errorText.begin(), errorText.end());

            LocalFree(messageBuffer);
        }
    #endif
    #ifdef TM_LINUX
        LastError = errno;
        Error     = strerror(errno);
    #endif
}
