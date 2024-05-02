#ifndef __ADDINNATIVE_H__
#define __ADDINNATIVE_H__

#include "ComponentBase.h"
#include "AddInDefBase.h"
#include "IMemoryManager.h"

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
        ePropWebPort,
        ePropConnected,
        ePropAdr,
        ePropCalm,
        ePropOverload,
        ePropError,
        ePropErrorCode,
        ePropEmulate,
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
        eMethGetPorts,
        eMethVersion,
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
    wstring GetParamString(tVariant* param);
    void   SetPropString(tVariant* pvarPropVal, wstring text);
    void   SetPropString(tVariant* pvarPropVal, u16string text);

    // Attributes
    IAddInDefBase      *m_iConnect;
    IMemoryManager     *m_iMemory;

    //wstring_convert<codecvt_utf8_utf16<char16_t, 0x10ffff, codecvt_mode::little_endian>, char16_t> u16Convert;
    wstring_convert<codecvt_utf8_utf16<char16_t>, char16_t> u16Convert;

    TenzoM tenzom;
};

#endif //__ADDINNATIVE_H__
