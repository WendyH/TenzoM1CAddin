#include "TenzoM.h"
#include <string>
using namespace std;

/// <summary>
/// Открыть COM порт для общения с устройствами тензотермическими датчиками
/// </summary>
/// <param name="portNumber"></param>
/// <param name="boud"></param>
/// <returns></returns>
BOOL TenzoM::OpenPort(int portNumber, DWORD boud, BYTE deviceAddr)
{
    if (PortOpened)
        ClosePort();

    Adr = deviceAddr;

    if (Emulate) return TRUE;

    SECURITY_ATTRIBUTES sa;
    ZeroMemory(&sa, sizeof(sa));
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;

    wstring comID = L"\\\\.\\COM" + to_wstring(portNumber);

    port = CreateFileW(comID.c_str(), GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, &sa, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, 0);

    if (port == INVALID_HANDLE_VALUE) {
        PortOpened = FALSE;
        return FALSE;
    }
    PortOpened = TRUE;

    EscapeCommFunction(port, SETDTR);

    DCB dcb = { 0 };
    dcb.DCBlength = sizeof(dcb);
    GetCommState(port, &dcb);
    dcb.BaudRate      = boud;
    dcb.fBinary       = TRUE;
    dcb.fAbortOnError = TRUE;
    dcb.ByteSize      = 8;
    dcb.Parity        = NOPARITY;
    dcb.StopBits      = ONESTOPBIT;
    SetCommState(port, &dcb);

    PurgeComm(port, PURGE_RXABORT | PURGE_RXCLEAR | PURGE_TXABORT | PURGE_TXCLEAR);

    COMMTIMEOUTS commTimeouts;
    if (GetCommTimeouts(port, &commTimeouts)) {
        commTimeouts.ReadIntervalTimeout         = MAXDWORD;
        commTimeouts.ReadTotalTimeoutMultiplier  = 10;
        commTimeouts.ReadTotalTimeoutConstant    = 50;
        commTimeouts.WriteTotalTimeoutMultiplier = 10;
        commTimeouts.WriteTotalTimeoutConstant   = 50;
        SetCommTimeouts(port, &commTimeouts);
    }

    return TRUE;
}

/// <summary>
/// Закрывает COM-порт и все дескрипторы.
/// Завершаем работу с весами по данному Com-порту.
/// </summary>
void TenzoM::ClosePort()
{
    try {
        if (port && PortOpened) {
            EscapeCommFunction(port, CLRDTR | CLRRTS);
            CloseHandle(port);
        }
    }
    catch (exception ex)
    {
    }
    port = 0;
    PortOpened = FALSE;
}

/// <summary>
/// Передать в весы последовательность байт в буфере через открытый Com-порт.
/// </summary>
/// <param name="message">Ссылка на буфер, содержащий данные для передачи.</param>
/// <param name="msgSize">Длина передаваемых данных</param>
/// <returns>Если завершилось всё без ошибок и данные переданы - возвращает TRUE. Иначе нет.</returns>
BOOL TenzoM::Send(unsigned char* message, DWORD msgSize)
{
    BOOL       bSuccess       = FALSE;
    OVERLAPPED osWrite        = { 0 };
    DWORD      dwBytesWritten = 0;

    // "Подписываем сообщение" - устанавливаем трейтий байт с конца (перед FF FF) как CRC сообщения
    SetCrcOfMessage(message, msgSize);

    osWrite.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    if (osWrite.hEvent)
    {
        if (!WriteFile(port, message, msgSize, &dwBytesWritten, &osWrite)) {
            LastError = GetLastError();
            if (LastError == ERROR_IO_PENDING) {
                // Write is pending.
                if (GetOverlappedResult(port, &osWrite, &dwBytesWritten, TRUE))
                    // Write operation completed successfully.
                    bSuccess = TRUE;
            }
        }
        else
        {
            // WriteFile completed immediately.
            bSuccess = TRUE;
        }
        CloseHandle(osWrite.hEvent);
    }

    return bSuccess;
}

