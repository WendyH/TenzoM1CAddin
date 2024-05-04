#if !defined( __linux__ ) && !defined(__APPLE__) && !defined(__ANDROID__)
#include "stdafx.h"
#else
#include <iconv.h>
#endif

#define CE_SERIAL_IMPLEMENTATION

#include <exception>
#include <string>
#include <locale>
#include <algorithm>
#include <codecvt>
#include <stdio.h>
#include <cuchar>
#include "AddInNative.h"
#include "TenzoM.h"

using namespace std;

static const u16string sClassName(u"TenzoM");
static const u16string sVersion(u"01.01");

static const array<u16string, CAddInNative::ePropLast> osProps =
{
	u"Protocol",
	u"IP",
	u"NetPort",
	u"WebPort",
	u"Connected",
	u"Adr",
	u"Calm",
	u"Overload",
	u"Error",
	u"ErrorCode",
	u"Emulate"
};
static const array<u16string, CAddInNative::ePropLast> osProps_ru =
{
	u"Протокол",
	u"СетевойАдрес",
	u"СетевойПорт",
	u"ВебПорт",
	u"Подключен",
	u"АдресУстройства",
	u"ВесСтабилен",
	u"Перегрузка",
	u"Ошибка",
	u"КодОшибки",
	u"РежимЭмуляции"
};
static const array<u16string, CAddInNative::eMethLast> osMethods =
{ 
	u"Connect",
	u"Disconnect",
	u"GetStatus",
	u"SetZero",
	u"GetWeight",
	u"SwitchToWeighing",
	u"GetPorts",
	u"Version",
};
static const array<u16string, CAddInNative::eMethLast> osMethods_ru = 
{ 
	u"Подключиться", 
	u"Отключиться", 
	u"ПолучитьСтатус", 
	u"ОбнулитьПоказанияВеса", 
	u"ПолучитьВес",
	u"ПереключитьВРежимВзвешивания",
	u"ПолучитьДоступныеПорты",
	u"Версия"
};

uint32_t convToShortWchar   (WCHAR_T** Dest, const wchar_t* Source, uint32_t len = 0);
uint32_t convFromShortWchar (wchar_t** Dest, const WCHAR_T* Source, uint32_t len = 0);
uint32_t getLenShortWcharStr(const WCHAR_T* Source);
AppCapabilities g_capabilities = eAppCapabilitiesInvalid;

//---------------------------------------------------------------------------//
long GetClassObject(const WCHAR_T* wsName, IComponentBase** pInterface)
{
	try
	{
		if (!*pInterface)
		{
			*pInterface = new CAddInNative;
			return (long)*pInterface;
		}
	}
	catch (...) {}
	return 0;
}
//---------------------------------------------------------------------------//
AppCapabilities SetPlatformCapabilities(const AppCapabilities capabilities)
{
	g_capabilities = capabilities;
	return eAppCapabilitiesLast;
}
//---------------------------------------------------------------------------//
long DestroyObject(IComponentBase** pIntf)
{
	try
	{
		if (!*pIntf)
			return -1;

		delete* pIntf;
		*pIntf = 0;
	}
	catch (...) {}
	return 0;
}
//---------------------------------------------------------------------------//
const WCHAR_T* GetClassNames()
{
	WCHAR_T* names = nullptr;
	try
	{
		names = (WCHAR_T*)sClassName.c_str();
	}
	catch (...) {  }
	return names; 
}
//---------------------------------------------------------------------------//

