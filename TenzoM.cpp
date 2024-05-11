#include <iostream>
#include <string>
#include <locale>
#include <codecvt>
#include <random>
#include <regex>
#include <fstream>
#include "TenzoM.h"

#if ISWINDOWS

#else
    #define ERROR_FILE_NOT_FOUND 2L
    #include <unistd.h>
	#include <fcntl.h>
	#include <termios.h>
	#include <sys/ioctl.h>
    #include <string.h>
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
bool TenzoM::OpenPort(u16string comName, long boud, int deviceAddress)
{
    LastError = 0;
    Error.clear();

    bool success = false;
    Adr = deviceAddress;

    if (PortOpened())
        ClosePort();

    if (Emulate) return true;

#ifdef ISWINDOWS
    SECURITY_ATTRIBUTES sa = { 0 };
    ZeroMemory(&sa, sizeof(sa));
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;

    wstring comWM(comName.begin(), comName.end());
    wstring comID = L"\\\\.\\" + comWM;

    port = CreateFileW(comID.c_str(), GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, &sa, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, 0);

    if (port == INVALID_HANDLE_VALUE) {
        success = false;
        CheckLastError();
        return false;
    }
    success = true;

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
#else // POSIX
	struct termios settings;
	memset(&settings, 0, sizeof(settings));
	settings.c_iflag = 0;
	settings.c_oflag = 0;

	settings.c_cflag = CREAD | CLOCAL | CS8; // see termios.h for more information

	settings.c_lflag = 0;
	settings.c_cc[VMIN ] = 1;
	settings.c_cc[VTIME] = 0;

    u16string wcomID = u"/dev/" + comName;
    string comID(wcomID.begin(), wcomID.end());
	fd = open(comID.c_str(), O_RDWR | O_NONBLOCK);
	
    if (fd == -1) {
        success = false;
        CheckLastError();
        return false;
	}

    cfsetospeed(&settings, boud);
    cfsetispeed(&settings, boud);
	tcsetattr(fd, TCSANOW, &settings);
	int flags = fcntl(fd, F_GETFL, 0);
	fcntl(fd, F_SETFL, flags | O_NONBLOCK);
#endif

    if (Protocol == eProtocol643)
    {
        // Приветствие весов
        char message[] = { '\x01', '\x30', '\x30', '\x30', '\x31', '\x00' };
        snprintf((char*)message + 1, 5, "%04d", Adr);

        success = Send(message, sizeof(message) - 1);
        if (success)
        {
            Delay(40);
            success = Receive() > 0;
        }
    }

    if (!success) CheckLastError();

    return success;
}

/// @brief Сделать паузу в указанное количество микросекунд
/// @param ms Количество микросекунд паузы
void TenzoM::Delay(unsigned long ms)
{
#ifdef ISWINDOWS
	Sleep(ms);
#else
	usleep(ms*1000);
#endif
}

/// <summary>
/// Провека подключения к порту (весам)
/// </summary>
/// <returns>Возвращает true - если к COM-порту в данный момент установлено соединение.</returns>
bool TenzoM::PortOpened()
{
#ifdef ISWINDOWS
    return (port != INVALID_HANDLE_VALUE);
#else
    return (fd != -1);
#endif
}

