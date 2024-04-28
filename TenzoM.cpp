#include <random>
#include "TenzoM.h"

using namespace std;

#define MAX_RECV_BYTES 1024*4

/// <summary>
/// Открыть COM порт для общения с устройствами тензотермическими датчиками
/// </summary>
/// <param name="comName"></param>
/// <param name="boud"></param>
/// <param name="boud"></param>
/// <returns></returns>
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
        vector<char> message = { 0x01, 0x30, 0x30, 0x30, 0x31, 0x00 };
        snprintf(&message.at(1), 5, "%04d", (int)Adr);
        success = com.Write(&message.at(0), 5);
        if (success)
        {
            success = (com.ReadChar(success) == '\xFF');
            com.Delay(20);
        }
    }

    if (!success)
        LastError = GetLastError();

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
/// Передать в весы последовательность байт в буфере через открытый Com-порт.
/// </summary>
/// <param name="message">Ссылка на буфер, содержащий данные для передачи.</param>
/// <param name="msgSize">Длина передаваемых данных</param>
/// <returns>Если завершилось всё без ошибок и данные переданы - возвращает TRUE. Иначе нет.</returns>
bool TenzoM::Send(vector<char> message, long msgSize)
{
    if (Protocol == eProtocolTenzoM) {
        // "Подписываем сообщение" - устанавливаем трейтий байт с конца (перед FF FF) как CRC сообщения
        SetCrcOfMessage(message, msgSize);
    }

    return com.Write(&message.at(0), msgSize);
}

/// <summary>
/// Послать команду весам без дополнительных передаваемых данных.
/// </summary>
/// <param name="command">Байт-команда</param>
/// <returns>Возвращает TRUE - если передача успешно завершилась</returns>
bool TenzoM::SendCommand(char command)
{
    vector<char> message = { '\xFF', (char)Adr, command, '\x00', '\xFF', '\xFF' };
    return Send(message, sizeof(message));
}

/// <summary>
/// Послать команду весам с указанимем дополнительного байта данных
/// </summary>
/// <param name="command">Байт-команда</param>
/// <param name="data">Дополнительный байт-данные</param>
/// <returns>Возвращает TRUE - если передача успешно завершилась</returns>
bool TenzoM::SendCommand(char command, char data)
{
    vector<char> message = { '\xFF', (char)Adr, command, data, '\x00', '\xFF', '\xFF' };
    return Send(message, sizeof(message));
}

/// <summary>
/// Получить ответ от весов в указанный буфер.
/// </summary>
/// <returns>Возвращает количество считанных байт в буфер. Если 0 - тогда проверять статус или LastError.</returns>
long TenzoM::Receive(vector<char> readBuffer)
{
    long readBytes    = 0;
    bool successRead  = false;
    char currentChar  = 0;
    char previousChar = 0;
    int  offset       = 0;

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
        readBytes = offset + 1;
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

    return readBytes;
}