/// <summary>
/// Послать команду весам без дополнительных передаваемых данных.
/// </summary>
/// <param name="command">Байт-команда</param>
/// <returns>Возвращает TRUE - если передача успешно завершилась</returns>
BOOL TenzoM::SendCommand(BYTE command)
{
    BYTE message[] = { 0xFF, Adr, command, 0x00, 0xFF, 0xFF };
    return Send(message, sizeof(message));
}

/// <summary>
/// Послать команду весам с указанимем дополнительного байта данных
/// </summary>
/// <param name="command">Байт-команда</param>
/// <param name="data">Дополнительный байт-данные</param>
/// <returns>Возвращает TRUE - если передача успешно завершилась</returns>
BOOL TenzoM::SendCommand(BYTE command, BYTE data)
{
    BYTE message[] = { 0xFF, Adr, command, data, 0x00, 0xFF, 0xFF };
    return Send(message, sizeof(message));
}

/// <summary>
/// Послать команду весам с дополнительными данными.
/// </summary>
/// <param name="command">Байт-команда</param>
/// <param name="lpData">Буфер с дополнительными данными</param>
/// <param name="dataLenght">Длина дополнительных данных</param>
/// <returns>Возвращает TRUE - если передача успешно завершилась</returns>
BOOL TenzoM::SendCommand(BYTE command, BYTE* lpData, size_t dataLenght)
{
    ZeroMemory(readBuffer, RECV_BUFFER_LENGHT);
    readBuffer[0] = 0xFF;
    readBuffer[1] = Adr;
    readBuffer[2] = command;
    memcpy(readBuffer + 3, lpData, dataLenght);
    int i = 3 + (int)dataLenght;
    readBuffer[i + 0] = 0;
    readBuffer[i + 1] = 0xFF;
    readBuffer[i + 2] = 0xFF;

    return Send(readBuffer, (DWORD)dataLenght + 6);
}

/// <summary>
/// Получить ответ от весов в указанный буфер.
/// </summary>
/// <returns>Возвращает количество считанных байт в буфер. Если 0 - тогда проверять статус или LastError.</returns>
DWORD TenzoM::Receive()
{
    BOOL  bSuccess      = FALSE;
    DWORD dwResult      = 0;
    DWORD dwReadBytes   = 0;
    OVERLAPPED osReader = { 0 };

    memset(readBuffer, 0, RECV_BUFFER_LENGHT);

    osReader.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);

    if (osReader.hEvent)
    {
        Sleep(100);
        if (!ReadFile(port, readBuffer, RECV_BUFFER_LENGHT, &dwReadBytes, &osReader)) {
            LastError = GetLastError();
            if (LastError == ERROR_IO_PENDING)
            {
                dwResult = WaitForSingleObject(osReader.hEvent, READ_TIMEOUT);
                switch (dwResult)
                {
                    // Read completed.
                    case WAIT_OBJECT_0:
                        if (!GetOverlappedResult(port, &osReader, &dwReadBytes, FALSE))
                        {
                            // Error in communications; report it.
                            LastError = GetLastError();
                        }
                        else
                        {
                            // Read completed successfully.
                            bSuccess = TRUE;
                        }
                        break;

                    case WAIT_TIMEOUT:
                        // Operation isn't complete yet. fWaitingOnRead flag isn't
                        // changed since I'll loop back around, and I don't want
                        // to issue another read until the first one finishes.
                        //
                        // This is a good time to do some background work.
                        break;

                    default:
                        // Error in the WaitForSingleObject; abort.
                        // This indicates a problem with the OVERLAPPED structure's
                        // event handle.
                        LastError = GetLastError();
                        break;
                }
            }
        }
        else
        {
            // read completed immediately
            bSuccess = TRUE;
        }
    }
    return dwReadBytes;
}