/// <summary>
/// Получение от устройства ответа.
/// Полученные данные хранятся в буффере readBuffer.
/// </summary>
/// <returns>Возвращается количество считанных байт</returns>
unsigned long TenzoM::Receive()
{
    bool bSuccess = false;
    long dwResult = 0;
    unsigned long dwReadBytes = 0;

    try
    {
        if (!PortOpened()) return 0;

        memset(readBuffer, 0, RECV_BUFFER_LENGHT);
        Delay(50);

#ifdef ISWINDOWS
        OVERLAPPED osReader = { 0 };
        osReader.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
        if (osReader.hEvent)
        {
            if (!ReadFile(port, readBuffer, RECV_BUFFER_LENGHT, &dwReadBytes, &osReader)) {
                if (GetLastError() == ERROR_IO_PENDING)
                {
                    dwResult = WaitForSingleObject(osReader.hEvent, READ_TIMEOUT);
                    switch (dwResult)
                    {
                        // Read completed.
                    case WAIT_OBJECT_0:
                        if (!GetOverlappedResult(port, &osReader, &dwReadBytes, FALSE))
                        {
                            // Error in communications; report it.
                            bSuccess = false;
                        }
                        else
                        {
                            // Read completed successfully.
                            bSuccess = true;
                        }
                        break;

                    case WAIT_TIMEOUT:
                        // Operation isn't complete yet. fWaitingOnRead flag isn't
                        // changed since I'll loop back around, and I don't want
                        // to issue another read until the first one finishes.
                        //
                        // This is a good time to do some background work.
                        LastError = 1;
                        Error = u"Нет ответа от переданных команд";
                        return 0;
                        break;

                    default:
                        // Error in the WaitForSingleObject; abort.
                        // This indicates a problem with the OVERLAPPED structure's
                        // event handle.
                        break;
                    }
                }
            }
            else
            {
                // read completed immediately
                bSuccess = true;
            }
        }
#else // POSIX
        dwReadBytes = read(fd, &readBuffer, RECV_BUFFER_LENGHT);
        bSuccess = dwReadBytes > 0;
#endif

    }
    catch (...) {}

    if (!bSuccess) CheckLastError();
    else if (dwReadBytes > 0)
    {
        Log("Receive", dwReadBytes);
    }

    return dwReadBytes;
}

/// <summary>
/// Передача в порт на устройство массива байт
/// </summary>
/// <param name="message">Ссылка на буфер сообщения</param>
/// <param name="msgSize">Длина сообщения в байтах</param>
/// <returns></returns>
bool TenzoM::Send(char* message, long msgSize)
{
    if (!PortOpened()) return 0;
    bool bSuccess = false;
    unsigned long dwBytesWritten = 0;

#ifdef ISWINDOWS
    OVERLAPPED osWrite  = { 0 };
    osWrite.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    if (osWrite.hEvent)
    {
        if (!WriteFile(port, message, msgSize, &dwBytesWritten, &osWrite))
        {
            if (GetLastError() == ERROR_IO_PENDING) {
                // Write is pending.
                if (GetOverlappedResult(port, &osWrite, &dwBytesWritten, TRUE))
                    // Write operation completed successfully.
                    bSuccess = true;
            }
        }
        else
        {
            // WriteFile completed immediately.
            bSuccess = true;
        }
        CloseHandle(osWrite.hEvent);
    }
#else // POSIX
	dwBytesWritten = write(fd, message, msgSize);
    bSuccess = (dwBytesWritten == msgSize);
#endif

    if (!bSuccess) CheckLastError();

    return bSuccess;
}

/// <summary>
/// Закрывает COM-порт и все дескрипторы.
/// Завершаем работу с весами по данному Com-порту.
/// </summary>
void TenzoM::ClosePort()
{
    try
    {
#ifdef ISWINDOWS
        if (port != INVALID_HANDLE_VALUE)
        {
            try
            {
                PurgeComm(port, PURGE_RXABORT | PURGE_RXCLEAR | PURGE_TXABORT | PURGE_TXCLEAR);
            } catch (...) {}
            CloseHandle(port);
        }
#else
        if (fd != -1)
            close(fd);
#endif
    }
    catch (...)
    {
    }
#ifdef ISWINDOWS
    port = INVALID_HANDLE_VALUE;
#else
    fd = -1;
#endif
}

