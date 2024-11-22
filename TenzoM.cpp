#ifdef __unix__
#else
#include <WinSock2.h>
#include <WS2tcpip.h>
#include <winuser.h>
#endif

#include "TenzoM.h"

#include <iostream>
#include <locale>
#include <codecvt>
#include <random>
#include <regex>
#include <fstream>
#include <ctime>
#include <filesystem>
#include <sstream>
#include <iomanip>
#include "TenzoM.h"
#include <codecvt>

#ifdef ISWINDOWS
#pragma comment (lib, "Ws2_32.lib")
#pragma comment (lib, "Mswsock.lib")
#pragma comment (lib, "AdvApi32.lib")
#pragma comment (lib, "user32.lib")
#endif

#ifdef ISWINDOWS
#else
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <netdb.h>
#include <string.h>
#include <linux/uinput.h>
#endif

using namespace std;

#pragma region ТехническиеФункцииРаботыСОборудованием

bool TenzoM::TryConnectTo()
{
    bool success = false;
    int  err;

#ifdef ISWINDOWS
    err = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (!err)
    {
        wsaStarted = true;
#endif
        struct addrinfo* result = NULL, * ptr = NULL, hints = { 0 };

        string port_as_string = to_string(NetPort);
        string node(IP.begin(), IP.end());

        hints.ai_family   = AF_UNSPEC;
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_protocol = IPPROTO_TCP;

        err = getaddrinfo(node.c_str(), port_as_string.c_str(), &hints, &result);
        if (!err) {
            for (ptr = result; ptr != NULL; ptr = ptr->ai_next) {
                clientSocket = socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol);

                if (clientSocket != INVALID_SOCKET)
                {
                    int optval = 1;
                    setsockopt(clientSocket, SOL_SOCKET, SO_RCVTIMEO, (char*)&tcpTimeout, sizeof(tcpTimeout));
                    err = connect(clientSocket, ptr->ai_addr, (int)ptr->ai_addrlen);
                    if (!err) {
                        success = true;
                        break;
                    }
                }
            }
            freeaddrinfo(result);
        }
#ifdef ISWINDOWS
    }
#else