// CAddInNative
//---------------------------------------------------------------------------//
CAddInNative::CAddInNative()
{
	try
	{
		m_iMemory  = 0;
		m_iConnect = 0;
#ifdef _DEBUG
		tenzom.IP  = L"Проверка строк!";
#endif
	}
	catch (...) { CatchedException(current_exception(), L"CAddInNative"); }
}
//---------------------------------------------------------------------------//
CAddInNative::~CAddInNative()
{
	try
	{
		tenzom.ClosePort();
	}
	catch (...) { CatchedException(current_exception(), L"~CAddInNative"); }
}
//---------------------------------------------------------------------------//
bool CAddInNative::Init(void* pConnection)
{
	try
	{
		m_iConnect = (IAddInDefBase*)pConnection;
	}
	catch (...) { CatchedException(current_exception(), L"Init"); }
	return m_iConnect != NULL;
}
//---------------------------------------------------------------------------//
long CAddInNative::GetInfo()
{
	// Component should put supported component technology version 
	// This component supports 2.1 version
	return 2000;
}
//---------------------------------------------------------------------------//
void CAddInNative::Done()
{
	try
	{
	}
	catch (...) { CatchedException(current_exception(), L"Done"); }
}
/////////////////////////////////////////////////////////////////////////////
// ILanguageExtenderBase
//---------------------------------------------------------------------------//
bool CAddInNative::RegisterExtensionAs(WCHAR_T** wsExtensionName)
{
	bool success = false;
	try
	{
		if (wsExtensionName != nullptr) {

			const size_t size = (sClassName.size() + 1) * sizeof(char16_t);

			if (m_iMemory && m_iMemory->AllocMemory(reinterpret_cast<void**>(wsExtensionName), size) && *wsExtensionName != nullptr) {
				memcpy(*wsExtensionName, sClassName.c_str(), size);
				success = true;
			}
			
		}
	}
	catch (...) { CatchedException(current_exception(), L"RegisterExtensionAs"); }

	return true;
}
//---------------------------------------------------------------------------//
long CAddInNative::GetNProps()
{
	return ePropLast;
}

//---------------------------------------------------------------------------//
long CAddInNative::FindProp(const WCHAR_T* wsPropName)
{
	long index = -1;
	try
	{

		index = FindName(osProps, wsPropName);
		if (index < 0)
			index = FindName(osProps_ru, wsPropName);
	}
	catch (...) { CatchedException(current_exception(), L"FindProp"); }

	return index;
}