/// <summary>
/// Послать команду весам без дополнительных передаваемых данных.
/// </summary>
/// <param name="command">Байт-команда</param>
/// <returns>Возвращает TRUE - если передача успешно завершилась</returns>
bool TenzoM::SendCommand(char command)
{
    bool success = false;

    switch (Protocol)
    {
    case TenzoM::eProtocolTenzoM:
    {
        char msg1[] = { '\xFF', Adr, command, '\x00', '\xFF', '\xFF' };
        SetCrcOfMessage(msg1, sizeof(msg1)); // "Подписываем" - устанавливаем трейтий байт с конца (перед FF FF) как CRC сообщения
        success = Send(msg1, sizeof(msg1));
        break;
    }
    case TenzoM::eProtocol643:
    {
        char msg2[] = { command };
        success = Send(msg2, 1);
        break;
    }
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
/// Устанавливает байт CRC для передаваемого сообщения
/// </summary>
/// <param name="buffer">Адрес сообщения</param>
/// <param name="bufSize">Длина сообщения</param>
void TenzoM::SetCrcOfMessage(char* buffer, long bufSize)
{
    if (bufSize < 6) return; // minimum message lenght
    buffer[bufSize - 3] = 0; // set crc to 0
    char byte, crc = 0; constexpr char polinom = '\x69';
    for (int i = 1; i < bufSize - 2; i++)
    {
        byte = buffer[i];
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
    buffer[bufSize - 3] = crc;
}

/// <summary>
/// Извлечение веса из полученных данных
/// </summary>
/// <returns>Возвращает полученный вес в граммах.
/// Также устанавливает свойства стабильности веса и перегруза.</returns>
int TenzoM::ExtractWeight()
{
    int       weight     = 0; // Вычисленный вес
    const int offset     = 3; // Смещение от начала буфера, где идут байты веса
    const int bytes      = 3; // Количество байт для считывания упакованного в BCD веса (у TenzoM - 3)
    int       multiplier = 1; // Множитель (вычисляется для разных разрядов)

    // Распаковка цифр веса из BCD формата (младшие байты первые)
    for (int i = 0; i < bytes; i++)
    {
        auto byte = readBuffer[offset + i];
        weight += ((byte >> 4) * 10 + (byte & '\x0F')) * multiplier;
        multiplier *= 100;
    }

    // За весом в полученном буфере идёт байт CON с дополнительными фалагами
    const char con = readBuffer[offset + bytes];

    // Т.к. возврращаем в граммах, то считаем, сколько добавить нулей к результату
    int addNulls = 3 - (con & '\x07'); // Первые три бита - позиция запятой
    multiplier = 1; while (addNulls--) multiplier *= 10;

    if (con & '\x80') weight *= -1; // Определяем, стоит ли флаг минус

    Calm = (con & '\x10'); // Флаг успокоения веса (вес стабилен)
    Overload = (con & '\x08'); // Флаг перегруза

    return weight * multiplier;
}


/// <summary>
/// Обнулить показания веса
/// </summary>
bool TenzoM::SetZero()
{
    bool success = false;

    if (Protocol == eProtocol643)
    {
        if (SendCommand('\x0D'))
        {
            success = Receive() > 0;
        }
    }
    else if (Protocol == eProtocolTenzoM)
    {
        if (SendCommand('\xBF'))
        {
            success = Receive() > 0;
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
    int  brutto = 0;
    long dwBytesRead;

    if (Emulate) return RandomWeight();

    if (Protocol == eProtocol643)
    {
        if (SendCommand('\x10'))
        {
            dwBytesRead = Receive();
            if (dwBytesRead > 8)
            {
                // поиск последнего символа '=' в полученной последовательности
                char* msg = nullptr;
                int offset = 0;
                while (dwBytesRead - offset > 8)
                {
                    char* currPointer = readBuffer + offset;
                    if (currPointer[0] == '=')
                        msg = currPointer;
                    offset++;
                }
                if (msg != nullptr)
                {
                    const char* p = msg + 1;
                    char* p_end{};

                    brutto = strtol(p, &p_end, 0) * 1000;

                    if (p == p_end)
                    {
                        brutto = 0;
                    }
                    else
                    {
                        char lastByte = msg[8];
                        if (!(lastByte & 0x04))
                        {
                            brutto /= 10;
                        }
                        Calm = !(lastByte & 0x01);
                    }
                }
            }
        }
    }

    if (Protocol == eProtocolTenzoM)
    {
        if (SendCommand('\xC3'))
        {
            dwBytesRead = Receive();
            if (dwBytesRead > 8)
            {
                brutto = ExtractWeight();
            }
        }
    }
    Log("Ves: " + to_string(brutto) + " Calm: " + to_string(Calm) + "\n", 0);

    return brutto;
}


/// <summary>
/// Переключить в режим взвешивания
/// </summary>
void TenzoM::SwitchToWeighing()
{
    switch (Protocol)
    {
    case TenzoM::eProtocolTenzoM:
    {
        if (SendCommand('\xCD'))
        {
            Receive();
        }
        break;
    }
    case TenzoM::eProtocol643:
    {
        if (SendCommand('\x18'))
        {
            Receive();
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
u16string TenzoM::GetFreeComPorts()
{
    u16string ports = { 0 };
    // TODO: Реализовать GetFreeComPorts
    return u16string(ports);
}

/// <summary>
/// Получение рандомного веса
/// </summary>
/// <returns>Случайный показатель веса</returns>
int TenzoM::RandomWeight()
{
    int offset;
    if (!emulTargetWeight)
    {
        random_device dev;
        mt19937 rng(dev());
        uniform_int_distribution<mt19937::result_type> dist48_145(EmulMinKg*10, EmulMaxKg*10);

        emulTargetWeight = dist48_145(rng) * 100;

        emulMaxOffset = (emulTargetWeight - emulCurrentWeight);
        for (int step = 0; step < emulTotalSteps; step++)
        {
            emulMaxOffset /= 2;
        }
    }

    if (emulCurrentWeight != emulTargetWeight)
    {
        Calm = false;
        offset = (emulTargetWeight - emulCurrentWeight) / 2;
        emulCurrentWeight += offset;
        emulCurrentWeight = (emulCurrentWeight / 100) * 100; // Округление до 100 грамм

        if (emulMaxOffset > 0)
        {
            if (offset <= emulMaxOffset)
                emulCurrentWeight = emulTargetWeight;
        }
        else
        {
            if (offset >= emulMaxOffset)
                emulCurrentWeight = emulTargetWeight;
        }
    }
    if (emulCurrentWeight == emulTargetWeight)
    {
        Calm = true;
        emulCalmStep++;
        if (emulCalmStep >= emulCalmSteps)
        {
            emulTargetWeight = 0;
            emulCalmStep = 0;
        }
    }

    return emulCurrentWeight;
}

/// <summary>
/// Проверка последнего кода ошибки и получение её текстового представления
/// в свойство Error.
/// </summary>
void TenzoM::CheckLastError()
{
    Error.clear();
#ifdef ISWINDOWS
    LastError = GetLastError();
#else
    LastError = errno;
#endif
    try
    {
        switch (LastError)
        {
        case  0: return; break;
        case  2: Error = u"COM-порт не существует"; break;
        case 32: Error = u"COM-порт занят"; break;
        default:
        {
#ifdef ISWINDOWS
            LPSTR messageBuffer = nullptr;
            const size_t size = FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                NULL, LastError, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&messageBuffer, 0, NULL);

            const int len = MultiByteToWideChar(CP_ACP, 0, messageBuffer, -1, NULL, 0) - 1;
            if (len > 0)
            {
                wstring errorText(len, 0);
                MultiByteToWideChar(CP_ACP, 0, messageBuffer, -1, &errorText[0], len);
                errorText = regex_replace(errorText, wregex(L"\r\n"), L"");
                Error.assign(errorText.begin(), errorText.end());
            }

            LocalFree(messageBuffer);
#else
            LastError = errno;
            string errortext = strerror(errno);
            Error.assign(errortext.begin(), errortext.end());
#endif
        }
        }
    }
    catch (...) {}
}
void TenzoM::Log(string logMsg, int buflen)
{
    if (!WriteLog) return;

    try
    {
        string name(LogFile.begin(), LogFile.end());
#pragma warning(suppress : 4996)
        FILE* file = fopen(name.c_str(), "ab");

        string s(logMsg.begin(), logMsg.end());
        s += "\n";
#pragma warning(suppress : 4996)
        fwrite(s.c_str(), sizeof(char), s.size(), file);

        if (buflen > 0)
        {
            char hexstr[201];
            memset(hexstr, 0, 201);
            int i;
            for (i = 0; i < buflen; i++) {
#pragma warning(suppress : 4996)
                sprintf(hexstr + i * 2, "%02x", readBuffer[i]);
            }
            hexstr[i * 2] = '\n';
#pragma warning(suppress : 4996)
            fwrite(hexstr, 1, i * 2 + 1, file);
        }

        fclose(file);
    }
    catch (...)
    {
        WriteLog = false;
    }
}

