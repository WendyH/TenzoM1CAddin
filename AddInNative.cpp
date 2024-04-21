#if !defined( __linux__ ) && !defined(__APPLE__) && !defined(__ANDROID__)
#include "stdafx.h"
#endif

#include <stdio.h>
#include <wchar.h>
#include "AddInNative.h"
#include "TenzoM.h"
using namespace std;

static const u16string sClassName(u"TenzoM");
static const u16string sVersion(u"00.01");

static array<u16string, CAddInNative::eMethLast> osMethods =
{ 
	u"OpenPort",
	u"ClosePort",
	u"GetFixedBrutto",
	u"GetStatus",
	u"SetZero",
	u"GetNetto",
	u"GetBrutto",
	u"GetIndicatorText",
	u"SwitchToWeighing",
	u"Version",
};
static array<u16string, CAddInNative::eMethLast> osMethods_ru = 
{ 
	u"ОткрытьПорт", 
	u"ЗакрытьПорт", 
	u"ПолучитьФиксированныйВесБрутто", 
	u"ПолучитьСтатус", 
	u"ОбнулитьПоказанияВеса", 
	u"ПолучитьВесНетто",
	u"ПолучитьВесБрутто",
	u"ПолучитьЗначенияИндикаторов",
	u"ПереключитьВРежимВзвешивания",
	u"Версия"
};
static array<u16string, CAddInNative::ePropLast> osProps =
{ 
	u"Adr", 
	u"Calm", 
	u"Overload", 
	u"ErrorCode" 
};
static array<u16string, CAddInNative::ePropLast> osProps_ru = 
{ 
	u"АдресУстройства", 
	u"ВесСтабилен", 
	u"Перегрузка", 
	u"КодОшибки" 
};

AppCapabilities g_capabilities = eAppCapabilitiesInvalid;

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
	m_iMemory = 0;
	m_iConnect = 0;

	for (int i = 0; i < osProps.size(); i++)
	{
		tolowerStr(osProps[i]);
	}

	for (int i = 0; i < osProps_ru.size(); i++)
	{
		tolowerStr(osProps_ru[i]);
	}
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
long CAddInNative::FindProp(const WCHAR_T* wsPropName)
{
	u16string usPropName = (char16_t*)(wsPropName);
	tolowerStr(usPropName);

	auto it = find(osProps.begin(), osProps.end(), usPropName);
	if (it != osProps.end()) {
		return it - osProps.begin();
	}

	it = find(osProps_ru.begin(), osProps_ru.end(), usPropName);
	if (it != osProps_ru.end()) {
		return it - osProps_ru.begin();
	}

	return -1;
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
	case ePropAdr:
	{
		TV_VT(pvarPropVal) = VTYPE_I1;
		pvarPropVal->lVal  = tenzom.Adr;
		return true;
	};
	case ePropCalm:
	{
		TV_VT(pvarPropVal) = VTYPE_BOOL;
		pvarPropVal->lVal  = tenzom.Calm;
		return true;
	}
	case ePropErrorCode:
	{
		TV_VT(pvarPropVal) = VTYPE_UI8;
		pvarPropVal->lVal  = tenzom.LastError;
		return true;
	}
	case ePropOverload:
	{
		TV_VT(pvarPropVal) = VTYPE_BOOL;
		pvarPropVal->lVal  = tenzom.Overload;
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
	case ePropAdr:
	{
		tenzom.Adr = TV_I1(varPropVal);
		return true;
	}
	case ePropErrorCode:
	{
		tenzom.LastError = TV_UI8(varPropVal);
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
	switch (lPropNum)
	{
	case ePropAdr:
	case ePropCalm:
	case ePropErrorCode:
	case ePropOverload:
		return true;
	default:
		return false;
	}

	return false;
}
//---------------------------------------------------------------------------//
bool CAddInNative::IsPropWritable(const long lPropNum)
{
	switch (lPropNum)
	{
	case ePropAdr:
	case ePropErrorCode:
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
	u16string usMethodName = (char16_t*)(wsMethodName);
	tolowerStr(usMethodName);

	auto it = find(osMethods.begin(), osMethods.end(), usMethodName);
	if (it != osMethods.end()) {
		return it - osMethods.begin();
	}

	it = find(osMethods_ru.begin(), osMethods_ru.end(), usMethodName);
	if (it != osMethods_ru.end()) {
		return it - osMethods_ru.begin();
	}

	return -1;
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
	case eMethOpenPort:
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
	case eMethOpenPort:
	case eMethGetFixedBrutto:
	case eMethGetStatus:
	case eMethGetNetto:
	case eMethGetBrutto:
	case eMethGetIndicatorText:
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
	case eMethClosePort:
		tenzom.ClosePort();
		return true;
	case eMethOpenPort:
	{
		int   portNumber = paParams[0].intVal;
		DWORD bound      = paParams[1].intVal;
		BYTE  deviceAdr  = paParams[2].ui8Val;
		tenzom.OpenPort(portNumber, bound, deviceAdr);
		return true;
	}
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
		case eMethOpenPort:
		{
			int   portNumber = paParams[0].intVal;
			DWORD bound      = paParams[1].intVal;
			BYTE  deviceAdr  = paParams[2].ui8Val;

			TV_VT(pvarRetValue) = VTYPE_BOOL;
			pvarRetValue->bVal = tenzom.OpenPort(portNumber, bound, deviceAdr);

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
		case eMethGetIndicatorText:
		{
			auto text = tenzom.GetIndicatorText();
			std::basic_string<char16_t> wsIndicatorText = u"TODO!"; // TODO: GetIndicatorText !!!!

			if (m_iMemory->AllocMemory((void**)&pvarRetValue->pwstrVal, (wsIndicatorText.length() + 1) * sizeof(char16_t)))
			{
				memcpy(pvarRetValue->pwstrVal, wsIndicatorText.c_str(), (wsIndicatorText.length() + 1) * sizeof(char16_t));
				pvarRetValue->wstrLen = wsIndicatorText.length();
			}
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

void CAddInNative::version(tVariant * pvarRetValue)
{
	TV_VT(pvarRetValue) = VTYPE_PWSTR;
	
	if (m_iMemory->AllocMemory((void**)&pvarRetValue->pwstrVal, (sVersion.length() + 1) * sizeof(char16_t)))
	{
		memcpy(pvarRetValue->pwstrVal, sVersion.c_str(), (sVersion.length() + 1) * sizeof(char16_t));
		pvarRetValue->wstrLen = sVersion.length();
	}
}