//---------------------------------------------------------------------------//
const WCHAR_T* CAddInNative::GetPropName(long lPropNum, long lPropAlias)
{
	WCHAR_T* result = nullptr;

	try
	{
		if (lPropNum >= ePropLast) return NULL;

		const basic_string<char16_t>* usCurrentName;

		switch (lPropAlias)
		{
		case 0: // First language
		{
			usCurrentName = &osProps[lPropNum];
			break;
		}
		case 1: // Second language
		{
			usCurrentName = &osProps_ru[lPropNum];
			break;
		}
		default:
			return 0;
		}

		if (usCurrentName == nullptr || usCurrentName->length() == 0) {
			return nullptr;
		}

		const size_t bytes = (usCurrentName->length() + 1) * sizeof(char16_t);

		if (!m_iMemory || !m_iMemory->AllocMemory(reinterpret_cast<void**>(&result), bytes)) {
			return nullptr;
		};

		memcpy(result, usCurrentName->c_str(), bytes);
	}
	catch (...) { CatchedException(current_exception(), L"GetPropName"); }

	return result;
}
//---------------------------------------------------------------------------//
bool CAddInNative::GetPropVal(const long lPropNum, tVariant* pvarPropVal)
{
	try
	{

		switch (lPropNum)
		{
		case ePropProtocol:
		{
			TV_VT(pvarPropVal) = VTYPE_I1;
			TV_I1(pvarPropVal) = tenzom.Protocol;
			return true;
		}
		case ePropIP:
		{
			SetPropString(pvarPropVal, tenzom.IP);
			return true;
		}
		case ePropNetPort:
		{
			TV_VT(pvarPropVal) = VTYPE_I4;
			TV_I4(pvarPropVal) = tenzom.NetPort;
			return true;
		}
		case ePropWebPort:
		{
			TV_VT(pvarPropVal) = VTYPE_I4;
			TV_I4(pvarPropVal) = tenzom.WebPort;
			return true;
		}
		case ePropConnected:
		{
			TV_VT(pvarPropVal) = VTYPE_BOOL;
			TV_BOOL(pvarPropVal) = tenzom.PortOpened();
			return true;
		}
		case ePropAdr:
		{
			TV_VT(pvarPropVal) = VTYPE_I1;
			TV_I1(pvarPropVal) = tenzom.Adr;
			return true;
		}
		case ePropCalm:
		{
			TV_VT(pvarPropVal) = VTYPE_BOOL;
			TV_BOOL(pvarPropVal) = tenzom.Calm;
			return true;
		}
		case ePropError:
		{
			SetPropString(pvarPropVal, tenzom.Error);
			return true;
		}
		case ePropErrorCode:
		{
			TV_VT(pvarPropVal) = VTYPE_I4;
			TV_I4(pvarPropVal) = tenzom.LastError;
			return true;
		}
		case ePropOverload:
		{
			TV_VT(pvarPropVal) = VTYPE_BOOL;
			TV_BOOL(pvarPropVal) = tenzom.Overload;
			return true;
		}
		case ePropEmulate:
		{
			TV_VT(pvarPropVal) = VTYPE_BOOL;
			TV_BOOL(pvarPropVal) = tenzom.Emulate;
			return true;
		}
		default:
			return false;
		}
	}
	catch (...) { CatchedException(current_exception(), L"GetPropVal"); }

	return true;
}
//---------------------------------------------------------------------------//
bool CAddInNative::SetPropVal(const long lPropNum, tVariant *varPropVal)
{
	try
	{
		switch (lPropNum)
		{
		case ePropProtocol:
		{
			const int prot = TV_I1(varPropVal);
			if ((prot < 0) || (prot > 3))
			{
				addError(u"Ошибка установки протокола - должно быть число от 0 до 3");
				return false;
			}
			tenzom.Protocol = static_cast<TenzoM::ProtocolType>(prot);
			return true;
		}
		case ePropIP:
		{
			tenzom.IP = GetParamString(varPropVal);
			return true;
		}
		case ePropNetPort:
		{
			tenzom.NetPort = TV_I4(varPropVal);
			return true;
		}
		case ePropWebPort:
		{
			tenzom.WebPort = TV_I4(varPropVal);
			return true;
		}
		case ePropConnected:
		{
			return true;
		}
		case ePropAdr:
		{
			tenzom.Adr = TV_I1(varPropVal);
			return true;
		}
		case ePropError:
		{
			tenzom.Error = GetParamString(varPropVal);
			return true;
		}
		case ePropErrorCode:
		{
			tenzom.LastError = TV_UI4(varPropVal);
			return true;
		}
		case ePropEmulate:
		{
			tenzom.Emulate = TV_BOOL(varPropVal);
			return true;
		}
		default:
			return false;
		}

	}
	catch (...) { CatchedException(current_exception(), L"SetPropVal"); }
	return false;
}
//---------------------------------------------------------------------------//
bool CAddInNative::IsPropReadable(const long lPropNum)
{
	return true;
}
//---------------------------------------------------------------------------//
bool CAddInNative::IsPropWritable(const long lPropNum)
{
	switch (lPropNum)
	{
	case ePropProtocol:
	case ePropIP:
	case ePropNetPort:
	case ePropWebPort:
	case ePropAdr:
	case ePropError:
	case ePropErrorCode:
	case ePropEmulate:
		return true;
	default:
		return false;
	}

	return false;
}
//---------------------------------------------------------------------------//
long CAddInNative::GetNMethods()
{
	return eMethLast;
}

