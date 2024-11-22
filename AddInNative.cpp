#if !defined( __linux__ ) && !defined(__APPLE__) && !defined(__ANDROID__)
#include "stdafx.h"
#else
#include <iconv.h>
#include <unistd.h>
#include <netdb.h>
#include <arpa/inet.h>
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
static const u16string sVersion(u"02.00");

static const array<u16string, CAddInNative::ePropLast> osProps =
{
	u"Protocol",
	u"IP",
	u"NetPort",
	u"NetMode",
	u"Name",
	u"Connected",
	u"Adr",
	u"Event",
	u"Scale2",
	u"Calm",
	u"Overload",
	u"RiseExternalEvent",
	u"SendKeys",
	u"NumpadKeys",
	u"Error",
	u"ErrorCode",
	u"Emulate",
	u"EmulMinKg",
	u"EmulMaxKg",
	u"WriteLog",
	u"LogFile",
	u"DecimalPoint"
};
static const array<u16string, CAddInNative::ePropLast> osProps_ru =
{
	u"Протокол",
	u"СетевойАдрес",
	u"СетевойПорт",
	u"СетевойРежим",
	u"ИмяВесов",
	u"Подключен",
	u"АдресУстройства",
	u"Событие",
	u"ВторойДатчик",
	u"ВесСтабилен",
	u"Перегрузка",
	u"ГенерироватьВнешнееСобытие",
	u"ПосылатьНажатиеКлавиш",
	u"ПосылатьКодыНумпадКлавиш",
	u"Ошибка",
	u"КодОшибки",
	u"РежимЭмуляции",
	u"ЭмуляцияКилограммМинимально",
	u"ЭмуляцияКилограммМаксимально",
	u"Логирование",
	u"ЛогФайл",
	u"РазделительДробнойЧасти"
};
static const array<u16string, CAddInNative::eMethLast> osMethods =
{ 
	u"Connect",
	u"Disconnect",
	u"GetStatus",
	u"SetZero",
	u"GetWeight",
	u"SwitchToWeighing",
	u"GetGetEnteredCode",
	u"GetIndicatorText",
	u"SetIndicatorText",
	u"SetInputChannel",
	u"Tare",
	u"GetSerialNum",
	u"GetDeviceInfo",
	u"GetPorts",
	u"Version",
	u"GetIP"
};
static const array<u16string, CAddInNative::eMethLast> osMethods_ru = 
{ 
	u"Подключиться", 
	u"Отключиться", 
	u"ПолучитьСтатус", 
	u"ОбнулитьПоказанияВеса", 
	u"ПолучитьВес",
	u"ПереключитьВРежимВзвешивания",
	u"ПолучитьВведенныйКод",
	u"ПолучитьТекстИндикатора",
	u"УстановитьТекстИндикатора",
	u"УстановитьВходнойКанал",
	u"Тара",
	u"СерийныйНомер",
	u"ИнформацияОбУстройстве",
	u"ПолучитьДоступныеПорты",
	u"Версия",
	u"ПолучитьIP"
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
		tenzom.IP  = u"Проверка строк!";
#endif
	}
	catch (...) { CatchedException(current_exception(), u"CAddInNative"); }
}
//---------------------------------------------------------------------------//
CAddInNative::~CAddInNative()
{
	try
	{
		tenzom.ClosePort();

	}
	catch (...) { CatchedException(current_exception(), u"~CAddInNative"); }
}
//---------------------------------------------------------------------------//
bool CAddInNative::Init(void* pConnection)
{
	try
	{
		m_iConnect = (IAddInDefBase*)pConnection;
		m_iConnect->SetEventBufferDepth(10);
	}
	catch (...) { CatchedException(current_exception(), u"Init"); }
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
	catch (...) { CatchedException(current_exception(), u"Done"); }
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
	catch (...) { CatchedException(current_exception(), u"RegisterExtensionAs"); }

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
	catch (...) { CatchedException(current_exception(), u"FindProp"); }
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
	catch (...) { CatchedException(current_exception(), u"GetPropName"); }

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
		case ePropNetMode:
		{
			TV_VT(pvarPropVal) = VTYPE_BOOL;
			TV_BOOL(pvarPropVal) = tenzom.NetMode;
			return true;
		}
		case ePropName:
		{
			SetPropString(pvarPropVal, tenzom.Name);
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
		case ePropEvent:
		{
			TV_VT(pvarPropVal) = VTYPE_BOOL;
			TV_BOOL(pvarPropVal) = tenzom.Event;
			return true;
		}
		case ePropNScal:
		{
			TV_VT(pvarPropVal) = VTYPE_BOOL;
			TV_BOOL(pvarPropVal) = tenzom.NScal;
			return true;
		}
		case ePropCalm:
		{
			TV_VT(pvarPropVal) = VTYPE_BOOL;
			TV_BOOL(pvarPropVal) = tenzom.Calm;
			return true;
		}
		case ePropRiseExternalEvent:
		{
			TV_VT(pvarPropVal) = VTYPE_BOOL;
			TV_BOOL(pvarPropVal) = RiseExternalEvent;
			return true;
		}
		case ePropSendKeys:
		{
			TV_VT(pvarPropVal) = VTYPE_BOOL;
			TV_BOOL(pvarPropVal) = tenzom.SendKeys;
			return true;
		}
		case ePropNumpadKeys:
		{
			TV_VT(pvarPropVal) = VTYPE_BOOL;
			TV_BOOL(pvarPropVal) = tenzom.NumpadKeys;
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
		case ePropEmulMinKg:
		{
			TV_VT(pvarPropVal) = VTYPE_I4;
			TV_I4(pvarPropVal) = tenzom.EmulMinKg;
			return true;
		}
		case ePropEmulMaxKg:
		{
			TV_VT(pvarPropVal) = VTYPE_I4;
			TV_I4(pvarPropVal) = tenzom.EmulMaxKg;
			return true;
		}
		case ePropWriteLog:
		{
			TV_VT  (pvarPropVal) = VTYPE_BOOL;
			TV_BOOL(pvarPropVal) = tenzom.WriteLog;
			return true;
		}
		case ePropLogFile:
		{
			SetPropString(pvarPropVal, tenzom.LogFile);
			return true;
		}
		case ePropDecimalPoint:
		{
			u16string s(1, tenzom.DecimalPoint);
			SetPropString(pvarPropVal, s);
			return true;
		}

		default:
			return false;
		}
	}
	catch (...) { CatchedException(current_exception(), u"GetPropVal"); }

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
		case ePropNetMode:
		{
			tenzom.NetMode = TV_BOOL(varPropVal);
			return true;
		}
		case ePropName:
		{
			tenzom.Name = GetParamString(varPropVal);
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
		case ePropRiseExternalEvent:
		{
			RiseExternalEvent = TV_BOOL(varPropVal);
			return true;
		}
		case ePropSendKeys:
		{
			tenzom.SendKeys = TV_BOOL(varPropVal);
			return true;
		}
		case ePropNumpadKeys:
		{
			tenzom.NumpadKeys = TV_BOOL(varPropVal);
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
		case ePropEmulMinKg:
		{
			tenzom.EmulMinKg = TV_I4(varPropVal);
			return true;
		}
		case ePropEmulMaxKg:
		{
			tenzom.EmulMaxKg = TV_I4(varPropVal);
			return true;
		}
		case ePropWriteLog:
		{
			tenzom.WriteLog = TV_BOOL(varPropVal);
			return true;
		}
		case ePropLogFile:
		{
			tenzom.LogFile = GetParamString(varPropVal);
			return true;
		}
		case ePropDecimalPoint:
		{
			u16string param = GetParamString(varPropVal);
			string s(param.begin(), param.end());
			tenzom.DecimalPoint = s[0];
			return true;
		}

		default:
			return false;
		}

	}
	catch (...) { CatchedException(current_exception(), u"SetPropVal"); }
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
	case ePropNetMode:
	case ePropName:
	case ePropAdr:
	case ePropError:
	case ePropErrorCode:
	case ePropEmulate:
	case ePropEmulMinKg:
	case ePropEmulMaxKg:
	case ePropWriteLog:
	case ePropLogFile:
	case ePropDecimalPoint:
	case ePropRiseExternalEvent:
	case ePropSendKeys:
	case ePropNumpadKeys:
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
	catch (...) { CatchedException(current_exception(), u"FindMethod"); }
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
	catch (...) { CatchedException(current_exception(), u"GetMethodName"); }
	return result;
}
//---------------------------------------------------------------------------//
long CAddInNative::GetNParams(const long lMethodNum)
{
	switch (lMethodNum)
	{
	case eMethConnect		  : return 3;
	case eMethGetIndicatorText: return 1;
	case eMethSetIndicatorText: return 2;
	case eMethSetInputChannel : return 1;
	case eMethGetIP           : return 1;
	default: return 0;
	}

	return 0;
}
//---------------------------------------------------------------------------//
bool CAddInNative::GetParamDefValue(const long lMethodNum, const long lParamNum, tVariant *pvarParamDefValue)
{
	try
	{
		TV_VT(pvarParamDefValue) = VTYPE_EMPTY;
		switch (lMethodNum)
		{
		case eMethGetIP:
		{
			TV_BOOL(pvarParamDefValue) = VTYPE_BOOL;
			return true;
		}
		default:
			return false;
		}
	}
	catch (...) { CatchedException(current_exception(), u"GetParamDefValue"); }

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
		case eMethGetGetEnteredCode:
		case eMethGetIndicatorText:
		case eMethGetSerialNum:
		case eMethGetDeviceInfo:
		case eMethGetPorts:
		case eMethVersion:
		case eMethGetIP:
			return true;
		default:
			return false;
		}
	}
	catch (...) { CatchedException(current_exception(), u"HasRetVal"); }

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
			const u16string name      = GetParamString(&paParams[0]);
			const long      bound     = paParams[1].intVal;
			const int       deviceAdr = paParams[2].ui8Val;
			tenzom.OpenPort(name, bound, deviceAdr);
			return true;
		}
		case eMethDisconnect:
			tenzom.ClosePort();
			return true;
		case eMethSetZero:
			tenzom.SetZero();
			return true;
		case eMethSetIndicatorText:
		{
			const int       line = paParams[0].ui8Val;
			const u16string text = GetParamString(&paParams[1]);
			tenzom.SetIndicatorText(line, text);
			return true;
		}
		case eMethSwitchToWeighing:
			tenzom.SwitchToWeighing();
			return true;
		case eMethSetInputChannel:
		{
			const int channelNum = paParams[0].ui8Val;
			tenzom.SetInputChannel(channelNum);
			return true;
		}
		case eMethTare:
			tenzom.Tare();
			return true;
		case eMethVersion:
			break;
		default:
			return false;
		}
	}
	catch (...) { CatchedException(current_exception(), u"CallAsProc"); }

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
		{
			TV_VT  (pvarRetValue) = VTYPE_UI1;
			TV_BOOL(pvarRetValue) = tenzom.GetStatus();
			return true;
		}
		case eMethGetStatus:
		{
			SetPropString(pvarRetValue, sVersion);
			return true;
		}
		case eMethConnect:
		{
			u16string name      = GetParamString(&paParams[0]);
			long      bound     = paParams[1].intVal;
			int       deviceAdr = paParams[2].ui8Val;
			TV_VT  (pvarRetValue) = VTYPE_BOOL;
			TV_BOOL(pvarRetValue) = tenzom.OpenPort(name, bound, deviceAdr);
			return true;
		}
		case eMethGetWeight:
		{
			auto weight = tenzom.GetWeight();
			if (RiseExternalEvent && tenzom.Event)
			{
				auto code = tenzom.GetEnteredCode();
				if (code != 0)
				{
					string s = "";
					s += code;
					ExternalEvent(u"EnteredCode", u16string(s.begin(), s.end()));
				}
			}
			TV_VT(pvarRetValue) = VTYPE_I4;
			TV_I4(pvarRetValue) = weight;
			return true;
		}
		case eMethGetGetEnteredCode:
		{
			auto code = tenzom.GetEnteredCode();
			string s = "";
			if (code != 0) s += code;
			SetPropString(pvarRetValue, u16string(s.begin(), s.end()));
			return true;
		}
		case eMethGetIndicatorText:
		{
			int line = paParams[0].ui8Val;
			SetPropString(pvarRetValue, tenzom.GetIndicatorText(line));
			return true;
		}
		case eMethGetSerialNum:
		{
			int sn = tenzom.GetSerialNum();
			TV_VT(pvarRetValue) = VTYPE_I4;
			TV_I4(pvarRetValue) = sn;
			return true;
		}
		case eMethGetDeviceInfo:
		{
			SetPropString(pvarRetValue, tenzom.Version());
			return true;
		}
		case eMethGetIP:
		{
			bool all = paParams[0].bVal;
			SetPropString(pvarRetValue, GetOwnIP(all));
			return true;
		}
		default:
			return false;
		}
	}
	catch (...) { CatchedException(current_exception(), u"CallAsFunc"); }
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
	catch (...) { CatchedException(current_exception(), u"setMemManager"); }
	return m_iMemory != 0;
}
template <size_t N>
//---------------------------------------------------------------------------//
long CAddInNative::FindName(array<u16string, N> arr, const WCHAR_T* name)
{
	long  index  = -1;
	char* oldLoc = nullptr;
	try
	{
		oldLoc = setlocale(LC_ALL, "");

		u16string lname((char16_t *)name);
		transform(lname.begin(), lname.end(), lname.begin(), towlower);
		const size_t namesize = lname.size();
		for (size_t i = 0; i < N; i++)
		{
			u16string word(arr.at(i));
			if (word.size() != namesize) continue;
			transform(word.begin(), word.end(), word.begin(), towlower);
			bool issame = true;
			for (size_t n = 0; n < word.size(); n++)
			{
				if (word[n] != lname[n])
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
	catch (...) { CatchedException(current_exception(), u"FindName"); }
	if (oldLoc)
		setlocale(LC_ALL, oldLoc);
	return index;
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

u16string CAddInNative::GetParamString(const tVariant * param)
{
	u16string name;
	try
	{
		if (param->vt == VTYPE_PWSTR)
		{
			wchar_t* prop = 0;
			size_t len = ::convFromShortWchar(&prop, param->pwstrVal);
			name = u16string((char16_t*)prop);
		}
		else if (param->vt == VTYPE_PSTR)
		{
			//name = u16string(param->pstrVal);
		}
	}
	catch (...) { CatchedException(current_exception(), u"GetParamString"); }

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
	catch (...) { CatchedException(current_exception(), u"SetPropString"); }
}
//---------------------------------------------------------------------------//
void CAddInNative::CatchedException(exception_ptr eptr, u16string funcName)
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
		const u16string sCaption = u"TenzoM1CAddin (" + funcName + u")";
		const u16string sMessage(msg.begin(), msg.end());
		addError(ADDIN_E_VERY_IMPORTANT, sCaption.c_str(), sMessage.c_str(), -1);
	}
}

bool CAddInNative::ExternalEvent(u16string what, u16string data)
{
	const u16string who = u"TenzoM1CAddin";

	try
	{
		if (m_iConnect)
		{
			m_iConnect->ExternalEvent((WCHAR_T*)who.c_str(), (WCHAR_T*)what.c_str(), (WCHAR_T*)data.c_str());
		}
		return true;
	}
	catch (...) { CatchedException(current_exception(), u"ExternalEvent"); }


	return false;
}

u16string CAddInNative::GetOwnIP(bool all)
{
	string ips = "";
#ifdef ISWINDOWS
	WSADATA wsaData;
	if (!WSAStartup(MAKEWORD(2, 2), &wsaData))
	{
#endif
		char chInfo[64];
		if (!gethostname(chInfo, sizeof(chInfo)))
		{
			struct hostent* sh;
			sh = gethostbyname((char*)&chInfo);
			if (sh != NULL)
			{
				int nAdapter = 0;
				while (sh->h_addr_list[nAdapter])
				{
					struct sockaddr_in adr;
					memcpy(&adr.sin_addr, sh->h_addr_list[nAdapter], sh->h_length);
					string ip = string(inet_ntoa(adr.sin_addr));
					if (all)
					{
						if (ips != "") ips += "\n";
						ips += ip;
					}
					else
					{
						ips = ip;
					}
					nAdapter++;
				}
			}
		}
#ifdef ISWINDOWS
	}
	WSACleanup();
#endif

	return u16string(ips.begin(), ips.end());
}
