# ETW Monitor Documentation

To monitor ETW logs using Log Monitor you required either a valid provider name or a provider GUID. Specifying either the provider name or provider GUID or both in the json configuration will work.

The provider that you pass in the json config file provider must register with ETW and instrumented to generate manifest-based events. 
To read more on this go through this microsoft docs https://learn.microsoft.com/en-us/windows-hardware/test/weg/instrumenting-your-code-with-etw

To confirm if you have a valid provider name or valid provider GUID you can type the command below in command prompt or powershell and check if the provider name or provider GUID you have exists in the list.

```
logman query providers
```

 Example configurations below would work fine

```
{
  "LogConfig": {
    "sources": [
      {
        "type": "ETW",
        "eventFormatMultiLine": false,
        "providers": [
          {
            "providerName": "Microsoft-Windows-WLAN-Drive",
            "level": "Information"
          }
        ]
      }
    ]
  }
}
```

```
{
  "LogConfig": {
    "sources": [
      {
        "type": "ETW",
        "eventFormatMultiLine": false,
        "providers": [
          {
            "providerGuid": "DAA6A96B-F3E7-4D4D-A0D6-31A350E6A445",
            "level": "Information"
          }
        ]
      }
    ]
  }
}
```

```
{
  "LogConfig": {
    "sources": [
      {
        "type": "ETW",
        "eventFormatMultiLine": false,
        "providers": [
          {
            "providerName": "Microsoft-Windows-WLAN-Drive",
            "providerGuid": "DAA6A96B-F3E7-4D4D-A0D6-31A350E6A445",
            "level": "Information"
          }
        ]
      }
    ]
  }
}
```