//---------------------------------------------------------------------------//
long CAddInNative::FindMethod(const WCHAR_T* wsMethodName)
{
	long index = -1;
	try
	{
		index = FindName(osMethods, wsMethodName);
		if (index < 0)
			index = FindName(osMethods_ru, wsMethodName);
	}
	catch (...) { CatchedException(current_exception(), L"FindMethod"); }
	return index;
}
//---------------------------------------------------------------------------//
const WCHAR_T* CAddInNative::GetMethodName(const long lMethodNum, const long lMethodAlias)
{
	WCHAR_T* result = nullptr;
	try
	{
		if (lMethodNum >= eMethLast)
			return nullptr;

		const basic_string<char16_t>* usCurrentName;

		switch (lMethodAlias)
		{
		case 0: // First language
		{
			usCurrentName = &osMethods[lMethodNum];
			break;
		}
		case 1: // Second language
		{
			usCurrentName = &osMethods_ru[lMethodNum];
			break;
		}
		default:
			return nullptr;
		}

		if (usCurrentName == nullptr)
			return nullptr;


		const size_t bytes = (usCurrentName->length() + 1) * sizeof(char16_t);

		if (!m_iMemory || !m_iMemory->AllocMemory(reinterpret_cast<void**>(&result), bytes)) {
			return nullptr;
		};

		memcpy(result, usCurrentName->c_str(), bytes);

	}
	catch (...) { CatchedException(current_exception(), L"GetMethodName"); }
	return result;
}
//---------------------------------------------------------------------------//
long CAddInNative::GetNParams(const long lMethodNum)
{
	switch (lMethodNum)
	{
	case eMethConnect:
		return 3;
	default:
		return 0;
	}

	return 0;
}
//---------------------------------------------------------------------------//
bool CAddInNative::GetParamDefValue(const long lMethodNum, const long lParamNum, tVariant *pvarParamDefValue)
{
	try
	{
		TV_VT(pvarParamDefValue) = VTYPE_EMPTY;
	}
	catch (...) { CatchedException(current_exception(), L"GetParamDefValue"); }

	return false;
}
//---------------------------------------------------------------------------//
bool CAddInNative::HasRetVal(const long lMethodNum)
{
	try
	{

		switch (lMethodNum)
		{
		case eMethConnect:
		case eMethGetStatus:
		case eMethGetWeight:
		case eMethGetPorts:
		case eMethVersion:
			return true;
		default:
			return false;
		}
	}
	catch (...) { CatchedException(current_exception(), L"HasRetVal"); }

	return false;
}
//---------------------------------------------------------------------------//
bool CAddInNative::CallAsProc(const long lMethodNum, tVariant* paParams, const long lSizeArray)
{
	try
	{
		switch (lMethodNum)
		{
		case eMethConnect:
		{
			const wstring name = GetParamString(&paParams[0]);
			const long    bound = paParams[1].intVal;
			const int     deviceAdr = paParams[2].ui8Val;
			tenzom.OpenPort(name, bound, deviceAdr);
			return true;
		}
		case eMethDisconnect:
			tenzom.ClosePort();
			return true;
		case eMethSetZero:
			tenzom.SetZero();
			return true;
		case eMethSwitchToWeighing:
			tenzom.SwitchToWeighing();
			return true;
		case eMethVersion:
			break;
		default:
			return false;
		}
	}
	catch (...) { CatchedException(current_exception(), L"CallAsProc"); }

	return true;
}
//---------------------------------------------------------------------------//
bool CAddInNative::CallAsFunc(const long lMethodNum, tVariant* pvarRetValue, tVariant* paParams, const long lSizeArray)
{
	try
	{
		switch (lMethodNum)
		{
		case eMethVersion:
			SetPropString(pvarRetValue, sVersion);
			return true;
		case eMethConnect:
		{
			wstring name = GetParamString(&paParams[0]);
			long    bound = paParams[1].intVal;
			int     deviceAdr = paParams[2].ui8Val;
			TV_VT(pvarRetValue) = VTYPE_BOOL;
			TV_BOOL(pvarRetValue) = tenzom.OpenPort(name, bound, deviceAdr);
			return true;
		}
		case eMethGetWeight:
		{
			TV_VT(pvarRetValue) = VTYPE_I4;
			TV_I4(pvarRetValue) = tenzom.GetWeight();
			return true;
		}
		default:
			return false;
		}
	}
	catch (...) { CatchedException(current_exception(), L"CallAsFunc"); }
	return true;
}
//---------------------------------------------------------------------------//
void CAddInNative::SetLocale(const WCHAR_T* loc)
{
#if !defined( __linux__ ) && !defined(__APPLE__) && !defined(__ANDROID__)
	_wsetlocale(LC_ALL, L"");
#else
	setlocale(LC_ALL, "");
#endif
}

