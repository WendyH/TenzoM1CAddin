#ifndef __ADDINNATIVE_H__
#define __ADDINNATIVE_H__

#include "include/ComponentBase.h"
#include "include/AddInDefBase.h"
#include "include/IMemoryManager.h"

#include <vector>
#include <map>
#include <array> 
#include <locale.h>

#include "TenzoM.h"

///////////////////////////////////////////////////////////////////////////////
// class CAddInNative
class CAddInNative : public IComponentBase
{
public:
    enum Props
    {
        ePropProtocol = 0,
        ePropIP,
        ePropNetPort,
        ePropNetMode,
        ePropName,
        ePropConnected,
        ePropAdr,
        ePropEvent,
        ePropNScal,
        ePropCalm,
        ePropOverload,
        ePropRiseExternalEvent,
        ePropError,
        ePropErrorCode,
        ePropEmulate,
        ePropEmulMinKg,
        ePropEmulMaxKg,
        ePropWriteLog,
        ePropLogFile,
        ePropDecimalPoint,
        ePropLast      // Always last
    };

    enum Methods
    {
        eMethConnect = 0,
        eMethDisconnect,
        eMethGetStatus,
        eMethSetZero,
        eMethGetWeight,
        eMethSwitchToWeighing,
        eMethGetGetEnteredCode,
        eMethGetIndicatorText,
        eMethSetIndicatorText,
        eMethSetInputChannel,
        eMethTare,
        eMethGetDeviceInfo,
        eMethGetPorts,
        eMethVersion,
        eMethGetIP,
        eMethLast      // Always last
    };

    CAddInNative(void);
    virtual ~CAddInNative();
    // IInitDoneBase
    virtual bool ADDIN_API Init(void*);
    virtual bool ADDIN_API setMemManager(void* mem);
    virtual long ADDIN_API GetInfo();
    virtual void ADDIN_API Done();
    // ILanguageExtenderBase
    virtual bool ADDIN_API RegisterExtensionAs(WCHAR_T**);
    virtual long ADDIN_API GetNProps();
    virtual long ADDIN_API FindProp(const WCHAR_T* wsPropName);
    virtual const WCHAR_T* ADDIN_API GetPropName(long lPropNum, long lPropAlias);
    virtual bool ADDIN_API GetPropVal(const long lPropNum, tVariant* pvarPropVal);
    virtual bool ADDIN_API SetPropVal(const long lPropNum, tVariant* varPropVal);
    virtual bool ADDIN_API IsPropReadable(const long lPropNum);
    virtual bool ADDIN_API IsPropWritable(const long lPropNum);
    virtual long ADDIN_API GetNMethods();
    virtual long ADDIN_API FindMethod(const WCHAR_T* wsMethodName);
    virtual const WCHAR_T* ADDIN_API GetMethodName(const long lMethodNum, 
                            const long lMethodAlias);
    virtual long ADDIN_API GetNParams(const long lMethodNum);
    virtual bool ADDIN_API GetParamDefValue(const long lMethodNum, const long lParamNum,
                            tVariant *pvarParamDefValue);   
    virtual bool ADDIN_API HasRetVal(const long lMethodNum);
    virtual bool ADDIN_API CallAsProc(const long lMethodNum,
                    tVariant* paParams, const long lSizeArray);
    virtual bool ADDIN_API CallAsFunc(const long lMethodNum,
                tVariant* pvarRetValue, tVariant* paParams, const long lSizeArray);
    // LocaleBase
    virtual void ADDIN_API SetLocale(const WCHAR_T* loc);
    
private:
    u16string GetParamString(const tVariant* param);
    void SetPropString(tVariant* pvarPropVal, u16string text);
    void addError(uint32_t wcode, const char16_t* source, const char16_t* descriptor, long code);
    void addError(const char16_t* descriptor);
    template <size_t N>
    long FindName(array<u16string, N>, const WCHAR_T* name);
    void CatchedException(exception_ptr eptr, u16string funcName);
    bool ExternalEvent(u16string message, u16string data);
    u16string GetOwnIP(bool all);

    wstring_convert<codecvt_utf8_utf16<char16_t>, char16_t> u16Convert;

    // Attributes
    IAddInDefBase      *m_iConnect;
    IMemoryManager     *m_iMemory;

    TenzoM tenzom;

    bool RiseExternalEvent;
};

#endif //__ADDINNATIVE_H__
