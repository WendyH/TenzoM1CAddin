#if !defined( __linux__ ) && !defined(__APPLE__) && !defined(__ANDROID__)
#include "stdafx.h"
#endif

#define CE_SERIAL_IMPLEMENTATION

#include <string>
#include <locale>
#include <algorithm>
#include <codecvt>
#include <stdio.h>
#include <wchar.h>
#include "AddInNative.h"
#include "TenzoM.h"
using namespace std;

static const u16string sClassName(u"TenzoM");
static const u16string sVersion(u"01.00");

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
	u"GetFixedBrutto",
	u"GetStatus",
	u"SetZero",
	u"GetNetto",
	u"GetBrutto",
	u"SwitchToWeighing",
	u"GetPorts",
	u"Version",
};
static const array<u16string, CAddInNative::eMethLast> osMethods_ru = 
{ 
	u"Подключиться", 
	u"Отключиться", 
	u"ПолучитьФиксированныйВесБрутто", 
	u"ПолучитьСтатус", 
	u"ОбнулитьПоказанияВеса", 
	u"ПолучитьВесНетто",
	u"ПолучитьВесБрутто",
	u"ПереключитьВРежимВзвешивания",
	u"ПолучитьДоступныеПорты",
	u"Версия"
};

uint32_t convToShortWchar(WCHAR_T** Dest, const wchar_t* Source, uint32_t len = 0);
uint32_t convFromShortWchar(wchar_t** Dest, const WCHAR_T* Source, uint32_t len = 0);
uint32_t getLenShortWcharStr(const WCHAR_T* Source);
AppCapabilities g_capabilities = eAppCapabilitiesInvalid;

template <typename T>
string toUTF8(const basic_string<T, char_traits<T>, allocator<T>>& source)
{
	string result;

	wstring_convert<codecvt_utf8_utf16<T>, T> convertor;
	result = convertor.to_bytes(source);

	return result;
}

template <typename T>
void fromUTF8(const string& source, basic_string<T, char_traits<T>, allocator<T>>& result)
{
	wstring_convert<codecvt_utf8_utf16<T>, T> convertor;
	result = convertor.from_bytes(source);
}

//---------------------------------------------------------------------------//
long GetClassObject(const WCHAR_T* wsName, IComponentBase** pInterface)
{
	if (!*pInterface)
	{
		*pInterface = new CAddInNative;
		return (long)*pInterface;
	}
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
	if (!*pIntf)
		return -1;

	delete *pIntf;
	*pIntf = 0;
	return 0;
}
//---------------------------------------------------------------------------//
const WCHAR_T* GetClassNames()
{
	return (WCHAR_T*)sClassName.c_str();
}
//---------------------------------------------------------------------------//

// CAddInNative
//---------------------------------------------------------------------------//
CAddInNative::CAddInNative()
{
	m_iMemory  = 0;
	m_iConnect = 0;
}
//---------------------------------------------------------------------------//
CAddInNative::~CAddInNative()
{
}
//---------------------------------------------------------------------------//
bool CAddInNative::Init(void* pConnection)
{
	m_iConnect = (IAddInDefBase*)pConnection;
	return m_iConnect != NULL;
}
//---------------------------------------------------------------------------//
long CAddInNative::GetInfo()
{
	// Component should put supported component technology version 
	// This component supports 2.1 version
	return 2100;
}
//---------------------------------------------------------------------------//
void CAddInNative::Done()
{

}
/////////////////////////////////////////////////////////////////////////////
// ILanguageExtenderBase
//---------------------------------------------------------------------------//
bool CAddInNative::RegisterExtensionAs(WCHAR_T** wsExtensionName)
{
	if (wsExtensionName == nullptr) {
		return false;
	}
	const size_t size = (sClassName.size() + 1) * sizeof(char16_t);

	if (!m_iMemory || !m_iMemory->AllocMemory(reinterpret_cast<void **>(wsExtensionName), size) || *wsExtensionName ==  nullptr) {
		return false;
	};

	memcpy(*wsExtensionName, sClassName.c_str(), size);

	return true;
}
//---------------------------------------------------------------------------//
long CAddInNative::GetNProps()
{
	return ePropLast;
}