/// <summary>
/// Устанавливает байт CRC для передаваемого сообщения
/// </summary>
/// <param name="buffer">Адрес сообщения</param>
/// <param name="bufSize">Длина сообщения</param>
void TenzoM::SetCrcOfMessage(vector<char> buffer, long bufSize)
{
    if (bufSize < 6) return; // minimum message lenght
    buffer[bufSize - 3] = 0; // set crc to 0
    BYTE byte, crc = 0, polinom = 0x69;
    for (int i = 1; i < bufSize - 2; i++)
    {
        byte = buffer[i];
        if ((byte == 0xFF) || (byte == 0xFE)) continue;
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
    buffer[bufSize - 3] = crc;
}

/// <summary>
/// Извлечение веса из полученных данных
/// </summary>
/// <returns>Возвращает полученный вес в граммах.
/// Также устанавливает свойства стабильности веса и перегруза.</returns>
int TenzoM::ExtractWeight(vector<char> readBuffer)
{
    int   dwWeight = 0;
    char* lpBuf = &readBuffer.at(2); // Указывает на начало последовательности байтов веса
    int   bytes = 3; // Количество байт для считывания упакованного в BCD веса (у TenzoM - 3)
    int   multiplier = 1;

    // Распаковка цифр веса из BCD формата (младшие байты первые)
    while (bytes--)
    {
        dwWeight += ((*lpBuf >> 4) * 10 + (*lpBuf & 0x0F)) * multiplier;
        multiplier *= 100;
        lpBuf++;
    }

    // За весом в полученном буфере идёт байт CON с дополнительными фалагами
    char con = *lpBuf;

    // Т.к. возврращаем в граммах, то считаем, сколько добавить нулей к результату
    int addNulls = 3 - (con & 0x07); // Первые три бита - позиция запятой
    multiplier = 1; while (addNulls--) multiplier *= 10;

    if (con & 0x80) dwWeight *= -1; // Определяем, стоит ли флаг минус

    Calm = (con & 0x10); // Флаг успокоения веса (вес стабилен)
    Overload = (con & 0x08); // Флаг перегруза

    return dwWeight * multiplier;
}


/////////////////////////////////////////////////////////////////////////////////////////////

/// <summary>
/// Получить зафикированный вес брутто
/// </summary>
/// <returns>Возвращает зафикированный вес брутто</returns>
int TenzoM::GetFixedBrutto()
{
    int fixedBrutto = 0;

    if (Protocol == eProtocolTenzoM)
    {
        if (SendCommand(0xB8))
        {
            vector<char> readBuffer;
            if (Receive(readBuffer))
            {
                fixedBrutto = ExtractWeight(readBuffer);
            }
        }
    }

    return fixedBrutto;
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
        if (SendCommand(0xBF))
        {
            vector<char> readBuffer;
            if (Receive(readBuffer))
            {
                unsigned char statusByte = readBuffer.at(2);
                status.Reset = (statusByte & 0x80);
                status.Error = (statusByte & 0x40);
                status.Netto = (statusByte & 0x20);
                status.KeyPressed = (statusByte & 0x10);
                status.EndDosing = (statusByte & 0x08);
                status.WeightFixed = (statusByte & 0x04);
                status.ADCCalibration = (statusByte & 0x02);
                status.Dosing = (statusByte & 0x01);
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
    bool success;

    if (Protocol == eProtocol643)
    {
        success = com.WriteChar(0x0D);
        if (success)
        {
            com.Delay(20);
            success = (com.ReadChar(success) == 0xFF);
        }
    }
    else if (Protocol == eProtocolTenzoM)
    {
        if (SendCommand(0xBF))
        {
            vector<char> readBuffer;
            return Receive(readBuffer) > 0;
        }
    }
}

/// <summary>
/// Получить вес НЕТТО
/// </summary>
/// <returns>Возвращает вес НЕТТО</returns>
int TenzoM::GetNetto()
{
    if (Emulate) return RandomWeight();

    if (Protocol == eProtocol643)
    {
        return GetWeight643();
    }

    int netto = 0;

    if (Protocol == eProtocolTenzoM)
    {
        if (SendCommand(0xC2))
        {
            vector<char> readBuffer;
            if (Receive(readBuffer))
            {
                netto = ExtractWeight(readBuffer);
            }
        }

    }

    return netto;
}

/// <summary>
/// Получить вес БРУТТО
/// </summary>
/// <returns>Возвращает вес БРУТТО</returns>
int TenzoM::GetBrutto()
{
    if (Emulate) return RandomWeight();

    if (Protocol == eProtocol643)
    {
        return GetWeight643();
    }

    int brutto = 0;

    if (Protocol == eProtocolTenzoM)
    {
        if (SendCommand(0xC3))
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
/// Получить значения индикаторов (то, что выводится на экране терминала)
/// </summary>
/// <returns>Возвращается вес текстом, который отображается на экране терминала.</returns>
char* TenzoM::GetIndicatorText()
{
    vector<char> readBuffer;

    if (Protocol == eProtocolTenzoM)
    {
        SendCommand(0xC6);

        const auto bytesRead = Receive(readBuffer);
        if (bytesRead > 5)
        {
            char statusByte = readBuffer.at(bytesRead - 2);
            Calm = (statusByte & 0x01);
            return &readBuffer.at(2);
        }
    }

    return &readBuffer.at(0);
}

void TenzoM::SwitchToWeighing()
{
    bool success;

    switch (Protocol)
    {
    case TenzoM::eProtocolTenzoM:
    {
        if (SendCommand(0xCD))
        {
            vector<char> readBuffer;
            Receive(readBuffer);
        }
        break;
    }
    case TenzoM::eProtocol643:
    {
        if (com.WriteChar(0x18))
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

    bool success = com.WriteChar(0x10);

    if (success)
    {
        const auto bytesRead = Receive(readBuffer);

        if (bytesRead > 8)
        {
            //=  2859. 
            //=   286.$
            string text(&readBuffer.at(1), 6);
            brutto = stoi(text, 0, 10) * 1000;

            char lastByte = readBuffer.at(8);
            if (!(lastByte & 0x04))
            {
                brutto /= 10;
            }
            Calm = (lastByte & 0x20);
            success = true;
        }
    }
    
    if (!success) LastError = GetLastError();

    return brutto;
}
