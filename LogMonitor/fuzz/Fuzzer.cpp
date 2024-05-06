#include <windows.h>
#include <jsonparser/json.h>

#ifdef __cplusplus
#define FUZZ_EXPORT extern "C" __declspec(dllexport)
#else
#define FUZZ_EXPORT __declspec(dllexport)
#endif
FUZZ_EXPORT int __cdecl LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    CJsonParser jsonParser;
    CJsonValue jsonValue;
    HRESULT hr = S_OK;
    const wchar_t* wideData = reinterpret_cast<const wchar_t*>(data);
    size_t wideSize = size / sizeof(wchar_t);
    hr = jsonParser.Initialize(wideData, wideSize, CJsonParser::JSON_ENABLECOMMENTS 
        | CJsonParser::JSON_ENABLE_RESJSON_CHECKS);

    if (FAILED(hr))
    {
        return 0; // always return 0
    }

    hr = jsonParser.Parse(&jsonValue);

    if (FAILED(hr))
    {
        return 0; // always return 0
    }

    return 0; // always return 0
}