//---------------------------------------------------------------------------//
template<std::size_t SIZE>
long FindInArrayCaseless(std::array<u16string, SIZE> arr, const WCHAR_T* name)
{
	wchar_t* wcName = 0;
	convFromShortWchar(&wcName, name);
	auto len = wcslen(wcName);
	auto it = find_if(arr.begin(), arr.end(),
		[&](auto& s) {
			if (s.size() != len)
				return false;
			for (size_t i = 0; i < s.size(); ++i)
				if (::tolower(s[i]) != ::tolower(wcName[i]))
					return false;
			return true;
		}
	);
	if (it != arr.end()) {
		return it - arr.begin();
	}
	return -1;
}

//---------------------------------------------------------------------------//
long CAddInNative::FindProp(const WCHAR_T* wsPropName)
{
	auto index = FindInArrayCaseless(osProps, wsPropName);
	if (index < 0)
		index = FindInArrayCaseless(osProps_ru, wsPropName);
	return index;
}

//---------------------------------------------------------------------------//
const WCHAR_T* CAddInNative::GetPropName(long lPropNum, long lPropAlias)
{
	if (lPropNum >= ePropLast)
		return NULL;

	const basic_string<char16_t> *usCurrentName;

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

	WCHAR_T *result = nullptr;

	const size_t bytes = (usCurrentName->length() + 1) * sizeof(char16_t);

	if (!m_iMemory || !m_iMemory->AllocMemory(reinterpret_cast<void **>(&result), bytes)) {
		return nullptr;
	};

	memcpy(result, usCurrentName->c_str(), bytes);

	return result;
}
//---------------------------------------------------------------------------//
bool CAddInNative::GetPropVal(const long lPropNum, tVariant* pvarPropVal)
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
		if (m_iMemory->AllocMemory((void**)&pvarPropVal->pwstrVal, (tenzom.IP.length() + 1) * sizeof(char16_t)))
		{
			memcpy(pvarPropVal->pwstrVal, tenzom.IP.c_str(), tenzom.IP.length() * sizeof(char16_t));
			TV_VT(pvarPropVal)   = VTYPE_PWSTR;
			pvarPropVal->wstrLen = tenzom.IP.length();
			return true;
		}
		else
		{
			TV_VT(pvarPropVal)    = VTYPE_PWSTR;
			pvarPropVal->pwstrVal = NULL;
			pvarPropVal->wstrLen  = 0;
			return true;
		}
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
		TV_VT  (pvarPropVal) = VTYPE_BOOL;
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
		TV_VT  (pvarPropVal) = VTYPE_BOOL;
		TV_BOOL(pvarPropVal) = tenzom.Calm;
		return true;
	}
	case ePropError:
	{
		if (m_iMemory->AllocMemory((void**)&pvarPropVal->pwstrVal, (tenzom.Error.length() + 1) * sizeof(char16_t)))
		{
			memcpy(pvarPropVal->pwstrVal, tenzom.Error.c_str(), tenzom.Error.length() * sizeof(char16_t));
			TV_VT(pvarPropVal)   = VTYPE_PWSTR;
			pvarPropVal->wstrLen = tenzom.Error.length();
			return true;
		}
		else
		{
			TV_VT(pvarPropVal)    = VTYPE_PWSTR;
			pvarPropVal->pwstrVal = NULL;
			pvarPropVal->wstrLen  = 0;
			return true;
		}
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
		TV_VT  (pvarPropVal) = VTYPE_BOOL;
		TV_BOOL(pvarPropVal) = tenzom.Overload;
		return true;
	}
	case ePropEmulate:
	{
		TV_VT  (pvarPropVal) = VTYPE_BOOL;
		TV_BOOL(pvarPropVal) = tenzom.Emulate;
		return true;
	}
	default:
		return false;
	}

	return true;
}
//---------------------------------------------------------------------------//
bool CAddInNative::SetPropVal(const long lPropNum, tVariant *varPropVal)
{
	switch (lPropNum)
	{
	case ePropProtocol:
	{
		tenzom.Protocol = static_cast<TenzoM::ProtocolType>(TV_I1(varPropVal));
		return true;
	}
	case ePropIP:
	{
		u16string sPattern;
		sPattern.clear();
		sPattern.assign(reinterpret_cast<char16_t*>(varPropVal->pwstrVal), varPropVal->wstrLen);

		//u16string ip(reinterpret_cast<char16_t*>(varPropVal->pwstrVal), varPropVal->wstrLen);
		//string ip(reinterpret_cast<char*>(varPropVal->pwstrVal), varPropVal->wstrLen);
		tenzom.IP = sPattern.b;
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
		u16string text(reinterpret_cast<char16_t*>(varPropVal->pwstrVal), varPropVal->wstrLen);
		tenzom.Error = toUTF8(text);
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
	auto index = FindInArrayCaseless(osMethods, wsMethodName);
	if (index < 0)
		index = FindInArrayCaseless(osMethods_ru, wsMethodName);
	return index;
}
//---------------------------------------------------------------------------//
const WCHAR_T* CAddInNative::GetMethodName(const long lMethodNum, const long lMethodAlias)
{

	if (lMethodNum >= eMethLast)
		return nullptr;

	const basic_string<char16_t> *usCurrentName;

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

	WCHAR_T *result = nullptr;

	const size_t bytes = (usCurrentName->length() + 1) * sizeof(char16_t);

	if (!m_iMemory || !m_iMemory->AllocMemory(reinterpret_cast<void **>(&result), bytes)) {
		return nullptr;
	};

	memcpy(result, usCurrentName->c_str(), bytes);

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
bool CAddInNative::GetParamDefValue(const long lMethodNum, const long lParamNum,
	tVariant *pvarParamDefValue)
{
	TV_VT(pvarParamDefValue) = VTYPE_EMPTY;
	return false;
}
//---------------------------------------------------------------------------//
bool CAddInNative::HasRetVal(const long lMethodNum)
{
	switch (lMethodNum)
	{
	case eMethConnect:
	case eMethGetFixedBrutto:
	case eMethGetStatus:
	case eMethGetNetto:
	case eMethGetBrutto:
	case eMethGetPorts:
	case eMethVersion:
		return true;
	default:
		return false;
	}

	return false;
}
//---------------------------------------------------------------------------//
bool CAddInNative::CallAsProc(const long lMethodNum,
	tVariant* paParams, const long lSizeArray)
{
	switch (lMethodNum)
	{
	case eMethConnect:
	{
		if (paParams[0].vt != VTYPE_PWSTR)
			return false;

		u16string name;
		name.clear();
		name.assign(reinterpret_cast<char16_t*>(paParams[0].pwstrVal), paParams[0].wstrLen);

		long  bound     = paParams[1].intVal;
		int   deviceAdr = paParams[2].ui8Val;
		tenzom.OpenPort(toUTF8(name), bound, deviceAdr);
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
	return true;
}
//---------------------------------------------------------------------------//
bool CAddInNative::CallAsFunc(const long lMethodNum,
	tVariant* pvarRetValue, tVariant* paParams, const long lSizeArray)
{
	switch (lMethodNum)
	{
		case eMethVersion:
			version(pvarRetValue);
			return true;
		case eMethConnect:
		{
			if (paParams[0].vt != VTYPE_PWSTR)
				return false;

			u16string name;
			name.clear();
			name.assign(reinterpret_cast<char16_t*>(paParams[0].pwstrVal), paParams[0].wstrLen);

			long  bound     = paParams[1].intVal;
			int   deviceAdr = paParams[2].ui8Val;
			TV_VT(pvarRetValue) = VTYPE_BOOL;
			pvarRetValue->bVal = tenzom.OpenPort(toUTF8(name), bound, deviceAdr);

			return true;
		}
		case eMethGetBrutto:
		{
			TV_VT(pvarRetValue) = VTYPE_I4;
			pvarRetValue->lVal  = tenzom.GetBrutto();
			return true;
		}
		case eMethGetFixedBrutto:
		{
			TV_VT(pvarRetValue) = VTYPE_I4;
			pvarRetValue->lVal = tenzom.GetFixedBrutto();
			return true;
		}
		case eMethGetNetto:
		{
			TV_VT(pvarRetValue) = VTYPE_I4;
			pvarRetValue->lVal = tenzom.GetNetto();
			return true;
		}
		default:
			return false;
	}
	return true;
}

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
	m_iMemory = (IMemoryManager*)mem;
	return m_iMemory != 0;
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

void CAddInNative::version(tVariant* pvarRetValue)
{
	TV_VT(pvarRetValue) = VTYPE_PWSTR;

	if (m_iMemory->AllocMemory((void**)&pvarRetValue->pwstrVal, (sVersion.length() + 1) * sizeof(char16_t)))
	{
		memcpy(pvarRetValue->pwstrVal, sVersion.c_str(), (sVersion.length() + 1) * sizeof(char16_t));
		pvarRetValue->wstrLen = sVersion.length();
	}
}