/// <summary>
/// Устанавливает байт CRC для передаваемого сообщения
/// </summary>
/// <param name="buffer">Адрес сообщения</param>
/// <param name="bufSize">Длина сообщения</param>
void TenzoM::SetCrcOfMessage(BYTE* buffer, int bufSize)
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
int TenzoM::ExtractWeight()
{
    int   dwWeight   = 0;
    BYTE* lpBuf      = readBuffer + 3; // Указывает на начало последовательности байтов веса
    int   bytes      = 3; // Количество байт для считывания упакованного в BCD веса (у TenzoM - 3)
    int   multiplier = 1;

    // Распаковка цифр веса из BCD формата (младшие байты первые)
    while (bytes--)
    {
        dwWeight += ((*lpBuf >> 4) * 10 + (*lpBuf & 0x0F)) * multiplier;
        multiplier *= 100;
        lpBuf++;
    }

    // За весом в полученном буфере идёт байт CON с дополнительными фалагами
    BYTE con = *lpBuf; 

    // Т.к. возврращаем в граммах, то считаем, сколько добавить нулей к результату
    int addNulls = 3 - (con & 0x07); // Первые три бита - позиция запятой
    multiplier = 1; while (addNulls--) multiplier *= 10;

    if (con & 0x80) dwWeight *= -1; // Определяем, стоит ли флаг минус

    Calm     = (con & 0x10); // Флаг успокоения веса (вес стабилен)
    Overload = (con & 0x08); // Флаг перегруза

    return dwWeight * multiplier;
}


/////////////////////////////////////////////////////////////////////////////////////////////

/// <summary>
/// Установить устройству новый адрес
/// </summary>
/// <param name="deviceAddress">Значение нового адреса: 0x01...0xFD</param>
void TenzoM::SetDeviceAddress(BYTE deviceAddress)
{
    SendCommand(0xA0, deviceAddress);

    if (Receive())
    {
        Adr = deviceAddress;
    }
}

/// <summary>
/// Получить зафикированный вес брутто
/// </summary>
/// <returns>Возвращает зафикированный вес брутто</returns>
int TenzoM::GetFixedBrutto()
{
    int fixedBrutto = 0;

    SendCommand(0xB8);

    if (Receive())
    {
        fixedBrutto = ExtractWeight();
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

    SendCommand(0xBF);

    if (Receive())
    {
        BYTE statusByte = readBuffer[3];
        status.Reset          = (BOOL)(statusByte & 0x80);
        status.Error          = (BOOL)(statusByte & 0x40);
        status.Netto          = (BOOL)(statusByte & 0x20);
        status.KeyPressed     = (BOOL)(statusByte & 0x10);
        status.EndDosing      = (BOOL)(statusByte & 0x08);
        status.WeightFixed    = (BOOL)(statusByte & 0x04);
        status.ADCCalibration = (BOOL)(statusByte & 0x02);
        status.Dosing         = (BOOL)(statusByte & 0x01);
    }

    return status;
}

/// <summary>
/// Обнулить показания веса
/// </summary>
void TenzoM::SetZero()
{
    SendCommand(0xBF);
    Receive();
}

/// <summary>
/// Получить вес НЕТТО
/// </summary>
/// <returns>Возвращает вес НЕТТО</returns>
int TenzoM::GetNetto()
{
    int netto = 0;

    SendCommand(0xC2);

    if (Receive())
    {
        netto = ExtractWeight();
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
    int brutto = 0;
    SendCommand(0xC3);

    if (Receive())
    {
        brutto = ExtractWeight();
    }

    return brutto;
}

/// <summary>
/// Получить значения индикаторов (то, что выводится на экране терминала)
/// </summary>
/// <returns>Возвращается вес текстом, который отображается на экране терминала.</returns>
LPSTR TenzoM::GetIndicatorText()
{
    SendCommand(0xC6);

    DWORD dwBytesRead = Receive();
    if (dwBytesRead > 5)
    {
        BYTE statusByte = readBuffer[dwBytesRead - 3];
        Calm = (BOOL)(statusByte & 0x01);
        readBuffer[dwBytesRead - 3];
        return (LPSTR)readBuffer + 3;
    }
    readBuffer[0] = 0;
    return (LPSTR)readBuffer;
}

/// <summary>
/// Получить счётчик
/// </summary>
/// <param name="numCounter">Номер счетчика (от 0 до 9)</param>
/// <returns></returns>
int TenzoM::GetCounter(BYTE numCounter)
{
    int counter = 0;
    SendCommand(0xC8, numCounter);

    // TODO: ?? test GetCounter

    return counter;
}

void TenzoM::SwitchToWeighing()
{
    SendCommand(0xCD);
    Receive();
}


int TenzoM::RandomWeight()
{
    return 0;
}