/////////////////////////////////////////////////////////////////////////////
// LocaleBase
//---------------------------------------------------------------------------//
bool CAddInNative::setMemManager(void* mem)
{
	try
	{
		m_iMemory = (IMemoryManager*)mem;
	}
	catch (...) { CatchedException(current_exception(), L"setMemManager"); }
	return m_iMemory != 0;
}
template <size_t N>
//---------------------------------------------------------------------------//
long CAddInNative::FindName(array<u16string, N> arr, const WCHAR_T* name)
{
	long index = -1;
	try
	{
		const size_t namesize = wcslen(name);
		for (size_t i = 0; i < N; i++)
		{
			u16string word = arr.at(i);
			if (word.size() != namesize) continue;
			bool issame = true;

			for (size_t n = 0; n < word.size(); n++)
			{
				if (::tolower(word[n]) != ::tolower(name[n]))
				{
					issame = false;
					break;
				}
			}
			if (issame)
			{
				index = i;
				break;
			}
		}
	}
	catch (...) { CatchedException(current_exception(), L"FindName"); }

	return index;
}
//---------------------------------------------------------------------------//
void CAddInNative::addError(uint32_t wcode, const wchar_t* source, const wchar_t* descriptor, long code)
{
	if (m_iConnect)
	{
		WCHAR_T* err = 0;
		WCHAR_T* descr = 0;

		::convToShortWchar(&err, source);
		::convToShortWchar(&descr, descriptor);

		m_iConnect->AddError(wcode, err, descr, code);
		delete[] err;
		delete[] descr;
	}
}
//---------------------------------------------------------------------------//
void CAddInNative::addError(uint32_t wcode, const char16_t* source, const char16_t* descriptor, long code)
{
	if (m_iConnect)
	{
		m_iConnect->AddError(wcode, (WCHAR_T*)source, (WCHAR_T*)descriptor, code);
	}
}
//---------------------------------------------------------------------------//
void CAddInNative::addError(const char16_t* descriptor)
{
	addError(ADDIN_E_VERY_IMPORTANT, sClassName.c_str(), descriptor, -1);
}
//---------------------------------------------------------------------------//
uint32_t convToShortWchar(WCHAR_T** Dest, const wchar_t* Source, uint32_t len)
{
	if (!len)
		len = ::wcslen(Source) + 1;

	if (!*Dest)
		*Dest = new WCHAR_T[len];

	WCHAR_T* tmpShort = *Dest;
	wchar_t* tmpWChar = (wchar_t*)Source;
	uint32_t res = 0;

	::memset(*Dest, 0, len * sizeof(WCHAR_T));
#ifdef __linux__
	size_t succeed = (size_t)-1;
	size_t f = len * sizeof(wchar_t), t = len * sizeof(WCHAR_T);
	const char* fromCode = sizeof(wchar_t) == 2 ? "UTF-16" : "UTF-32";
	iconv_t cd = iconv_open("UTF-16LE", fromCode);
	if (cd != (iconv_t)-1)
	{
		succeed = iconv(cd, (char**)&tmpWChar, &f, (char**)&tmpShort, &t);
		iconv_close(cd);
		if (succeed != (size_t)-1)
			return (uint32_t)succeed;
	}
#endif //__linux__
	for (; len; --len, ++res, ++tmpWChar, ++tmpShort)
	{
		*tmpShort = (WCHAR_T)*tmpWChar;
	}

	return res;
}
//---------------------------------------------------------------------------//
uint32_t convFromShortWchar(wchar_t** Dest, const WCHAR_T* Source, uint32_t len)
{
	if (!len)
		len = getLenShortWcharStr(Source) + 1;

	if (!*Dest)
		*Dest = new wchar_t[len];

	wchar_t* tmpWChar = *Dest;
	WCHAR_T* tmpShort = (WCHAR_T*)Source;
	uint32_t res = 0;

	::memset(*Dest, 0, len * sizeof(wchar_t));
#ifdef __linux__
	size_t succeed = (size_t)-1;
	const char* fromCode = sizeof(wchar_t) == 2 ? "UTF-16" : "UTF-32";
	size_t f = len * sizeof(WCHAR_T), t = len * sizeof(wchar_t);
	iconv_t cd = iconv_open("UTF-32LE", fromCode);
	if (cd != (iconv_t)-1)
	{
		succeed = iconv(cd, (char**)&tmpShort, &f, (char**)&tmpWChar, &t);
		iconv_close(cd);
		if (succeed != (size_t)-1)
			return (uint32_t)succeed;
	}
#endif //__linux__
	for (; len; --len, ++res, ++tmpWChar, ++tmpShort)
	{
		*tmpWChar = (wchar_t)*tmpShort;
	}

	return res;
}
//---------------------------------------------------------------------------//
uint32_t getLenShortWcharStr(const WCHAR_T* Source)
{
	uint32_t res = 0;
	WCHAR_T* tmpShort = (WCHAR_T*)Source;

	while (*tmpShort++)
		++res;

	return res;
}
//---------------------------------------------------------------------------//