#endif

    return success;
}

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

    if (NetMode)
    {
        success = TryConnectTo();
    }
    else
    {
#ifdef ISWINDOWS
        SECURITY_ATTRIBUTES sa = { 0 };
        sa.nLength = sizeof(sa);
        sa.bInheritHandle = TRUE;

        wstring comWN(comName.begin(), comName.end());
        wstring comID = L"\\\\.\\" + comWN;

        comPort = CreateFileW(comID.c_str(), GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, &sa, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, 0);

        if (comPort == INVALID_HANDLE_VALUE) {
            success = false;
            CheckLastError();
            return false;
        }
        success = true;

        EscapeCommFunction(comPort, SETDTR);

        DCB dcb = { 0 };
        dcb.DCBlength = sizeof(dcb);
        GetCommState(comPort, &dcb);
        dcb.BaudRate      = boud;
        dcb.fBinary       = TRUE;
        dcb.fAbortOnError = TRUE;
        dcb.ByteSize      = 8;
        dcb.Parity        = NOPARITY;
        dcb.StopBits      = ONESTOPBIT;
        SetCommState(comPort, &dcb);

        PurgeComm(comPort, PURGE_RXABORT | PURGE_RXCLEAR | PURGE_TXABORT | PURGE_TXCLEAR);

        COMMTIMEOUTS commTimeouts;
        if (GetCommTimeouts(comPort, &commTimeouts)) {
            commTimeouts.ReadIntervalTimeout         = MAXDWORD;
            commTimeouts.ReadTotalTimeoutMultiplier  = 10;
            commTimeouts.ReadTotalTimeoutConstant    = 50;
            commTimeouts.WriteTotalTimeoutMultiplier = 10;
            commTimeouts.WriteTotalTimeoutConstant   = 50;
            SetCommTimeouts(comPort, &commTimeouts);
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

    }
    if (success && Protocol == eProtocol643)
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
    if (NetMode)
    {
#ifdef ISWINDOWS
        return (clientSocket != INVALID_SOCKET);
#else
        return (clientSocket != INVALID_SOCKET);
#endif
    }
    else
    {
#ifdef ISWINDOWS
        return (comPort != INVALID_HANDLE_VALUE);
#else
        return (fd != -1);
#endif
    }
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
        memset(readBuffer, 0, RECV_BUFFER_LENGHT);

        if (!PortOpened()) return 0;
        Delay(50);

        if (NetMode)
        {
            dwReadBytes = recv(clientSocket, readBuffer, sizeof(readBuffer), 0);
            bSuccess = (long)dwReadBytes > 0;
        }
        else
        {
    #ifdef ISWINDOWS
            OVERLAPPED osReader = { 0 };
            osReader.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
            if (osReader.hEvent)
            {
                if (!ReadFile(comPort, readBuffer, RECV_BUFFER_LENGHT, &dwReadBytes, &osReader)) {
                    if (GetLastError() == ERROR_IO_PENDING)
                    {
                        dwResult = WaitForSingleObject(osReader.hEvent, READ_TIMEOUT);
                        switch (dwResult)
                        {
                            // Read completed.
                        case WAIT_OBJECT_0:
                            if (!GetOverlappedResult(comPort, &osReader, &dwReadBytes, FALSE))
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
    }
    catch (...) {}

    if (!bSuccess) CheckLastError();
    else if (WriteLog && ((long)dwReadBytes > 0))
    {
        Log(u"Receive", readBuffer, dwReadBytes);
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
    bool bSuccess = false;
    if (!PortOpened()) return bSuccess;
    unsigned long dwBytesWritten = 0;
    
    if (NetMode)
    {
        dwBytesWritten = send(clientSocket, message, msgSize, 0);
        bSuccess = dwBytesWritten > 0;
    }
    else
    {
#ifdef ISWINDOWS
        OVERLAPPED osWrite = { 0 };
        osWrite.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
        if (osWrite.hEvent)
        {
            if (!WriteFile(comPort, message, msgSize, &dwBytesWritten, &osWrite))
            {
                if (GetLastError() == ERROR_IO_PENDING) {
                    // Write is pending.
                    if (GetOverlappedResult(comPort, &osWrite, &dwBytesWritten, TRUE))
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
    }

    if (!bSuccess) CheckLastError();
    else if (WriteLog && (dwBytesWritten > 0))
    {
        Log(u"Send", message, dwBytesWritten);
    }

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
        if (clientSocket != INVALID_SOCKET)
        {
#ifdef ISWINDOWS
            closesocket(clientSocket);
#else
            close(clientSocket);
#endif
            clientSocket = INVALID_SOCKET;
        }
#ifdef ISWINDOWS
        if (comPort != INVALID_HANDLE_VALUE)
        {
            try
            {
                PurgeComm(comPort, PURGE_RXABORT | PURGE_RXCLEAR | PURGE_TXABORT | PURGE_TXCLEAR);
            } catch (...) {}
            CloseHandle(comPort);
        }
        if (wsaStarted)
        {
            WSACleanup();
            wsaStarted = false;
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
    comPort = INVALID_HANDLE_VALUE;
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
/// Поиск в считнанных данных readBuffer начала и конца пакета по продоколу TenzoM
/// </summary>
/// <param name="bytesRead">Количество считанных байт</param>
/// <param name="command">Код операции, ответ на который мы посылали ищем</param>
/// <returns>Возвращает true, пакет с нашим адресом устройства и кодом операции был найден</returns>
bool TenzoM::FindTenzoMPacket(long bytesRead, char command)
{
    char* data     = readBuffer;
    long  dataSize = 0;
    return FindTenzoMPacket(bytesRead, command, data, dataSize);
}

/// <summary>
/// Поиск в считнанных данных readBuffer начала и конца пакета по продоколу TenzoM
/// </summary>
/// <param name="bytesRead">Количество считанных байт</param>
/// <param name="command">Код операции, ответ на который мы посылали ищем</param>
/// <param name="data">Выходной параметр: указатель на начало данных в пакете</param>
/// <param name="dataSize">Выходной параметр: длина данных в найденном пакете</param>
/// <returns>Возвращает true, пакет с нашим адресом устройства и кодом операции был найден</returns>
bool TenzoM::FindTenzoMPacket(long bytesRead, char command, char*& data, long& dataSize)
{
    bool success     = false;
    char prevByte    = '\x00';
    int  offsetBegin = 0;
    int  offsetEnd   = 0;
    int  adr         = 0;
    char cop         = 0;
    bool packetFound = false;
    data     = readBuffer;
    dataSize = 0;
    if (bytesRead > 5)
    {
        // Поиск начала пакета
        while (offsetBegin < bytesRead - 5)
        {
            if ((readBuffer[offsetBegin] == '\xFF') && (prevByte != '\xFE'))
            {
                // Это начало пакета. Получаем адрес устройства и код операции.
                adr = readBuffer[offsetBegin + 1];
                cop = readBuffer[offsetBegin + 2];
                if (adr == 0)
                {
                    adr = (unsigned char)readBuffer[offsetBegin + 2] | (unsigned short)(readBuffer[offsetBegin + 3] << 8) | (unsigned long)(readBuffer[offsetBegin + 4] << 16);
                    cop = readBuffer[offsetBegin + 5];
                    offsetBegin += 6; // Фокусируемся начале данных (байт после адреса)
                }
                else
                {
                    offsetBegin += 3; // Фокусируемся начале данных (байт после адреса)
                }
                if ((adr != Adr) || (cop != command)) continue; // Проверяем, наш ли это пакет
                packetFound = true;
                break;
            }
            prevByte = readBuffer[offsetBegin];
            offsetBegin++;
        }
        if (packetFound)
        {
            offsetEnd = offsetBegin;
            // Поиск конца пакета
            while (offsetEnd < bytesRead - 1)
            {
                if ((readBuffer[offsetEnd] == '\xFF') && (readBuffer[offsetEnd + 1] == '\xFF'))
                {
                    offsetEnd -= 1; // Фокусируемся на смещении после данных (байт контрольного значением CRC)
                    break;
                }
                offsetEnd++;
            }
        }
    }

    if (packetFound) // Если был найден пакет
    {
        success = true;
        if (offsetEnd > offsetBegin)
        {
            data     = readBuffer + offsetBegin; // Указатель на начало данных в пакете
            dataSize = offsetEnd  - offsetBegin; // Длина данных
        }
    }

    return success;
}

bool TenzoM::CheckErrorPacket(long bytesRead)
{
    bool found = false;
    char* data = readBuffer;
    long dataSize = 0;

    found = FindTenzoMPacket(bytesRead, '\xEE', data, dataSize);
    if (found)
    {
        LastError = data[0];
        switch (LastError)
        {
        case 0x03: Error = u"Ошибка диапазона обнуления"; break;
        case 0x04: Error = u"Изменение параметров запрещено"; break;
        case 0x05: Error = u"Ошибка превышения длины посылки (входного буфера)"; break;
        case 0x06: Error = u"Ошибка CRC-кода"; break;
        case 0x20: Error = u"Внутренняя калибровка нуля АЦП не завершена"; break;
        case 0x21: Error = u"Внутренняя калибровка шкалы АЦП не завершена"; break;
        default:
            Error = u"Ошибка выполнения команды"; break;
        }
    }

    return found;
}

/// <summary>
/// Извлечение веса из полученных данных
/// </summary>
/// <returns>Возвращает полученный вес в граммах.
/// Также устанавливает свойства стабильности веса и перегруза.</returns>
int TenzoM::ExtractWeight(char* data, long dataSize)
{
    if (dataSize < 4) return 0;

    int       weight     = 0; // Вычисленный вес
    const int bytes      = 3; // Количество байт для считывания упакованного в BCD веса (у TenzoM - 3)
    int       multiplier = 1; // Множитель (вычисляется для разных разрядов)

    // Распаковка цифр веса из BCD формата (младшие байты первые)
    for (int i = 0; i < bytes; i++)
    {
        auto byte = data[i];
        weight += ((byte >> 4) * 10 + (byte & '\x0F')) * multiplier;
        multiplier *= 100;
    }

    weight = weight * 1000; // Возвращаем вес всегда граммах

    // За весом в полученном буфере идёт байт CON с дополнительными фалагами
    const char con = data[bytes];

    int commaPosition = con & '\x07'; // Первые три бита - позиция запятой 
    int delimiter = 1; while (commaPosition--) delimiter *= 10;
    weight /= delimiter; // Делим вес на посчитанный делитель (при сдвиге запятой)

    if (con & '\x80') weight *= -1; // Определяем, стоит ли флаг минус
    Event    = (con & '\x40'); // Флаг присутствия события (введён код с клиавиатуры)
    NScal    = (con & '\x20'); // Флаг использования второго тезнодатчика (в описании протокола написано "текущий номер используемых весов")
    Calm     = (con & '\x10'); // Флаг успокоения веса (вес стабилен)
    Overload = (con & '\x08'); // Флаг перегруза

    return weight;
}

/// <summary>
/// Получение списка доступных COM-портов
/// </summary>
/// <returns>Возвращается строка с именами доступных COM-портов, разделёнными точкой с запятой.</returns>
u16string TenzoM::GetFreeComPorts()
{
    vector<string> port_names;
#ifdef ISWINDOWS

    TCHAR lpTargetPath[5000]; // buffer to store the path of the COMPORTS
    for (int i = 0; i < 255; i++) // checking ports from COM0 to COM255
    {
        string name("COM" + i);
        if (QueryDosDeviceA((LPCSTR)name.c_str(), (LPSTR)lpTargetPath, (DWORD)5000))
        {
            port_names.push_back(name);
        }
    }

#else

    filesystem::path p("/dev/serial/by-id");
    try
    {
        if (!exists(p))
        {
            LastError = 2;
            Error = p.generic_u16string() + u" does not exist";
            if (WriteLog)
            {
                Log(u"Error " + Error);
            }
        }
        else
        {
            for (auto de : filesystem::directory_iterator(p))
            {
                if (is_symlink(de.symlink_status()))
                {
                    filesystem::path symlink_points_at = read_symlink(de);
                    filesystem::path canonical_path = filesystem::canonical(p / symlink_points_at);
                    port_names.push_back(canonical_path.generic_string());
                }
            }
        }
    }
    catch (...)
    {
        CheckLastError();
    }
#endif
    sort(port_names.begin(), port_names.end());
    string s = "";
    for (size_t i = 0; i < port_names.size(); i++)
    {
        if (i) s += ";";
        s += port_names[i];
    }

    return u16string(s.begin(), s.end());
}

#pragma endregion

#pragma region ВспомогательныеФункции

u16string GetText(char* data, long datsSize)
{
    u16string text = u"";
    long realSize = 0;
    while (realSize < datsSize)
    {
        if (data[realSize] == 0)
            break;
        realSize++;
    }

    string ss = "БГЁЖЗИЙЛПУФЧШЪЫЭЮЯбвгёжзийклмнптчшъыьэюя<>\"\"№                   ДЦЩдфцщ"; // A0...
    string e1 = "A B  E     K MHO PCT  X      b   ";
    string e2 = "a    e         o pc y x      b   ";
    string r1 = "АБВГДЕЁЖЗИЙКЛМНОПРСТУФХЦЧШЩЪЫЬЭЮЯ";
    string r2 = "абвгдеёжзийклмнопрстуфхцчшщъыьэюя";

    int pos = 0; unsigned char chzam;
    for (int i = 0; i < realSize; i++)
    {
        chzam = 0x00;
        auto ch = data[i];
        if ((unsigned char)ch > 0x40)
        {
            pos = (unsigned char)ch - 0xA0;
            if ((pos >= 0) && (pos < ss.length()))
            {
                chzam = ss[pos];
            }
            else
            {
                pos = e1.find(ch);
                if ((pos >= 0) && (e1[pos] != 0x20))
                {
                    chzam = r1[pos];
                }
                else
                {
                    pos = e2.find(ch);
                    if ((pos >= 0) && (e2[pos] != 0x20))
                    {
                        chzam = r2[pos];
                    }
                }
            }
            if ((chzam != 0x00) && (chzam != 0x20))
            {
                data[i] = chzam;
            }
        }
    }


#ifdef ISWINDOWS
    int wchlen = MultiByteToWideChar(CP_ACP, 0, data, realSize, NULL, 0);
    if (wchlen > 0 && wchlen != 0xFFFD)
    {
        wstring wstr(wchlen, 0);
        MultiByteToWideChar(CP_ACP, 0, data, realSize, &wstr[0], wchlen);
        text = u16string(wstr.begin(), wstr.end());
    }
#else
    string s = string(&data[0], realSize);
    wstring_convert<codecvt_utf8_utf16<char16_t>,char16_t> convert;
    text = convert.from_bytes(s);
#endif
    return text;
}

/// <summary>
/// Конвертация кодировки из u16string в UTF-8 cp1251
/// </summary>
string UTF16_to_CP1251(u16string const& utf16)

{
    if (!utf16.empty())
    {
        wstring_convert<codecvt_utf8_utf16<char16_t>,char16_t> convert;
        string utf8 = convert.to_bytes(utf16);
        //wstring str = wstring(utf16.begin(), utf16.end());
        //wstring_convert<codecvt_utf8_utf16<wchar_t>> convert;
        //string utf8 = convert.to_bytes(str);

#ifdef ISWINDOWS
        int wchlen = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), utf8.size(), NULL, 0);
        if (wchlen > 0 && wchlen != 0xFFFD)
        {
            vector<wchar_t> wbuf(wchlen);
            MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), utf8.size(), &wbuf[0], wchlen);
            vector<char> buf(wchlen);
            WideCharToMultiByte(1251, 0, &wbuf[0], wchlen, &buf[0], wchlen, 0, 0);
            return string(&buf[0], wchlen);
        }
#else

            return string(utf8.begin(), utf8.end());
#endif
    }
    return string();
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
        uniform_int_distribution<mt19937::result_type> dist48_145(EmulMinKg * 10, EmulMaxKg * 10);

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
/// Получение текста описания ошибки по его коду и установка этого текста в свойстве Error
/// </summary>
/// <param name="errorCode">Код системной ошибки</param>
void TenzoM::SetErrorText(unsigned long errorCode)
{
    try
    {
        switch (errorCode)
        {
        case  0: return; break;
//        case  2: Error = u"COM-порт не существует"; break;
//        case 32: Error = u"COM-порт занят"; break;
        default:
        {
#ifdef ISWINDOWS
            LPSTR messageBuffer = nullptr;
            const size_t size = FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                NULL, errorCode, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&messageBuffer, 0, NULL);

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
        string errortext = "";
    	if (locale == (locale_t)0)
        {
            errortext = strerror(errorCode);
        }
        else
        {
            errortext = strerror_l(errorCode, locale);
        }
        wstring_convert<codecvt_utf8_utf16<char16_t>, char16_t> conv;
        Error = conv.from_bytes(&errortext[0]);
#endif
        }
        }
        if (WriteLog && LastError)
        {
            Log(u"Error: " + Error);
        }
    }
    catch (...) {}
}

/// <summary>
/// Проверка последнего кода ошибки и получение её текстового представления
/// в свойство Error.
/// </summary>
void TenzoM::CheckLastError()
{
    Error.clear();
#ifdef ISWINDOWS
    if (NetMode)
        LastError = WSAGetLastError();
    else
        LastError = GetLastError();
#else
    LastError = errno;
#endif
    if (LastError)
        SetErrorText(LastError);
}

void TenzoM::Log(u16string logMsg)
{
    Log(logMsg, nullptr, 0);
}

#if defined(__linux)
void send_event(const int type, const int code, const int value)
{
    struct input_event e;
    memset(&e, 0, sizeof(e));
    
    gettimeofday(&e.time, nullptr);
    
    e.type = type;
    e.code = code;
    e.value = value;

    int fd = open("/dev/uinput", O_WRONLY | O_NONBLOCK);
    if (fd >= 0)
    {
        write(fd, &e, sizeof(e));
    }

    if (fd >= 0)
    {
        ioctl(fd, UI_DEV_DESTROY);
        close(fd);
        fd = 0;
    }
}
#endif

void TenzoM::SendKey(unsigned short keycode)
{
#if defined(_WIN64) || defined(_WIN32)
    INPUT inputs[2] = { 0 };
    inputs[0].type = INPUT_KEYBOARD;
    inputs[0].ki.wVk = keycode;
    inputs[1].type = INPUT_KEYBOARD;
    inputs[1].ki.wVk = keycode;
    inputs[1].ki.dwFlags = KEYEVENTF_KEYUP;
    SendInput(ARRAYSIZE(inputs), inputs, sizeof(INPUT));

#elif defined(__linux)
    send_event(EV_KEY, keycode, false);
    send_event(EV_KEY, keycode, 0);
    send_event(EV_SYN, SYN_REPORT, 0);
#endif
}

void TenzoM::SetUnsupportedComadrError()
{
    Error = u"Команда не поддерживается устройством";
    LastError = 0x01;
}

template<typename TInputIter>
string make_hex_string(TInputIter first, TInputIter last, bool use_uppercase = true, bool insert_spaces = false)
{
    std::ostringstream ss;
    ss << std::hex << std::setfill('0');
    if (use_uppercase)
        ss << std::uppercase;
    while (first != last)
    {
        ss << std::setw(2) << static_cast<int>(*first++);
        if (insert_spaces && first != last)
            ss << " ";
    }
    return ss.str();
}

u16string to_u16string(int const& i) {
    wstring_convert<codecvt_utf8_utf16<char16_t, 0x10ffff, little_endian>, char16_t> conv;
    return conv.from_bytes(to_string(i));
}

void TenzoM::Log(u16string logMsg, char* buf, int buflen)
{
    if (!WriteLog) return;

    try
    {
        time_t rawtime = {};
        time(&rawtime);
        struct tm timeinfo = {};
#ifdef ISWINDOWS
        localtime_s(&timeinfo, &rawtime);
#else
        localtime_r(&rawtime, &timeinfo);
#endif
        char timeLabel[256];
        size_t size = strftime(timeLabel, sizeof(timeLabel), "%Y.%m.%d %H:%M:%S ", &timeinfo);

        string filename(LogFile.begin(), LogFile.end());

        basic_ofstream<char16_t> outfile(filename, std::ios_base::app);
        outfile << timeLabel;
        outfile << logMsg;

        if (buflen > 0)
        {
            outfile << " ";
            vector<uint8_t> bytearr(buf, buf + buflen);
            string hex = make_hex_string(bytearr.begin(), bytearr.end(), true);
            u16string u16hex(hex.begin(), hex.end());
            outfile << u16hex;
        }
        outfile << "\n";

        outfile.close();

    }
    catch (...)
    {
        WriteLog = false;
    }
}

#pragma endregion

#pragma region РаботаСВесами

unsigned char TenzoM::GetStatus()
{
    unsigned char status = 0x00;
    long dwBytesRead = 0;

    if (Protocol == eProtocolTenzoM)
    {
        constexpr char command = '\xBF';
        if (SendCommand(command))
        {
            dwBytesRead = Receive();
            if (dwBytesRead > 8)
            {
                char* data = readBuffer;
                long  dataSize = 0;
                if (FindTenzoMPacket(dwBytesRead, command, data, dataSize))
                {
                    status = data[0];

                    Event     = (status | 0x10);
                    Calm      = (status | 0x04);

                    // Если включена отсылка в систему нажатия клавиш и есть что посылать
                    if (Event && SendKeys)
                    {
                        auto keycode = GetEnteredCode();
                        if (keycode != 0)
                        {
                            if (NumpadKeys && (keycode >= 0x30) && (keycode <= 0x39))
                            {
                                keycode += 0x30;
                            }
                            SendKey(keycode);
                        }

                    }

                }
                else
                {
                    SetUnsupportedComadrError();
                }
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
        if (SendCommand('\x0D'))
        {
            success = Receive() > 0;
        }
    }
    else if (Protocol == eProtocolTenzoM)
    {
        if (SendCommand('\xC0'))
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
    int  weight = 0;
    long dwBytesRead = 0;

    if (Emulate) return RandomWeight();

    if (!PortOpened()) return weight;

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
                while (dwBytesRead - offset >= 8)
                {
                    char* currPointer = readBuffer + offset;
                    if (currPointer[0] == '=')
                        msg = currPointer;
                    offset++;
                }
                if (msg != nullptr)
                {
                    // Заменяем знак разделителя точки на знак разделителя дробной части
                    // как указано в нашей системе чтобы сработала функция stof
                    //char decimalPoint = localeconv()->decimal_point[0];
                    for (int i = 0; i < (dwBytesRead - offset); i++)
                    {
                        if (msg[i] == '\x2E') msg[i] = DecimalPoint;
                    }

                    try
                    {
                        auto val = stof(msg + 1);
                        weight = (int)(val * 1000);
                    }
                    catch (...) {}

                    char lastByte = msg[8];
                    if (!(lastByte & 0x04))
                    {
                        weight /= 10;
                    }
                    Calm = !(lastByte & 0x01);
                }
            }
        }
    }

    if (Protocol == eProtocolTenzoM)
    {
        constexpr char command = '\xC3';
        if (SendCommand(command))
        {
            dwBytesRead = Receive();
            if (dwBytesRead > 8)
            {
                char* data = readBuffer;
                long  dataSize = 0;
                FindTenzoMPacket(dwBytesRead, command, data, dataSize);
                if (dataSize > 0)
                {
                    weight = ExtractWeight(data, dataSize);

                    // Если включена отсылка в систему нажатия клавиш и есть что посылать
                    if (Event && SendKeys)
                    {
                        auto keycode = GetEnteredCode();
                        if (keycode != 0)
                        {
                            if (NumpadKeys && (keycode >= 0x30) && (keycode <= 0x39))
                            {
                                keycode += 0x30;
                            }
                            SendKey(keycode);
                        }

                    }

                }
            }
        }
    }

    if (WriteLog)
    {
        Log((u16string)(u"Ves: " + to_u16string(weight) + u" Calm: " + (Calm ? u"1" : u"0")));
    }

    return weight;
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
    default:
        break;
    }
}

/// <summary>
/// Получить значение индикаторов (высвечивающегося текста)
/// </summary>
/// <param name="line">0 - обе строки, 1 - верзняя строка, 2 - нижняя строка</param>
/// <returns>Возвращается текст индикаторов</returns>
u16string TenzoM::GetIndicatorText(int line)
{
    long dwBytesRead = 0;
    u16string text = u"";

    if (Protocol == eProtocolTenzoM)
    {
        char command  = '\xC6';
        char lineCode = '\x21';
        switch (line)
        {
        case 1: lineCode = '\x1F'; break;
        case 2: lineCode = '\x20'; break;
        default: break;
        }

        char msg[] = { '\xFF', Adr, command, lineCode, '\x00', '\xFF', '\xFF' };
        SetCrcOfMessage(msg, sizeof(msg)); // "Подписываем" - устанавливаем трейтий байт с конца (перед FF FF) как CRC сообщения
        bool success = Send(msg, sizeof(msg));
        if (success)
        {
            dwBytesRead = Receive();
            if (dwBytesRead > 8)
            {
                char* data = readBuffer;
                long  dataSize = 0;
                FindTenzoMPacket(dwBytesRead, command, data, dataSize);
                if (dataSize > 0)
                {
                    long msgLen = dataSize - 2;
                    text = GetText(data+2, msgLen);
                }
            }
        }
    }
    else if (Protocol == eProtocol643)
    {
        if (SendCommand('\x10'))
        {
            dwBytesRead = Receive();
            if (dwBytesRead > 8)
            {
                text = GetText(readBuffer, dwBytesRead - 2);
            }
        }
    }

    return text;
}

/// <summary>
/// Вывести символьное сообщение на устройство отображения или вывода
/// </summary>
/// <param name="line">0 - на все строки, 1 = верхняя строка, 2 - нижняя строка</param>
/// <param name="text">Текст, выводимый на индикаторы</param>
/// <returns>Возвращает true, если передача прошла успешно</returns>
bool TenzoM::SetIndicatorText(int line, u16string text)
{
    bool success = false;
    long dwBytesRead = 0;

    if (Protocol == eProtocolTenzoM)
    {
        char lineCode = '\x22';
        switch (line)
        {
        case 1: lineCode = '\x21'; break;
        case 2: lineCode = '\x20'; break;
        default:
            break;
        }

        auto t = UTF16_to_CP1251(text);
        auto s = t.c_str();
 
        string ss = "БГЁЖЗИЙЛПУФЧШЪЫЭЮЯбвгёжзийклмнптчшъыьэюя<>\"\"№                   ДЦЩдфцщ"; // A0...
        string e1 = "A B  E     K MHO PCT  X      b   ";
        string e2 = "a    e         o pc y x      b   ";
        string r1 = "АБВГДЕЁЖЗИЙКЛМНОПРСТУФХЦЧШЩЪЫЬЭЮЯ";
        string r2 = "абвгдеёжзийклмнопрстуфхцчшщъыьэюя";

        int len = strlen(s);
        if (len > 50) len = 50;

        readBuffer[0] = '\xFF';
        readBuffer[1] = Adr;
        readBuffer[2] = '\xD2';
        readBuffer[3] = lineCode;
        readBuffer[4] = len;

        int pos = 0; unsigned char chzam;
        for (int i = 0; i < len; i++)
        {
            chzam = 0x00;
            auto ch = s[i];
            if ((unsigned char)ch > 0x40)
            {
                pos = ss.find(ch);
                if (pos >= 0)
                {
                    chzam = 0xA0 + pos;
                }
                else
                {
                    pos = r1.find(ch);
                    if (pos >= 0)
                    {
                        chzam = e1[pos];
                    }
                    else
                    {
                        pos = r2.find(ch);
                        if (pos >= 0)
                        {
                            chzam = e2[pos];
                        }
                    }
                }
                if ((chzam != 0x00) && (chzam != 0x20))
                {
                    ch = chzam;
                }
            }
            readBuffer[5 + i] = ch;
        }
        readBuffer[len + 5] = '\x00';
        readBuffer[len + 6] = '\xFF';
        readBuffer[len + 7] = '\xFF';

        //for (int i = 0; i < 40; i++)
        //{
        //    readBuffer[5 + i] = 0xD0 + i;
        //}

        int messageLen = len + 8;

        SetCrcOfMessage(readBuffer, messageLen); // "Подписываем" - устанавливаем трейтий байт с конца (перед FF FF) как CRC сообщения
        success = Send(readBuffer, messageLen);
        if (success)
        {
            dwBytesRead = Receive();
        }
    }
    return success;
}

/// <summary>
/// Запрос введенного кода
/// </summary>
/// <returns>Возвращается символ введённого кода ("0"..."9", Перевод строки или пустая строка)</returns>
char TenzoM::GetEnteredCode()
{
    long dwBytesRead = 0;
    char code = '\x00';

    if (Protocol == eProtocolTenzoM)
    {
        char constexpr command = '\xC7';
        if (SendCommand(command))
        {
            dwBytesRead = Receive();
            if (dwBytesRead > 6)
            {
                char* data = readBuffer;
                long  dataSize = 0;
                FindTenzoMPacket(dwBytesRead, command, data, dataSize);
                if (dataSize > 0)
                {
                    const char event = data[0];
                    switch (event)
                    {
                    case '\x30': code = '\x30'; break;
                    case '\xF1': code = '\x31'; break;
                    case '\xF2': code = '\x32'; break;
                    case '\xF3': code = '\x33'; break;
                    case '\xF4': code = '\x34'; break;
                    case '\xF5': code = '\x35'; break;
                    case '\xF6': code = '\x36'; break;
                    case '\xF7': code = '\x37'; break;
                    case '\xF8': code = '\x38'; break;
                    case '\xF9': code = '\x39'; break;
                    case '\x31': code = '\x0D'; break;
                    default: break;
                    }
                }
                char msg[] = { '\xFF', Adr, '\xD2', '\x00', '\x00', '\xFF', '\xFF' };
                SetCrcOfMessage(msg, sizeof(msg));
                if (Send(msg, sizeof(msg)))
                {
                    dwBytesRead = Receive();
                }

            }
        }
    }
    else if (Protocol == eProtocol643)
    {
        if (SendCommand('\x11'))
        {
            dwBytesRead = Receive();
            if (dwBytesRead > 0)
            {
                code = readBuffer[0];
                if (code == 0x3D) code = 0x0D;
                SendCommand('\x19');
            }
        }
    }
    return code;
}

/// <summary>
/// Установить номер входного канала
/// </summary>
/// <param name="channelNum">Номер канала (0...255)</param>
/// <returns>Возвращает true, если передача команды прошла успешно</returns>
bool TenzoM::SetInputChannel(int channelNum)
{
    bool success = false;
    long dwBytesRead = 0;
    if (channelNum > 255) channelNum = 0;

    if (Protocol == eProtocolTenzoM)
    {
        char msg1[] = { '\xFF', Adr, '\xDC', char(channelNum), '\x00', '\xFF', '\xFF'};
        SetCrcOfMessage(msg1, sizeof(msg1));
        success = Send(msg1, sizeof(msg1));
        if (success)
        {
            dwBytesRead = Receive();
        }
    }
    return success;
}

/// <summary>
/// Компенсировать вес тары:
/// (Команда, эквивалентная нажатию кнопки «>Т<»)
/// </summary>
/// <returns>Возвращает true, если передача команды прошла успешно</returns>
bool TenzoM::Tare()
{
    bool success = false;
    if (Protocol == eProtocolTenzoM)
    {
        if (SendCommand('\xCE'))
        {
            success = Receive() > 0;
        }
    }
    return success;
}

bool TenzoM::Calibrate()
{
    long dwBytesRead = 0;
    bool success = false;

    if (Protocol == eProtocolTenzoM)
    {
        char constexpr command = '\xA2';
        char constexpr proc    = '\x22';

        char msg[] = { '\xFF', Adr, command, proc, '\x00', '\xFF', '\xFF' };
        SetCrcOfMessage(msg, sizeof(msg));
        if (Send(msg, sizeof(msg)))
        {
            char* data = readBuffer;
            long  dataSize = 0;
            dwBytesRead = Receive();
            if (dwBytesRead > 6)
            {
                if (FindTenzoMPacket(dwBytesRead, command, data, dataSize))
                {
                    char retCode = data[0];
                    success = (retCode == proc);
                }
            }
            if (!success)
            {
                CheckErrorPacket(dwBytesRead);
            }
        }
    }

    return success;
}

int TenzoM::GetSerialNum()
{
    long dwBytesRead = 0;
    int  serialNum   = 0;

    if (Protocol == eProtocolTenzoM)
    {
        char constexpr command = '\xA1';
        if (SendCommand(command))
        {
            dwBytesRead = Receive();

            if (dwBytesRead > 6)
            {
                char* data = readBuffer;
                long  dataSize = 0;
                FindTenzoMPacket(dwBytesRead, command, data, dataSize);
                if (dataSize > 0)
                {
                    serialNum = (unsigned char)data[0] | (unsigned short)(data[1] << 8) | (unsigned long)(data[2] << 16);
                }
            }
        }
    }
    return serialNum;
}

/// <summary>
/// Тип устройства и версии ПО
/// </summary>
u16string TenzoM::Version()
{
    long dwBytesRead = 0;
    u16string text = u"";

    if (Protocol == eProtocolTenzoM)
    {
        constexpr char command = '\xFD';
        if (SendCommand(command))
        {
            dwBytesRead = Receive();
            if (dwBytesRead > 6)
            {
                char* data = readBuffer;
                long  dataSize = 0;
                FindTenzoMPacket(dwBytesRead, command, data, dataSize);
                if (dataSize > 0)
                {
                    text = GetText(data, dataSize);
                }
            }
        }
    }

    return text;
}

#pragma endregion