wstring CAddInNative::GetParamString(const tVariant * param)
{
	wstring name;
	try
	{
		if (param->vt == VTYPE_PWSTR)
		{
			wchar_t* prop = 0;
#if defined(__linux__) || defined(__APPLE__) || defined(__ANDROID__)
			size_t len = ::convFromShortWchar(&prop, param->pwstrVal);
			const size_t size_need = wcstombs(NULL, prop, len);
			wstring text(prop);
			name.assign(text.begin(), text.end());
			//name = u16Convert.to_bytes(param->pwstrVal);  
#else
			size_t len = ::convFromShortWchar(&prop, param->pwstrVal);
			const size_t size_need = wcstombs(NULL, prop, len);
			wstring text(prop);
			//wcstombs(&text.at(0), prop, len);
			name.assign(text.begin(), text.end());
#endif
		}
		else if (param->vt == VTYPE_PSTR)
		{
			//name = string(param->pstrVal);
		}
	}
	catch (...) { CatchedException(current_exception(), L"GetParamString"); }

	return name;
}
//---------------------------------------------------------------------------//
void CAddInNative::SetPropString(tVariant* pvarRetValue, u16string text)
{
	try
	{
		if (m_iMemory->AllocMemory((void**)&pvarRetValue->pwstrVal, (text.length() + 1) * sizeof(char16_t)))
		{
			memcpy(pvarRetValue->pwstrVal, text.c_str(), text.length() * sizeof(char16_t));
			TV_VT(pvarRetValue)   = VTYPE_PWSTR;
			pvarRetValue->wstrLen = text.length();
		}
		else
		{
			TV_VT(pvarRetValue)    = VTYPE_PWSTR;
			pvarRetValue->pwstrVal = NULL;
			pvarRetValue->wstrLen  = 0;
		}
	}
	catch (...) { CatchedException(current_exception(), L"SetPropString"); }
}
//---------------------------------------------------------------------------//
void CAddInNative::SetPropString(tVariant* pvarRetValue, wstring text)
{
	try
	{
#if defined(__linux__) || defined(__APPLE__) || defined(__ANDROID__)
		char16_t newtext[text.length() + 1] = { 0 };
		auto p = &newtext[0];
		convToShortWchar(&p, text.c_str(), text.length());
		u16string wcText(newtext);
#else
		//int count = MultiByteToWideChar(CP_ACP, 0, text.c_str(), text.length(), NULL, 0);
		//wstring wstr(count, 0);
		//MultiByteToWideChar(CP_ACP, 0, text.c_str(), text.length(), &wstr.at(0), count);
		//u16string wcText(wstr.begin(), wstr.end());
		u16string wcText(text.begin(), text.end());
#endif
		SetPropString(pvarRetValue, wcText);
	}
	catch (...) { CatchedException(current_exception(), L"SetPropString"); }
}
//---------------------------------------------------------------------------//
void CAddInNative::CatchedException(exception_ptr eptr, wstring funcName)
{
	string msg;
	try
	{
		if (eptr)
			rethrow_exception(eptr);
	}
	catch (exception& ex) {
		msg = ex.what();
	}
	catch (const string& ex) {
		msg = ex;
	}
	catch (...) {
		msg = "Exception error";
	}

	if (!msg.empty())
	{
		const wstring sCaption = L"TenzoM1CAddin (" + funcName + L")";
		const wstring sMessage(msg.begin(), msg.end());
		addError(ADDIN_E_VERY_IMPORTANT, sCaption.c_str(), sMessage.c_str(), -1);
	}
}